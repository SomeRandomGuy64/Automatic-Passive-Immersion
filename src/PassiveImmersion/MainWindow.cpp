#include "MainWindow.h"

#include "PassiveImmersion/AppIcon.h"
#include "PassiveImmersion/Autostart.h"
#include "PassiveImmersion/Core.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QSpinBox>
#include <QSystemTrayIcon>
#include <QToolButton>
#include <QVBoxLayout>

namespace PassiveImmersion {

	static constexpr int s_stateIconSize{ 40 };

	// On Linux Ctrl+Alt+F1-F12 switch virtual terminal before any app sees them,
	// so pressing one as the pause hotkey would throw you out of the desktop to a blank console
	static bool isSystemShortcut(const QKeySequence& sequence) {

#ifdef PI_PLATFORM_LINUX
		if (sequence.isEmpty()) {
			return false;
		}

		QKeyCombination combination{ sequence[0] };
		Qt::KeyboardModifiers modifiers{ combination.keyboardModifiers() };
		int key{ combination.key() };

		return modifiers.testFlags(Qt::ControlModifier | Qt::AltModifier) && key >= Qt::Key_F1 && key <= Qt::Key_F12;
#else
		Q_UNUSED(sequence);
		return false;
#endif

	}

	static QString joinSourceNames(const std::vector<MediaSource>& sources) {

		QStringList names{};
		for (const MediaSource& source : sources) {
			if (!names.contains(source.name)) {
				names.append(source.name);
			}
		}
		return names.join(", ");

	}

	MainWindow::MainWindow(ImmersionController& controller, const Settings& settings, QWidget* parent)
		: QWidget{ parent }
		, m_controller{ controller }
		, m_settings{ settings }
	{
		m_buildUi();
		m_buildTray();
		m_loadSettingsIntoUi();

		connect(&m_controller, &ImmersionController::stateChanged, this, &MainWindow::m_refreshStatus);
		connect(&m_controller, &ImmersionController::errorOccurred, this, &MainWindow::m_showError);
		connect(&m_controller, &ImmersionController::hotkeyRegistrationChanged, this, &MainWindow::m_refreshHotkeyStatus);

		m_refreshStatus();
		m_refreshHotkeyStatus();
	}

	void MainWindow::showAndRaise() {

		show();
		setWindowState(windowState() & ~Qt::WindowMinimized);
		raise();
		activateWindow();

	}

	void MainWindow::closeEvent(QCloseEvent* event) {

		// With a tray icon, closing the window just hides it - passive audio keeps going
		if (m_trayIcon == nullptr) {
			event->accept();
			QApplication::quit();
			return;
		}

		event->ignore();
		hide();

		if (!m_shownTrayHint) {
			m_trayIcon->showMessage(s_appDisplayName, "Still running in the tray. Use the tray icon's menu to quit.", QSystemTrayIcon::Information, 4000);
			m_shownTrayHint = true;
		}

	}

