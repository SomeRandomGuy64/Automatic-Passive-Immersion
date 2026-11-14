#include "MpvPlayer.h"

#include "PassiveImmersion/Core.h"
#include "PassiveImmersion/FolderWatcher.h"

#include <QCollator>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRandomGenerator>
#include <QSet>
#include <QStandardPaths>

#include <algorithm>
#include <ranges>

#ifdef PI_PLATFORM_WINDOWS
	#ifndef NOMINMAX
		#define NOMINMAX
	#endif
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <windows.h>
#elif defined(PI_PLATFORM_LINUX)
	#include <csignal>
	#include <sys/prctl.h>
#endif

namespace PassiveImmersion {

	// IDs mpv echoes back so its replies can be told apart
	static constexpr int s_pauseObserverId{ 1 };
	static constexpr int s_titleObserverId{ 2 };
	static constexpr int s_pidRequestId{ 3 };
	static constexpr int s_volumeObserverId{ 4 };
	static constexpr int s_fileNameObserverId{ 5 };
	static constexpr int s_playlistObserverId{ 6 };
	static constexpr int s_audioExtsRequestId{ 7 };
	static constexpr int s_videoExtsRequestId{ 8 };

	static constexpr int s_connectIntervalMs{ 200 };
	static constexpr int s_maxConnectAttempts{ 50 };	// 10 seconds
	static constexpr int s_outputTailLines{ 5 };

	MpvPlayer::MpvPlayer(QObject* parent)
		: QObject{ parent }
	{
		m_process.setProcessChannelMode(QProcess::MergedChannels);

#ifdef PI_PLATFORM_WINDOWS
		// Stops a console window flashing up if mpv was pointed at mpv.com instead of mpv.exe
		m_process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* arguments) {
			arguments->flags |= CREATE_NO_WINDOW;
		});

		// mpv goes into a job object that's killed along with this app, so it can't be left playing if we crash
		m_job = CreateJobObjectW(nullptr, nullptr);
		if (m_job != nullptr) {
			JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
			limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
			SetInformationJobObject(m_job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
		}
#elif defined(PI_PLATFORM_LINUX)
		// Same idea on Linux: the kernel sends mpv SIGTERM if this app dies without cleaning up
		m_process.setChildProcessModifier([]() {
			prctl(PR_SET_PDEATHSIG, SIGTERM);
		});
#endif

		m_connectTimer.setInterval(s_connectIntervalMs);

		connect(&m_connectTimer, &QTimer::timeout, this, &MpvPlayer::m_tryConnect);
		connect(&m_socket, &QLocalSocket::connected, this, &MpvPlayer::m_onConnected);
		connect(&m_socket, &QLocalSocket::readyRead, this, &MpvPlayer::m_onReadyRead);
		connect(&m_process, &QProcess::readyReadStandardOutput, this, &MpvPlayer::m_onProcessOutput);
		connect(&m_process, &QProcess::finished, this, &MpvPlayer::m_onProcessFinished);
		connect(&m_process, &QProcess::errorOccurred, this, &MpvPlayer::m_onProcessError);
	}

	MpvPlayer::~MpvPlayer()
	{
		stop();

#ifdef PI_PLATFORM_WINDOWS
		if (m_job != nullptr) {
			CloseHandle(m_job);
		}
#endif
	}

	void MpvPlayer::start(const Options& options) {

		stop();
		m_reset();

		// One IPC endpoint per app instance, so a second copy of mpv (or the old daemon) can't collide with it
#ifdef PI_PLATFORM_WINDOWS
		m_ipcName = QString{ "passive-immersion-mpv-%1" }.arg(QCoreApplication::applicationPid());
		QString ipcServer{ R"(\\.\pipe\)" + m_ipcName };
#else
		m_ipcName = QDir::temp().filePath(QString{ "passive-immersion-mpv-%1.sock" }.arg(QCoreApplication::applicationPid()));
		QFile::remove(m_ipcName);
		QString ipcServer{ m_ipcName };
#endif

		QStringList arguments{
			"--input-ipc-server=" + ipcServer,
			"--pause",
			"--no-video",
			"--force-window=no",
			"--input-terminal=no",
			"--loop-playlist=inf",
		};

		if (options.shuffle) {
			arguments.append("--shuffle");
		}

		arguments.append(options.extraArgs);
		arguments.append("--");
		arguments.append(options.directory);

		m_startupKeys = options.startupKeys;

		qInfo() << "Starting mpv:" << options.mpvPath << arguments;
		m_process.start(options.mpvPath, arguments);
		m_processId = m_process.processId();

#ifdef PI_PLATFORM_WINDOWS
		if (m_job != nullptr && m_processId != 0) {
			HANDLE process{ OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE, static_cast<DWORD>(m_processId)) };
			if (process != nullptr) {
				AssignProcessToJobObject(m_job, process);
				CloseHandle(process);
			}
		}
