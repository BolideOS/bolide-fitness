# BolideMetrics QML Plugin
#
# Standalone D-Bus client plugin that provides MetricsService {} to any QML app.
# Does NOT link to bolide-fitness — purely a D-Bus consumer.

cmake_minimum_required(VERSION 3.14)
project(bolidemetricsplugin LANGUAGES CXX)

find_package(Qt5 REQUIRED COMPONENTS Core Qml DBus)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)

add_library(bolidemetricsplugin SHARED
    plugin.cpp
    metricsclient.cpp
    metricsclient.h
)

target_link_libraries(bolidemetricsplugin PRIVATE
    Qt5::Core
    Qt5::Qml
    Qt5::DBus
)

# Install to the QML import path so any app can: import BolideMetrics 1.0
set(QML_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/lib/qt5/qml/BolideMetrics"
    CACHE STRING "QML plugin install directory")

install(TARGETS bolidemetricsplugin DESTINATION ${QML_INSTALL_DIR})
install(FILES qmldir DESTINATION ${QML_INSTALL_DIR})
