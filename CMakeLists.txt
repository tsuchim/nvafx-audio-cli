cmake_minimum_required(VERSION 3.20)

project(nvafx_audio_cli VERSION 0.1.0 LANGUAGES CXX)

add_executable(nvafx-audio-cli
    src/main.cpp
)

target_compile_features(nvafx-audio-cli PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(nvafx-audio-cli PRIVATE /W4 /permissive-)
else()
    target_compile_options(nvafx-audio-cli PRIVATE -Wall -Wextra -pedantic)
endif()
