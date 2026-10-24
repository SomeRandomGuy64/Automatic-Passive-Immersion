#include "Settings.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

#include <array>
#include <utility>

namespace PassiveImmersion {

	// If this file sits next to the executable the app runs in portable mode and keeps its settings there
	static const QString s_portableFileName{ "PassiveImmersion.ini" };

	// Enums are stored as words so the ini stays easy to edit by hand
	static constexpr std::array s_otherMediaActionNames{
		std::pair{ Settings::OtherMediaAction::Pause, "pause" },
		std::pair{ Settings::OtherMediaAction::Duck, "duck" },
		std::pair{ Settings::OtherMediaAction::DuckThenPause, "duckThenPause" },
	};

	static constexpr std::array s_trackSubcategoryNames{
		std::pair{ Settings::TrackSubcategory::None, "none" },
		std::pair{ Settings::TrackSubcategory::Fixed, "fixed" },
		std::pair{ Settings::TrackSubcategory::FileName, "fileName" },
		std::pair{ Settings::TrackSubcategory::DayOfWeek, "dayOfWeek" },
	};

	template<typename Enum, size_t Count>
	static QString toName(const std::array<std::pair<Enum, const char*>, Count>& names, Enum value) {

		for (const auto& [entry, name] : names) {
			if (entry == value) {
				return name;
			}
		}
		return names[0].second;

	}

	template<typename Enum, size_t Count>
	static Enum fromName(const std::array<std::pair<Enum, const char*>, Count>& names, const QString& text, Enum fallback) {

		for (const auto& [entry, name] : names) {
			if (text == name) {
				return entry;
			}
		}
		return fallback;

	}

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

	QString Settings::getDataDirectory() {

		QString directory{ isPortable()
			? QCoreApplication::applicationDirPath()
			: QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) };

		QDir{}.mkpath(directory);
		return directory;

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
		settings.otherMediaAction = fromName(s_otherMediaActionNames, file.value("otherMediaAction").toString(), settings.otherMediaAction);
		settings.duckVolumePercent = file.value("duckVolumePercent", settings.duckVolumePercent).toInt();
		settings.duckPauseAfterSeconds = file.value("duckPauseAfterSeconds", settings.duckPauseAfterSeconds).toInt();
		settings.resumeDelaySeconds = file.value("resumeDelaySeconds", settings.resumeDelaySeconds).toInt();
		settings.ignoredApps = splitList(file.value("ignoredApps").toString());
		settings.playOnLaunch = file.value("playOnLaunch", settings.playOnLaunch).toBool();

		settings.checkForUpdates = file.value("checkForUpdates", settings.checkForUpdates).toBool();
		settings.skippedVersion = file.value("skippedVersion", settings.skippedVersion).toString();

		QTime dayStart{ QTime::fromString(file.value("dayStartTime").toString(), "HH:mm") };
		if (dayStart.isValid()) {
			settings.dayStartTime = dayStart;
		}

		settings.trackEnabled = file.value("track/enabled", settings.trackEnabled).toBool();
		settings.trackApiKey = file.value("track/apiKey", settings.trackApiKey).toString();
		settings.trackSubcategory = fromName(s_trackSubcategoryNames, file.value("track/subcategory").toString(), settings.trackSubcategory);
		settings.trackSubcategoryName = file.value("track/subcategoryName", settings.trackSubcategoryName).toString();
		settings.trackApiUrl = file.value("track/apiUrl", settings.trackApiUrl).toString();

		settings.mpvPath = file.value("advanced/mpvPath", settings.mpvPath).toString();
		settings.extraMpvArgs = file.value("advanced/extraMpvArgs", settings.extraMpvArgs).toString();
		settings.startupKeys = file.value("advanced/startupKeys", settings.startupKeys).toString();

		return settings;

	}

	void Settings::save() const {

		QString path{ getFilePath() };

		{
			QSettings file{ path, QSettings::IniFormat };

			file.setValue("audioDirectory", audioDirectory);
			file.setValue("pauseHotkey", pauseHotkey.toString(QKeySequence::PortableText));
			file.setValue("shuffle", shuffle);
			file.setValue("otherMediaAction", toName(s_otherMediaActionNames, otherMediaAction));
			file.setValue("duckVolumePercent", duckVolumePercent);
			file.setValue("duckPauseAfterSeconds", duckPauseAfterSeconds);
			file.setValue("resumeDelaySeconds", resumeDelaySeconds);
			file.setValue("ignoredApps", ignoredApps.join(", "));
			file.setValue("playOnLaunch", playOnLaunch);
			file.setValue("checkForUpdates", checkForUpdates);
			file.setValue("skippedVersion", skippedVersion);
			file.setValue("dayStartTime", dayStartTime.toString("HH:mm"));

			file.setValue("track/enabled", trackEnabled);
			file.setValue("track/apiKey", trackApiKey);
			file.setValue("track/subcategory", toName(s_trackSubcategoryNames, trackSubcategory));
			file.setValue("track/subcategoryName", trackSubcategoryName);
			file.setValue("track/apiUrl", trackApiUrl);

			file.setValue("advanced/mpvPath", mpvPath);
			file.setValue("advanced/extraMpvArgs", extraMpvArgs);
			file.setValue("advanced/startupKeys", startupKeys);
		}

		// The file holds the Track API key, so only its owner gets to read it (does nothing on Windows)
		QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner);

	}

}
