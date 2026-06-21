#include "stdio_binary.hpp"
#include "wav.hpp"

#include <exception>
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: wav-stdout <input.wav>\n";
        return 2;
    }

    try {
        const nvafx::AudioBuffer audio = nvafx::read_wav_file(argv[1]);
        nvafx::set_stdout_binary_mode();
        nvafx::write_float_wav_stream(std::cout, audio, "<stdout>");
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << "\n";
        return 1;
    }

    return 0;
}
