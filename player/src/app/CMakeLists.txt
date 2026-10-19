qt_add_executable(dk2music
    main.cpp
    mainwindow.cpp
    mainwindow.h
)

target_link_libraries(dk2music
    PRIVATE
        dk2music_core
        Qt6::Widgets
)
