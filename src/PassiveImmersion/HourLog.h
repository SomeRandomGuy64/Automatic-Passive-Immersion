#pragma once

#include <QDate>
#include <QDateTime>
#include <QObject>
#include <QString>
#include <QTime>

#include <map>

namespace PassiveImmersion {

	// How long passive audio has played, per day and per file. Kept in a JSON file in the data directory
	// and saved after every change, so a crash loses at most the last half-minute.
	class HourLog : public QObject {

		Q_OBJECT

	public:
		enum class ExportFormat {
			DailyTotals,	// date, hours
			PerFile			// date, file, hours
		};

		explicit HourLog(QObject* parent = nullptr);

		// Listening before this time of day counts towards the day before. Only affects time logged from now on.
		void setDayStart(const QTime& dayStart);

		void add(const QString& fileName, const QDateTime& start, const QDateTime& end);

		// Which day of the log a moment belongs to
		inline QDate getLogDate(const QDateTime& time) const { return getLogDate(time, m_dayStart); }
		static QDate getLogDate(const QDateTime& time, const QTime& dayStart);

		qint64 getTodayMs() const;
		inline qint64 getTotalMs() const { return m_totalMs; }

		bool exportCsv(const QString& path, ExportFormat format) const;

		static QString getFilePath();

		// "1h 05m", or "23m" under an hour
		static QString formatDuration(qint64 ms);

	signals:
		void changed();

	private:
		void m_load();
		bool m_save() const;

	private:
		QTime m_dayStart{ 0, 0 };
		std::map<QDate, std::map<QString, qint64>> m_days{};	// date -> file -> milliseconds
		qint64 m_totalMs{};

	};

}