#endif
		m_connectTimer.start();

	}

	void MpvPlayer::stop() {

		if (!isRunning()) {
			return;
		}

		m_stopRequested = true;
		m_connectTimer.stop();

		if (isConnected()) {
			m_sendCommand({ "quit" });
			m_socket.flush();
		}
		else {
			m_process.terminate();
		}

		if (!m_process.waitForFinished(2000)) {
			m_process.kill();
			m_process.waitForFinished(1000);
		}

	}

	void MpvPlayer::setPaused(bool paused) {
		m_sendCommand({ "set_property", "pause", paused });
	}

	void MpvPlayer::setDucked(bool ducked, int percent) {

		if (ducked == m_ducked && percent == m_duckPercent) {
			return;
		}

		m_ducked = ducked;
		m_duckPercent = percent;
		m_applyVolume();

	}

	void MpvPlayer::seekRelative(double seconds) {
		m_sendCommand({ "seek", seconds, "relative" });
	}

	void MpvPlayer::syncPlaylist(const QStringList& files, const QString& directory, bool shuffle) {

		if (!isConnected() || !m_playlistKnown) {
			return;
		}

		// Out: entries whose file or folder is gone. The one playing is left to finish, it goes on a later pass.
		for (qsizetype i{ m_playlist.size() - 1 }; i >= 0; --i) {
			if (i != m_playlistPos && !QFileInfo::exists(m_playlist[i])) {
				qInfo() << "Removing from playlist:" << m_playlist[i];
				m_sendCommand({ "playlist-remove", i });
				m_playlist.removeAt(i);
				if (i < m_playlistPos) {
					--m_playlistPos;
				}
			}
		}

		// Older mpv versions leave a subfolder as one entry and only open it up when it's reached,
		// so a file counts as already there if it or any folder above it (inside the audio folder) is in the playlist
		QSet<QString> known{};
		for (const QString& entry : m_playlist) {
			known.insert(FolderWatcher::pathKey(entry));
		}
		QString rootKey{ FolderWatcher::pathKey(directory) };

		auto isKnown{ [&](const QString& file) {
			QString key{ FolderWatcher::pathKey(file) };
			while (key.size() > rootKey.size()) {
				if (known.contains(key)) {
					return true;
				}
				key = key.left(key.lastIndexOf('/'));
			}
			return false;
		} };

		QStringList added{};
		for (const QString& file : files) {
			if (!isKnown(file)) {
				added.append(file);
			}
		}

		if (added.isEmpty()) {
			return;
		}

		QCollator collator{};
		collator.setNumericMode(true);
		std::ranges::sort(added, [&](const QString& a, const QString& b) { return collator.compare(a, b) < 0; });

		// In: appended, then moved into place. (loadfile's own insert-at only exists from mpv 0.38.)
		for (const QString& file : added) {
			qsizetype target{};
			if (shuffle) {
				// Somewhere after the current file, so a new file is heard this time round rather than after a whole loop
				qsizetype first{ m_playlistPos + 1 };
				target = first + QRandomGenerator::global()->bounded(m_playlist.size() - first + 1);
			}
			else {
				auto after{ std::ranges::find_if(m_playlist, [&](const QString& entry) { return collator.compare(entry, file) > 0; }) };
				target = std::distance(m_playlist.begin(), after);
			}

			qInfo() << "Adding to playlist:" << file << "at" << target;
			m_sendCommand({ "loadfile", file, "append" });
			if (target != m_playlist.size()) {
				m_sendCommand({ "playlist-move", m_playlist.size(), target });
			}

			m_playlist.insert(target, file);
			if (target <= m_playlistPos) {
				++m_playlistPos;
			}
		}

	}

	QString MpvPlayer::findMpv(const QString& configuredPath) {

		if (!configuredPath.isEmpty()) {
			return configuredPath;
		}

		// A copy of mpv shipped next to the app comes first, so the whole folder can be carried around
		QDir appDir{ QCoreApplication::applicationDirPath() };
		QString found{ QStandardPaths::findExecutable("mpv", { appDir.path(), appDir.filePath("mpv") }) };
		if (!found.isEmpty()) {
			return found;
		}

		found = QStandardPaths::findExecutable("mpv");
		if (!found.isEmpty()) {
			return found;
		}

		// Apps opened from Finder/Explorer don't always get the same PATH a terminal does
		QStringList fallbackDirs{
#if defined(PI_PLATFORM_MACOS)
			"/opt/homebrew/bin",
			"/usr/local/bin",
			"/opt/local/bin",
			"/Applications/mpv.app/Contents/MacOS",
#elif defined(PI_PLATFORM_WINDOWS)
			qEnvironmentVariable("ProgramFiles") + "/mpv",
			qEnvironmentVariable("LOCALAPPDATA") + "/Programs/mpv",
			QDir::homePath() + "/scoop/apps/mpv/current",
#else
			"/usr/bin",
			"/usr/local/bin",
			"/snap/bin",
#endif
		};

		return QStandardPaths::findExecutable("mpv", fallbackDirs);

	}

	void MpvPlayer::m_tryConnect() {

		if (!isRunning() || isConnected()) {
			m_connectTimer.stop();
			return;
		}

		++m_connectAttempts;
		if (m_connectAttempts > s_maxConnectAttempts) {
			m_connectTimer.stop();
			m_outputTail.append("mpv never opened its IPC connection");
			m_process.kill();
			return;
		}

		// mpv creates the socket/pipe a moment after it starts, so keep knocking until it's there
		if (m_socket.state() == QLocalSocket::UnconnectedState) {
			m_socket.connectToServer(m_ipcName);
		}

	}

	void MpvPlayer::m_onConnected() {

		m_connectTimer.stop();

		m_sendCommand({ "observe_property", s_pauseObserverId, "pause" });
		m_sendCommand({ "observe_property", s_titleObserverId, "media-title" });
		m_sendCommand({ "observe_property", s_volumeObserverId, "volume" });
		m_sendCommand({ "observe_property", s_fileNameObserverId, "filename/no-ext" });
		m_sendCommand({ "observe_property", s_playlistObserverId, "playlist" });
		m_sendCommand({ "get_property", "audio-exts" }, s_audioExtsRequestId);
		m_sendCommand({ "get_property", "video-exts" }, s_videoExtsRequestId);

		// If mpv was started through a launcher/shim the process we spawned isn't the one playing audio,
		// so ask mpv for its own PID - that's the one the media detectors will see
		m_sendCommand({ "get_property", "pid" }, s_pidRequestId);

		for (const QString& key : m_startupKeys) {
			m_sendCommand({ "keypress", key });
		}

		emit connected();

	}

	void MpvPlayer::m_onReadyRead() {

		m_readBuffer.append(m_socket.readAll());

		// mpv sends one JSON object per line
		qsizetype newline{ m_readBuffer.indexOf('\n') };
		while (newline != -1) {
			QByteArray line{ m_readBuffer.left(newline) };
			m_readBuffer.remove(0, newline + 1);

			QJsonDocument document{ QJsonDocument::fromJson(line) };
			if (document.isObject()) {
				m_handleMessage(document.object());
			}

			newline = m_readBuffer.indexOf('\n');
		}

	}

	void MpvPlayer::m_handleMessage(const QJsonObject& message) {

		if (message.value("event").toString() == "property-change") {
			int id{ message.value("id").toInt() };

			if (id == s_pauseObserverId) {
				bool paused{ message.value("data").toBool() };
				if (paused != m_paused) {
					m_paused = paused;
					emit pauseChanged(paused);
				}
			}
			else if (id == s_titleObserverId) {
				m_mediaTitle = message.value("data").toString();
				emit mediaTitleChanged(m_mediaTitle);
			}
			else if (id == s_fileNameObserverId) {
				QString fileName{ message.value("data").toString() };
				if (fileName != m_fileName) {
					m_fileName = fileName;
					emit fileNameChanged(m_fileName);
				}
			}
			else if (id == s_playlistObserverId) {
				m_playlist.clear();
				m_playlistPos = -1;
				for (const QJsonValue& entry : message.value("data").toArray()) {
					QJsonObject object{ entry.toObject() };
					if (object.value("current").toBool()) {
						m_playlistPos = m_playlist.size();
					}
					m_playlist.append(object.value("filename").toString());
				}
				m_playlistKnown = true;
				emit playlistChanged();
			}
			else if (id == s_volumeObserverId) {
				double volume{ message.value("data").toDouble(-1.0) };
				bool isOurDuck{ m_duckedVolume >= 0.0 && qFuzzyCompare(volume, m_duckedVolume) };

				// The first report is the volume mpv started with (100, or --volume from the extra arguments).
				// After that a change only counts as the new normal volume if we weren't the ones turning it down.
				if (m_volume < 0.0) {
					m_volume = volume;
					m_applyVolume();
				}
				else if (!m_ducked && !isOurDuck) {
					m_volume = volume;
				}
			}
		}
		else if (message.value("request_id").toInt() == s_pidRequestId && message.value("error").toString() == "success") {
			m_processId = message.value("data").toInteger(m_processId);
		}
		else if (message.value("request_id").toInt() == s_audioExtsRequestId && message.value("error").toString() == "success") {
			// Held until video-exts arrives, mpv loads both from a folder (audio still plays from video files with --no-video)
			m_audioExts.clear();
			for (const QJsonValue& extension : message.value("data").toArray()) {
				m_audioExts.append(extension.toString());
			}
		}
		else if (message.value("request_id").toInt() == s_videoExtsRequestId && message.value("error").toString() == "success"
			&& !m_audioExts.isEmpty()) {
			QStringList extensions{ m_audioExts };
			for (const QJsonValue& extension : message.value("data").toArray()) {
				extensions.append(extension.toString());
			}
			emit extensionsReported(extensions);
		}

	}

	void MpvPlayer::m_sendCommand(const QJsonArray& command, int requestId) {

		if (!isConnected()) {
			return;
		}

		QJsonObject request{ { "command", command } };
		if (requestId != 0) {
			request.insert("request_id", requestId);
		}

		// MPV IPC requires a newline-terminated JSON payload
		m_socket.write(QJsonDocument{ request }.toJson(QJsonDocument::Compact) + '\n');

	}

	void MpvPlayer::m_applyVolume() {

		if (m_volume < 0.0) {
			// Not known yet, this runs again once mpv reports it
			return;
		}

		if (m_ducked) {
			m_duckedVolume = m_volume * m_duckPercent / 100.0;
			m_sendCommand({ "set_property", "volume", m_duckedVolume });
		}
		else {
			m_sendCommand({ "set_property", "volume", m_volume });
		}

	}

	void MpvPlayer::m_onProcessOutput() {

		QString output{ QString::fromUtf8(m_process.readAllStandardOutput()) };
		for (const QString& line : output.split('\n', Qt::SkipEmptyParts)) {
			m_outputTail.append(line.trimmed());
		}

		while (m_outputTail.size() > s_outputTailLines) {
			m_outputTail.removeFirst();
		}

	}

	void MpvPlayer::m_onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {

		qInfo() << "mpv exited with code" << exitCode << exitStatus;
		m_connectTimer.stop();
		m_socket.abort();

#ifndef PI_PLATFORM_WINDOWS
		QFile::remove(m_ipcName);
#endif

		QString error{};
		if (!m_stopRequested && exitStatus == QProcess::CrashExit) {
			error = "mpv was closed or crashed.";
		}
		else if (!m_stopRequested) {
			// mpv's own output usually says why, e.g. no playable files in the folder
			error = QString{ "mpv stopped (exit code %1)" }.arg(exitCode);
			if (!m_outputTail.isEmpty()) {
				error += ":\n" + m_outputTail.join('\n');
			}
		}

		m_paused = true;
		emit stopped(error);

	}

	void MpvPlayer::m_onProcessError(QProcess::ProcessError error) {

		qInfo() << "mpv process error:" << m_process.errorString();

		// Crashes also emit finished(), only a failed launch needs handling here
		if (error == QProcess::FailedToStart) {
			m_connectTimer.stop();
			emit stopped(QString{ "Couldn't start mpv at \"%1\". Install mpv or set its path under Advanced." }.arg(m_process.program()));
		}

	}

	void MpvPlayer::m_reset() {

		m_connectAttempts = 0;
		m_readBuffer.clear();
		m_outputTail.clear();
		m_paused = true;
		m_stopRequested = false;
		m_processId = 0;
		m_mediaTitle.clear();
		m_fileName.clear();
		m_playlist.clear();
		m_playlistPos = -1;
		m_playlistKnown = false;
		m_audioExts.clear();
		m_volume = -1.0;
		m_duckedVolume = -1.0;
		m_ducked = false;
		m_duckPercent = 100;

	}

}
