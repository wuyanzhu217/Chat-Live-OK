cmake_minimum_required(VERSION 3.16)

# =============================================================================
# Windows: Qt 6.9.1 MinGW + bundled FFmpeg (lib/*.dll.a)
# Linux:   system FFmpeg (pkg-config) + Qt via CMAKE_PREFIX_PATH / aqtinstall
# Env:
#   QT_ROOT / QTDIR / CMAKE_PREFIX_PATH — Qt install prefix
#   MINGW_ROOT — MinGW on Windows
# =============================================================================
if(WIN32)
    if(DEFINED ENV{QT_ROOT})
        set(CCV_QT_ROOT "$ENV{QT_ROOT}")
    elseif(DEFINED ENV{QTDIR})
        set(CCV_QT_ROOT "$ENV{QTDIR}")
    else()
        set(CCV_QT_ROOT "E:/QT/6.9.1/mingw_64")
    endif()

    if(DEFINED ENV{MINGW_ROOT})
        set(CCV_MINGW_ROOT "$ENV{MINGW_ROOT}")
    else()
        set(CCV_MINGW_ROOT "E:/QT/Tools/mingw1310_64")
    endif()

    if(EXISTS "${CCV_QT_ROOT}/lib/cmake/Qt6")
        list(PREPEND CMAKE_PREFIX_PATH "${CCV_QT_ROOT}")
        set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}" CACHE PATH "Qt prefix path" FORCE)
    else()
        message(WARNING "Qt not found at CCV_QT_ROOT=${CCV_QT_ROOT}. "
                        "Set QT_ROOT or CMAKE_PREFIX_PATH.")
    endif()

    if(NOT CMAKE_CXX_COMPILER AND EXISTS "${CCV_MINGW_ROOT}/bin/g++.exe")
        set(CMAKE_CXX_COMPILER "${CCV_MINGW_ROOT}/bin/g++.exe"
            CACHE FILEPATH "MinGW g++" FORCE)
        set(CMAKE_C_COMPILER "${CCV_MINGW_ROOT}/bin/gcc.exe"
            CACHE FILEPATH "MinGW gcc" FORCE)
    endif()
elseif(UNIX AND NOT APPLE)
    if(DEFINED ENV{QT_ROOT})
        list(PREPEND CMAKE_PREFIX_PATH "$ENV{QT_ROOT}")
    elseif(DEFINED ENV{QTDIR})
        list(PREPEND CMAKE_PREFIX_PATH "$ENV{QTDIR}")
    endif()
endif()

project(ccvWriter VERSION 0.1 LANGUAGES CXX)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR
        "Do not build inside the source tree.\n"
        "  Windows: cmake -S . -B build/mingw-debug -G Ninja\n"
        "  Linux:   cmake -S . -B build/linux-debug -G Ninja")
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 REQUIRED COMPONENTS Quick Core)

qt_standard_project_setup(REQUIRES 6.8)

set(STREAMING_CAPTURE_SOURCES
    streaming/capture/FfmpegPlatformCapture.cpp
    streaming/capture/FfmpegDeviceEnumerator.cpp
    streaming/capture/FfmpegCaptureBase.cpp
    streaming/capture/FfmpegVideoCapture.cpp
    streaming/capture/FfmpegAudioCapture.cpp
)

set(STREAMING_EFFECTS_SOURCES
    streaming/core/FramePool.cpp
    streaming/effects/FfmpegFilterGraph.cpp
    streaming/effects/EffectChain.cpp
    streaming/effects/EffectChainFactory.cpp
    streaming/effects/EffectController.cpp
    streaming/effects/plugins/EqEffect.cpp
    streaming/effects/plugins/BlurEffect.cpp
    streaming/effects/plugins/GrayscaleEffect.cpp
    streaming/effects/plugins/FlipEffect.cpp
    streaming/effects/plugins/SharpenEffect.cpp
)

set(STREAMING_RENDER_SOURCES
    streaming/render/FfmpegVideoRendererBase.cpp
    streaming/render/LocalVideoRenderer.cpp
    streaming/render/RemoteVideoRenderer.cpp
)

set(STREAMING_PIPELINE_SOURCES
    streaming/pipeline/PublishPipeline.cpp
    streaming/pipeline/PlayPipeline.cpp
)

set(STREAMING_CALL_SOURCES
    streaming/call/CallSessionController.cpp
)

qt_add_executable(appccvWriter
    main.cpp
    ${STREAMING_CAPTURE_SOURCES}
    ${STREAMING_EFFECTS_SOURCES}
    ${STREAMING_RENDER_SOURCES}
    ${STREAMING_PIPELINE_SOURCES}
    ${STREAMING_CALL_SOURCES}
)

