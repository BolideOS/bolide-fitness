set(SRC
    main.cpp
    workoutcontroller.cpp
)
set(HEADERS
    workoutcontroller.h
)

add_library(asteroid-workout ${SRC} ${HEADERS} resources.qrc)
set_target_properties(asteroid-workout PROPERTIES PREFIX "")

find_package(PkgConfig REQUIRED)
pkg_check_modules(MLITE5 REQUIRED mlite5)

target_include_directories(asteroid-workout PRIVATE ${MLITE5_INCLUDE_DIRS})

target_link_libraries(asteroid-workout PRIVATE
    Qt5::Qml
    Qt5::Quick
    Qt5::DBus
    AsteroidApp
    ${MLITE5_LIBRARIES})

install(TARGETS asteroid-workout
    DESTINATION ${CMAKE_INSTALL_LIBDIR})
