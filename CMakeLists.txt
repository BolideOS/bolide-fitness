cmake_minimum_required(VERSION 3.6.0)

project(bolide-fitness
    VERSION 0.1.0
    DESCRIPTION "Workout tracking app for AsteroidOS")

find_package(ECM REQUIRED NO_MODULE)
find_package(AsteroidApp REQUIRED)

set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ASTEROID_MODULE_PATH})
set(CMAKE_AUTOMOC ON)

include(FeatureSummary)
include(GNUInstallDirs)
include(ECMFindQmlModule)
include(AsteroidCMakeSettings)
include(AsteroidTranslations)

find_package(Qt5 COMPONENTS Core Qml Quick DBus REQUIRED)

ecm_find_qmlmodule(Nemo.DBus 2.0)

add_subdirectory(src)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/bolide-fitness.in
    ${CMAKE_BINARY_DIR}/bolide-fitness @ONLY)
install(PROGRAMS ${CMAKE_BINARY_DIR}/bolide-fitness
    DESTINATION ${CMAKE_INSTALL_BINDIR})

build_translations(i18n)
generate_desktop(${CMAKE_SOURCE_DIR} bolide-fitness)
feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
