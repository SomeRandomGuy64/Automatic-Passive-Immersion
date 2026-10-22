#pragma once

namespace PassiveImmersion {

	// "Launch at login", done the native way on each OS:
	//   Linux   - ~/.config/autostart/AutomaticPassiveImmersion.desktop
	//   Windows - HKCU\Software\Microsoft\Windows\CurrentVersion\Run
	//   macOS   - ~/Library/LaunchAgents/com.somerandomguy64.automaticpassiveimmersion.plist
	// The app is launched with --minimised so it goes straight to the tray.
	class Autostart {

	public:
		static bool isEnabled();
		static bool setEnabled(bool enabled);

	};

}
