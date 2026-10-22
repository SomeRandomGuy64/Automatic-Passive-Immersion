#pragma once

#include "PassiveImmersion/ImmersionController.h"

#include <QIcon>

namespace PassiveImmersion {

	// Draws the window/tray icon at runtime so the tray shows the current state at a glance
	// (green play = playing, amber pause = other media playing, blue pause = paused by you, grey = stopped)
	class AppIcon {

	public:
		static QIcon create(ImmersionController::State state);

	};

}
