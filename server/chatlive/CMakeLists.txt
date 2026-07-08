cmake_minimum_required(VERSION 3.20)
project(chatlive-server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Drogon：优先 /usr/local（sudo make install），否则用 third_party/local
if(EXISTS "/usr/local/lib/cmake/Drogon/DrogonConfig.cmake")
    list(PREPEND CMAKE_PREFIX_PATH "/usr/local")
    message(STATUS "Using Drogon from /usr/local")
else()
    set(CHATLIVE_LOCAL_DEPS "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/local")
    if(EXISTS "${CHATLIVE_LOCAL_DEPS}/lib/cmake/Drogon/DrogonConfig.cmake")
        list(PREPEND CMAKE_PREFIX_PATH "${CHATLIVE_LOCAL_DEPS}" "${CHATLIVE_LOCAL_DEPS}/usr")
        message(STATUS "Using local deps: ${CHATLIVE_LOCAL_DEPS}")
    endif()
endif()

find_package(Drogon REQUIRED)
find_package(CURL REQUIRED)

# jwt-cpp：优先 third_party/jwt-cpp（离线），否则 FetchContent
set(JWT_CPP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/jwt-cpp")
if(EXISTS "${JWT_CPP_DIR}/CMakeLists.txt")
    set(JWT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(JWT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    add_subdirectory("${JWT_CPP_DIR}" "${CMAKE_BINARY_DIR}/jwt-cpp" EXCLUDE_FROM_ALL)
    message(STATUS "Using vendored jwt-cpp: ${JWT_CPP_DIR}")
else()
    include(FetchContent)
    FetchContent_Declare(
        jwt-cpp
        GIT_REPOSITORY https://github.com/Thalhammer/jwt-cpp.git
        GIT_TAG v0.7.0
    )
    set(JWT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(JWT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(jwt-cpp)
endif()
add_executable(chatlive-server
    main.cpp
    modules/OidcTokenValidator.cpp
    filters/JwtAuthFilter.cpp
    ws/WsController.cpp
    ws/WsEvent.cpp
    ws/WsEnvelope.cpp
    ws/WsHub.cpp
    services/UserService.cpp
    services/PresenceService.cpp
    services/ConversationService.cpp
    services/MessageService.cpp
    services/MinioService.cpp
    services/CallService.cpp
    services/TurnCredentialService.cpp
    utils/ApiResponse.cpp
    controllers/FriendController.cpp
    controllers/ConversationController.cpp
    controllers/MessageController.cpp
    controllers/CallController.cpp
    controllers/UploadController.cpp
)

target_link_libraries(chatlive-server
    PRIVATE
        Drogon::Drogon
        jwt-cpp::jwt-cpp
        CURL::libcurl
        OpenSSL::Crypto
        zstd
)

# Enable PostgreSQL support in Drogon
target_compile_definitions(chatlive-server PRIVATE DROGON_USE_POSTGRESQL)

target_include_directories(chatlive-server
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

install(TARGETS chatlive-server DESTINATION bin)
