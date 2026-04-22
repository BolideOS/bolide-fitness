# ── Source files ─────────────────────────────────────────────────────────────
set(CORE_SRC
    core/sensorservice.cpp
    core/datapipeline.cpp
    core/database.cpp
    core/metricsengine.cpp
    core/pluginregistry.cpp
    core/screenregistry.cpp
    core/glanceprovider.cpp
)

set(CORE_HEADERS
    core/ringbuffer.h
    core/sensorservice.h
    core/datapipeline.h
    core/database.h
    core/metricsengine.h
    core/pluginregistry.h
    core/screenregistry.h
    core/glanceprovider.h
)

set(METRICS_SRC
    metrics/hillscore.cpp
    metrics/hrvmetrics.cpp
    metrics/calorieestimate.cpp
    metrics/vo2maxestimate.cpp
)

set(METRICS_HEADERS
    metrics/metricplugin.h
    metrics/hillscore.h
    metrics/hrvmetrics.h
    metrics/calorieestimate.h
    metrics/vo2maxestimate.h
)

set(DOMAIN_SRC
    domain/workoutmanager.cpp
    domain/sleeptracker.cpp
    domain/readinessscore.cpp
    domain/bodymetrics.cpp
    domain/trendsmanager.cpp
    domain/sensorslabmanager.cpp
)

set(DOMAIN_HEADERS
    domain/workoutmanager.h
    domain/sleeptracker.h
    domain/readinessscore.h
    domain/bodymetrics.h
    domain/trendsmanager.h
    domain/sensorslabmanager.h
)

set(EXPORT_SRC
    export/fitexporter.cpp
    export/tcxexporter.cpp
    export/gpxexporter.cpp
    export/exportmanager.cpp
    export/bletransport.cpp
)

set(EXPORT_HEADERS
    export/exportplugin.h
    export/fitexporter.h
    export/tcxexporter.h
    export/gpxexporter.h
    export/exportmanager.h
    export/bletransport.h
)

set(ML_SRC
    ml/mlregistry.cpp
    ml/builtinmodels.cpp
)

set(ML_HEADERS
    ml/mlplugin.h
    ml/mlregistry.h
    ml/builtinmodels.h
)

set(COMPANION_SRC
    companion/companionservice.cpp
    companion/gadgetbridgesync.cpp
    companion/metricsbroadcaster.cpp
)

set(COMPANION_HEADERS
    companion/companionservice.h
    companion/gadgetbridgesync.h
    companion/metricsbroadcaster.h
)

set(SRC
    main.cpp
    ${CORE_SRC}
    ${METRICS_SRC}
    ${DOMAIN_SRC}
    ${EXPORT_SRC}
    ${ML_SRC}
    ${COMPANION_SRC}
)

set(HEADERS
    ${CORE_HEADERS}
    ${METRICS_HEADERS}
    ${DOMAIN_HEADERS}
    ${EXPORT_HEADERS}
    ${ML_HEADERS}
    ${COMPANION_HEADERS}
)

# ── Target ───────────────────────────────────────────────────────────────────
find_package(Qt5QuickCompiler)
if(Qt5QuickCompiler_FOUND)
    qtquick_compiler_add_resources(RESOURCES resources.qrc)
else()
    qt5_add_resources(RESOURCES resources.qrc)
endif()

add_library(bolide-fitness ${SRC} ${HEADERS} ${RESOURCES})
set_target_properties(bolide-fitness PROPERTIES PREFIX "")

find_package(PkgConfig REQUIRED)
pkg_check_modules(MLITE5 REQUIRED mlite5)
pkg_check_modules(SQLITE3 REQUIRED sqlite3)

target_include_directories(bolide-fitness PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${MLITE5_INCLUDE_DIRS}
    ${SQLITE3_INCLUDE_DIRS}
)

target_link_libraries(bolide-fitness PRIVATE
    Qt5::Qml
    Qt5::Quick
    Qt5::DBus
    Qt5::Xml
    AsteroidApp
    ${MLITE5_LIBRARIES}
    ${SQLITE3_LIBRARIES}
)

install(TARGETS bolide-fitness
    DESTINATION ${CMAKE_INSTALL_LIBDIR})

# ── BolideMetrics QML plugin (standalone D-Bus consumer) ─────────────────────
add_subdirectory(metrics-plugin)
