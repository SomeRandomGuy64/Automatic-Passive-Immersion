# cmake/compiler_options.cmake

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Function to set compiler flags
function(set_compiler_flags target)
    # Common compiler warnings and standards
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4                     # Enable high-level warnings
            /WX                     # Treat warnings as errors
            /external:anglebrackets # Don't hold Windows SDK and Qt headers to the same standard
            /external:W0
        )
    else()
        target_compile_options(${target} PRIVATE
            -Wall   # Enable all warnings
            -Werror # Treat warnings as errors
        )
    endif()
endfunction()
