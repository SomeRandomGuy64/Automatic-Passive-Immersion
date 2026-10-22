# cmake/dependencies.cmake

include(FetchContent)

# QHotkey: global hotkeys on Windows, macOS and X11
set(QT_DEFAULT_MAJOR_VERSION 6 CACHE STRING "Qt version QHotkey builds against" FORCE)
set(QHOTKEY_INSTALL OFF CACHE BOOL "Don't install QHotkey alongside the app" FORCE)
set(QHOTKEY_EXAMPLES OFF CACHE BOOL "Don't build QHotkey's examples" FORCE)

FetchContent_Declare(QHotkey
    GIT_REPOSITORY https://github.com/Skycoder42/QHotkey.git
    GIT_TAG 4e3a244d87f1f7e741e1395f2ffe825f3a8ada45
)
FetchContent_MakeAvailable(QHotkey)
