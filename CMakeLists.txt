cmake_minimum_required(VERSION 3.22)
project(sbk VERSION 0.3.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(sbk MODULE
    src/plugin-main.c
    src/sbk-state.c
    src/sbk-audio.c
    src/sbk-net.c
    src/sbk-qr.c
    src/source-onair.c
    src/source-lower-third.c
    src/source-ticker.c
    src/source-frame.c
    src/source-visualizer.c
    src/source-clock.c
    src/source-countdown.c
    src/source-card.c
    src/source-backdrop.c
    src/source-chip.c
    src/source-progress.c
    src/source-meter.c
    src/source-stats.c
    src/source-counter.c
    src/source-qr.c
    src/transition-wipe.c
    src/scenes.c)

target_compile_definitions(sbk PRIVATE SBK_VERSION="${PROJECT_VERSION}")
target_compile_options(sbk PRIVATE -Wall -Wextra -Wno-unused-parameter)

if(APPLE)
    # OBS.app ships the libobs binary but not its headers, so the matching
    # headers live in deps/include and we link straight against the frameworks
    # inside the app. No SDK download, no network at build time.
    set(OBS_APP "/Applications/OBS.app/Contents/Frameworks")
    if(NOT EXISTS "${OBS_APP}/libobs.framework")
        message(FATAL_ERROR "OBS.app not found in /Applications — see docs/INSTALL.md")
    endif()

    # libobs uses SIMDe to provide SSE intrinsics on Apple silicon. Upstream it
    # is a git submodule, so it is not in the header set.
    find_path(SIMDE_INCLUDE_DIR simde/x86/sse2.h PATHS /opt/homebrew/include /usr/local/include)
    if(NOT SIMDE_INCLUDE_DIR)
        message(FATAL_ERROR "SIMDe headers not found — run: brew install simde")
    endif()

    # jansson parses what the live-data sources fetch. Static, deliberately:
    # linking the dylib bakes an absolute Homebrew path into the plugin, which
    # breaks the moment brew moves or the bundle meets another Mac.
    find_path(JANSSON_INCLUDE_DIR jansson.h PATHS /opt/homebrew/include /usr/local/include)
    find_library(JANSSON_LIB NAMES libjansson.a jansson PATHS /opt/homebrew/lib /usr/local/lib)
    if(NOT JANSSON_INCLUDE_DIR OR NOT JANSSON_LIB)
        message(FATAL_ERROR "jansson not found — run: brew install jansson")
    endif()

    target_include_directories(sbk PRIVATE
        "${CMAKE_SOURCE_DIR}/deps/include/libobs"
        "${CMAKE_SOURCE_DIR}/deps/include"
        "${CMAKE_SOURCE_DIR}/deps/include/generated"
        "${CMAKE_SOURCE_DIR}/deps/include/obs-frontend-api"
        "${SIMDE_INCLUDE_DIR}"
        "${JANSSON_INCLUDE_DIR}")

    target_link_libraries(sbk PRIVATE
        "${OBS_APP}/libobs.framework/Versions/Current/libobs"
        "${OBS_APP}/obs-frontend-api.dylib"
        # curl is on every Mac; nothing to install for the live-data sources
        "curl"
        "${JANSSON_LIB}"
        "-framework CoreFoundation"
        "-framework Security"
        "pthread")

    set_target_properties(sbk PROPERTIES
        BUNDLE TRUE
        BUNDLE_EXTENSION "plugin"
        OUTPUT_NAME "sbk"
        MACOSX_BUNDLE_BUNDLE_NAME "sbk"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.imswarnil.sbk"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")

    # obs_module_file() looks in the bundle's Resources, so the shaders and the
    # locale are copied in rather than left in the source tree.
    file(GLOB SBK_EFFECTS "${CMAKE_SOURCE_DIR}/data/effects/*.effect")
    set_source_files_properties(${SBK_EFFECTS} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/effects")
    file(GLOB SBK_LOCALES "${CMAKE_SOURCE_DIR}/data/locale/*.ini")
    set_source_files_properties(${SBK_LOCALES} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/locale")
    target_sources(sbk PRIVATE ${SBK_EFFECTS} ${SBK_LOCALES})
else()
    # Linux and Windows: build against an installed OBS development package.
    find_package(libobs REQUIRED)
    find_package(obs-frontend-api REQUIRED)
    target_link_libraries(sbk PRIVATE OBS::libobs OBS::obs-frontend-api curl jansson pthread)
    set_target_properties(sbk PROPERTIES PREFIX "")
    install(TARGETS sbk LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/obs-plugins")
    install(DIRECTORY data/ DESTINATION "share/obs/obs-plugins/sbk")
endif()
