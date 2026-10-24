#include "TrackSync.h"

#include "PassiveImmersion/Core.h"
#include "PassiveImmersion/HourLog.h"

#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QTimeZone>

#include <algorithm>
#include <array>

namespace PassiveImmersion {

	static constexpr const char* s_category{ "passive" };

	// Track counts hundredths of an hour (36 seconds), anything smaller waits in the queue for more
	static constexpr qint64 s_msPerHundredth{ 36'000 };

	static constexpr int s_syncIntervalMs{ 5 * 60'000 };
	static constexpr int s_requestTimeoutMs{ 15'000 };
	static constexpr int s_maxSubcategoryLength{ 40 };

	// English whatever the system language is, so the sub-categories match the site's wording
	static constexpr std::array s_dayNames{ "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday" };

	static QString getQueuePath() {
		return QDir{ Settings::getDataDirectory() }.filePath("track-queue.json");
	}

	// The site's rules for a name: 1-40 characters, trimmed, no control characters
	static QString cleanSubcategoryName(const QString& text) {

		QString name{};
		for (QChar character : text) {
			name.append(character.category() == QChar::Other_Control ? QChar{ ' ' } : character);
		}
		return name.simplified().left(s_maxSubcategoryLength).trimmed();

	}

	TrackSync::TrackSync(const Settings& settings, QObject* parent)
		: QObject{ parent }
		, m_settings{ settings }
	{
		m_syncTimer.setInterval(s_syncIntervalMs);
		m_retryTimer.setSingleShot(true);

		connect(&m_syncTimer, &QTimer::timeout, this, &TrackSync::sync);
		connect(&m_retryTimer, &QTimer::timeout, this, &TrackSync::sync);

		m_load();
		m_syncTimer.start();

		// Whatever was left over from last time
		QTimer::singleShot(0, this, &TrackSync::sync);
	}

	void TrackSync::applySettings(const Settings& settings) {

		bool connectionChanged{
			settings.trackEnabled != m_settings.trackEnabled
			|| settings.trackApiKey != m_settings.trackApiKey
			|| settings.trackApiUrl != m_settings.trackApiUrl
		};
		m_settings = settings;

		if (connectionChanged) {
			// A new key or server gets a clean slate
			m_blocked = false;
			m_error.clear();
			m_warning.clear();
			m_subcategoriesFetched = false;
			m_knownSubcategories.clear();
			m_unavailableSubcategories.clear();
			sync();
		}

		emit statusChanged();

	}

	void TrackSync::add(const QString& fileName, const QDateTime& start, const QDateTime& end) {

		if (!m_settings.trackEnabled) {
			return;
		}

		// Split the stretch wherever it crosses into the next Track week, or the next day of the hour log
		// (which changes the sub-category when it's the day of the week)
		QDateTime from{ start.toUTC() };
		QDateTime until{ end.toUTC() };

		while (from < until) {
			QDate logDate{ HourLog::getLogDate(from, m_settings.dayStartTime) };
			QDateTime nextLogDay{ QDateTime{ logDate.addDays(1), m_settings.dayStartTime }.toUTC() };
			QDateTime nextWeek{ getWeekStart(from).addDays(7) };
			QDateTime pieceEnd{ std::min({ until, nextWeek, nextLogDay }) };
			QString subcategory{ m_getSubcategoryFor(fileName, logDate) };

			qint64 ms{ from.msecsTo(pieceEnd) };
			if (ms <= 0) {
				break;
			}

			m_queue[{ getWeekKey(from), subcategory }] += ms;
			from = pieceEnd;
		}

		m_save();
		emit statusChanged();

	}

	void TrackSync::sync() {
		m_sendNext();
	}

	void TrackSync::flush(int timeoutMs) {

		m_sendNext();
		if (m_reply == nullptr) {
			return;
		}

		QEventLoop loop{};
		QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);
		connect(this, &TrackSync::idle, &loop, &QEventLoop::quit);
		loop.exec();

	}

	QString TrackSync::getStatus() const {

		if (!m_settings.trackEnabled) {
			return {};
		}

		if (m_settings.trackApiKey.trimmed().isEmpty()) {
			return "Paste a Read & write API key from your fazon.nexus profile (Developers section).";
		}

		QStringList lines{};
		if (!m_error.isEmpty()) {
			lines.append(m_error);
		}
		if (!m_warning.isEmpty()) {
			lines.append(m_warning);
		}

		qint64 waitingMs{};
		for (const auto& [key, ms] : m_queue) {
			waitingMs += ms;
		}

		QString state{ m_lastSent.isValid()
			? QString{ "Last sent at %1." }.arg(m_lastSent.toLocalTime().toString("HH:mm"))
			: QString{ "Nothing sent yet this session." } };
		if (waitingMs >= 60'000) {
			state += QString{ " %1 min waiting to be sent." }.arg(waitingMs / 60'000);
		}
		lines.append(state);

		return lines.join('\n');

	}

	QDateTime TrackSync::getWeekStart(const QDateTime& time) {

		// Anything before 03:00 UTC on a Sunday still belongs to the week before
		QDate date{ time.toUTC().addSecs(-3 * 3600).date() };
		QDate sunday{ date.addDays(-(date.dayOfWeek() % 7)) };
		return QDateTime{ sunday, QTime{ 3, 0 }, QTimeZone::UTC };

	}

	QString TrackSync::getWeekKey(const QDateTime& time) {
		return getWeekStart(time).date().addDays(-1).toString(Qt::ISODate);
	}

	void TrackSync::m_sendNext() {

		if (m_reply != nullptr) {
			return;
		}

		if (!m_canSend()) {
			emit idle();
			return;
		}

		// Every named sub-category has to exist on Track before hours can be logged to it
		for (const auto& [key, ms] : m_queue) {
			const QString& name{ key.second };
			if (ms < s_msPerHundredth || name.isEmpty() || m_isUnavailable(name)) {
				continue;
			}

			if (!m_subcategoriesFetched) {
				m_fetchSubcategories();
				return;
			}
			if (!m_knownSubcategories.contains(name.toLower())) {
				m_createSubcategory(name);
				return;
			}
		}

		// Then the oldest week first, one request per week
		for (const auto& [key, ms] : m_queue) {
			if (ms >= s_msPerHundredth) {
				m_sendWeek(key.first);
				return;
			}
		}

		emit idle();

	}

	void TrackSync::m_fetchSubcategories() {
		m_reply = m_request("GET", "/v1/api/track/subcats");
	}

	void TrackSync::m_createSubcategory(const QString& name) {

		QJsonObject body{ { "category", s_category }, { "name", name } };
		m_reply = m_request("POST", "/v1/api/track/subcats", QJsonDocument{ body }.toJson(QJsonDocument::Compact));
		m_reply->setProperty("subcategory", name);

	}

	void TrackSync::m_sendWeek(const QString& week) {

		QJsonArray entries{};
		m_inFlight.clear();

		for (const auto& [key, ms] : m_queue) {
			if (key.first != week || ms < s_msPerHundredth) {
				continue;
			}

			// Only whole hundredths go now, the remainder stays queued so no time is lost to rounding
			qint64 hundredths{ ms / s_msPerHundredth };
			m_inFlight.push_back({ key, hundredths * s_msPerHundredth });

			QJsonObject entry{
				{ "category", s_category },
				{ "add", static_cast<double>(hundredths) / 100.0 },
			};
			if (!key.second.isEmpty() && !m_isUnavailable(key.second)) {
				entry.insert("subcat", key.second);
			}
			entries.append(entry);
		}

		QJsonObject body{ { "entries", entries } };
		m_reply = m_request("PATCH", QString{ "/v1/api/track/weeks/%1/hours" }.arg(week), QJsonDocument{ body }.toJson(QJsonDocument::Compact));

	}

	QNetworkReply* TrackSync::m_request(const QByteArray& method, const QString& path, const QByteArray& body) {

		QString baseUrl{ m_settings.trackApiUrl.trimmed() };
		while (baseUrl.endsWith('/')) {
			baseUrl.chop(1);
		}

		QNetworkRequest request{ QUrl{ baseUrl + path } };
		request.setRawHeader("Authorization", "Bearer " + m_settings.trackApiKey.trimmed().toUtf8());
		request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
		request.setHeader(QNetworkRequest::UserAgentHeader, QString{ "%1/%2" }.arg(s_appId, PI_VERSION));
		request.setTransferTimeout(s_requestTimeoutMs);

		QNetworkReply* reply{ m_network.sendCustomRequest(request, method, body) };
		reply->setProperty("method", method);
		connect(reply, &QNetworkReply::finished, this, [this, reply]() { m_onReplyFinished(reply); });
		return reply;

	}

	void TrackSync::m_onReplyFinished(QNetworkReply* reply) {

		reply->deleteLater();
		m_reply.clear();

		int status{ reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() };
		QJsonObject json{ QJsonDocument::fromJson(reply->readAll()).object() };
		QString message{ json.value("error").toString() };
		QByteArray method{ reply->property("method").toByteArray() };

		if (m_handleCommonErrors(reply, status, message)) {
			m_inFlight.clear();
			return;
		}

		if (method == "GET") {
			m_knownSubcategories.clear();
			for (const QJsonValue& value : json.value("subcats").toArray()) {
				QJsonObject subcategory{ value.toObject() };
				if (subcategory.value("category").toString() == s_category && !subcategory.value("archived").toBool()) {
					m_knownSubcategories.insert(subcategory.value("name").toString().toLower());
				}
			}
			m_subcategoriesFetched = true;
		}
		else if (method == "POST") {
			QString name{ reply->property("subcategory").toString() };

			if (status == 201) {
				m_knownSubcategories.insert(name.toLower());
			}
			else if (status == 409 && message.contains("already")) {
				// It exists after all (made on the site since the list was fetched)
				m_knownSubcategories.insert(name.toLower());
			}
			else {
				// Most likely the 30 sub-categories per category limit. The time still counts, as unassigned.
				m_unavailableSubcategories.insert(name.toLower());
				m_warning = QString{ "Couldn't create the sub-category \"%1\" (%2), logging it as unassigned." }
					.arg(name, message.isEmpty() ? QString{ "HTTP %1" }.arg(status) : message);
			}
		}
		else if (status == 200) {
			for (const SentEntry& sent : m_inFlight) {
				auto entry{ m_queue.find(sent.key) };
				if (entry != m_queue.end()) {
					entry->second -= sent.ms;
					if (entry->second <= 0) {
						m_queue.erase(entry);
					}
				}
			}

			m_inFlight.clear();
			m_lastSent = QDateTime::currentDateTimeUtc();
			m_error.clear();
			m_refetchedSubcategories = false;
			m_save();
		}
		else if (status == 400 && message.contains("no sub-category named") && !m_refetchedSubcategories) {
			// Renamed or archived on the site since the list was fetched
			m_inFlight.clear();
			m_subcategoriesFetched = false;
			m_refetchedSubcategories = true;
		}
		else {
			m_inFlight.clear();
			m_stop(QString{ "Track refused the hours: %1" }.arg(message.isEmpty() ? QString{ "HTTP %1" }.arg(status) : message));
			return;
		}

		emit statusChanged();
		m_sendNext();

	}

	bool TrackSync::m_handleCommonErrors(QNetworkReply* reply, int status, const QString& message) {

		if (status == 0) {
			m_stop(QString{ "Couldn't reach Track (%1), will try again." }.arg(reply->errorString()));
			return true;
		}

		if (status == 401) {
			m_blocked = true;
			m_stop("Track didn't accept the API key. Check it, or create a new one on your profile.");
			return true;
		}

		if (status == 403) {
			m_blocked = true;
			m_stop("This API key is read-only. Create a Read & write key on your profile.");
			return true;
		}

		if (status == 429) {
			int waitSeconds{ std::max(1, reply->rawHeader("Retry-After").toInt()) };
			m_retryTimer.start(waitSeconds * 1000);
			m_stop(QString{ "Sending too fast, waiting %1 s." }.arg(waitSeconds));
			return true;
		}

		if (status >= 500) {
			m_stop(QString{ "Track had a problem (%1), will try again." }.arg(message.isEmpty() ? QString{ "HTTP %1" }.arg(status) : message));
			return true;
		}

		return false;

	}

	QString TrackSync::m_getSubcategoryFor(const QString& fileName, const QDate& logDate) const {

		switch (m_settings.trackSubcategory) {
			case Settings::TrackSubcategory::None:
				return {};

			case Settings::TrackSubcategory::Fixed:
				return cleanSubcategoryName(m_settings.trackSubcategoryName);

			case Settings::TrackSubcategory::FileName:
				return cleanSubcategoryName(fileName);

			case Settings::TrackSubcategory::DayOfWeek:
				return s_dayNames[logDate.dayOfWeek() - 1];
		}

		return {};

	}

	bool TrackSync::m_isUnavailable(const QString& name) const {
		return m_unavailableSubcategories.contains(name.toLower());
	}

	bool TrackSync::m_canSend() const {
		return m_settings.trackEnabled && !m_settings.trackApiKey.trimmed().isEmpty() && !m_blocked && !m_queue.empty();
	}

	void TrackSync::m_stop(const QString& error) {

		m_error = error;
		emit statusChanged();
		emit idle();

	}

	void TrackSync::m_load() {

		QFile file{ getQueuePath() };
		if (!file.open(QIODevice::ReadOnly)) {
			return;
		}

		for (const QJsonValue& value : QJsonDocument::fromJson(file.readAll()).object().value("queue").toArray()) {
			QJsonObject entry{ value.toObject() };
			qint64 ms{ static_cast<qint64>(entry.value("ms").toDouble()) };
			if (ms > 0) {
				m_queue[{ entry.value("week").toString(), entry.value("subcat").toString() }] += ms;
			}
		}

	}

	void TrackSync::m_save() const {

		QJsonArray queue{};
		for (const auto& [key, ms] : m_queue) {
			queue.append(QJsonObject{
				{ "week", key.first },
				{ "subcat", key.second },
				{ "ms", static_cast<double>(ms) },
			});
		}

		QSaveFile file{ getQueuePath() };
		if (file.open(QIODevice::WriteOnly)) {
			file.write(QJsonDocument{ QJsonObject{ { "queue", queue } } }.toJson(QJsonDocument::Indented));
			file.commit();
		}

	}

}
