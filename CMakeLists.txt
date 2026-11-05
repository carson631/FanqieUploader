cmake_minimum_required(VERSION 3.21)
project(FanqieUploader VERSION 1.0.9 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# 单配置生成器（Ninja / MinGW Makefiles）不指定类型时默认 Release：
# 无优化的 Debug 版启动要多花一倍时间，界面也更迟钝
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    message(STATUS "CMAKE_BUILD_TYPE 未指定 → 默认 Release")
endif()

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

# 开发时也让 build 目录有 js/site.js：程序优先读 exe 旁边的脚本，
# 否则会退回资源里的旧副本，改了 js 却像没生效。
add_custom_command(TARGET FanqieUploader POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:FanqieUploader>/js"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/js/site.js"
            "$<TARGET_FILE_DIR:FanqieUploader>/js/site.js"
)
