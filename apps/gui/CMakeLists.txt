set(CMAKE_AUTOMOC ON)

add_executable(akai2sfz_gui
    main.cpp
    MainWindow.cpp
    MainWindow.h
)
target_link_libraries(akai2sfz_gui PRIVATE akai2sfz_core Qt6::Widgets)
