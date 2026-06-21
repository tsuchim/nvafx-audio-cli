#include "wav.hpp"

#include <algorithm>

#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <istream>
#include <limits>
#include <ostream>
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

std::runtime_error wav_error(const std::string& context, const std::string& message) {
    return std::runtime_error(context + ": " + message);
}

void read_exact(std::istream& input, char* data, std::size_t size, const std::string& context) {
    if (!input.read(data, static_cast<std::streamsize>(size))) {
        throw wav_error(context, "truncated file");
    }
}

std::array<char, 4> read_tag(std::istream& input, const std::string& context) {
    std::array<char, 4> tag{};
    read_exact(input, tag.data(), tag.size(), context);
    return tag;
}

bool tag_equals(const std::array<char, 4>& tag, const char* value) {
    return std::memcmp(tag.data(), value, tag.size()) == 0;
}

std::uint16_t read_u16(std::istream& input, const std::string& context) {
    std::array<unsigned char, 2> bytes{};
    read_exact(input, reinterpret_cast<char*>(bytes.data()), bytes.size(), context);
    return static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[0]) |
                                      (static_cast<std::uint16_t>(bytes[1]) << 8));
}

std::uint32_t read_u32(std::istream& input, const std::string& context) {
    std::array<unsigned char, 4> bytes{};
    read_exact(input, reinterpret_cast<char*>(bytes.data()), bytes.size(), context);
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8) |
           (static_cast<std::uint32_t>(bytes[2]) << 16) |
           (static_cast<std::uint32_t>(bytes[3]) << 24);
}

std::int16_t read_i16(std::istream& input, const std::string& context) {
    return static_cast<std::int16_t>(read_u16(input, context));
}

float read_f32(std::istream& input, const std::string& context) {
    const std::uint32_t bits = read_u32(input, context);
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

void write_f32(std::ostream& output, float value) {
    std::uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value), "float must be 32-bit");
    std::memcpy(&bits, &value, sizeof(bits));
    write_u32(output, bits);
}

void skip_chunk_data(std::istream& input, std::uint32_t size, const std::string& context) {
    std::array<char, 4096> buffer{};
    std::uint32_t remaining = size;
    while (remaining > 0) {
        const std::size_t to_read = std::min<std::size_t>(buffer.size(), remaining);
        read_exact(input, buffer.data(), to_read, context);
        remaining -= static_cast<std::uint32_t>(to_read);
    }

    if ((size % 2U) == 1U) {
        char padding = 0;
        read_exact(input, &padding, 1, context);
    }
}

FormatChunk read_format_chunk(std::istream& input, std::uint32_t size, const std::string& context) {
    if (size < 16) {
        throw wav_error(context, "malformed fmt chunk");
    }

    FormatChunk format;
    format.audio_format = read_u16(input, context);
    format.effective_format = format.audio_format;
    format.channels = read_u16(input, context);
    format.sample_rate = read_u32(input, context);
    (void)read_u32(input, context);
    format.block_align = read_u16(input, context);
    format.bits_per_sample = read_u16(input, context);
    format.valid_bits_per_sample = format.bits_per_sample;

    if (format.audio_format == kFormatExtensible) {
        if (size < 40) {
            throw wav_error(context, "malformed WAVE_FORMAT_EXTENSIBLE fmt chunk");
        }

        const std::uint16_t extension_size = read_u16(input, context);
        if (extension_size < 22) {
            throw wav_error(context, "malformed WAVE_FORMAT_EXTENSIBLE extension");
        }

        format.valid_bits_per_sample = read_u16(input, context);
        (void)read_u32(input, context);

        std::array<unsigned char, 16> subformat{};
        read_exact(input, reinterpret_cast<char*>(subformat.data()), subformat.size(), context);

        if (subformat == kPcmSubformat) {
            format.effective_format = kFormatPcm;
        } else if (subformat == kFloatSubformat) {
            format.effective_format = kFormatIeeeFloat;
        } else {
            throw wav_error(context, "unsupported WAVE_FORMAT_EXTENSIBLE subformat");
        }

        const std::uint32_t consumed = 40;
        if (size > consumed) {
            skip_chunk_data(input, size - consumed, context);
        } else if ((size % 2U) == 1U) {
            skip_chunk_data(input, 0, context);
        }
        return format;
    }

    const std::uint32_t consumed = 16;
    if (size > consumed) {
        skip_chunk_data(input, size - consumed, context);
    } else if ((size % 2U) == 1U) {
        skip_chunk_data(input, 0, context);
    }

    return format;
}

void validate_format(const FormatChunk& format, const std::string& context) {
    if (!is_supported_wav_channel_count(format.channels)) {
        throw wav_error(context, "unsupported channel count; expected mono or stereo");
    }

    if (!is_supported_wav_sample_rate(format.sample_rate)) {
        throw wav_error(context, "unsupported sample rate; expected 16000 or 48000 Hz");
    }

    const bool pcm16 = format.effective_format == kFormatPcm && format.bits_per_sample == kSupportedPcmBits &&
                       format.valid_bits_per_sample == kSupportedPcmBits;
    const bool float32 =
        format.effective_format == kFormatIeeeFloat && format.bits_per_sample == kSupportedFloatBits &&
        format.valid_bits_per_sample == kSupportedFloatBits;

    if (!pcm16 && !float32) {
        throw wav_error(context, "unsupported format; expected PCM signed 16-bit or IEEE float 32-bit");
    }

    const std::uint16_t expected_align =
        static_cast<std::uint16_t>(format.channels * (format.bits_per_sample / 8));
    if (format.block_align != expected_align) {
        throw wav_error(context, "malformed fmt chunk block alignment");
    }
}

