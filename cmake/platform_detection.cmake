# cmake/platform_detection.cmake

# Platform-specific compile definitions
if(WIN32)
    set(PI_PLATFORM_WINDOWS ON)
    add_compile_definitions(PI_PLATFORM_WINDOWS)
elseif(APPLE)
    set(PI_PLATFORM_MACOS ON)
    add_compile_definitions(PI_PLATFORM_MACOS)
elseif(UNIX)
    set(PI_PLATFORM_LINUX ON)
    add_compile_definitions(PI_PLATFORM_LINUX)
else()
    message(FATAL_ERROR "Automatic Passive Immersion only supports Windows, macOS and Linux")
endif()
