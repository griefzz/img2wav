add_executable(img2wav img2wav.c)

if(NOT WIN32)
    target_link_libraries(img2wav m)
endif()