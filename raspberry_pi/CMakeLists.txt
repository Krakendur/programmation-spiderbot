cmake_minimum_required(VERSION 3.16)
project(spider_bot_rpi LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
find_package(ALSA REQUIRED)

# libcamera (Camera Module 3 Wide)
pkg_check_modules(LIBCAMERA REQUIRED libcamera)

add_executable(spider_bot_rpi
    main.cpp
    audio_manager.cpp
    camera_manager.cpp
    av_fsm.cpp
)

target_include_directories(spider_bot_rpi PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${LIBCAMERA_INCLUDE_DIRS}
)

target_link_libraries(spider_bot_rpi PRIVATE
    ALSA::ALSA
    ${LIBCAMERA_LIBRARIES}
    pthread
)

target_compile_options(spider_bot_rpi PRIVATE
    -Wall -Wextra -O2
    ${LIBCAMERA_CFLAGS_OTHER}
)
