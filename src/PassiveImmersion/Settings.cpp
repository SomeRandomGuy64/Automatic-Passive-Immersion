#include "Settings.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

namespace PassiveImmersion {

	// If this file sits next to the executable the app runs in portable mode and keeps its settings there
	static const QString s_portableFileName{ "PassiveImmersion.ini" };

	static QString getPortablePath() {
		return QDir{ QCoreApplication::applicationDirPath() }.filePath(s_portableFileName);
	}

	// Stored as "discord, steam" rather than QSettings' list syntax so the ini stays easy to edit by hand
	static QStringList splitList(const QString& text) {

		QStringList items{};
		for (const QString& item : text.split(',')) {
			QString trimmed{ item.trimmed() };
			if (!trimmed.isEmpty()) {
				items.append(trimmed);
			}
		}
		return items;

	}

	bool Settings::isPortable() {
		return QFileInfo::exists(getPortablePath());
	}

	QString Settings::getFilePath() {

		if (isPortable()) {
			return getPortablePath();
		}

		QString configDir{ QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) };
		return QDir{ configDir }.filePath("settings.ini");

	}

	Settings Settings::load() {

		QSettings file{ getFilePath(), QSettings::IniFormat };
		Settings settings{};

		settings.audioDirectory = file.value("audioDirectory", settings.audioDirectory).toString();
		settings.pauseHotkey = QKeySequence{
			file.value("pauseHotkey", settings.pauseHotkey.toString(QKeySequence::PortableText)).toString(),
			QKeySequence::PortableText
		};
		settings.shuffle = file.value("shuffle", settings.shuffle).toBool();
		settings.resumeDelaySeconds = file.value("resumeDelaySeconds", settings.resumeDelaySeconds).toInt();
		settings.ignoredApps = splitList(file.value("ignoredApps").toString());
		settings.playOnLaunch = file.value("playOnLaunch", settings.playOnLaunch).toBool();

		settings.mpvPath = file.value("advanced/mpvPath", settings.mpvPath).toString();
		settings.extraMpvArgs = file.value("advanced/extraMpvArgs", settings.extraMpvArgs).toString();
		settings.startupKeys = file.value("advanced/startupKeys", settings.startupKeys).toString();

		return settings;

	}

	void Settings::save() const {

		QSettings file{ getFilePath(), QSettings::IniFormat };

		file.setValue("audioDirectory", audioDirectory);
		file.setValue("pauseHotkey", pauseHotkey.toString(QKeySequence::PortableText));
		file.setValue("shuffle", shuffle);
		file.setValue("resumeDelaySeconds", resumeDelaySeconds);
		file.setValue("ignoredApps", ignoredApps.join(", "));
		file.setValue("playOnLaunch", playOnLaunch);

		file.setValue("advanced/mpvPath", mpvPath);
		file.setValue("advanced/extraMpvArgs", extraMpvArgs);
		file.setValue("advanced/startupKeys", startupKeys);

	}

}
