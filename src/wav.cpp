#include "wav.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace nvafx {
namespace {

constexpr std::uint16_t kFormatPcm = 1;
constexpr std::uint16_t kFormatIeeeFloat = 3;
constexpr std::uint16_t kFormatExtensible = 0xfffe;
constexpr std::uint16_t kSupportedPcmBits = 16;
constexpr std::uint16_t kSupportedFloatBits = 32;
constexpr std::uint32_t kWaveHeaderSize = 8;

constexpr std::array<unsigned char, 16> kPcmSubformat = {
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
    0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71,
};

constexpr std::array<unsigned char, 16> kFloatSubformat = {
    0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
    0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71,
};

struct FormatChunk {
    std::uint16_t audio_format = 0;
    std::uint16_t effective_format = 0;
    std::uint16_t channels = 0;
    std::uint32_t sample_rate = 0;
    std::uint16_t block_align = 0;
    std::uint16_t bits_per_sample = 0;
    std::uint16_t valid_bits_per_sample = 0;
};

std::runtime_error wav_error(const std::string& path, const std::string& message) {
    return std::runtime_error(path + ": " + message);
}

void read_exact(std::istream& input, char* data, std::size_t size, const std::string& path) {
    if (!input.read(data, static_cast<std::streamsize>(size))) {
        throw wav_error(path, "truncated file");
    }
}

std::array<char, 4> read_tag(std::istream& input, const std::string& path) {
    std::array<char, 4> tag{};
    read_exact(input, tag.data(), tag.size(), path);
    return tag;
}

bool tag_equals(const std::array<char, 4>& tag, const char* value) {
    return std::memcmp(tag.data(), value, tag.size()) == 0;
}

std::uint16_t read_u16(std::istream& input, const std::string& path) {
    std::array<unsigned char, 2> bytes{};
    read_exact(input, reinterpret_cast<char*>(bytes.data()), bytes.size(), path);
    return static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[0]) |
                                      (static_cast<std::uint16_t>(bytes[1]) << 8));
}

std::uint32_t read_u32(std::istream& input, const std::string& path) {
    std::array<unsigned char, 4> bytes{};
    read_exact(input, reinterpret_cast<char*>(bytes.data()), bytes.size(), path);
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8) |
           (static_cast<std::uint32_t>(bytes[2]) << 16) |
           (static_cast<std::uint32_t>(bytes[3]) << 24);
}

std::int16_t read_i16(std::istream& input, const std::string& path) {
    return static_cast<std::int16_t>(read_u16(input, path));
}

