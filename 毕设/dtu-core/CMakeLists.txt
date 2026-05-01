cmake_minimum_required(VERSION 3.16)
project(dtu_core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(include)

add_executable(dtu_core
    src/main.cpp
    src/dlt645_parser.cpp
    src/serial_reader.cpp
    src/db_handler.cpp
    src/mqtt_client.cpp
)

# SQLite - 手动指定
target_link_libraries(dtu_core pthread sqlite3)
