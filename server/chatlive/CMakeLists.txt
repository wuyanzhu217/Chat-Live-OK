cmake_minimum_required(VERSION 3.20)
project(chatlive-server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Drogon REQUIRED)
find_package(CURL REQUIRED)

include(FetchContent)

FetchContent_Declare(
    jwt-cpp
    GIT_REPOSITORY https://github.com/Thalhammer/jwt-cpp.git
    GIT_TAG v0.7.0
)

set(JWT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JWT_BUILD_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(jwt-cpp)

add_executable(chatlive-server
    main.cpp
    modules/OidcTokenValidator.cpp
    filters/JwtAuthFilter.cpp
    ws/WsController.cpp
    ws/WsEvent.cpp
)

target_link_libraries(chatlive-server
    PRIVATE
        Drogon::Drogon
        jwt-cpp::jwt-cpp
        CURL::libcurl
)

# Enable PostgreSQL support in Drogon
target_compile_definitions(chatlive-server PRIVATE DROGON_USE_POSTGRESQL)

target_include_directories(chatlive-server
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

install(TARGETS chatlive-server DESTINATION bin)