qt_add_qml_module(appccvWriter
    URI ccvWriter
    VERSION 1.0
    QML_FILES
        Main.qml
        qml/VideoPanel.qml
    SOURCES
        media/muxerh264toMp4/ccvideowriter.h
        media/muxerh264toMp4/ccvideowriter.cpp
        streaming/capture/FfmpegPlatformCapture.h
        streaming/capture/FfmpegDeviceEnumerator.h
        streaming/capture/FfmpegCaptureBase.h
        streaming/capture/FfmpegVideoCapture.h
        streaming/capture/FfmpegAudioCapture.h
        streaming/core/AvFramePtr.h
        streaming/core/FramePool.h
        streaming/pipeline/PublishPipeline.h
        streaming/pipeline/PlayPipeline.h
        streaming/effects/IVideoEffect.h
        streaming/effects/EffectContext.h
        streaming/effects/EffectChain.h
        streaming/effects/EffectChainFactory.h
        streaming/effects/FfmpegFilterGraph.h
        streaming/effects/EffectController.h
        streaming/effects/plugins/EqEffect.h
        streaming/effects/plugins/BlurEffect.h
        streaming/effects/plugins/GrayscaleEffect.h
        streaming/effects/plugins/FlipEffect.h
        streaming/effects/plugins/SharpenEffect.h
)

if(APPLE)
    set_target_properties(appccvWriter PROPERTIES
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
        MACOSX_BUNDLE TRUE
    )
elseif(WIN32)
    set_target_properties(appccvWriter PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

target_include_directories(appccvWriter PRIVATE "${CMAKE_SOURCE_DIR}")

set(FFMPEG_LIB_DIR "${CMAKE_SOURCE_DIR}/lib")
set(FFMPEG_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")

if(WIN32 OR MINGW)
    target_include_directories(appccvWriter PRIVATE "${FFMPEG_INCLUDE_DIR}")
    set(FFMPEG_LIBS
        "${FFMPEG_LIB_DIR}/libavdevice.dll.a"
        "${FFMPEG_LIB_DIR}/libavfilter.dll.a"
        "${FFMPEG_LIB_DIR}/libavformat.dll.a"
        "${FFMPEG_LIB_DIR}/libavcodec.dll.a"
        "${FFMPEG_LIB_DIR}/libavutil.dll.a"
        "${FFMPEG_LIB_DIR}/libswscale.dll.a"
        "${FFMPEG_LIB_DIR}/libswresample.dll.a"
    )
elseif(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
        libavdevice
        libavfilter
        libavformat
        libavcodec
        libavutil
        libswscale
        libswresample
    )
    target_include_directories(appccvWriter PRIVATE ${FFMPEG_INCLUDE_DIRS})
    set(FFMPEG_LIBS PkgConfig::FFMPEG)
else()
    target_include_directories(appccvWriter PRIVATE "${FFMPEG_INCLUDE_DIR}")
    find_library(AVDEVICE_LIB avdevice PATHS "${FFMPEG_LIB_DIR}" NO_DEFAULT_PATH REQUIRED)
    find_library(AVFILTER_LIB avfilter  PATHS "${FFMPEG_LIB_DIR}" NO_DEFAULT_PATH REQUIRED)
    find_library(AVFORMAT_LIB avformat PATHS "${FFMPEG_LIB_DIR}" NO_DEFAULT_PATH REQUIRED)
    find_library(AVCODEC_LIB  avcodec  PATHS "${FFMPEG_LIB_DIR}" NO_DEFAULT_PATH REQUIRED)
    find_library(AVUTIL_LIB   avutil   PATHS "${FFMPEG_LIB_DIR}" NO_DEFAULT_PATH REQUIRED)
    find_library(SWSCALE_LIB  swscale  PATHS "${FFMPEG_LIB_DIR}" NO_DEFAULT_PATH REQUIRED)
    find_library(SWRESAMPLE_LIB swresample PATHS "${FFMPEG_LIB_DIR}" NO_DEFAULT_PATH REQUIRED)
    set(FFMPEG_LIBS
        ${AVDEVICE_LIB} ${AVFILTER_LIB} ${AVFORMAT_LIB}
        ${AVCODEC_LIB} ${AVUTIL_LIB} ${SWSCALE_LIB} ${SWRESAMPLE_LIB}
    )
endif()

target_link_libraries(appccvWriter
    PRIVATE
        Qt6::Quick
        Qt6::Core
        ${FFMPEG_LIBS}
)

if(WIN32)
    target_link_libraries(appccvWriter PRIVATE
        bcrypt secur32 ole32 strmiids uuid oleaut32
    )

    file(GLOB FFMPEG_RUNTIME_DLLS CONFIGURE_DEPENDS "${FFMPEG_LIB_DIR}/*-*.dll")
    add_custom_command(TARGET appccvWriter POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${FFMPEG_RUNTIME_DLLS}
            $<TARGET_FILE_DIR:appccvWriter>
        COMMENT "Copy FFmpeg DLLs next to appccvWriter.exe"
    )
endif()

include(GNUInstallDirs)
install(TARGETS appccvWriter
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

message(STATUS "ccvWriter Qt prefix: ${CMAKE_PREFIX_PATH}")
message(STATUS "ccvWriter C++ compiler: ${CMAKE_CXX_COMPILER}")
