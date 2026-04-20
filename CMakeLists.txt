cmake_minimum_required(VERSION 3.6.0)

project(bolide-fitness
    VERSION 1.0.0
    DESCRIPTION "Modular fitness & health application for BolideOS / AsteroidOS")

find_package(ECM REQUIRED NO_MODULE)
find_package(AsteroidApp REQUIRED)

set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ASTEROID_MODULE_PATH})
set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FeatureSummary)
include(GNUInstallDirs)
include(ECMFindQmlModule)
include(AsteroidCMakeSettings)
include(AsteroidTranslations)

find_package(Qt5 COMPONENTS Core Qml Quick DBus Sql Xml REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SQLITE3 REQUIRED sqlite3)

ecm_find_qmlmodule(Nemo.DBus 2.0)

add_subdirectory(src)

# ── Install JSON config files ───────────────────────────────────────────────
set(BOLIDE_FITNESS_CONFIG_DIR "${CMAKE_INSTALL_DATAROOTDIR}/bolide-fitness/configs")

install(DIRECTORY configs/workouts   DESTINATION ${BOLIDE_FITNESS_CONFIG_DIR})
install(DIRECTORY configs/screens    DESTINATION ${BOLIDE_FITNESS_CONFIG_DIR})
install(DIRECTORY configs/navigation DESTINATION ${BOLIDE_FITNESS_CONFIG_DIR})

# ── Launcher script ─────────────────────────────────────────────────────────
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/bolide-fitness.in
    ${CMAKE_BINARY_DIR}/bolide-fitness @ONLY)
install(PROGRAMS ${CMAKE_BINARY_DIR}/bolide-fitness
    DESTINATION ${CMAKE_INSTALL_BINDIR})

# ── Translations & desktop ──────────────────────────────────────────────────
build_translations(i18n)
generate_desktop(${CMAKE_SOURCE_DIR} bolide-fitness)

# ── Tests (optional) ────────────────────────────────────────────────────────
option(BUILD_TESTS "Build unit tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