float read_f32(std::istream& input, const std::string& path) {
    const std::uint32_t bits = read_u32(input, path);
    float value = 0.0F;
    static_assert(sizeof(bits) == sizeof(value), "float must be 32-bit");
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

void write_u16(std::ostream& output, std::uint16_t value) {
    const std::array<char, 2> bytes = {
        static_cast<char>(value & 0xff),
        static_cast<char>((value >> 8) & 0xff),
    };
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

void write_u32(std::ostream& output, std::uint32_t value) {
    const std::array<char, 4> bytes = {
        static_cast<char>(value & 0xff),
        static_cast<char>((value >> 8) & 0xff),
        static_cast<char>((value >> 16) & 0xff),
        static_cast<char>((value >> 24) & 0xff),
    };
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

std::uintmax_t file_size(std::ifstream& input, const std::string& path) {
    const auto current = input.tellg();
    input.seekg(0, std::ios::end);
    if (!input) {
        throw wav_error(path, "failed to determine file size");
    }
    const auto end = input.tellg();
    input.seekg(current, std::ios::beg);
    if (!input || end < 0) {
        throw wav_error(path, "failed to determine file size");
    }
    return static_cast<std::uintmax_t>(end);
}

void write_f32(std::ostream& output, float value) {
    std::uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value), "float must be 32-bit");
    std::memcpy(&bits, &value, sizeof(bits));
    write_u32(output, bits);
}

void skip_chunk_data(std::istream& input, std::uint32_t size, const std::string& path) {
    input.seekg(size, std::ios::cur);
    if (!input) {
        throw wav_error(path, "malformed chunk");
    }

    if ((size % 2U) == 1U) {
        input.seekg(1, std::ios::cur);
        if (!input) {
            throw wav_error(path, "malformed chunk padding");
        }
    }
}

FormatChunk read_format_chunk(std::istream& input, std::uint32_t size, const std::string& path) {
    if (size < 16) {
        throw wav_error(path, "malformed fmt chunk");
    }

    FormatChunk format;
    format.audio_format = read_u16(input, path);
    format.effective_format = format.audio_format;
    format.channels = read_u16(input, path);
    format.sample_rate = read_u32(input, path);
    (void)read_u32(input, path);
    format.block_align = read_u16(input, path);
    format.bits_per_sample = read_u16(input, path);
    format.valid_bits_per_sample = format.bits_per_sample;

    if (format.audio_format == kFormatExtensible) {
        if (size < 40) {
            throw wav_error(path, "malformed WAVE_FORMAT_EXTENSIBLE fmt chunk");
        }

        const std::uint16_t extension_size = read_u16(input, path);
        if (extension_size < 22) {
            throw wav_error(path, "malformed WAVE_FORMAT_EXTENSIBLE extension");
        }

        format.valid_bits_per_sample = read_u16(input, path);
        (void)read_u32(input, path);

        std::array<unsigned char, 16> subformat{};
        read_exact(input, reinterpret_cast<char*>(subformat.data()), subformat.size(), path);

        if (subformat == kPcmSubformat) {
            format.effective_format = kFormatPcm;
        } else if (subformat == kFloatSubformat) {
            format.effective_format = kFormatIeeeFloat;
        } else {
            throw wav_error(path, "unsupported WAVE_FORMAT_EXTENSIBLE subformat");
        }

        const std::uint32_t consumed = 40;
        const std::uint32_t remaining = size - consumed;
        if (remaining > 0) {
            skip_chunk_data(input, remaining, path);
        }
        return format;
    }

    const std::uint32_t remaining = size - 16;
    if (remaining > 0) {
        skip_chunk_data(input, remaining, path);
    } else if ((size % 2U) == 1U) {
        skip_chunk_data(input, 0, path);
    }

    return format;
}

void validate_format(const FormatChunk& format, const std::string& path) {
    if (!is_supported_wav_channel_count(format.channels)) {
        throw wav_error(path, "unsupported channel count; expected mono or stereo");
    }

    if (!is_supported_wav_sample_rate(format.sample_rate)) {
        throw wav_error(path, "unsupported sample rate; expected 16000 or 48000 Hz");
    }

    const bool pcm16 = format.effective_format == kFormatPcm && format.bits_per_sample == kSupportedPcmBits &&
                       format.valid_bits_per_sample == kSupportedPcmBits;
    const bool float32 =
        format.effective_format == kFormatIeeeFloat && format.bits_per_sample == kSupportedFloatBits &&
        format.valid_bits_per_sample == kSupportedFloatBits;

    if (!pcm16 && !float32) {
        throw wav_error(path, "unsupported format; expected PCM signed 16-bit or IEEE float 32-bit");
    }

    const std::uint16_t expected_align =
        static_cast<std::uint16_t>(format.channels * (format.bits_per_sample / 8));
    if (format.block_align != expected_align) {
        throw wav_error(path, "malformed fmt chunk block alignment");
    }
}

AudioBuffer read_data_chunk(std::istream& input, std::uint32_t size, const FormatChunk& format,
                            const std::string& path) {
    validate_format(format, path);

    if (format.block_align == 0 || (size % format.block_align) != 0) {
        throw wav_error(path, "malformed data chunk size");
    }

    const std::uint32_t frame_count = size / format.block_align;
    AudioBuffer audio;
    audio.channels = format.channels;
    audio.sample_rate = format.sample_rate;
    audio.samples.assign(audio.channels, std::vector<float>(frame_count, 0.0F));

    for (std::uint32_t frame = 0; frame < frame_count; ++frame) {
        for (std::uint16_t channel = 0; channel < audio.channels; ++channel) {
            if (format.effective_format == kFormatPcm) {
                audio.samples[channel][frame] =
                    static_cast<float>(read_i16(input, path)) / 32768.0F;
            } else {
                audio.samples[channel][frame] = read_f32(input, path);
            }
        }
    }

    if ((size % 2U) == 1U) {
        skip_chunk_data(input, 0, path);
    }

    return audio;
}

void validate_audio_for_write(const AudioBuffer& audio, const std::string& path) {
    if (!is_supported_wav_channel_count(audio.channels)) {
        throw wav_error(path, "unsupported channel count for write; expected mono or stereo");
    }

    if (!is_supported_wav_sample_rate(audio.sample_rate)) {
        throw wav_error(path, "unsupported sample rate for write; expected 16000 or 48000 Hz");
    }

    if (audio.samples.size() != audio.channels) {
        throw wav_error(path, "malformed planar audio buffer");
    }

    if (audio.samples.empty()) {
        return;
    }

    const std::size_t frame_count = audio.samples.front().size();
    for (const auto& channel : audio.samples) {
        if (channel.size() != frame_count) {
            throw wav_error(path, "malformed planar audio buffer");
        }
    }
}

}  // namespace

