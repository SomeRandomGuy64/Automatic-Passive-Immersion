#pragma once

#include "PassiveImmersion/MediaDetector.h"

namespace PassiveImmersion {

	// Asks Core Audio which processes are currently running audio output (needs macOS 14.2 or newer).
	// Apple locked down the "Now Playing" API in macOS 15.4, so this is the reliable option left.
	class MacMediaDetector : public MediaDetector {

	public:
		std::vector<MediaSource> getPlayingSources() override;

		QString getDescription() const override;

	};

}
