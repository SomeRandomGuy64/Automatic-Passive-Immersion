#include "PassiveImmersion/Core.h"
#include "PassiveImmersion/HourLog.h"
#include "PassiveImmersion/ImmersionController.h"
#include "PassiveImmersion/MainWindow.h"
#include "PassiveImmersion/MediaDetector.h"
#include "PassiveImmersion/Settings.h"
#include "PassiveImmersion/SingleInstance.h"
#include "PassiveImmersion/TrackSync.h"
#include "PassiveImmersion/UpdateChecker.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QSystemTrayIcon>
#include <QTextStream>

#ifdef PI_PLATFORM_WINDOWS
	#ifndef NOMINMAX
		#define NOMINMAX
	#endif
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <windows.h>
	#include <cstdio>
#else
	#include <QSocketNotifier>
	#include <csignal>
	#include <sys/socket.h>
	#include <unistd.h>
#endif

using namespace PassiveImmersion;

#ifndef PI_PLATFORM_WINDOWS
	// kill, logout and Ctrl+C would otherwise end the app instantly and leave mpv playing.
	// A signal handler can't safely touch Qt, so it writes a byte to a socket that Qt is watching instead.
	static int s_signalSockets[2]{};

	static void installQuitSignalHandlers(QApplication& app) {

		if (socketpair(AF_UNIX, SOCK_STREAM, 0, s_signalSockets) != 0) {
			return;
		}

		auto* notifier{ new QSocketNotifier{ s_signalSockets[1], QSocketNotifier::Read, &app } };
		QObject::connect(notifier, &QSocketNotifier::activated, &app, &QApplication::quit);

		struct sigaction action{};
		action.sa_handler = [](int) {
			char byte{ 1 };
			[[maybe_unused]] ssize_t written{ ::write(s_signalSockets[0], &byte, 1) };
		};
		sigemptyset(&action.sa_mask);

		for (int quitSignal : { SIGTERM, SIGINT, SIGHUP }) {
			sigaction(quitSignal, &action, nullptr);
		}

	}
#endif

// --list-playing: shows what the media detector sees, for checking detection works on a new machine/OS
static int listPlayingSources() {

#ifdef PI_PLATFORM_WINDOWS
	// GUI apps have no console of their own on Windows, so borrow the one we were launched from
	if (AttachConsole(ATTACH_PARENT_PROCESS)) {
		FILE* console{ nullptr };
		freopen_s(&console, "CONOUT$", "w", stdout);
	}
#endif

	std::unique_ptr<MediaDetector> detector{ MediaDetector::create() };
	std::vector<MediaSource> sources{ detector->getPlayingSources() };

	QTextStream out{ stdout };
	out << detector->getDescription() << '\n';

	if (sources.empty()) {
		out << "Nothing is playing.\n";
	}
	for (const MediaSource& source : sources) {
		out << "  " << source.name << " (PID " << source.processId << ")\n";
	}

	return 0;

}

int main(int argc, char* argv[]) {

	QApplication app{ argc, argv };
	QApplication::setApplicationName(s_appId);
	QApplication::setApplicationDisplayName(s_appDisplayName);
	QApplication::setApplicationVersion(PI_VERSION);

	QCommandLineParser parser{};
	parser.setApplicationDescription("Plays passive immersion audio and pauses it whenever other media is playing.");
	parser.addHelpOption();
	parser.addVersionOption();

	QCommandLineOption minimisedOption{ { "m", "minimised" }, "Start hidden in the system tray." };
	QCommandLineOption listPlayingOption{ "list-playing", "Print which apps are detected as playing media, then exit." };
	parser.addOption(minimisedOption);
	parser.addOption(listPlayingOption);
	parser.process(app);

	if (parser.isSet(listPlayingOption)) {
		return listPlayingSources();
	}

	SingleInstance instance{};
	if (!instance.tryLock()) {
		// The running copy has been told to show its window
		return 0;
	}

#ifndef PI_PLATFORM_WINDOWS
	installQuitSignalHandlers(app);
#endif

	bool hasTray{ QSystemTrayIcon::isSystemTrayAvailable() };

	// The window hides into the tray, so closing it mustn't end the app
	app.setQuitOnLastWindowClosed(!hasTray);

	Settings settings{ Settings::load() };

	// Declared before the controller so they outlive it: it reports its last bit of listening as it shuts down
	HourLog hourLog{};
	hourLog.setDayStart(settings.dayStartTime);
	TrackSync trackSync{ settings };
	UpdateChecker updateChecker{};
	updateChecker.setEnabled(settings.checkForUpdates);

	ImmersionController controller{ settings };
	MainWindow window{ controller, hourLog, trackSync, updateChecker, settings };

	QObject::connect(&controller, &ImmersionController::listened, &hourLog, &HourLog::add);
	QObject::connect(&controller, &ImmersionController::listened, &trackSync, &TrackSync::add);

	// Send to Track as soon as playback stops rather than waiting for the next 5-minute sync
	QObject::connect(&controller, &ImmersionController::stateChanged, &trackSync, [&controller, &trackSync]() {
		if (controller.getState() == ImmersionController::State::Stopped) {
			trackSync.sync();
		}
	});

	QObject::connect(&instance, &SingleInstance::activationRequested, &window, &MainWindow::showAndRaise);
	QObject::connect(&app, &QApplication::aboutToQuit, &controller, &ImmersionController::stop);
	QObject::connect(&app, &QApplication::aboutToQuit, &trackSync, [&trackSync]() {
		// Whatever doesn't make it in time stays queued on disk for next launch
		trackSync.flush(3000);
	});

	if (!parser.isSet(minimisedOption) || !hasTray) {
		window.show();
	}

	if (settings.playOnLaunch && !settings.audioDirectory.isEmpty()) {
		controller.start();
	}

	return app.exec();

}
