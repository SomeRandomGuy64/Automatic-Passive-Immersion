#include "UpdateChecker.h"

#include "PassiveImmersion/Core.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QVersionNumber>

namespace PassiveImmersion {

	static constexpr const char* s_repository{ "SomeRandomGuy64/Automatic-Passive-Immersion" };

	// Not straight away: right after login the network often isn't up yet
	static constexpr int s_firstCheckDelayMs{ 30'000 };
	static constexpr int s_checkIntervalMs{ 24 * 60 * 60'000 };
	static constexpr int s_requestTimeoutMs{ 15'000 };

	// The archive names the release workflow uploads
	static const char* getAssetName() {

#if defined(PI_PLATFORM_WINDOWS)
		return "AutomaticPassiveImmersion-windows-x64.zip";
#elif defined(PI_PLATFORM_MACOS)
		return "AutomaticPassiveImmersion-macos-universal.zip";
#else
		return "AutomaticPassiveImmersion-linux-x64.tar.gz";
#endif

	}

	UpdateChecker::UpdateChecker(QObject* parent)
		: QObject{ parent }
	{
		m_dailyTimer.setInterval(s_checkIntervalMs);
		m_firstCheckTimer.setInterval(s_firstCheckDelayMs);
		m_firstCheckTimer.setSingleShot(true);

		connect(&m_dailyTimer, &QTimer::timeout, this, [this]() { check(); });
		connect(&m_firstCheckTimer, &QTimer::timeout, this, [this]() { check(); });
	}

	void UpdateChecker::setEnabled(bool enabled) {

		if (enabled == m_dailyTimer.isActive()) {
			return;
		}

		if (enabled) {
			m_firstCheckTimer.start();
			m_dailyTimer.start();
		}
		else {
			m_firstCheckTimer.stop();
			m_dailyTimer.stop();
		}

	}

	void UpdateChecker::check(bool userAsked) {

		if (m_reply != nullptr) {
			return;
		}

		QNetworkRequest request{ QUrl{ QString{ "https://api.github.com/repos/%1/releases/latest" }.arg(s_repository) } };
		request.setRawHeader("Accept", "application/vnd.github+json");
		request.setHeader(QNetworkRequest::UserAgentHeader, QString{ "%1/%2" }.arg(s_appId, PI_VERSION));
		request.setTransferTimeout(s_requestTimeoutMs);

		QNetworkReply* reply{ m_network.get(request) };
		m_reply = reply;
		connect(reply, &QNetworkReply::finished, this, [this, reply, userAsked]() { m_onReplyFinished(reply, userAsked); });

	}

	QString UpdateChecker::getReleasesUrl() {
		return QString{ "https://github.com/%1/releases" }.arg(s_repository);
	}

	void UpdateChecker::m_onReplyFinished(QNetworkReply* reply, bool userAsked) {

		reply->deleteLater();
		m_reply.clear();

		if (reply->error() != QNetworkReply::NoError) {
			if (userAsked) {
				emit checkFailed(reply->errorString());
			}
			return;
		}

		QJsonObject json{ QJsonDocument::fromJson(reply->readAll()).object() };

		QString tag{ json.value("tag_name").toString() };
		QVersionNumber latest{ QVersionNumber::fromString(tag.startsWith('v') ? tag.mid(1) : tag) };
		QVersionNumber current{ QVersionNumber::fromString(PI_VERSION) };

		if (latest.isNull()) {
			if (userAsked) {
				emit checkFailed("GitHub's answer didn't include a version.");
			}
			return;
		}

		if (latest <= current) {
			if (userAsked) {
				emit upToDate();
			}
			return;
		}

		Release release{};
		release.version = latest.toString();
		release.notes = json.value("body").toString();
		release.pageUrl = QUrl{ json.value("html_url").toString() };
		release.downloadUrl = release.pageUrl;

		for (const QJsonValue& value : json.value("assets").toArray()) {
			QJsonObject asset{ value.toObject() };
			if (asset.value("name").toString() == getAssetName()) {
				release.downloadUrl = QUrl{ asset.value("browser_download_url").toString() };
			}
		}

		emit updateAvailable(release, userAsked);

	}

}