AudioBuffer read_data_chunk(std::istream& input, std::uint32_t size, const FormatChunk& format,
                            const std::string& context) {
    validate_format(format, context);

    if (format.block_align == 0 || (size % format.block_align) != 0) {
        throw wav_error(context, "malformed data chunk size");
    }

    const std::uint32_t frame_count = size / format.block_align;
    AudioBuffer audio;
    audio.channels = format.channels;
    audio.sample_rate = format.sample_rate;
    audio.samples.assign(audio.channels, std::vector<float>(frame_count, 0.0F));

    for (std::uint32_t frame = 0; frame < frame_count; ++frame) {
        for (std::uint16_t channel = 0; channel < audio.channels; ++channel) {
            if (format.effective_format == kFormatPcm) {
                audio.samples[channel][frame] = static_cast<float>(read_i16(input, context)) / 32768.0F;
            } else {
                audio.samples[channel][frame] = read_f32(input, context);
            }
        }
    }

    if ((size % 2U) == 1U) {
        skip_chunk_data(input, 0, context);
    }

    return audio;
}

void validate_audio_for_write(const AudioBuffer& audio, const std::string& context) {
    if (!is_supported_wav_channel_count(audio.channels)) {
        throw wav_error(context, "unsupported channel count for write; expected mono or stereo");
    }

    if (!is_supported_wav_sample_rate(audio.sample_rate)) {
        throw wav_error(context, "unsupported sample rate for write; expected 16000 or 48000 Hz");
    }

    if (audio.samples.size() != audio.channels) {
        throw wav_error(context, "malformed planar audio buffer");
    }

    if (audio.samples.empty()) {
        return;
    }

    const std::size_t frame_count = audio.samples.front().size();
    for (const auto& channel : audio.samples) {
        if (channel.size() != frame_count) {
            throw wav_error(context, "malformed planar audio buffer");
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

AudioBuffer read_wav_stream(std::istream& input, const std::string& context) {
    if (!tag_equals(read_tag(input, context), "RIFF")) {
        throw wav_error(context, "unsupported container; expected RIFF");
    }

    const std::uint32_t riff_size = read_u32(input, context);
    if (riff_size < 4) {
        throw wav_error(context, "malformed RIFF size");
    }

    if (!tag_equals(read_tag(input, context), "WAVE")) {
        throw wav_error(context, "unsupported file type; expected WAVE");
    }

    std::uint64_t remaining = static_cast<std::uint64_t>(riff_size) - 4U;
    bool have_format = false;
    bool have_data = false;
    FormatChunk format;
    AudioBuffer audio;

    while (remaining > 0) {
        if (remaining < 8) {
            throw wav_error(context, "malformed chunk header");
        }

        const auto chunk_id = read_tag(input, context);
        const std::uint32_t chunk_size = read_u32(input, context);
        remaining -= 8;
        const std::uint64_t padded_chunk_size = static_cast<std::uint64_t>(chunk_size) + (chunk_size % 2U);
        if (padded_chunk_size > remaining) {
            throw wav_error(context, "truncated file");
        }

        if (tag_equals(chunk_id, "fmt ")) {
            format = read_format_chunk(input, chunk_size, context);
            have_format = true;
        } else if (tag_equals(chunk_id, "data")) {
            if (!have_format) {
                throw wav_error(context, "data chunk appeared before fmt chunk");
            }
            audio = read_data_chunk(input, chunk_size, format, context);
            have_data = true;
            break;
        } else {
            skip_chunk_data(input, chunk_size, context);
        }

        remaining -= padded_chunk_size;
    }

    if (!have_format) {
        throw wav_error(context, "missing fmt chunk");
    }

    if (!have_data) {
        throw wav_error(context, "missing data chunk");
    }

    return audio;
}

AudioBuffer read_wav_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw wav_error(path, "failed to open input WAV file");
    }

    return read_wav_stream(input, path);
}

void write_float_wav_stream(std::ostream& output, const AudioBuffer& audio, const std::string& context) {
    validate_audio_for_write(audio, context);

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
        throw wav_error(context, "WAV output would exceed RIFF size limits");
    }
    const auto data_size = static_cast<std::uint32_t>(data_size_64);
    const auto riff_size = static_cast<std::uint32_t>(riff_size_64);

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
        throw wav_error(context, "failed to write output WAV file");
    }
}

void write_float_wav_file(const std::string& path, const AudioBuffer& audio) {
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw wav_error(path, "failed to open output WAV file");
    }

    write_float_wav_stream(output, audio, path);
}

}  // namespace nvafx
