#include "LinuxMediaDetector.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusVariant>

namespace PassiveImmersion {

	static const QString s_mprisPrefix{ "org.mpris.MediaPlayer2." };

	// A frozen player shouldn't be able to freeze the GUI along with it
	static constexpr int s_dbusTimeoutMs{ 250 };

	std::unique_ptr<MediaDetector> MediaDetector::create() {
		return std::make_unique<LinuxMediaDetector>();
	}

	// "org.mpris.MediaPlayer2.firefox.instance_1_42" -> "firefox"
	static QString getPlayerName(const QString& service) {
		return service.mid(s_mprisPrefix.size()).section('.', 0, 0);
	}

	std::vector<MediaSource> LinuxMediaDetector::getPlayingSources() {

		std::vector<MediaSource> sources{};

		QDBusConnection bus{ QDBusConnection::sessionBus() };
		if (!bus.isConnected()) {
			return sources;
		}

		QDBusReply<QStringList> services{ bus.interface()->registeredServiceNames() };
		if (!services.isValid()) {
			return sources;
		}

		for (const QString& service : services.value()) {
			if (!service.startsWith(s_mprisPrefix)) {
				continue;
			}

			QDBusMessage request{ QDBusMessage::createMethodCall(
				service,
				"/org/mpris/MediaPlayer2",
				"org.freedesktop.DBus.Properties",
				"Get"
			) };
			request << "org.mpris.MediaPlayer2.Player" << "PlaybackStatus";

			QDBusReply<QDBusVariant> status{ bus.call(request, QDBus::Block, s_dbusTimeoutMs) };
			if (!status.isValid() || status.value().variant().toString() != "Playing") {
				continue;
			}

			QDBusReply<uint> processId{ bus.interface()->servicePid(service) };
			sources.push_back({ getPlayerName(service), processId.isValid() ? processId.value() : 0 });
		}

		return sources;

	}

}