bool is_supported_wav_sample_rate(std::uint32_t sample_rate) {
    return sample_rate == 16000 || sample_rate == 48000;
}

bool is_supported_wav_channel_count(std::uint16_t channels) {
    return channels == 1 || channels == 2;
}

AudioBuffer read_wav_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw wav_error(path, "failed to open input WAV file");
    }

    const std::uintmax_t total_size = file_size(input, path);

    if (total_size < 12) {
        throw wav_error(path, "truncated file");
    }

    if (!tag_equals(read_tag(input, path), "RIFF")) {
        throw wav_error(path, "unsupported container; expected RIFF");
    }

    const std::uint32_t riff_size = read_u32(input, path);
    if (riff_size < 4) {
        throw wav_error(path, "malformed RIFF size");
    }
    if (static_cast<std::uintmax_t>(riff_size) > total_size - kWaveHeaderSize) {
        throw wav_error(path, "truncated file");
    }
    const std::uintmax_t riff_end = static_cast<std::uintmax_t>(riff_size) + kWaveHeaderSize;

    if (!tag_equals(read_tag(input, path), "WAVE")) {
        throw wav_error(path, "unsupported file type; expected WAVE");
    }

    bool have_format = false;
    bool have_data = false;
    FormatChunk format;
    AudioBuffer audio;

    while (static_cast<std::uintmax_t>(input.tellg()) < riff_end) {
        const auto chunk_id = read_tag(input, path);
        const std::uint32_t chunk_size = read_u32(input, path);
        const auto chunk_data_start = input.tellg();
        const std::uintmax_t padded_chunk_size = chunk_size + (chunk_size % 2U);
        if (chunk_data_start < 0 ||
            static_cast<std::uintmax_t>(chunk_data_start) + padded_chunk_size > riff_end) {
            throw wav_error(path, "truncated file");
        }

        if (tag_equals(chunk_id, "fmt ")) {
            format = read_format_chunk(input, chunk_size, path);
            have_format = true;
        } else if (tag_equals(chunk_id, "data")) {
            if (!have_format) {
                throw wav_error(path, "data chunk appeared before fmt chunk");
            }
            audio = read_data_chunk(input, chunk_size, format, path);
            have_data = true;
            break;
        } else {
            skip_chunk_data(input, chunk_size, path);
        }
    }

    if (!have_format) {
        throw wav_error(path, "missing fmt chunk");
    }

    if (!have_data) {
        throw wav_error(path, "missing data chunk");
    }

    return audio;
}

void write_float_wav_file(const std::string& path, const AudioBuffer& audio) {
    validate_audio_for_write(audio, path);

    const std::uint64_t frame_count = audio.samples.empty()
                                          ? 0U
                                          : static_cast<std::uint64_t>(audio.samples.front().size());
    const std::uint16_t bytes_per_sample = 4;
    const std::uint16_t block_align = static_cast<std::uint16_t>(audio.channels * bytes_per_sample);
    const std::uint32_t byte_rate = audio.sample_rate * block_align;
    const std::uint64_t data_size_64 = frame_count * block_align;
    const std::uint64_t riff_size_64 = 4 + (8 + 16) + (8 + data_size_64);
    if (data_size_64 > std::numeric_limits<std::uint32_t>::max() ||
        riff_size_64 > std::numeric_limits<std::uint32_t>::max()) {
        throw wav_error(path, "WAV output would exceed RIFF size limits");
    }
    const auto data_size = static_cast<std::uint32_t>(data_size_64);
    const auto riff_size = static_cast<std::uint32_t>(riff_size_64);

    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw wav_error(path, "failed to open output WAV file");
    }

    output.write("RIFF", 4);
    write_u32(output, riff_size);
    output.write("WAVE", 4);
    output.write("fmt ", 4);
    write_u32(output, 16);
    write_u16(output, kFormatIeeeFloat);
    write_u16(output, audio.channels);
    write_u32(output, audio.sample_rate);
    write_u32(output, byte_rate);
    write_u16(output, block_align);
    write_u16(output, kSupportedFloatBits);
    output.write("data", 4);
    write_u32(output, data_size);

    for (std::uint64_t frame = 0; frame < frame_count; ++frame) {
        for (std::uint16_t channel = 0; channel < audio.channels; ++channel) {
            write_f32(output, audio.samples[channel][frame]);
        }
    }

    if (!output) {
        throw wav_error(path, "failed to write output WAV file");
    }
}

}  // namespace nvafx
