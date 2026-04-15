cmake_minimum_required(VERSION 3.10)
project(CameraViewerApp)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(ENABLE_FFMPEG "Build with FFmpeg video decoding" OFF)

# Default to a local 'ffmpeg' folder (Windows); can be overridden with -DFFMPEG_ROOT=...
set(FFMPEG_ROOT "${CMAKE_SOURCE_DIR}/ffmpeg" CACHE PATH "Path to FFmpeg install prefix (bin/include/lib layout)")

# Camera SDK
if(NOT DEFINED CAMERA_SDK_PATH)
    message(FATAL_ERROR "ERROR: CAMERA_SDK_PATH is not set! Please provide it when running CMake.")
endif()
find_package(CameraLibrary REQUIRED HINTS ${CAMERA_SDK_PATH})

# CameraLibrary ships as Release-only; map all other configs to it so the
# linker finds the import lib in Debug / RelWithDebInfo / MinSizeRel builds.
set_target_properties(CameraLibrary::CameraLibrary PROPERTIES
    MAP_IMPORTED_CONFIG_DEBUG          "Release"
    MAP_IMPORTED_CONFIG_RELWITHDEBINFO "Release"
    MAP_IMPORTED_CONFIG_MINSIZEREL     "Release"
)

# Qt & OpenGL
find_package(Qt6 REQUIRED COMPONENTS Core Gui OpenGLWidgets Concurrent Svg LinguistTools)
find_package(OpenGL REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Sources
set(APP_SOURCES
    main.cpp
	FocusEval.cpp
	MetricsManager.cpp
    CircleMarkerDetector.cpp
    BitmapPool.cpp
    QtCameraViewer.cpp
    QtCameraPicker.cpp
    QtCameraConnectionManager.cpp
    QtVideoWidget.cpp
    QtCameraControlPanel.cpp
    metricscontroller.cpp
    widgets/graphwidget.cpp
	FocusEval.h
)

# Only compile the decoder if FFmpeg is enabled
if(ENABLE_FFMPEG)
    list(APPEND APP_SOURCES videodecoder.cpp)
    add_compile_definitions(HAVE_FFMPEG=1)
endif()

qt6_wrap_cpp(MOC_SOURCES
    QtCameraViewer.h
    QtCameraConnectionManager.h
    QtCameraPicker.h
    QtVideoWidget.h
    QtCameraControlPanel.h
    metricscontroller.h
	FocusEval.h
	MetricsManager.h
    widgets/graphwidget.h
    FocusEval.h
)

add_executable(CameraViewerApp ${APP_SOURCES} ${MOC_SOURCES})
target_include_directories(CameraViewerApp PRIVATE widgets)

# CameraLibrary is Release-only (no debug DLL). Disable MSVC's iterator
# debugging in Debug builds so our STL containers have the same memory
# layout as the SDK's containers, preventing the xmemory proxy crash.
if(MSVC)
    target_compile_definitions(CameraViewerApp PRIVATE
        $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>
    )
endif()

# Translations
# Compile each .ts file to .qm directly into the per-config output directory as
# a POST_BUILD step. This runs for every build configuration (Debug, Release,
# RelWithDebInfo, etc.) and avoids the multi-config generator pitfall of the
# qt6_add_translation / custom-target approach, where the QM generation custom
# command may not re-trigger for configurations built after the first one.

get_target_property(_lrelease_location Qt6::lrelease IMPORTED_LOCATION)

set(TS_FILES i18n/app_en.ts i18n/app_zh_CN.ts)

foreach(_ts_file IN LISTS TS_FILES)
    get_filename_component(_ts_base "${_ts_file}" NAME_WE)
    add_custom_command(TARGET CameraViewerApp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:CameraViewerApp>/i18n"
        COMMAND "${_lrelease_location}"
            "${CMAKE_CURRENT_SOURCE_DIR}/${_ts_file}"
            -qm "$<TARGET_FILE_DIR:CameraViewerApp>/i18n/${_ts_base}.qm"
        COMMENT "Compiling translation: ${_ts_file}"
        VERBATIM
    )
endforeach()

# Copy assets to output folders
set(DESIGNS_DIR "${CMAKE_SOURCE_DIR}/../Designs")

add_custom_command(
    TARGET CameraViewerApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${DESIGNS_DIR}/motive.css"
            "$<TARGET_FILE_DIR:CameraViewerApp>/motive.css"
    VERBATIM
)

# make assets dir and copy 
add_custom_command(
    TARGET CameraViewerApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:CameraViewerApp>/Assets"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${DESIGNS_DIR}/Small/Lock/Lock-On.svg"
            "$<TARGET_FILE_DIR:CameraViewerApp>/Assets/Lock-On.svg"
    VERBATIM
)


# ------------------------------------------
# OpenCV
# ------------------------------------------
# Respect an explicit -DOpenCV_DIR first; otherwise fall back to environment.
if(NOT OpenCV_DIR AND DEFINED ENV{OPENCV_DIR} AND NOT "$ENV{OPENCV_DIR}" STREQUAL "")
    set(OpenCV_DIR "$ENV{OPENCV_DIR}")
endif()
find_package(OpenCV REQUIRED)
message(STATUS "OpenCV version: ${OpenCV_VERSION}")
include_directories(${OpenCV_INCLUDE_DIRS})
target_link_libraries(CameraViewerApp PRIVATE ${OpenCV_LIBS})

# Link basics
target_link_libraries(CameraViewerApp
    PRIVATE 
        Qt6::Core
        Qt6::Gui 
        Qt6::OpenGLWidgets
        Qt6::Concurrent
        Qt6::Svg
        OpenGL::GL
    )

# FFmpeg if included
if(ENABLE_FFMPEG)
    if(WIN32)
        set(FFMPEG_INCLUDE_DIR "${FFMPEG_ROOT}/include")

        set(_ff_lib_candidates
            "${FFMPEG_ROOT}/lib/x64"
            "${FFMPEG_ROOT}/lib/win64"
            "${FFMPEG_ROOT}/lib/amd64"
            "${FFMPEG_ROOT}/lib"
        )
        set(FFMPEG_LIB_DIR "")
        foreach(_cand IN LISTS _ff_lib_candidates)
            if(EXISTS "${_cand}")
                set(FFMPEG_LIB_DIR "${_cand}")
                break()
            endif()
        endforeach()

        if(NOT EXISTS "${FFMPEG_INCLUDE_DIR}")
            message(FATAL_ERROR "FFmpeg include dir not found: ${FFMPEG_INCLUDE_DIR}. Set FFMPEG_ROOT.")
        endif()
        if(NOT FFMPEG_LIB_DIR)
            message(FATAL_ERROR "FFmpeg lib dir not found under ${FFMPEG_ROOT} (tried lib/x64, lib/win64, lib/amd64, lib). Set FFMPEG_ROOT.")
        endif()

        find_library(AVCODEC_LIB
            NAMES avcodec avcodec-61 avcodec-60 avcodec-59
            HINTS "${FFMPEG_LIB_DIR}"
        )
        find_library(AVUTIL_LIB
            NAMES avutil avutil-59 avutil-58
            HINTS "${FFMPEG_LIB_DIR}"
        )
        find_library(SWSCALE_LIB
            NAMES swscale swscale-8 swscale-7
            HINTS "${FFMPEG_LIB_DIR}"
        )

        if(NOT AVCODEC_LIB OR NOT AVUTIL_LIB OR NOT SWSCALE_LIB)
            message(FATAL_ERROR
                "Could not find FFmpeg import libs in ${FFMPEG_LIB_DIR}.\n"
                "Expected avcodec(.lib), avutil(.lib), swscale(.lib) (versioned names also supported)."
            )
        endif()

        target_include_directories(CameraViewerApp PRIVATE "${FFMPEG_INCLUDE_DIR}")
        target_link_libraries(CameraViewerApp PRIVATE ${AVCODEC_LIB} ${AVUTIL_LIB} ${SWSCALE_LIB})

        set(_ff_bin_candidates
            "${FFMPEG_ROOT}/bin/x64"
            "${FFMPEG_ROOT}/bin"
        )
        foreach(_bindir IN LISTS _ff_bin_candidates)
            if(EXISTS "${_bindir}")
                add_custom_command(
                    TARGET CameraViewerApp POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E echo "Copying FFmpeg DLLs from ${_bindir}"
                    COMMAND ${CMAKE_COMMAND} -E copy_directory
                        "${_bindir}"
                        "$<TARGET_FILE_DIR:CameraViewerApp>"
                )
                break()
            endif()
        endforeach()

    elseif(UNIX)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(FFMPEG_PKG QUIET libavcodec libavutil libswscale)
        endif()
        if(FFMPEG_PKG_FOUND)
            target_include_directories(CameraViewerApp PRIVATE ${FFMPEG_PKG_INCLUDE_DIRS})
            target_link_libraries(CameraViewerApp PRIVATE ${FFMPEG_PKG_LIBRARIES})
        else()
            find_library(AVCODEC_LIB avcodec)
            find_library(AVUTIL_LIB  avutil)
            find_library(SWSCALE_LIB swscale)
            if(NOT AVCODEC_LIB OR NOT AVUTIL_LIB OR NOT SWSCALE_LIB)
                message(FATAL_ERROR "System FFmpeg not found. Install libavcodec-dev libavutil-dev libswscale-dev.")
            endif()
            target_link_libraries(CameraViewerApp PRIVATE ${AVCODEC_LIB} ${AVUTIL_LIB} ${SWSCALE_LIB})
        endif()
    endif()
endif()

# CameraLibrary + platform libs
if(WIN32)
    target_link_libraries(
        CameraViewerApp
        PRIVATE
        CameraLibrary::CameraLibrary
        opengl32
        glu32
        ws2_32
        setupapi
    )

    add_custom_command(
        TARGET CameraViewerApp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Copying: $<TARGET_FILE:CameraLibrary::CameraLibrary> -> $<TARGET_FILE_DIR:CameraViewerApp>"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:CameraViewerApp>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:CameraLibrary::CameraLibrary>"
                "$<TARGET_FILE_DIR:CameraViewerApp>"
        VERBATIM
    )
	
	    # Derive Qt bin dir from qmake (works regardless of install location)
    get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
    get_filename_component(QT_INSTALL_PREFIX "${QT_BIN_DIR}" DIRECTORY)
    message(STATUS "Qt bin dir: ${QT_BIN_DIR}")

    # Debug builds use 'd' suffix (Qt6Cored.dll), Release does not
    set(QT_DEBUG_SUFFIX $<$<CONFIG:Debug>:d>)

set(QT_DLLS
    Qt6Core${QT_DEBUG_SUFFIX}.dll
    Qt6Gui${QT_DEBUG_SUFFIX}.dll
    Qt6Widgets${QT_DEBUG_SUFFIX}.dll
    Qt6OpenGLWidgets${QT_DEBUG_SUFFIX}.dll
    Qt6OpenGL${QT_DEBUG_SUFFIX}.dll
    Qt6Concurrent${QT_DEBUG_SUFFIX}.dll
    Qt6Svg${QT_DEBUG_SUFFIX}.dll 
)

foreach(_dll ${QT_DLLS})
    add_custom_command(TARGET CameraViewerApp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Copying Qt DLL: ${_dll}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${QT_BIN_DIR}/${_dll}"
                "$<TARGET_FILE_DIR:CameraViewerApp>"
    )
endforeach()

    # Copy Qt's zh_CN base translations into <exe>/translations/ so that
    # QLibraryInfo::TranslationsPath (which qt.conf redirects there) resolves them.
    add_custom_command(TARGET CameraViewerApp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:CameraViewerApp>/translations"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT_INSTALL_PREFIX}/translations/qtbase_zh_CN.qm"
            "$<TARGET_FILE_DIR:CameraViewerApp>/translations/qtbase_zh_CN.qm"
        VERBATIM
    )

# Copy OpenCV runtime DLLs.
# Supports both layouts:
# 1) flat: <bin>/*.dll
# 2) split: <bin>/Debug and <bin>/Release
set(OPENCV_BIN_DIR "$ENV{OPENCV_BIN_DIR}")

# If OPENCV_BIN_DIR is not provided, infer from OpenCV_DIR.
if(NOT OPENCV_BIN_DIR)
    if(OpenCV_DIR)
        get_filename_component(_opencv_cfg_parent "${OpenCV_DIR}" DIRECTORY)
        if(EXISTS "${_opencv_cfg_parent}/bin")
            set(OPENCV_BIN_DIR "${_opencv_cfg_parent}/bin")
        elseif(EXISTS "${OpenCV_DIR}/bin")
            set(OPENCV_BIN_DIR "${OpenCV_DIR}/bin")
        endif()
    endif()
endif()

if(OPENCV_BIN_DIR)
    set(_opencv_runtime_src "${OPENCV_BIN_DIR}")
    if(EXISTS "${OPENCV_BIN_DIR}/Debug" OR EXISTS "${OPENCV_BIN_DIR}/Release")
        # If config-specific folders exist, map non-Debug configs to Release.
        set(_opencv_runtime_src "${OPENCV_BIN_DIR}/$<IF:$<CONFIG:Debug>,Debug,Release>")
    endif()

    add_custom_command(TARGET CameraViewerApp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Copying OpenCV DLLs from ${_opencv_runtime_src}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${_opencv_runtime_src}"
            "$<TARGET_FILE_DIR:CameraViewerApp>"
    )
else()
    message(WARNING "OPENCV_BIN_DIR is not set and could not be inferred from OpenCV_DIR; OpenCV runtime DLLs will not be copied.")
endif()

elseif(UNIX)
    target_link_libraries(CameraViewerApp
        PRIVATE
        CameraLibrary::CameraLibrary
        rt
        stdc++
        pthread
        jpeg
    )
endif()