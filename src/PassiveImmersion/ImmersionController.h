#pragma once

#include "PassiveImmersion/MediaDetector.h"
#include "PassiveImmersion/MpvPlayer.h"
#include "PassiveImmersion/Settings.h"

#include <QElapsedTimer>
#include <QHotkey>
#include <QObject>
#include <QTimer>

#include <memory>
#include <vector>

namespace PassiveImmersion {

	// The daemon part of the app: keeps mpv playing passive audio and pauses it while anything else is playing.
	// Knows nothing about the GUI, the window just reads its state and calls into it.
	class ImmersionController : public QObject {

		Q_OBJECT

	public:
		enum class State {
			Stopped,
			Starting,
			Playing,
			AutoPaused,		// other media is playing
			UserPaused		// paused with the hotkey / button / tray
		};

		explicit ImmersionController(const Settings& settings, QObject* parent = nullptr);
		~ImmersionController() override;

		// Restarts mpv if anything it was launched with changed
		void applySettings(const Settings& settings);

		void start();
		void stop();

		// What the hotkey does. Starts playback if it's stopped.
		void togglePause();

		inline State getState() const { return m_state; }
		inline const std::vector<MediaSource>& getActiveSources() const { return m_activeSources; }
		inline const QString& getMediaTitle() const { return m_player.getMediaTitle(); }
		inline const QString& getLastError() const { return m_lastError; }
		inline bool isHotkeyRegistered() const { return m_hotkeyRegistered; }
		inline QString getDetectorDescription() const { return m_detector->getDescription(); }

	signals:
		void stateChanged();
		void errorOccurred(const QString& message);
		void hotkeyRegistrationChanged(bool registered);

	private:
		void m_startPlayer();
		void m_poll();
		void m_updatePlayerPause();
		void m_updateState();
		void m_registerHotkey();
		void m_fail(const QString& message);

		void m_onPlayerPauseChanged(bool paused);
		void m_onPlayerStopped(const QString& error);

		bool m_isIgnored(const MediaSource& source) const;

	private:
		Settings m_settings{};
		MpvPlayer m_player{};
		std::unique_ptr<MediaDetector> m_detector{};
		QHotkey m_hotkey{};
		bool m_hotkeyRegistered{ false };

		QTimer m_pollTimer{};
		QTimer m_restartTimer{};
		QElapsedTimer m_playerUptime{};
		QElapsedTimer m_sinceOtherMedia{};			// time since other media was last heard
		bool m_otherMediaPlaying{ false };			// stays true for resumeDelaySeconds after it goes quiet
		std::vector<MediaSource> m_activeSources{};
		std::vector<MediaSource> m_reportedSources{};	// what the last stateChanged() was sent with

		bool m_userPaused{ false };
		bool m_expectedPause{ true };				// what we last told mpv, so pauses from elsewhere can be spotted

		State m_state{ State::Stopped };
		QString m_lastError{};

	};

}
