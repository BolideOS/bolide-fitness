set(SRC
    main.cpp
    workoutcontroller.cpp
)
set(HEADERS
    workoutcontroller.h
)

add_library(bolide-fitness ${SRC} ${HEADERS} resources.qrc)
set_target_properties(bolide-fitness PROPERTIES PREFIX "")

find_package(PkgConfig REQUIRED)
pkg_check_modules(MLITE5 REQUIRED mlite5)

target_include_directories(bolide-fitness PRIVATE ${MLITE5_INCLUDE_DIRS})

target_link_libraries(bolide-fitness PRIVATE
    Qt5::Qml
    Qt5::Quick
    Qt5::DBus
    AsteroidApp
    ${MLITE5_LIBRARIES})

install(TARGETS bolide-fitness
    DESTINATION ${CMAKE_INSTALL_LIBDIR})
