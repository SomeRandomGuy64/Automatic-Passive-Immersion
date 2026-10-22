#pragma once

#include <QLocalServer>
#include <QObject>

namespace PassiveImmersion {

	// Makes sure only one copy of the app runs (two would mean two mpv instances playing over each other).
	// Opening the app again just brings the existing window forward.
	class SingleInstance : public QObject {

		Q_OBJECT

	public:
		explicit SingleInstance(QObject* parent = nullptr);

		// Returns false if another instance is already running, after asking it to show its window
		bool tryLock();

	signals:
		void activationRequested();

	private:
		QLocalServer m_server{};

	};

}
