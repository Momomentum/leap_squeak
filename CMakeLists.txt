cmake_minimum_required(VERSION 3.28)

project(leap_squeak VERSION "1.0.0" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED On)
set(CMAKE_CXX_EXTENSIONS Off)

add_executable(${CMAKE_PROJECT_NAME})

add_subdirectory(libremidi)

set(APP ${CMAKE_PROJECT_NAME})

find_package(LeapSDK 5 REQUIRED)

set(HDRS
    src/ExampleConnection.h)

target_sources(${APP}
    PUBLIC
    src/main.cpp
    src/ExampleConnection.cpp
    ${HDRS}
)

target_link_libraries(${APP}
    PRIVATE
    libremidi::libremidi
)

find_package(Threads REQUIRED)    

target_link_libraries(${APP}
    PRIVATE
    LeapSDK::LeapC)

