#pragma once

#if !defined(PI_PLATFORM_WINDOWS) && !defined(PI_PLATFORM_MACOS) && !defined(PI_PLATFORM_LINUX)
	#error "Automatic Passive Immersion only supports Windows, macOS and Linux!"
#endif

namespace PassiveImmersion {

	// Used for the settings folder, the single-instance lock and the autostart entry
	inline constexpr const char* s_appId{ "AutomaticPassiveImmersion" };
	inline constexpr const char* s_appDisplayName{ "Automatic Passive Immersion" };

}
