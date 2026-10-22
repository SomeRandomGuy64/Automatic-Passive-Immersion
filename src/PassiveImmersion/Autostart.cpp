#include "Autostart.h"

#include "PassiveImmersion/Core.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

namespace PassiveImmersion {

	static constexpr const char* s_minimisedFlag{ "--minimised" };

	// When running as an AppImage the real executable lives in a temporary mount, $APPIMAGE is the file to relaunch
	static QString getExecutablePath() {

		QString appImage{ qEnvironmentVariable("APPIMAGE") };
		if (!appImage.isEmpty()) {
			return appImage;
		}
		return QCoreApplication::applicationFilePath();

	}

#if defined(PI_PLATFORM_WINDOWS)

	static const QString s_runKey{ R"(HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run)" };

	bool Autostart::isEnabled() {
		return QSettings{ s_runKey, QSettings::NativeFormat }.contains(s_appId);
	}

	bool Autostart::setEnabled(bool enabled) {

		QSettings runKey{ s_runKey, QSettings::NativeFormat };
		if (enabled) {
			QString command{ QString{ "\"%1\" %2" }.arg(QDir::toNativeSeparators(getExecutablePath()), s_minimisedFlag) };
			runKey.setValue(s_appId, command);
		}
		else {
			runKey.remove(s_appId);
		}

		runKey.sync();
		return runKey.status() == QSettings::NoError;

	}

#elif defined(PI_PLATFORM_MACOS)

	static QString getLaunchAgentPath() {
		return QDir::homePath() + "/Library/LaunchAgents/com.somerandomguy64.automaticpassiveimmersion.plist";
	}

	bool Autostart::isEnabled() {
		return QFile::exists(getLaunchAgentPath());
	}

	bool Autostart::setEnabled(bool enabled) {

		QString path{ getLaunchAgentPath() };
		if (!enabled) {
			return !QFile::exists(path) || QFile::remove(path);
		}

		QDir{}.mkpath(QFileInfo{ path }.absolutePath());

		QString executable{ getExecutablePath().toHtmlEscaped() };
		QString plist{ QString{ R"(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>Label</key>
	<string>com.somerandomguy64.automaticpassiveimmersion</string>
	<key>ProgramArguments</key>
	<array>
		<string>%1</string>
		<string>%2</string>
	</array>
	<key>RunAtLoad</key>
	<true/>
</dict>
</plist>
)" }.arg(executable, s_minimisedFlag) };

		QFile file{ path };
		if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
			return false;
		}
		file.write(plist.toUtf8());
		return true;

	}

#else

	static QString getDesktopFilePath() {
		return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/autostart/" + s_appId + ".desktop";
	}

	bool Autostart::isEnabled() {
		return QFile::exists(getDesktopFilePath());
	}

	bool Autostart::setEnabled(bool enabled) {

		QString path{ getDesktopFilePath() };
		if (!enabled) {
			return !QFile::exists(path) || QFile::remove(path);
		}

		QDir{}.mkpath(QFileInfo{ path }.absolutePath());

		// Desktop Entry spec: quote the path and escape the characters that are special inside quotes
		QString executable{ getExecutablePath() };
		for (const char* special : { "\\", "\"", "`", "$" }) {
			executable.replace(special, QString{ "\\" } + special);
		}

		QString desktopEntry{ QString{
			"[Desktop Entry]\n"
			"Type=Application\n"
			"Name=%1\n"
			"Exec=\"%2\" %3\n"
			"X-GNOME-Autostart-enabled=true\n"
		}.arg(s_appDisplayName, executable, s_minimisedFlag) };

		QFile file{ path };
		if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
			return false;
		}
		file.write(desktopEntry.toUtf8());
		return true;

	}

#endif

}
