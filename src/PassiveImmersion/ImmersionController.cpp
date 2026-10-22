#include "ImmersionController.h"

#include <QCoreApplication>
#include <QDir>
#include <QRegularExpression>

#include <algorithm>

namespace PassiveImmersion {

	static constexpr int s_pollIntervalMs{ 500 };

	// If mpv dies after running at least this long it's brought back, like the old systemd Restart=always.
	// Anything shorter is probably a setup problem (bad folder, bad arguments) and restarting would just loop.
	static constexpr int s_minUptimeForRestartMs{ 10'000 };
	static constexpr int s_restartDelayMs{ 3'000 };

	static bool hasSameNames(const std::vector<MediaSource>& a, const std::vector<MediaSource>& b) {
		return std::ranges::equal(a, b, [](const MediaSource& x, const MediaSource& y) { return x.name == y.name; });
	}

	ImmersionController::ImmersionController(const Settings& settings, QObject* parent)
		: QObject{ parent }
		, m_settings{ settings }
		, m_detector{ MediaDetector::create() }
	{
		m_pollTimer.setInterval(s_pollIntervalMs);
		m_restartTimer.setInterval(s_restartDelayMs);
		m_restartTimer.setSingleShot(true);

		connect(&m_pollTimer, &QTimer::timeout, this, &ImmersionController::m_poll);
		connect(&m_restartTimer, &QTimer::timeout, this, &ImmersionController::m_startPlayer);
		connect(&m_player, &MpvPlayer::connected, this, &ImmersionController::m_poll);
		connect(&m_player, &MpvPlayer::pauseChanged, this, &ImmersionController::m_onPlayerPauseChanged);
		connect(&m_player, &MpvPlayer::stopped, this, &ImmersionController::m_onPlayerStopped);
		connect(&m_player, &MpvPlayer::mediaTitleChanged, this, &ImmersionController::stateChanged);
		connect(&m_hotkey, &QHotkey::activated, this, &ImmersionController::togglePause);

		m_registerHotkey();
	}

	ImmersionController::~ImmersionController()
	{
		// Stop mpv while every member is still alive, the player reports back to us as it shuts down
		stop();
	}

	void ImmersionController::applySettings(const Settings& settings) {

		Settings previous{ m_settings };
		m_settings = settings;

		if (previous.pauseHotkey != settings.pauseHotkey) {
			m_registerHotkey();
		}

		bool playerOptionsChanged{
			previous.audioDirectory != settings.audioDirectory
			|| previous.shuffle != settings.shuffle
			|| previous.mpvPath != settings.mpvPath
			|| previous.extraMpvArgs != settings.extraMpvArgs
			|| previous.startupKeys != settings.startupKeys
		};

		if (playerOptionsChanged && m_player.isRunning()) {
			m_startPlayer();
		}

	}

	void ImmersionController::start() {

		m_userPaused = false;
		m_startPlayer();

	}

	void ImmersionController::stop() {

		m_restartTimer.stop();
		m_pollTimer.stop();
		m_player.stop();

		m_userPaused = false;
		m_otherMediaPlaying = false;
		m_activeSources.clear();

		m_updateState();

	}

	void ImmersionController::togglePause() {

		if (!m_player.isRunning()) {
			start();
			return;
		}

		m_userPaused = !m_userPaused;
		m_updatePlayerPause();
		m_updateState();

	}

	void ImmersionController::m_startPlayer() {

		m_restartTimer.stop();
		m_lastError.clear();

		if (m_settings.audioDirectory.isEmpty() || !QDir{ m_settings.audioDirectory }.exists()) {
			m_fail("Choose an audio folder that exists first.");
			return;
		}

		QString mpvPath{ MpvPlayer::findMpv(m_settings.mpvPath) };
		if (mpvPath.isEmpty()) {
			m_fail("Couldn't find mpv. Install it from https://mpv.io (or put it next to this app), or set its path under Advanced.");
			return;
		}

		MpvPlayer::Options options{};
		options.mpvPath = mpvPath;
		options.directory = m_settings.audioDirectory;
		options.shuffle = m_settings.shuffle;
		options.extraArgs = QProcess::splitCommand(m_settings.extraMpvArgs);
		options.startupKeys = m_settings.startupKeys.split(QRegularExpression{ "\\s+" }, Qt::SkipEmptyParts);

		// mpv starts paused, the first poll unpauses it unless something else is already playing
		m_expectedPause = true;
		m_otherMediaPlaying = false;
		m_activeSources.clear();

		m_player.start(options);
		m_playerUptime.start();
		m_pollTimer.start();
		m_updateState();

	}

	void ImmersionController::m_poll() {

		if (!m_player.isRunning()) {
			m_pollTimer.stop();
			m_updateState();
			return;
		}

		std::vector<MediaSource> sources{ m_detector->getPlayingSources() };
		std::erase_if(sources, [this](const MediaSource& source) { return m_isIgnored(source); });

		if (!sources.empty()) {
			m_activeSources = std::move(sources);
			m_otherMediaPlaying = true;
			m_sinceOtherMedia.restart();
		}
		else if (m_otherMediaPlaying && m_sinceOtherMedia.elapsed() >= m_settings.resumeDelaySeconds * 1000) {
			m_otherMediaPlaying = false;
			m_activeSources.clear();
		}

		m_updatePlayerPause();
		m_updateState();

	}

	void ImmersionController::m_updatePlayerPause() {

		if (!m_player.isConnected()) {
			return;
		}

		bool shouldPause{ m_userPaused || m_otherMediaPlaying };
		if (shouldPause != m_expectedPause) {
			m_expectedPause = shouldPause;
			m_player.setPaused(shouldPause);
		}

	}

	void ImmersionController::m_updateState() {

		State state{};
		if (!m_player.isRunning()) {
			state = State::Stopped;
		}
		else if (!m_player.isConnected()) {
			state = State::Starting;
		}
		else if (m_userPaused) {
			state = State::UserPaused;
		}
		else if (m_otherMediaPlaying) {
			state = State::AutoPaused;
		}
		else {
			state = State::Playing;
		}

		if (state != m_state || !hasSameNames(m_activeSources, m_reportedSources)) {
			m_state = state;
			m_reportedSources = m_activeSources;
			emit stateChanged();
		}

	}

	void ImmersionController::m_registerHotkey() {

		if (m_settings.pauseHotkey.isEmpty()) {
			m_hotkey.setRegistered(false);
			m_hotkeyRegistered = false;
		}
		else {
			m_hotkeyRegistered = m_hotkey.setShortcut(m_settings.pauseHotkey, true);
		}

		emit hotkeyRegistrationChanged(m_hotkeyRegistered);

	}

	void ImmersionController::m_fail(const QString& message) {

		m_lastError = message;
		emit errorOccurred(message);
		m_updateState();

	}

	void ImmersionController::m_onPlayerPauseChanged(bool paused) {

		// Paused or resumed from somewhere other than this app (e.g. an mpv key binding):
		// respect it the same way the old daemon did, rather than fighting it
		if (paused != m_expectedPause) {
			m_userPaused = paused;
			m_expectedPause = paused;
		}

		m_updateState();

	}

	void ImmersionController::m_onPlayerStopped(const QString& error) {

		if (!m_player.isRunning()) {
			m_pollTimer.stop();
		}

		if (error.isEmpty()) {
			m_updateState();
			return;
		}

		if (m_playerUptime.isValid() && m_playerUptime.elapsed() >= s_minUptimeForRestartMs) {
			m_fail(error + QString{ "\nRestarting in %1 seconds..." }.arg(s_restartDelayMs / 1000));
			m_restartTimer.start();
		}
		else {
			m_fail(error);
		}

	}

	bool ImmersionController::m_isIgnored(const MediaSource& source) const {

		// The passive player must never pause itself
		if (source.processId != 0
			&& (source.processId == m_player.getProcessId() || source.processId == QCoreApplication::applicationPid())) {
			return true;
		}

		for (const QString& app : m_settings.ignoredApps) {
			if (source.name.contains(app, Qt::CaseInsensitive)) {
				return true;
			}
		}

		return false;

	}

}
