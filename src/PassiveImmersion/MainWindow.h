#pragma once

#include "PassiveImmersion/HourLog.h"
#include "PassiveImmersion/ImmersionController.h"
#include "PassiveImmersion/Settings.h"
#include "PassiveImmersion/TrackSync.h"
#include "PassiveImmersion/UpdateChecker.h"

#include <QTimer>
#include <QWidget>

#include <optional>

class QAction;
class QCheckBox;
class QComboBox;
class QKeySequenceEdit;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QSystemTrayIcon;
class QTimeEdit;

namespace PassiveImmersion {

	// Settings + status window, plus the tray icon that keeps the app reachable while the window is hidden.
	// Every edit is saved straight away and handed to the controller, there's no Apply button.
	class MainWindow : public QWidget {

		Q_OBJECT

	public:
		MainWindow(ImmersionController& controller, HourLog& hourLog, TrackSync& trackSync, UpdateChecker& updateChecker,
			const Settings& settings, QWidget* parent = nullptr);

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
		void m_refreshHours();
		void m_refreshTrackStatus();
		void m_refreshEnabledWidgets();

		void m_exportHours();

		void m_onUpdateAvailable(const UpdateChecker::Release& release, bool userAsked);
		void m_showUpdateDialog(const UpdateChecker::Release& release);
		void m_showError(const QString& message);

		QString m_getHotkeyText() const;

	private:
		ImmersionController& m_controller;
		HourLog& m_hourLog;
		TrackSync& m_trackSync;
		UpdateChecker& m_updateChecker;
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
		QComboBox* m_otherMediaActionCombo{ nullptr };
		QSpinBox* m_duckVolumeSpin{ nullptr };
		QSpinBox* m_duckPauseAfterSpin{ nullptr };
		QSpinBox* m_resumeDelaySpin{ nullptr };
		QLineEdit* m_ignoredAppsEdit{ nullptr };
		QCheckBox* m_playOnLaunchCheck{ nullptr };
		QCheckBox* m_launchAtLoginCheck{ nullptr };
		QCheckBox* m_checkForUpdatesCheck{ nullptr };

		// Hours
		QLabel* m_todayLabel{ nullptr };
		QLabel* m_totalLabel{ nullptr };
		QTimeEdit* m_dayStartEdit{ nullptr };
		QTimer m_hoursRefreshTimer{};		// so "today" rolls over even when nothing's playing

		// Track
		QCheckBox* m_trackEnabledCheck{ nullptr };
		QLineEdit* m_trackApiKeyEdit{ nullptr };
		QComboBox* m_trackSubcategoryCombo{ nullptr };
		QLineEdit* m_trackSubcategoryNameEdit{ nullptr };
		QLabel* m_trackStatusLabel{ nullptr };

		// Advanced
		QLineEdit* m_mpvPathEdit{ nullptr };
		QLineEdit* m_extraArgsEdit{ nullptr };
		QLineEdit* m_startupKeysEdit{ nullptr };

		// Tray
		QSystemTrayIcon* m_trayIcon{ nullptr };
		QAction* m_trayStatusAction{ nullptr };
		QAction* m_trayHoursAction{ nullptr };
		QAction* m_trayPauseAction{ nullptr };
		QAction* m_trayStartStopAction{ nullptr };
		bool m_shownTrayHint{ false };

		// An update announced in a tray message, shown properly once the message is clicked
		std::optional<UpdateChecker::Release> m_pendingUpdate{};

	};

}
