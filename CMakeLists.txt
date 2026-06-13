cmake_minimum_required(VERSION 3.10)

project(LAN-chat)

set(CMAKE_CXX_STANDARD 17)

file(GLOB SRC_FILES "src/*.cpp")

add_executable(${PROJECT_NAME} ${SRC_FILES})

target_include_directories(${PROJECT_NAME}
    PRIVATE
    lib
)

find_package(Curses REQUIRED)

target_include_directories(${PROJECT_NAME}
    PRIVATE
    ${CURSES_INCLUDE_DIR}
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    ${CURSES_LIBRARIES}
)
