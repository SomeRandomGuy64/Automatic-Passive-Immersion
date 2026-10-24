#pragma once

#include <QKeySequence>
#include <QString>
#include <QStringList>
#include <QTime>

namespace PassiveImmersion {

	struct Settings {

		enum class OtherMediaAction {
			Pause,
			Duck,				// keep playing quietly
			DuckThenPause		// quietly at first, so short sounds don't stop anything, then pause
		};

		// Which Track sub-category passive hours are logged under
		enum class TrackSubcategory {
			None,		// the passive category's unassigned hours
			Fixed,		// trackSubcategoryName
			FileName,	// the audio file's name
			DayOfWeek	// "Monday", "Tuesday"... (the day of the hour log, so it follows dayStartTime)
		};

		// Folder of audio to loop through (e.g. condensed audio)
		QString audioDirectory{};

		// Global hotkey that pauses/resumes passive audio
		QKeySequence pauseHotkey{ "Ctrl+Alt+P", QKeySequence::PortableText };

		bool shuffle{ true };

		OtherMediaAction otherMediaAction{ OtherMediaAction::Pause };
		int duckVolumePercent{ 20 };		// of the normal volume, on mpv's volume scale
		int duckPauseAfterSeconds{ 10 };	// DuckThenPause only

		// How long other media has to be quiet before passive audio comes back
		int resumeDelaySeconds{ 2 };

		// Apps that never pause passive audio (matched against the app/process name, case-insensitive)
		QStringList ignoredApps{};

		// Start playing as soon as the app opens
		bool playOnLaunch{ true };

		bool checkForUpdates{ true };
		QString skippedVersion{};	// an update the user chose to skip, not offered again

		// Listening before this time of day counts towards the day before
		QTime dayStartTime{ 0, 0 };

		// fazon.nexus Track
		bool trackEnabled{ false };
		QString trackApiKey{};
		TrackSubcategory trackSubcategory{ TrackSubcategory::None };
		QString trackSubcategoryName{ "Passive audio" };
		QString trackApiUrl{ "https://api.fazon.nexus" };	// not in the GUI, only for testing against a local server

		// Advanced
		QString mpvPath{};		// empty = find mpv automatically
		QString extraMpvArgs{};
		QString startupKeys{};	// keys pressed in mpv once it starts, e.g. "ctrl+p" for an mpv tracker script

		static Settings load();
		void save() const;

		// The ini file settings are read from and written to
		static QString getFilePath();
		static bool isPortable();

		// Where the hour log and anything else the app writes lives (next to the exe in portable mode)
		static QString getDataDirectory();

	};

}
