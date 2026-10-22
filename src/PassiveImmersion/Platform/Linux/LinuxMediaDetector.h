#pragma once

#include "PassiveImmersion/MediaDetector.h"

namespace PassiveImmersion {

	// Asks every MPRIS media player on the session D-Bus whether it's playing - the same thing `playerctl -a status` does
	class LinuxMediaDetector : public MediaDetector {

	public:
		std::vector<MediaSource> getPlayingSources() override;

		inline QString getDescription() const override { return "Watching MPRIS media players (browsers, mpv with mpv-mpris, VLC, Spotify...)"; }

	};

}
