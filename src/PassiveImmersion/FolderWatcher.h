#pragma once

#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>

namespace PassiveImmersion {

	// Watches the audio folder (and every folder inside it) and reports the playable files in it whenever that changes.
	// File system notifications do the quick work, a slow rescan backs them up for drives that never send any.
	class FolderWatcher : public QObject {

		Q_OBJECT

	public:
		explicit FolderWatcher(QObject* parent = nullptr);

		void start(const QString& directory);
		void stop();

		// Swaps the built-in extension list for the one mpv reports, so both agree on what counts as playable
		void setExtensions(const QStringList& extensions);

		inline bool isRunning() const { return !m_directory.isEmpty(); }
		inline bool hasFiles() const { return !m_settledFiles.isEmpty() || !m_pendingSizes.isEmpty(); }

		// Absolute, slashes forward, unicode composed and (on Windows and macOS) lowercased,
		// so the same file always gives the same key however mpv or the OS spelled it
		static QString pathKey(const QString& path);

	signals:
		// Every playable file that's finished arriving, as absolute paths. Sent after each scan, changed or not.
		void scanned(const QStringList& files);

	private:
		void m_scheduleScan();
		void m_scan();

	private:
		QString m_directory{};
		QFileSystemWatcher m_watcher{};
		QTimer m_debounceTimer{};
		QTimer m_fallbackTimer{};

		QSet<QString> m_extensions{};
		QHash<QString, qint64> m_pendingSizes{};		// files seen once whose size has to hold still before they count
		QSet<QString> m_settledFiles{};

	};

}
