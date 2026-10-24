cmake_minimum_required(VERSION 3.21)

project(AutomaticPassiveImmersion
    VERSION 0.2.0
    DESCRIPTION "Plays passive immersion audio and pauses it whenever other media is playing"
    LANGUAGES CXX
)

# Add cmake directory to module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Include modular cmake configurations
include(compiler_options)
include(platform_detection)

# Windows gets a flat portable folder (exe next to its DLLs), which is what people expect on Windows.
# This has to happen before Qt is found: Qt's deploy tool locks in the install layout during qt_standard_project_setup()
if(PI_PLATFORM_WINDOWS)
    set(CMAKE_INSTALL_BINDIR "." CACHE PATH "Executables go in the root of the portable folder" FORCE)
endif()

set(QT_COMPONENTS Core Gui Widgets Network)
if(PI_PLATFORM_LINUX)
    list(APPEND QT_COMPONENTS DBus)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS ${QT_COMPONENTS})
qt_standard_project_setup()

include(dependencies)

qt_add_executable(AutomaticPassiveImmersion WIN32 MACOSX_BUNDLE
    src/main.cpp

    src/PassiveImmersion/Core.h

    src/PassiveImmersion/Settings.h
    src/PassiveImmersion/Settings.cpp

    src/PassiveImmersion/MpvPlayer.h
    src/PassiveImmersion/MpvPlayer.cpp

    src/PassiveImmersion/MediaDetector.h

    src/PassiveImmersion/ImmersionController.h
    src/PassiveImmersion/ImmersionController.cpp

    src/PassiveImmersion/HourLog.h
    src/PassiveImmersion/HourLog.cpp

    src/PassiveImmersion/TrackSync.h
    src/PassiveImmersion/TrackSync.cpp

    src/PassiveImmersion/UpdateChecker.h
    src/PassiveImmersion/UpdateChecker.cpp

    src/PassiveImmersion/MainWindow.h
    src/PassiveImmersion/MainWindow.cpp

    src/PassiveImmersion/AppIcon.h
    src/PassiveImmersion/AppIcon.cpp

    src/PassiveImmersion/Autostart.h
    src/PassiveImmersion/Autostart.cpp

    src/PassiveImmersion/SingleInstance.h
    src/PassiveImmersion/SingleInstance.cpp
)

# Each OS gets its own way of finding out whether something else is playing
if(PI_PLATFORM_LINUX)
    target_sources(AutomaticPassiveImmersion PRIVATE
        src/PassiveImmersion/Platform/Linux/LinuxMediaDetector.h
        src/PassiveImmersion/Platform/Linux/LinuxMediaDetector.cpp
    )
    target_link_libraries(AutomaticPassiveImmersion PRIVATE Qt6::DBus)

elseif(PI_PLATFORM_WINDOWS)
    target_sources(AutomaticPassiveImmersion PRIVATE
        src/PassiveImmersion/Platform/Windows/WindowsMediaDetector.h
        src/PassiveImmersion/Platform/Windows/WindowsMediaDetector.cpp
    )
    target_link_libraries(AutomaticPassiveImmersion PRIVATE ole32 uuid)

elseif(PI_PLATFORM_MACOS)
    target_sources(AutomaticPassiveImmersion PRIVATE
        src/PassiveImmersion/Platform/MacOS/MacMediaDetector.h
        src/PassiveImmersion/Platform/MacOS/MacMediaDetector.cpp
    )
    target_link_libraries(AutomaticPassiveImmersion PRIVATE "-framework CoreAudio")

endif()

target_link_libraries(AutomaticPassiveImmersion PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    QHotkey::QHotkey
)

target_include_directories(AutomaticPassiveImmersion PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(AutomaticPassiveImmersion PRIVATE cxx_std_23)
target_compile_definitions(AutomaticPassiveImmersion PRIVATE
    PI_VERSION="${PROJECT_VERSION}"
)

set_compiler_flags(AutomaticPassiveImmersion)

set_target_properties(AutomaticPassiveImmersion PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.somerandomguy64.automaticpassiveimmersion"
    MACOSX_BUNDLE_BUNDLE_NAME "Automatic Passive Immersion"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/MacOSXBundleInfo.plist.in"
)

# `cmake --install build --prefix <absolute path>` produces a self-contained folder with Qt copied next to the app
include(GNUInstallDirs)

install(TARGETS AutomaticPassiveImmersion
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_generate_deploy_app_script(
    TARGET AutomaticPassiveImmersion
    OUTPUT_SCRIPT deployScript
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deployScript})

# Ship the MSVC runtime too, so the app runs on PCs without the Visual C++ Redistributable installed
if(MSVC)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ${CMAKE_INSTALL_BINDIR})
    include(InstallRequiredSystemLibraries)
endif()
