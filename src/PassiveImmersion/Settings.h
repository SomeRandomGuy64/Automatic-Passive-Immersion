#pragma once

#include <QKeySequence>
#include <QString>
#include <QStringList>

namespace PassiveImmersion {

	struct Settings {

		// Folder of audio to loop through (e.g. condensed audio)
		QString audioDirectory{};

		// Global hotkey that pauses/resumes passive audio
		QKeySequence pauseHotkey{ "Ctrl+Alt+P", QKeySequence::PortableText };

		bool shuffle{ true };

		// How long other media has to be quiet before passive audio comes back
		int resumeDelaySeconds{ 2 };

		// Apps that never pause passive audio (matched against the app/process name, case-insensitive)
		QStringList ignoredApps{};

		// Start playing as soon as the app opens
		bool playOnLaunch{ true };

		// Advanced
		QString mpvPath{};		// empty = find mpv automatically
		QString extraMpvArgs{};
		QString startupKeys{};	// keys pressed in mpv once it starts, e.g. "ctrl+p" for an mpv tracker script

		static Settings load();
		void save() const;

		// The ini file settings are read from and written to
		static QString getFilePath();
		static bool isPortable();

	};

}
