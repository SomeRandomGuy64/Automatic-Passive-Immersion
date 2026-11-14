#include "FolderWatcher.h"

#include "PassiveImmersion/Core.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QSignalBlocker>

namespace PassiveImmersion {

	// Copying a batch of files fires a burst of notifications, wait for it to calm down before scanning
	static constexpr int s_debounceMs{ 1'500 };

	// Network drives and some mounts never send notifications, so look anyway every so often
	static constexpr int s_fallbackScanMs{ 60'000 };

	// mpv's own default audio-exts and video-exts (0.38+), used until mpv says otherwise.
	// Older mpv versions don't report a list, they hand every file in the folder over and skip what they can't play.
	static const QStringList s_defaultExtensions{
		"aac", "ac3", "aiff", "ape", "au", "awb", "dsf", "dts", "dtshd", "eac3", "f4a", "flac", "lpcm", "m1a",
		"m2a", "m4a", "mka", "mp2", "mp3", "mpa", "mpc", "oga", "ogg", "opus", "pcm", "qcp", "spx", "tak",
		"thd", "tta", "wav", "weba", "wma", "wv",

		"3g2", "3ga", "3gp", "3gp2", "3gpp", "amv", "asf", "avi", "divx", "f4v", "flv", "h264", "h265", "hevc",
		"m2t", "m2ts", "m2v", "m4v", "mk3d", "mkv", "mov", "mp4", "mp4v", "mpe", "mpeg", "mpg", "mpv", "mts",
		"mxf", "nsv", "nut", "ogm", "ogv", "qt", "rm", "rmvb", "ts", "vob", "webm", "wmv", "y4m",
	};

	FolderWatcher::FolderWatcher(QObject* parent)
		: QObject{ parent }
	{
		setExtensions(s_defaultExtensions);

		m_debounceTimer.setInterval(s_debounceMs);
		m_debounceTimer.setSingleShot(true);
		m_fallbackTimer.setInterval(s_fallbackScanMs);

		connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &FolderWatcher::m_scheduleScan);
		connect(&m_debounceTimer, &QTimer::timeout, this, &FolderWatcher::m_scan);
		connect(&m_fallbackTimer, &QTimer::timeout, this, &FolderWatcher::m_scan);
	}

	void FolderWatcher::start(const QString& directory) {

		stop();
		m_directory = QFileInfo{ directory }.absoluteFilePath();

		// Whatever's already there is what mpv just loaded, so none of it needs to wait to settle
		{
			QSignalBlocker blocker{ this };
			m_scan();
		}
		m_settledFiles.unite(QSet<QString>{ m_pendingSizes.keyBegin(), m_pendingSizes.keyEnd() });
		m_pendingSizes.clear();
		m_debounceTimer.stop();

		m_fallbackTimer.start();

	}

	void FolderWatcher::stop() {

		m_directory.clear();
		m_debounceTimer.stop();
		m_fallbackTimer.stop();
		m_pendingSizes.clear();
		m_settledFiles.clear();

		QStringList watched{ m_watcher.directories() };
		if (!watched.isEmpty()) {
			m_watcher.removePaths(watched);
		}

	}

	void FolderWatcher::setExtensions(const QStringList& extensions) {

		m_extensions.clear();
		for (const QString& extension : extensions) {
			QString trimmed{ extension.trimmed().toLower() };
			if (trimmed.startsWith('.')) {
				trimmed.remove(0, 1);
			}
			if (!trimmed.isEmpty()) {
				m_extensions.insert(trimmed);
			}
		}

	}

	QString FolderWatcher::pathKey(const QString& path) {

		QString key{ QDir::cleanPath(QFileInfo{ QDir::fromNativeSeparators(path) }.absoluteFilePath()) };
		key = key.normalized(QString::NormalizationForm_C);

#if defined(PI_PLATFORM_WINDOWS) || defined(PI_PLATFORM_MACOS)
		key = key.toCaseFolded();
#endif

		return key;

	}

	void FolderWatcher::m_scheduleScan() {

		if (isRunning()) {
			m_debounceTimer.start();
		}

	}

	void FolderWatcher::m_scan() {

		if (!isRunning()) {
			return;
		}

		QStringList directories{ m_directory };
		QStringList files{};
		QHash<QString, qint64> pending{};
		QSet<QString> settled{};

		QDirIterator iterator{ m_directory, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories };
		while (iterator.hasNext()) {
			iterator.next();
			QFileInfo info{ iterator.fileInfo() };

			if (info.isDir()) {
				directories.append(info.absoluteFilePath());
				continue;
			}

			if (!m_extensions.contains(info.suffix().toLower())) {
				continue;
			}

			// A file still being copied in keeps growing, so it only counts once two scans in a row see the same size.
			// (Explorer keeps the original modified time when copying, so the size is the only thing to go on.)
			QString path{ info.absoluteFilePath() };
			qint64 size{ info.size() };
			if (m_settledFiles.contains(path) || (size > 0 && m_pendingSizes.value(path, -1) == size)) {
				settled.insert(path);
				files.append(path);
			}
			else {
				pending.insert(path, size);
			}
		}

		m_settledFiles = std::move(settled);
		m_pendingSizes = std::move(pending);

		if (!m_pendingSizes.isEmpty()) {
			m_debounceTimer.start();
		}

		// Folders come and go, keep the watch list matching what's there
		QStringList watched{ m_watcher.directories() };
		QSet<QString> current{ directories.begin(), directories.end() };
		QStringList gone{};
		for (const QString& directory : watched) {
			if (!current.contains(directory)) {
				gone.append(directory);
			}
			current.remove(directory);
		}
		if (!gone.isEmpty()) {
			m_watcher.removePaths(gone);
		}
		if (!current.isEmpty()) {
			m_watcher.addPaths(QStringList{ current.begin(), current.end() });
		}

		emit scanned(files);

	}

}
