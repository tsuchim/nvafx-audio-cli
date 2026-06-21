#include "wav.hpp"

#include <cmath>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: wav-roundtrip <input.wav> <output.wav>\n";
        return 2;
    }

    try {
        const nvafx::AudioBuffer input = nvafx::read_wav_file(argv[1]);
        nvafx::write_float_wav_file(argv[2], input);
        const nvafx::AudioBuffer output = nvafx::read_wav_file(argv[2]);

        if (input.channels != output.channels || input.sample_rate != output.sample_rate ||
            input.samples.size() != output.samples.size()) {
            std::cerr << "Round-trip metadata mismatch\n";
            return 1;
        }

        for (std::size_t channel = 0; channel < input.samples.size(); ++channel) {
            if (input.samples[channel].size() != output.samples[channel].size()) {
                std::cerr << "Round-trip frame count mismatch\n";
                return 1;
            }

            for (std::size_t frame = 0; frame < input.samples[channel].size(); ++frame) {
                if (std::fabs(input.samples[channel][frame] - output.samples[channel][frame]) > 0.000001F) {
                    std::cerr << "Round-trip sample mismatch\n";
                    return 1;
                }
            }
        }
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << "\n";
        return 1;
    }

    return 0;
}
