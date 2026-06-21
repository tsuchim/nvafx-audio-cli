#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

namespace nvafx {

struct AudioBuffer {
    std::uint16_t channels = 0;
    std::uint32_t sample_rate = 0;
    std::vector<std::vector<float>> samples;
};

bool is_supported_wav_sample_rate(std::uint32_t sample_rate);
bool is_supported_wav_channel_count(std::uint16_t channels);
AudioBuffer read_wav_file(const std::string& path);
AudioBuffer read_wav_stream(std::istream& input, const std::string& context);
void write_float_wav_file(const std::string& path, const AudioBuffer& audio);
void write_float_wav_stream(std::ostream& output, const AudioBuffer& audio, const std::string& context);

}  // namespace nvafx
