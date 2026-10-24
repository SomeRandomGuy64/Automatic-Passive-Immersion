#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <QUrl>

class QNetworkReply;

namespace PassiveImmersion {

	// Asks GitHub for the latest release once a day and says when it's newer than this build
	class UpdateChecker : public QObject {

		Q_OBJECT

	public:
		struct Release {

			QString version{};		// "0.2.0"
			QString notes{};		// the release description, Markdown
			QUrl pageUrl{};
			QUrl downloadUrl{};		// this OS's archive, or the release page if it has none

		};

		explicit UpdateChecker(QObject* parent = nullptr);

		void setEnabled(bool enabled);

		// userAsked: also report "you're up to date" and failures, instead of staying quiet
		void check(bool userAsked = false);

		static QString getReleasesUrl();

	signals:
		void updateAvailable(const Release& release, bool userAsked);
		void upToDate();
		void checkFailed(const QString& message);

	private:
		void m_onReplyFinished(QNetworkReply* reply, bool userAsked);

	private:
		QNetworkAccessManager m_network{};
		QPointer<QNetworkReply> m_reply{};
		QTimer m_dailyTimer{};
		QTimer m_firstCheckTimer{};

	};

}
