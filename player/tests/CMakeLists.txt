qt_add_executable(tst_dk2music
    tst_dk2music.cpp
)

target_link_libraries(tst_dk2music
    PRIVATE
        dk2music_core
        Qt6::Test
)

# Tests that read the game data are skipped unless DK2_GAME_DIR points at a DK II install
add_test(NAME tst_dk2music COMMAND tst_dk2music)
