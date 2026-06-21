#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

constexpr std::array<unsigned char, 16> kPcmSubformat = {
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
    0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71,
};

constexpr std::array<unsigned char, 16> kFloatSubformat = {
    0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
    0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71,
};

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

}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 5) {
        std::cerr << "Usage: wav-fixture <path> <sample-rate> <channels> "
                     "<pcm16|float32|extensible-pcm16|extensible-float32|truncated>\n";
        return 2;
    }

    const std::filesystem::path path(argv[1]);
    const auto sample_rate = static_cast<std::uint32_t>(std::stoul(argv[2]));
    const auto channels = static_cast<std::uint16_t>(std::stoul(argv[3]));
    const std::string format(argv[4]);

    if (format == "truncated") {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream output(path, std::ios::binary);
        output.write("RIFF", 4);
        write_u32(output, 100);
        output.write("WAVEfmt ", 8);
        return output ? 0 : 1;
    }

    const bool pcm16 = format == "pcm16" || format == "extensible-pcm16";
    const bool float32 = format == "float32" || format == "extensible-float32";
    const bool extensible = format == "extensible-pcm16" || format == "extensible-float32";
    if (!pcm16 && !float32) {
        std::cerr << "Unsupported fixture format\n";
        return 2;
    }

    std::filesystem::create_directories(path.parent_path());

    const std::uint32_t frames = 4;
    const std::uint16_t bits_per_sample = pcm16 ? 16 : 32;
    const std::uint16_t bytes_per_sample = static_cast<std::uint16_t>(bits_per_sample / 8);
    const std::uint16_t block_align = static_cast<std::uint16_t>(channels * bytes_per_sample);
    const std::uint32_t byte_rate = sample_rate * block_align;
    const std::uint32_t data_size = frames * block_align;
    const std::uint32_t fmt_size = extensible ? 40 : 16;
    const std::uint32_t riff_size = 4 + (8 + fmt_size) + (8 + data_size);

    std::ofstream output(path, std::ios::binary);
    if (!output) {
        std::cerr << "Failed to open fixture output\n";
        return 1;
    }

    output.write("RIFF", 4);
    write_u32(output, riff_size);
    output.write("WAVE", 4);
    output.write("fmt ", 4);
    write_u32(output, fmt_size);
    write_u16(output, extensible ? 0xfffe : (pcm16 ? 1 : 3));
    write_u16(output, channels);
    write_u32(output, sample_rate);
    write_u32(output, byte_rate);
    write_u16(output, block_align);
    write_u16(output, bits_per_sample);
    if (extensible) {
        write_u16(output, 22);
        write_u16(output, bits_per_sample);
        write_u32(output, channels == 1 ? 0x4 : 0x3);
        const auto& subformat = pcm16 ? kPcmSubformat : kFloatSubformat;
        output.write(reinterpret_cast<const char*>(subformat.data()),
                     static_cast<std::streamsize>(subformat.size()));
    }
    output.write("data", 4);
    write_u32(output, data_size);

    for (std::uint32_t frame = 0; frame < frames; ++frame) {
        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            if (pcm16) {
                write_u16(output, static_cast<std::uint16_t>(frame * 256 + channel));
            } else {
                write_u32(output, 0);
            }
        }
    }

    return output ? 0 : 1;
}
