#pragma once

#include "PassiveImmersion/MediaDetector.h"

#ifndef NOMINMAX
	#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
	#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <objbase.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>

namespace PassiveImmersion {

	// Looks at the Windows audio mixer: any app with an active audio session on any output device
	// that is actually producing sound (peak meter above silence) counts as playing
	class WindowsMediaDetector : public MediaDetector {

	public:
		WindowsMediaDetector();
		~WindowsMediaDetector() override;

		std::vector<MediaSource> getPlayingSources() override;

		inline QString getDescription() const override { return "Watching apps that are making sound (Windows audio sessions)"; }

	private:
		static QString m_getProcessName(DWORD processId);

	private:
		Microsoft::WRL::ComPtr<IMMDeviceEnumerator> m_deviceEnumerator{};
		bool m_comInitialised{ false };

	};

}