	void MainWindow::m_buildUi() {

		setWindowIcon(AppIcon::create(ImmersionController::State::Playing));
		setMinimumWidth(520);

		auto* rootLayout{ new QVBoxLayout{ this } };

		// --- Status ---------------------------------------------------------------------------------
		auto* statusBox{ new QGroupBox{ "Status" } };
		auto* statusLayout{ new QHBoxLayout{ statusBox } };

		m_stateIcon = new QLabel{};
		m_stateIcon->setFixedSize(s_stateIconSize, s_stateIconSize);

		m_stateLabel = new QLabel{};
		QFont stateFont{ m_stateLabel->font() };
		stateFont.setPointSizeF(stateFont.pointSizeF() * 1.3);
		stateFont.setBold(true);
		m_stateLabel->setFont(stateFont);

		m_detailLabel = new QLabel{};
		m_detailLabel->setWordWrap(true);
		m_detailLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

		auto* statusText{ new QVBoxLayout{} };
		statusText->addWidget(m_stateLabel);
		statusText->addWidget(m_detailLabel);

		m_pauseButton = new QPushButton{};
		m_startStopButton = new QPushButton{};

		statusLayout->addWidget(m_stateIcon, 0, Qt::AlignTop);
		statusLayout->addLayout(statusText, 1);
		statusLayout->addWidget(m_pauseButton, 0, Qt::AlignTop);
		statusLayout->addWidget(m_startStopButton, 0, Qt::AlignTop);

		connect(m_pauseButton, &QPushButton::clicked, &m_controller, &ImmersionController::togglePause);
		connect(m_startStopButton, &QPushButton::clicked, this, [this]() {
			if (m_controller.getState() == ImmersionController::State::Stopped) {
				m_controller.start();
			}
			else {
				m_controller.stop();
			}
		});

		// --- Audio ----------------------------------------------------------------------------------
		auto* audioBox{ new QGroupBox{ "Passive audio" } };
		auto* audioForm{ new QFormLayout{ audioBox } };

		m_directoryEdit = new QLineEdit{};
		m_directoryEdit->setPlaceholderText("Folder of audio to loop, e.g. your condensed audio");
		auto* browseDirectoryButton{ new QPushButton{ "Browse..." } };
		auto* directoryRow{ new QHBoxLayout{} };
		directoryRow->addWidget(m_directoryEdit);
		directoryRow->addWidget(browseDirectoryButton);
		audioForm->addRow("Audio folder:", directoryRow);

		m_shuffleCheck = new QCheckBox{ "Shuffle" };
		audioForm->addRow("", m_shuffleCheck);

		connect(browseDirectoryButton, &QPushButton::clicked, this, &MainWindow::m_browseForDirectory);
		connect(m_directoryEdit, &QLineEdit::editingFinished, this, &MainWindow::m_onSettingsEdited);
		connect(m_shuffleCheck, &QCheckBox::toggled, this, &MainWindow::m_onSettingsEdited);

		// --- Pausing --------------------------------------------------------------------------------
		auto* pauseBox{ new QGroupBox{ "Pausing" } };
		auto* pauseForm{ new QFormLayout{ pauseBox } };

		m_hotkeyEdit = new QKeySequenceEdit{};
		m_hotkeyEdit->setMaximumSequenceLength(1);
		m_hotkeyEdit->setClearButtonEnabled(true);
		m_hotkeyEdit->setToolTip("Click, then press the key combination you want. Works from any app.");
		pauseForm->addRow("Pause hotkey:", m_hotkeyEdit);

		m_hotkeyStatusLabel = new QLabel{};
		m_hotkeyStatusLabel->setWordWrap(true);
		pauseForm->addRow("", m_hotkeyStatusLabel);

		m_resumeDelaySpin = new QSpinBox{};
		m_resumeDelaySpin->setRange(0, 600);
		m_resumeDelaySpin->setSuffix(" s");
		m_resumeDelaySpin->setToolTip("How long other media has to stay quiet before passive audio comes back");
		pauseForm->addRow("Resume after:", m_resumeDelaySpin);

		m_ignoredAppsEdit = new QLineEdit{};
		m_ignoredAppsEdit->setPlaceholderText("e.g. discord, steam");
		m_ignoredAppsEdit->setToolTip("Comma-separated app names that never pause passive audio");
		pauseForm->addRow("Never pause for:", m_ignoredAppsEdit);

		auto* detectorLabel{ new QLabel{ m_controller.getDetectorDescription() } };
		detectorLabel->setWordWrap(true);
		detectorLabel->setEnabled(false);
		pauseForm->addRow("", detectorLabel);

		connect(m_hotkeyEdit, &QKeySequenceEdit::editingFinished, this, &MainWindow::m_onSettingsEdited);
		connect(m_hotkeyEdit, &QKeySequenceEdit::keySequenceChanged, this, [this](const QKeySequence& sequence) {
			// The clear button doesn't emit editingFinished
			if (sequence.isEmpty()) {
				m_onSettingsEdited();
			}
		});
		connect(m_resumeDelaySpin, &QSpinBox::valueChanged, this, &MainWindow::m_onSettingsEdited);
		connect(m_ignoredAppsEdit, &QLineEdit::editingFinished, this, &MainWindow::m_onSettingsEdited);

		// --- Startup --------------------------------------------------------------------------------
		auto* startupBox{ new QGroupBox{ "Startup" } };
		auto* startupLayout{ new QVBoxLayout{ startupBox } };

		m_playOnLaunchCheck = new QCheckBox{ "Start playing when the app opens" };
		m_launchAtLoginCheck = new QCheckBox{ "Launch at login (starts hidden in the tray)" };
		startupLayout->addWidget(m_playOnLaunchCheck);
		startupLayout->addWidget(m_launchAtLoginCheck);

		connect(m_playOnLaunchCheck, &QCheckBox::toggled, this, &MainWindow::m_onSettingsEdited);
		connect(m_launchAtLoginCheck, &QCheckBox::toggled, this, &MainWindow::m_onLaunchAtLoginToggled);

		// --- Advanced (collapsed by default) --------------------------------------------------------
		auto* advancedToggle{ new QToolButton{} };
		advancedToggle->setText("Advanced");
		advancedToggle->setCheckable(true);
		advancedToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		advancedToggle->setArrowType(Qt::RightArrow);
		advancedToggle->setAutoRaise(true);

		auto* advancedBox{ new QGroupBox{} };
		auto* advancedForm{ new QFormLayout{ advancedBox } };
		advancedBox->setVisible(false);

		m_mpvPathEdit = new QLineEdit{};
		QString detectedMpv{ MpvPlayer::findMpv({}) };
		m_mpvPathEdit->setPlaceholderText(detectedMpv.isEmpty() ? "mpv not found - set its path here" : "Auto: " + detectedMpv);
		auto* browseMpvButton{ new QPushButton{ "Browse..." } };
		auto* mpvRow{ new QHBoxLayout{} };
		mpvRow->addWidget(m_mpvPathEdit);
		mpvRow->addWidget(browseMpvButton);
		advancedForm->addRow("mpv executable:", mpvRow);

		m_extraArgsEdit = new QLineEdit{};
		m_extraArgsEdit->setPlaceholderText("e.g. --volume=70");
		advancedForm->addRow("Extra mpv arguments:", m_extraArgsEdit);

		m_startupKeysEdit = new QLineEdit{};
		m_startupKeysEdit->setPlaceholderText("e.g. ctrl+p");
		m_startupKeysEdit->setToolTip("Keys pressed in mpv right after it starts (space-separated), handy for mpv scripts");
		advancedForm->addRow("Keys to press on start:", m_startupKeysEdit);

		auto* settingsPathLabel{ new QLabel{ QString{ "%1settings: %2" }
			.arg(Settings::isPortable() ? "Portable " : "", QDir::toNativeSeparators(Settings::getFilePath())) } };
		settingsPathLabel->setWordWrap(true);
		settingsPathLabel->setEnabled(false);
		settingsPathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
		advancedForm->addRow(settingsPathLabel);

		connect(advancedToggle, &QToolButton::toggled, this, [this, advancedToggle, advancedBox](bool open) {
			advancedToggle->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
			advancedBox->setVisible(open);
			adjustSize();
		});
		connect(browseMpvButton, &QPushButton::clicked, this, &MainWindow::m_browseForMpv);
		connect(m_mpvPathEdit, &QLineEdit::editingFinished, this, &MainWindow::m_onSettingsEdited);
		connect(m_extraArgsEdit, &QLineEdit::editingFinished, this, &MainWindow::m_onSettingsEdited);
		connect(m_startupKeysEdit, &QLineEdit::editingFinished, this, &MainWindow::m_onSettingsEdited);

		rootLayout->addWidget(statusBox);
		rootLayout->addWidget(audioBox);
		rootLayout->addWidget(pauseBox);
		rootLayout->addWidget(startupBox);
		rootLayout->addWidget(advancedToggle, 0, Qt::AlignLeft);
		rootLayout->addWidget(advancedBox);
		rootLayout->addStretch();

	}

