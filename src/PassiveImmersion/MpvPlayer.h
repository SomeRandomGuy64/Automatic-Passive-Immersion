#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QLocalSocket>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <cstdint>

namespace PassiveImmersion {

	// Runs a headless mpv instance and controls it over mpv's JSON IPC
	// (a Unix socket on Linux/macOS, a named pipe on Windows - QLocalSocket handles both)
	class MpvPlayer : public QObject {

		Q_OBJECT

	public:
		struct Options {

			QString mpvPath{};
			QString directory{};
			bool shuffle{ true };
			QStringList extraArgs{};
			QStringList startupKeys{};

		};

		explicit MpvPlayer(QObject* parent = nullptr);
		~MpvPlayer() override;

		// mpv always starts paused, whoever owns the player decides when it should play
		void start(const Options& options);
		void stop();

		void setPaused(bool paused);

		// Turns the volume down to percent of normal (on mpv's own volume scale), or back up again
		void setDucked(bool ducked, int percent);

		void seekRelative(double seconds);

		inline bool isRunning() const { return m_process.state() != QProcess::NotRunning; }
		inline bool isConnected() const { return m_socket.state() == QLocalSocket::ConnectedState; }
		inline bool isPaused() const { return m_paused; }
		inline int64_t getProcessId() const { return m_processId; }
		inline const QString& getMediaTitle() const { return m_mediaTitle; }
		inline const QString& getFileName() const { return m_fileName; }	// without the extension

		// Returns the configured path if set, otherwise looks next to the app, on PATH and in common install folders
		static QString findMpv(const QString& configuredPath);

	signals:
		void connected();
		void stopped(const QString& error);	// error is empty when stop() was asked for
		void pauseChanged(bool paused);
		void mediaTitleChanged(const QString& title);
		void fileNameChanged(const QString& fileName);

	private:
		void m_tryConnect();
		void m_onConnected();
		void m_onReadyRead();
		void m_onProcessOutput();
		void m_onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
		void m_onProcessError(QProcess::ProcessError error);
		void m_handleMessage(const QJsonObject& message);
		void m_sendCommand(const QJsonArray& command, int requestId = 0);
		void m_applyVolume();
		void m_reset();

	private:
		QProcess m_process{};
		QLocalSocket m_socket{};
		QTimer m_connectTimer{};
		int m_connectAttempts{};

		QString m_ipcName{};
		QStringList m_startupKeys{};
		QByteArray m_readBuffer{};
		QStringList m_outputTail{};		// last few lines mpv printed, shown if it dies

		bool m_paused{ true };
		bool m_stopRequested{ false };
		int64_t m_processId{};
		QString m_mediaTitle{};
		QString m_fileName{};

		double m_volume{ -1.0 };		// the normal (not ducked) volume, negative until mpv reports it
		double m_duckedVolume{ -1.0 };	// what we last turned it down to, so mpv echoing it back isn't mistaken for a new volume
		bool m_ducked{ false };
		int m_duckPercent{ 100 };

#ifdef PI_PLATFORM_WINDOWS
		void* m_job{ nullptr };		// HANDLE, kept as void* so this header doesn't need windows.h
#endif

	};

}
