#include "MacMediaDetector.h"

#include <CoreAudio/CoreAudio.h>
#include <libproc.h>

namespace PassiveImmersion {

	std::unique_ptr<MediaDetector> MediaDetector::create() {
		return std::make_unique<MacMediaDetector>();
	}

	template<typename T>
	static bool getProperty(AudioObjectID object, AudioObjectPropertySelector selector, T& value) {

		AudioObjectPropertyAddress address{ selector, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
		UInt32 size{ sizeof(T) };
		return AudioObjectGetPropertyData(object, &address, 0, nullptr, &size, &value) == noErr;

	}

	static QString getProcessName(pid_t processId) {

		char name[256]{};
		if (proc_name(processId, name, sizeof(name)) <= 0) {
			return QString{ "PID %1" }.arg(processId);
		}
		return QString::fromUtf8(name);

	}

	std::vector<MediaSource> MacMediaDetector::getPlayingSources() {

		std::vector<MediaSource> sources{};

		if (__builtin_available(macOS 14.2, *)) {
			AudioObjectPropertyAddress address{
				kAudioHardwarePropertyProcessObjectList,
				kAudioObjectPropertyScopeGlobal,
				kAudioObjectPropertyElementMain
			};

			UInt32 size{};
			if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &address, 0, nullptr, &size) != noErr) {
				return sources;
			}

			std::vector<AudioObjectID> processes(size / sizeof(AudioObjectID));
			if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, nullptr, &size, processes.data()) != noErr) {
				return sources;
			}

			for (AudioObjectID process : processes) {
				UInt32 isRunningOutput{};
				if (!getProperty(process, kAudioProcessPropertyIsRunningOutput, isRunningOutput) || isRunningOutput == 0) {
					continue;
				}

				pid_t processId{};
				getProperty(process, kAudioProcessPropertyPID, processId);

				sources.push_back({ getProcessName(processId), processId });
			}
		}

		return sources;

	}

	QString MacMediaDetector::getDescription() const {

		if (__builtin_available(macOS 14.2, *)) {
			return "Watching apps that are playing audio (Core Audio)";
		}
		return "Detecting other media needs macOS 14.2 or newer, passive audio won't pause automatically";

	}

}
