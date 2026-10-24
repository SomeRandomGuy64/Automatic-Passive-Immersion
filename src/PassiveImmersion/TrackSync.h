#pragma once

#include "PassiveImmersion/Settings.h"

#include <QDateTime>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QTimer>

#include <map>
#include <utility>
#include <vector>

class QNetworkReply;

namespace PassiveImmersion {

	// Logs passive listening time to fazon.nexus Track (https://fazon.nexus/developers/).
	// Time waits in a queue, saved to disk, until it reaches Track, so nothing is lost while offline or
	// if the app closes before a send. The queue is sent every few minutes and when playback stops.
	class TrackSync : public QObject {

		Q_OBJECT

	public:
		explicit TrackSync(const Settings& settings, QObject* parent = nullptr);

		void applySettings(const Settings& settings);

		void add(const QString& fileName, const QDateTime& start, const QDateTime& end);

		// Starts sending the queue now (if there's anything to send and nothing on its way already)
		void sync();

		// For quitting: sends what it can, waiting at most timeoutMs
		void flush(int timeoutMs);

		inline bool isEnabled() const { return m_settings.trackEnabled; }
		QString getStatus() const;
		inline bool hasProblem() const { return !m_error.isEmpty() || !m_warning.isEmpty(); }

		// Track weeks run from Sunday 03:00 UTC, and are named after the Saturday before
		static QString getWeekKey(const QDateTime& time);
		static QDateTime getWeekStart(const QDateTime& time);

	signals:
		void statusChanged();
		void idle();	// nothing more will be sent until the next sync()

	private:
		using QueueKey = std::pair<QString, QString>;	// week, sub-category name ("" = unassigned)

		struct SentEntry {

			QueueKey key{};
			qint64 ms{};

		};

		void m_sendNext();
		void m_fetchSubcategories();
		void m_createSubcategory(const QString& name);
		void m_sendWeek(const QString& week);

		QNetworkReply* m_request(const QByteArray& method, const QString& path, const QByteArray& body = {});
		void m_onReplyFinished(QNetworkReply* reply);
		bool m_handleCommonErrors(QNetworkReply* reply, int status, const QString& message);

		QString m_getSubcategoryFor(const QString& fileName, const QDate& logDate) const;
		bool m_isUnavailable(const QString& name) const;
		bool m_canSend() const;
		void m_stop(const QString& error = {});

		void m_load();
		void m_save() const;

	private:
		Settings m_settings{};
		QNetworkAccessManager m_network{};
		QPointer<QNetworkReply> m_reply{};
		QTimer m_syncTimer{};
		QTimer m_retryTimer{};

		std::map<QueueKey, qint64> m_queue{};		// milliseconds still to send
		std::vector<SentEntry> m_inFlight{};		// what the PATCH on its way will take off the queue

		bool m_subcategoriesFetched{ false };
		bool m_refetchedSubcategories{ false };		// only retry once after a "no such sub-category"
		QSet<QString> m_knownSubcategories{};		// lowercase names of the active passive sub-categories
		QSet<QString> m_unavailableSubcategories{};	// couldn't be created, their time goes to unassigned

		bool m_blocked{ false };					// the key was refused, wait for new settings
		QString m_error{};
		QString m_warning{};
		QDateTime m_lastSent{};

	};

}
