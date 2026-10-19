qt_add_library(dk2music_core STATIC
    clipcache.cpp
    clipcache.h
    engine.cpp
    engine.h
    gamedata.cpp
    gamedata.h
    layer.cpp
    layer.h
    mixer.cpp
    mixer.h
    musiclevel.h
    sequencer.cpp
    sequencer.h
    sdtarchive.cpp
    sdtarchive.h
    soundmaps.cpp
    soundmaps.h
)

target_include_directories(dk2music_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(dk2music_core
    PUBLIC
        Qt6::Core
        Qt6::Multimedia
)
