#pragma once

#include "PassiveImmersion/ImmersionController.h"
#include "PassiveImmersion/Settings.h"

#include <QWidget>

class QAction;
class QCheckBox;
class QKeySequenceEdit;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QSystemTrayIcon;

namespace PassiveImmersion {

	// Settings + status window, plus the tray icon that keeps the app reachable while the window is hidden.
	// Every edit is saved straight away and handed to the controller, there's no Apply button.
	class MainWindow : public QWidget {

		Q_OBJECT

	public:
		MainWindow(ImmersionController& controller, const Settings& settings, QWidget* parent = nullptr);

		void showAndRaise();

	protected:
		void closeEvent(QCloseEvent* event) override;

	private:
		void m_buildUi();
		void m_buildTray();
		void m_loadSettingsIntoUi();

		void m_onSettingsEdited();
		void m_onLaunchAtLoginToggled(bool enabled);
		void m_browseForDirectory();
		void m_browseForMpv();

		void m_refreshStatus();
		void m_refreshHotkeyStatus();
		void m_showError(const QString& message);

		QString m_getHotkeyText() const;

	private:
		ImmersionController& m_controller;
		Settings m_settings{};

		// Status
		QLabel* m_stateIcon{ nullptr };
		QLabel* m_stateLabel{ nullptr };
		QLabel* m_detailLabel{ nullptr };
		QPushButton* m_startStopButton{ nullptr };
		QPushButton* m_pauseButton{ nullptr };

		// Settings
		QLineEdit* m_directoryEdit{ nullptr };
		QCheckBox* m_shuffleCheck{ nullptr };
		QKeySequenceEdit* m_hotkeyEdit{ nullptr };
		QLabel* m_hotkeyStatusLabel{ nullptr };
		QSpinBox* m_resumeDelaySpin{ nullptr };
		QLineEdit* m_ignoredAppsEdit{ nullptr };
		QCheckBox* m_playOnLaunchCheck{ nullptr };
		QCheckBox* m_launchAtLoginCheck{ nullptr };

		// Advanced
		QLineEdit* m_mpvPathEdit{ nullptr };
		QLineEdit* m_extraArgsEdit{ nullptr };
		QLineEdit* m_startupKeysEdit{ nullptr };

		// Tray
		QSystemTrayIcon* m_trayIcon{ nullptr };
		QAction* m_trayStatusAction{ nullptr };
		QAction* m_trayPauseAction{ nullptr };
		QAction* m_trayStartStopAction{ nullptr };
		bool m_shownTrayHint{ false };

	};

}
