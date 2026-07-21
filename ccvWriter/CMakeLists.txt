cmake_minimum_required(VERSION 3.20)
project(chatlive-desktop VERSION 0.2.0 LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Chat-Live-OK Qt Desktop (ccvWriter)
# Phase 0: OIDC + REST + WS + IM UI
# Phase 2: libdatachannel audio calls (Opus)
# ---------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(CCV_USE_SYSTEM_DATACHANNEL "Use system-installed libdatachannel" OFF)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network WebSockets Multimedia MultimediaWidgets)
find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(OPUS REQUIRED opus)
pkg_check_modules(LIBAV REQUIRED libavcodec libavutil libswscale)

# --- libdatachannel (vendored under third_party/) ---
if(CCV_USE_SYSTEM_DATACHANNEL)
    find_package(LibDataChannel REQUIRED)
    set(CCV_DATACHANNEL_TARGET LibDataChannel::LibDataChannel)
else()
    set(NO_EXAMPLES ON CACHE BOOL "" FORCE)
    set(NO_TESTS ON CACHE BOOL "" FORCE)
    set(NO_WEBSOCKET ON CACHE BOOL "" FORCE) # we use Qt WebSockets for app signalling
    set(NO_MEDIA OFF CACHE BOOL "" FORCE)
    set(USE_GNUTLS OFF CACHE BOOL "" FORCE)
    set(USE_NICE OFF CACHE BOOL "" FORCE)
    add_subdirectory(third_party/libdatachannel EXCLUDE_FROM_ALL)
    set(CCV_DATACHANNEL_TARGET datachannel)
endif()

set(CCV_SOURCES
    src/main.cpp
    src/app/AppConfig.cpp
    src/app/Application.cpp
    src/auth/TokenStore.cpp
    src/auth/OidcClient.cpp
    src/api/ApiClient.cpp
    src/api/UsersApi.cpp
    src/api/FriendsApi.cpp
    src/api/ConversationsApi.cpp
    src/api/MessagesApi.cpp
    src/api/CallsApi.cpp
    src/api/LiveApi.cpp
    src/api/UploadsApi.cpp
    src/ws/RealtimeClient.cpp
    src/domain/AuthStore.cpp
    src/domain/FriendsStore.cpp
    src/domain/ConversationsStore.cpp
    src/domain/MessagesStore.cpp
    src/domain/CallsStore.cpp
    src/domain/LiveStore.cpp
    src/util/MediaUrl.cpp
    src/media/OpusCodec.cpp
    src/media/AudioCapture.cpp
    src/media/AudioPlayer.cpp
    src/media/H264Encoder.cpp
    src/media/H264Decoder.cpp
    src/media/VideoCapture.cpp
    src/media/RtmpPublisher.cpp
    src/call/CallPeer.cpp
    src/ui/LoginWidget.cpp
    src/ui/MainWindow.cpp
    src/ui/ConversationsPage.cpp
    src/ui/ChatPage.cpp
    src/ui/FriendsPage.cpp
    src/ui/CallOverlayWidget.cpp
    src/ui/VideoRenderer.cpp
    src/ui/ProfilePage.cpp
    src/ui/live/LivePage.cpp
    src/ui/live/LiveSquareView.cpp
    src/ui/live/LiveStudioView.cpp
    src/ui/live/LiveWatchView.cpp
)

add_executable(chatlive-desktop
    ${CCV_SOURCES}
    resources/resources.qrc
)

target_include_directories(chatlive-desktop PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${OPUS_INCLUDE_DIRS}
    ${LIBAV_INCLUDE_DIRS}
)

target_link_libraries(chatlive-desktop PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::WebSockets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    OpenSSL::SSL
    OpenSSL::Crypto
    ${CCV_DATACHANNEL_TARGET}
    ${OPUS_LIBRARIES}
    ${LIBAV_LIBRARIES}
)

target_compile_definitions(chatlive-desktop PRIVATE CCV_HAS_OPENSSL=1 RTC_ENABLE_MEDIA=1)

add_custom_command(TARGET chatlive-desktop POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:chatlive-desktop>/config
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/config/app.example.json
        $<TARGET_FILE_DIR:chatlive-desktop>/config/app.json
    COMMENT "Copy config/app.json beside chatlive-desktop"
)

install(TARGETS chatlive-desktop RUNTIME DESTINATION bin)
install(FILES config/app.example.json DESTINATION share/chatlive-desktop)
