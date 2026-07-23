add_executable(test_codec test_codec.cpp)
target_link_libraries(test_codec PRIVATE akai2sfz_core)
add_test(NAME test_codec COMMAND test_codec)
