#include "wav.hpp"

#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    if (argc != 5) {
        std::cerr << "Usage: wav-inspect <path> <sample-rate> <channels> <frames>\n";
        return 2;
    }

    try {
        const nvafx::AudioBuffer audio = nvafx::read_wav_file(argv[1]);
        const auto expected_rate = static_cast<std::uint32_t>(std::stoul(argv[2]));
        const auto expected_channels = static_cast<std::uint16_t>(std::stoul(argv[3]));
        const auto expected_frames = static_cast<std::size_t>(std::stoull(argv[4]));
        const std::size_t frames = audio.samples.empty() ? 0U : audio.samples.front().size();

        if (audio.sample_rate != expected_rate) {
            std::cerr << "sample-rate mismatch: " << audio.sample_rate << "\n";
            return 1;
        }
        if (audio.channels != expected_channels) {
            std::cerr << "channel-count mismatch: " << audio.channels << "\n";
            return 1;
        }
        if (frames != expected_frames) {
            std::cerr << "frame-count mismatch: " << frames << "\n";
            return 1;
        }
        if (frames == 0) {
            std::cerr << "output is empty\n";
            return 1;
        }

        std::cout << "WAV metadata: " << audio.sample_rate << " Hz, " << audio.channels
                  << " channel(s), " << frames << " frame(s)\n";
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << "\n";
        return 1;
    }

    return 0;
}
