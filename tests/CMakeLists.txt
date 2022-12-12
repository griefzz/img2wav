include(CTest)

add_executable(wav_test wav_test.c)

enable_testing()
add_test(NAME wav_test COMMAND wav_test)