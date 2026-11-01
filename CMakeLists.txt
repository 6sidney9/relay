cmake_minimum_required(VERSION 3.20)

project(relay VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG asio-1-30-2
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(asio)

add_executable(relay
    src/main.cpp
    src/config/config.cpp
    src/http/parser.cpp
    src/http/mime.cpp
    src/net/server.cpp
    src/net/connection.cpp
    src/handlers/static_files.cpp
    src/handlers/proxy.cpp
    src/router/router.cpp
)

target_include_directories(relay PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${asio_SOURCE_DIR}/asio/include
)

target_compile_definitions(relay PRIVATE
    ASIO_STANDALONE
    ASIO_NO_DEPRECATED
)

if(WIN32)
    target_compile_definitions(relay PRIVATE _WIN32_WINNT=0x0A00)
    target_link_libraries(relay PRIVATE ws2_32 mswsock)
endif()

if(MSVC)
    target_compile_options(relay PRIVATE /W4 /permissive-)
else()
    target_compile_options(relay PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS relay RUNTIME DESTINATION bin)
install(FILES config/relay.example.toml DESTINATION share/relay)
