find_package(Qt5 COMPONENTS Test REQUIRED)

# ── RingBuffer tests ─────────────────────────────────────────────────────────
add_executable(test_ringbuffer test_ringbuffer.cpp)
target_include_directories(test_ringbuffer PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_ringbuffer Qt5::Test)
add_test(NAME test_ringbuffer COMMAND test_ringbuffer)

# ── HillScore tests ──────────────────────────────────────────────────────────
add_executable(test_hillscore test_hillscore.cpp
    ${CMAKE_SOURCE_DIR}/src/metrics/hillscore.cpp)
target_include_directories(test_hillscore PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_hillscore Qt5::Core Qt5::Test)
add_test(NAME test_hillscore COMMAND test_hillscore)

# ── DataPipeline tests ───────────────────────────────────────────────────────
add_executable(test_datapipeline test_datapipeline.cpp
    ${CMAKE_SOURCE_DIR}/src/core/datapipeline.cpp
    ${CMAKE_SOURCE_DIR}/src/core/sensorservice.cpp)
target_include_directories(test_datapipeline PRIVATE ${CMAKE_SOURCE_DIR}/src)
pkg_check_modules(SQLITE3 REQUIRED sqlite3)
target_link_libraries(test_datapipeline
    Qt5::Core Qt5::DBus Qt5::Test ${SQLITE3_LIBRARIES})
add_test(NAME test_datapipeline COMMAND test_datapipeline)

# ── SleepTracker tests ───────────────────────────────────────────────────────
add_executable(test_sleeptracker test_sleeptracker.cpp
    ${CMAKE_SOURCE_DIR}/src/domain/sleeptracker.cpp)
target_include_directories(test_sleeptracker PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_sleeptracker Qt5::Core Qt5::Test)
add_test(NAME test_sleeptracker COMMAND test_sleeptracker)

# ── ReadinessScore tests ─────────────────────────────────────────────────────
add_executable(test_readiness test_readiness.cpp
    ${CMAKE_SOURCE_DIR}/src/domain/readinessscore.cpp)
target_include_directories(test_readiness PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_readiness Qt5::Core Qt5::Test)
add_test(NAME test_readiness COMMAND test_readiness)

# ── BodyMetrics tests ────────────────────────────────────────────────────────
add_executable(test_bodymetrics test_bodymetrics.cpp
    ${CMAKE_SOURCE_DIR}/src/domain/bodymetrics.cpp)
target_include_directories(test_bodymetrics PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_bodymetrics Qt5::Core Qt5::Test)
add_test(NAME test_bodymetrics COMMAND test_bodymetrics)

# ── TrendsManager tests ─────────────────────────────────────────────────────
add_executable(test_trendsmanager test_trendsmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/domain/trendsmanager.cpp)
target_include_directories(test_trendsmanager PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_trendsmanager Qt5::Core Qt5::Test)
add_test(NAME test_trendsmanager COMMAND test_trendsmanager)

# ── Export format tests ──────────────────────────────────────────────────────
add_executable(test_export test_export.cpp
    ${CMAKE_SOURCE_DIR}/src/export/fitexporter.cpp
    ${CMAKE_SOURCE_DIR}/src/export/tcxexporter.cpp
    ${CMAKE_SOURCE_DIR}/src/export/gpxexporter.cpp
    ${CMAKE_SOURCE_DIR}/src/export/exportmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/export/bletransport.cpp)
target_include_directories(test_export PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_export Qt5::Core Qt5::DBus Qt5::Xml Qt5::Test)
add_test(NAME test_export COMMAND test_export)

# ── ML Registry tests ───────────────────────────────────────────────────────
add_executable(test_mlregistry test_mlregistry.cpp
    ${CMAKE_SOURCE_DIR}/src/ml/mlregistry.cpp
    ${CMAKE_SOURCE_DIR}/src/ml/builtinmodels.cpp)
target_include_directories(test_mlregistry PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_mlregistry Qt5::Core Qt5::Test)
add_test(NAME test_mlregistry COMMAND test_mlregistry)

# ── SensorsLab tests ────────────────────────────────────────────────────────
add_executable(test_sensorslab test_sensorslab.cpp
    ${CMAKE_SOURCE_DIR}/src/domain/sensorslabmanager.cpp)
target_include_directories(test_sensorslab PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_sensorslab Qt5::Core Qt5::Test)
add_test(NAME test_sensorslab COMMAND test_sensorslab)

# ── CompanionService tests ──────────────────────────────────────────────────
add_executable(test_companion test_companion.cpp
    ${CMAKE_SOURCE_DIR}/src/companion/companionservice.cpp)
target_include_directories(test_companion PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_companion Qt5::Core Qt5::DBus Qt5::Test)
add_test(NAME test_companion COMMAND test_companion)
