add_executable(test_codec test_codec.cpp)
target_link_libraries(test_codec PRIVATE akai2sfz_core)
add_test(NAME test_codec COMMAND test_codec)

# ferramenta de validacao manual (nao roda via ctest)
add_executable(roland_probe roland_probe.cpp)
target_link_libraries(roland_probe PRIVATE akai2sfz_core)

add_executable(emu_probe emu_probe.cpp)
target_link_libraries(emu_probe PRIVATE akai2sfz_core)
