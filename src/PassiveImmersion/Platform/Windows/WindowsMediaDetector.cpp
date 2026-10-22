#include "WindowsMediaDetector.h"

#include <QDebug>
#include <QFileInfo>

#include <audiopolicy.h>
#include <endpointvolume.h>

using Microsoft::WRL::ComPtr;

namespace PassiveImmersion {

	// Peak meter values are 0.0 - 1.0, anything below this is treated as silence
	static constexpr float s_silenceThreshold{ 0.0001f };

	std::unique_ptr<MediaDetector> MediaDetector::create() {
		return std::make_unique<WindowsMediaDetector>();
	}

	WindowsMediaDetector::WindowsMediaDetector()
	{
		// Qt has normally already set up COM on the GUI thread, in which case this just bumps its ref count
		HRESULT result{ CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED) };
		m_comInitialised = SUCCEEDED(result);

		result = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&m_deviceEnumerator));
		if (FAILED(result)) {
			qWarning() << "Couldn't create the audio device enumerator, HRESULT" << Qt::hex << result;
		}
	}

	WindowsMediaDetector::~WindowsMediaDetector()
	{
		m_deviceEnumerator.Reset();
		if (m_comInitialised) {
			CoUninitialize();
		}
	}

	std::vector<MediaSource> WindowsMediaDetector::getPlayingSources() {

		std::vector<MediaSource> sources{};
		if (!m_deviceEnumerator) {
			return sources;
		}

		ComPtr<IMMDeviceCollection> devices{};
		if (FAILED(m_deviceEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &devices))) {
			return sources;
		}

		UINT deviceCount{};
		devices->GetCount(&deviceCount);

		// Every output device, not just the default one, so headphones + speakers setups still work
		for (UINT deviceIndex{ 0 }; deviceIndex < deviceCount; ++deviceIndex) {
			ComPtr<IMMDevice> device{};
			ComPtr<IAudioSessionManager2> sessionManager{};
			ComPtr<IAudioSessionEnumerator> sessions{};

			if (FAILED(devices->Item(deviceIndex, &device))
				|| FAILED(device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(sessionManager.GetAddressOf())))
				|| FAILED(sessionManager->GetSessionEnumerator(&sessions))) {
				continue;
			}

			int sessionCount{};
			sessions->GetCount(&sessionCount);

			for (int sessionIndex{ 0 }; sessionIndex < sessionCount; ++sessionIndex) {
				ComPtr<IAudioSessionControl> control{};
				if (FAILED(sessions->GetSession(sessionIndex, &control))) {
					continue;
				}

				// Inactive = the app has no open stream (e.g. a paused browser tab after a few seconds)
				AudioSessionState state{};
				if (FAILED(control->GetState(&state)) || state != AudioSessionStateActive) {
					continue;
				}

				ComPtr<IAudioSessionControl2> control2{};
				if (FAILED(control.As(&control2)) || control2->IsSystemSoundsSession() == S_OK) {
					continue;
				}

				// Some apps (games especially) keep a stream open while silent, so also require actual sound
				ComPtr<IAudioMeterInformation> meter{};
				float peak{};
				if (FAILED(control.As(&meter)) || FAILED(meter->GetPeakValue(&peak)) || peak < s_silenceThreshold) {
					continue;
				}

				DWORD processId{};
				control2->GetProcessId(&processId);

				sources.push_back({ m_getProcessName(processId), static_cast<int64_t>(processId) });
			}
		}

		return sources;

	}

	// "C:\Program Files\Google\Chrome\Application\chrome.exe" -> "chrome"
	QString WindowsMediaDetector::m_getProcessName(DWORD processId) {

		HANDLE process{ OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId) };
		if (process == nullptr) {
			return QString{ "PID %1" }.arg(processId);
		}

		wchar_t path[MAX_PATH]{};
		DWORD size{ MAX_PATH };
		BOOL success{ QueryFullProcessImageNameW(process, 0, path, &size) };
		CloseHandle(process);

		if (!success) {
			return QString{ "PID %1" }.arg(processId);
		}

		return QFileInfo{ QString::fromWCharArray(path, static_cast<qsizetype>(size)) }.completeBaseName();

	}

}
