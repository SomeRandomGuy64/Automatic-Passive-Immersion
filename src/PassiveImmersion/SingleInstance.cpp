#include "SingleInstance.h"

#include "PassiveImmersion/Core.h"

#include <QLocalSocket>

namespace PassiveImmersion {

	// Per-user, so two people logged into the same machine don't block each other
	static QString getServerName() {

		QString user{ qEnvironmentVariable("USER", qEnvironmentVariable("USERNAME")) };
		return QString{ "%1-%2" }.arg(s_appId, user);

	}

	SingleInstance::SingleInstance(QObject* parent)
		: QObject{ parent }
	{
		connect(&m_server, &QLocalServer::newConnection, this, [this]() {
			while (QLocalSocket* socket{ m_server.nextPendingConnection() }) {
				socket->deleteLater();
				emit activationRequested();
			}
		});
	}

	bool SingleInstance::tryLock() {

		QString name{ getServerName() };

		QLocalSocket probe{};
		probe.connectToServer(name);
		if (probe.waitForConnected(500)) {
			probe.disconnectFromServer();
			return false;
		}

		// Nobody answered, so anything left at that name is from a crashed instance
		QLocalServer::removeServer(name);
		return m_server.listen(name);

	}

}
