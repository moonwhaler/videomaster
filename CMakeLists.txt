cmake_minimum_required(VERSION 3.16)
project(VideoMaster VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Multimedia MultimediaWidgets)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
    libavcodec
    libavformat
    libavutil
    libswscale
    libswresample
)

# Enable Qt's automatic MOC, UIC, and RCC processing
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/videowidget.cpp
    src/videocomparator.cpp
    src/ffmpeghandler.cpp
    src/batchprocessor.cpp
)

set(HEADERS
    src/mainwindow.h
    src/videowidget.h
    src/videocomparator.h
    src/ffmpeghandler.h
    src/batchprocessor.h
)

add_executable(VideoMaster ${SOURCES} ${HEADERS})

target_link_libraries(VideoMaster 
    Qt6::Core 
    Qt6::Widgets 
    Qt6::Multimedia 
    Qt6::MultimediaWidgets
    PkgConfig::FFMPEG
)