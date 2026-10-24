#include "HourLog.h"

#include "PassiveImmersion/Settings.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTextStream>

#include <algorithm>

namespace PassiveImmersion {

	static constexpr qint64 s_msPerMinute{ 60'000 };
	static constexpr qint64 s_msPerHour{ 60 * s_msPerMinute };

	static QString quoteCsv(const QString& text) {

		if (!text.contains(',') && !text.contains('"') && !text.contains('\n')) {
			return text;
		}

		QString quoted{ text };
		quoted.replace('"', "\"\"");
		return '"' + quoted + '"';

	}

	// Decimal hours plus h:mm, so the file works both in a spreadsheet and when read by eye
	static QString formatCsvHours(qint64 ms) {

		qint64 minutes{ (ms + s_msPerMinute / 2) / s_msPerMinute };
		return QString{ "%1,%2:%3" }
			.arg(static_cast<double>(ms) / s_msPerHour, 0, 'f', 2)
			.arg(minutes / 60)
			.arg(minutes % 60, 2, 10, QChar{ '0' });

	}

	HourLog::HourLog(QObject* parent)
		: QObject{ parent }
	{
		m_load();
	}

	void HourLog::setDayStart(const QTime& dayStart) {

		if (dayStart == m_dayStart) {
			return;
		}

		m_dayStart = dayStart;
		emit changed();

	}

	void HourLog::add(const QString& fileName, const QDateTime& start, const QDateTime& end) {

		// Split the stretch wherever it crosses into the next log day
		QDateTime from{ start.toLocalTime() };
		QDateTime until{ end.toLocalTime() };

		while (from < until) {
			QDate date{ getLogDate(from) };
			QDateTime nextDay{ date.addDays(1), m_dayStart };
			QDateTime pieceEnd{ std::min(until, nextDay) };

			qint64 ms{ from.msecsTo(pieceEnd) };
			if (ms <= 0) {
				break;
			}

			m_days[date][fileName] += ms;
			m_totalMs += ms;
			from = pieceEnd;
		}

		m_save();
		emit changed();

	}

	QDate HourLog::getLogDate(const QDateTime& time) const {

		QDateTime local{ time.toLocalTime() };
		QDate date{ local.date() };
		return local.time() < m_dayStart ? date.addDays(-1) : date;

	}

	qint64 HourLog::getTodayMs() const {

		auto day{ m_days.find(getLogDate(QDateTime::currentDateTime())) };
		if (day == m_days.end()) {
			return 0;
		}

		qint64 total{};
		for (const auto& [file, ms] : day->second) {
			total += ms;
		}
		return total;

	}

	bool HourLog::exportCsv(const QString& path, ExportFormat format) const {

		QSaveFile file{ path };
		if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
			return false;
		}

		QTextStream out{ &file };

		if (format == ExportFormat::DailyTotals) {
			out << "date,hours,time\n";
			for (const auto& [date, files] : m_days) {
				qint64 total{};
				for (const auto& [name, ms] : files) {
					total += ms;
				}
				out << date.toString(Qt::ISODate) << ',' << formatCsvHours(total) << '\n';
			}
		}
		else {
			out << "date,file,hours,time\n";
			for (const auto& [date, files] : m_days) {
				for (const auto& [name, ms] : files) {
					out << date.toString(Qt::ISODate) << ',' << quoteCsv(name) << ',' << formatCsvHours(ms) << '\n';
				}
			}
		}

		out.flush();
		return file.commit();

	}

	QString HourLog::getFilePath() {
		return QDir{ Settings::getDataDirectory() }.filePath("passive-hours.json");
	}

	QString HourLog::formatDuration(qint64 ms) {

		qint64 minutes{ ms / s_msPerMinute };
		if (minutes < 60) {
			return QString{ "%1m" }.arg(minutes);
		}
		return QString{ "%1h %2m" }.arg(minutes / 60).arg(minutes % 60, 2, 10, QChar{ '0' });

	}

	void HourLog::m_load() {

		QFile file{ getFilePath() };
		if (!file.open(QIODevice::ReadOnly)) {
			return;
		}

		QJsonObject days{ QJsonDocument::fromJson(file.readAll()).object().value("days").toObject() };

		for (auto day{ days.begin() }; day != days.end(); ++day) {
			QDate date{ QDate::fromString(day.key(), Qt::ISODate) };
			if (!date.isValid()) {
				continue;
			}

			QJsonObject files{ day.value().toObject() };
			for (auto entry{ files.begin() }; entry != files.end(); ++entry) {
				qint64 ms{ static_cast<qint64>(entry.value().toDouble()) };
				m_days[date][entry.key()] += ms;
				m_totalMs += ms;
			}
		}

	}

	bool HourLog::m_save() const {

		QJsonObject days{};
		for (const auto& [date, files] : m_days) {
			QJsonObject fileTimes{};
			for (const auto& [name, ms] : files) {
				fileTimes.insert(name, static_cast<double>(ms));
			}
			days.insert(date.toString(Qt::ISODate), fileTimes);
		}

		QJsonObject root{
			{ "version", 1 },
			{ "unit", "milliseconds" },
			{ "days", days },
		};

		QSaveFile file{ getFilePath() };
		if (!file.open(QIODevice::WriteOnly)) {
			return false;
		}

		file.write(QJsonDocument{ root }.toJson(QJsonDocument::Indented));
		return file.commit();

	}

}
