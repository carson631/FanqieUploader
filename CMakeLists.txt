cmake_minimum_required(VERSION 3.21)
project(FanqieUploader VERSION 1.0.2 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network WebSockets)

add_executable(FanqieUploader WIN32
    main.cpp
    mainwindow.cpp
    mainwindow.h
    cdpclient.cpp
    cdpclient.h
    browserlauncher.cpp
    browserlauncher.h
    uploadworker.cpp
    uploadworker.h
    app.qrc
    app.rc
)

target_link_libraries(FanqieUploader PRIVATE Qt6::Widgets Qt6::Network Qt6::WebSockets)
