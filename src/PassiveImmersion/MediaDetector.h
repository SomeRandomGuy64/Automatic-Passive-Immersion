#pragma once

#include <QString>

#include <cstdint>
#include <memory>
#include <vector>

namespace PassiveImmersion {

	// An app that is playing something right now
	struct MediaSource {

		QString name{};
		int64_t processId{};

	};

	// Interface for asking the OS which apps are currently playing media.
	// Each platform implements it in Platform/<OS>/ and only that file gets compiled.
	class MediaDetector {

	public:
		virtual ~MediaDetector() = default;

		// May include the passive player itself, the caller filters it out by PID
		virtual std::vector<MediaSource> getPlayingSources() = 0;

		// One line shown in the GUI explaining what is being watched
		virtual QString getDescription() const = 0;

		static std::unique_ptr<MediaDetector> create();

	};

}