	void MainWindow::m_buildTray() {

		if (!QSystemTrayIcon::isSystemTrayAvailable()) {
			return;
		}

		m_trayIcon = new QSystemTrayIcon{ AppIcon::create(ImmersionController::State::Stopped), this };
		auto* menu{ new QMenu{ this } };

		m_trayStatusAction = menu->addAction("");
		m_trayStatusAction->setEnabled(false);
		menu->addSeparator();

		menu->addAction("Show window", this, &MainWindow::showAndRaise);
		m_trayPauseAction = menu->addAction("", &m_controller, &ImmersionController::togglePause);
		m_trayStartStopAction = menu->addAction("", this, [this]() {
			m_startStopButton->click();
		});
		menu->addSeparator();
		menu->addAction("Quit", qApp, &QApplication::quit);

		m_trayIcon->setContextMenu(menu);

		connect(m_trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
			if (reason != QSystemTrayIcon::Trigger) {
				return;
			}
			if (isVisible()) {
				hide();
			}
			else {
				showAndRaise();
			}
		});

		m_trayIcon->show();

	}

	void MainWindow::m_loadSettingsIntoUi() {

		// Filling the widgets would otherwise fire every "edited" handler
		const QList<QWidget*> widgets{
			m_directoryEdit, m_shuffleCheck, m_hotkeyEdit, m_resumeDelaySpin, m_ignoredAppsEdit,
			m_playOnLaunchCheck, m_launchAtLoginCheck, m_mpvPathEdit, m_extraArgsEdit, m_startupKeysEdit
		};
		for (QWidget* widget : widgets) {
			widget->blockSignals(true);
		}

		m_directoryEdit->setText(QDir::toNativeSeparators(m_settings.audioDirectory));
		m_shuffleCheck->setChecked(m_settings.shuffle);
		m_hotkeyEdit->setKeySequence(m_settings.pauseHotkey);
		m_resumeDelaySpin->setValue(m_settings.resumeDelaySeconds);
		m_ignoredAppsEdit->setText(m_settings.ignoredApps.join(", "));
		m_playOnLaunchCheck->setChecked(m_settings.playOnLaunch);
		m_launchAtLoginCheck->setChecked(Autostart::isEnabled());
		m_mpvPathEdit->setText(QDir::toNativeSeparators(m_settings.mpvPath));
		m_extraArgsEdit->setText(m_settings.extraMpvArgs);
		m_startupKeysEdit->setText(m_settings.startupKeys);

		for (QWidget* widget : widgets) {
			widget->blockSignals(false);
		}

	}

	void MainWindow::m_onSettingsEdited() {

		if (isSystemShortcut(m_hotkeyEdit->keySequence())) {
			m_hotkeyEdit->blockSignals(true);
			m_hotkeyEdit->setKeySequence(m_settings.pauseHotkey);
			m_hotkeyEdit->blockSignals(false);

			m_hotkeyStatusLabel->setText("Ctrl+Alt+F1-F12 switch virtual terminal on Linux, pick a different hotkey.");
			m_hotkeyStatusLabel->setStyleSheet("color: #c0392b;");
			return;
		}

		m_settings.audioDirectory = QDir::fromNativeSeparators(m_directoryEdit->text().trimmed());
		m_settings.shuffle = m_shuffleCheck->isChecked();
		m_settings.pauseHotkey = m_hotkeyEdit->keySequence();
		m_settings.resumeDelaySeconds = m_resumeDelaySpin->value();
		m_settings.playOnLaunch = m_playOnLaunchCheck->isChecked();
		m_settings.mpvPath = QDir::fromNativeSeparators(m_mpvPathEdit->text().trimmed());
		m_settings.extraMpvArgs = m_extraArgsEdit->text().trimmed();
		m_settings.startupKeys = m_startupKeysEdit->text().trimmed();

		m_settings.ignoredApps.clear();
		for (const QString& app : m_ignoredAppsEdit->text().split(',')) {
			if (!app.trimmed().isEmpty()) {
				m_settings.ignoredApps.append(app.trimmed());
			}
		}

		m_settings.save();
		m_controller.applySettings(m_settings);
		m_refreshStatus();

	}

	void MainWindow::m_onLaunchAtLoginToggled(bool enabled) {

		if (!Autostart::setEnabled(enabled)) {
			m_showError("Couldn't change the launch-at-login setting.");
			m_launchAtLoginCheck->blockSignals(true);
			m_launchAtLoginCheck->setChecked(Autostart::isEnabled());
			m_launchAtLoginCheck->blockSignals(false);
		}

	}

	void MainWindow::m_browseForDirectory() {

		QString directory{ QFileDialog::getExistingDirectory(this, "Choose the passive audio folder", m_directoryEdit->text()) };
		if (directory.isEmpty()) {
			return;
		}

		m_directoryEdit->setText(QDir::toNativeSeparators(directory));
		m_onSettingsEdited();

		// Picking a folder for the first time is as good as pressing Start
		if (m_controller.getState() == ImmersionController::State::Stopped) {
			m_controller.start();
		}

	}

	void MainWindow::m_browseForMpv() {

		QString path{ QFileDialog::getOpenFileName(this, "Choose the mpv executable", m_mpvPathEdit->text()) };
		if (path.isEmpty()) {
			return;
		}

		m_mpvPathEdit->setText(QDir::toNativeSeparators(path));
		m_onSettingsEdited();

	}

	void MainWindow::m_refreshStatus() {

		using State = ImmersionController::State;
		State state{ m_controller.getState() };

		QString stateText{};
		QString detailText{};

		switch (state) {
			case State::Stopped:
				stateText = "Stopped";
				detailText = m_controller.getLastError().isEmpty() ? "Press Start to begin." : m_controller.getLastError();
				break;

			case State::Starting:
				stateText = "Starting mpv...";
				break;

			case State::Playing:
				stateText = "Playing";
				detailText = m_controller.getMediaTitle();
				break;

			case State::AutoPaused:
				stateText = "Paused while other media plays";
				detailText = QString{ "Playing: %1" }.arg(joinSourceNames(m_controller.getActiveSources()));
				break;

			case State::UserPaused:
				stateText = "Paused";
				detailText = m_controller.isHotkeyRegistered()
					? QString{ "Press %1 to resume." }.arg(m_getHotkeyText())
					: QString{ "Press Resume to carry on." };
				break;
		}

		bool showingError{ state == State::Stopped && !m_controller.getLastError().isEmpty() };
		m_detailLabel->setStyleSheet(showingError ? "color: #c0392b;" : "");

		QIcon icon{ AppIcon::create(state) };
		m_stateIcon->setPixmap(icon.pixmap(s_stateIconSize, s_stateIconSize));
		m_stateLabel->setText(stateText);
		m_detailLabel->setText(detailText);

		bool running{ state != State::Stopped };
		bool paused{ state == State::UserPaused };
		m_startStopButton->setText(running ? "Stop" : "Start");
		m_pauseButton->setText(paused ? "Resume" : "Pause");
		m_pauseButton->setEnabled(running && state != State::Starting);

		if (m_trayIcon != nullptr) {
			m_trayIcon->setIcon(icon);
			m_trayIcon->setToolTip(QString{ "%1\n%2" }.arg(s_appDisplayName, stateText));
			m_trayStatusAction->setText(stateText);
			m_trayPauseAction->setText(m_pauseButton->text());
			m_trayPauseAction->setEnabled(m_pauseButton->isEnabled());
			m_trayStartStopAction->setText(m_startStopButton->text());
		}

	}

	void MainWindow::m_refreshHotkeyStatus() {

		if (m_settings.pauseHotkey.isEmpty()) {
			m_hotkeyStatusLabel->setText("No hotkey set.");
			m_hotkeyStatusLabel->setStyleSheet("");
		}
		else if (m_controller.isHotkeyRegistered()) {
			m_hotkeyStatusLabel->setText(QString{ "%1 pauses and resumes passive audio from any app." }.arg(m_getHotkeyText()));
			m_hotkeyStatusLabel->setStyleSheet("");
		}
		else if (QGuiApplication::platformName() == "wayland") {
			m_hotkeyStatusLabel->setText("Global hotkeys don't work on Wayland. Use the tray menu, or run the app under X11.");
			m_hotkeyStatusLabel->setStyleSheet("color: #c0392b;");
		}
		else {
			m_hotkeyStatusLabel->setText("Couldn't register this hotkey, another app is probably using it.");
			m_hotkeyStatusLabel->setStyleSheet("color: #c0392b;");
		}

	}

	void MainWindow::m_showError(const QString& message) {

		m_refreshStatus();

		if (m_trayIcon != nullptr && !isVisible()) {
			m_trayIcon->showMessage(s_appDisplayName, message, QSystemTrayIcon::Warning, 8000);
		}

	}

	QString MainWindow::m_getHotkeyText() const {
		return m_settings.pauseHotkey.toString(QKeySequence::NativeText);
	}

}
