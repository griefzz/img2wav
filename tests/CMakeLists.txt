include(CTest)

add_executable(wav_test wav_test.c)

if(NOT WIN32)
    target_link_libraries(wav_test PRIVATE m)
endif()

enable_testing()
add_test(NAME wav_test COMMAND wav_test)