#include <cstdlib>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

namespace {

constexpr const char* kVersion = "0.1.0";

struct Options {
    bool help = false;
    bool version = false;
    std::optional<std::string> input;
    std::optional<std::string> output;
    std::optional<std::string> effect;
    std::optional<int> sample_rate;
    std::optional<double> intensity;
    std::optional<std::string> sdk_root;
};

void print_help() {
    std::cout
        << "nvafx-audio-cli " << kVersion << "\n"
        << "Small offline audio processing CLI for NVIDIA Audio Effects SDK.\n\n"
        << "Usage:\n"
        << "  nvafx-audio-cli --help\n"
        << "  nvafx-audio-cli --version\n"
        << "  nvafx-audio-cli --input in.wav --output out.wav --effect denoiser "
           "--sample-rate 48000 --intensity 1.0 [--sdk-root path]\n\n"
        << "Options:\n"
        << "  --help                 Show this help text.\n"
        << "  --version              Show version information.\n"
        << "  --input <path>         Input WAV file.\n"
        << "  --output <path>        Output WAV file.\n"
        << "  --effect <name>        denoiser, dereverb, or dereverb_denoiser.\n"
        << "  --sample-rate <hz>     Sample rate, for example 48000.\n"
        << "  --intensity <value>    Effect intensity, for example 1.0.\n"
        << "  --sdk-root <path>      Future NVIDIA Audio Effects SDK root.\n";
}

bool is_accepted_effect(const std::string& effect) {
    return effect == "denoiser" || effect == "dereverb" || effect == "dereverb_denoiser";
}

std::string require_value(int& index, int argc, char* argv[], const std::string& option) {
    if (index + 1 >= argc) {
        throw std::runtime_error(option + " requires a value");
    }

    ++index;
    return argv[index];
}

int parse_int(const std::string& value, const std::string& option) {
    std::size_t consumed = 0;
    const int parsed = std::stoi(value, &consumed);
    if (consumed != value.size() || parsed <= 0) {
        throw std::runtime_error(option + " must be a positive integer");
    }

    return parsed;
}

double parse_double(const std::string& value, const std::string& option) {
    std::size_t consumed = 0;
    const double parsed = std::stod(value, &consumed);
    if (consumed != value.size() || parsed < 0.0) {
        throw std::runtime_error(option + " must be a non-negative number");
    }

    return parsed;
}

Options parse_options(int argc, char* argv[]) {
    Options options;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "--help") {
            options.help = true;
        } else if (arg == "--version") {
            options.version = true;
        } else if (arg == "--input") {
            options.input = require_value(i, argc, argv, arg);
        } else if (arg == "--output") {
            options.output = require_value(i, argc, argv, arg);
        } else if (arg == "--effect") {
            options.effect = require_value(i, argc, argv, arg);
        } else if (arg == "--sample-rate") {
            options.sample_rate = parse_int(require_value(i, argc, argv, arg), arg);
        } else if (arg == "--intensity") {
            options.intensity = parse_double(require_value(i, argc, argv, arg), arg);
        } else if (arg == "--sdk-root") {
            options.sdk_root = require_value(i, argc, argv, arg);
        } else {
            throw std::runtime_error("Unknown option: " + arg);
        }
    }

    if (options.effect && !is_accepted_effect(*options.effect)) {
        throw std::runtime_error(
            "Unsupported effect: " + *options.effect +
            " (expected denoiser, dereverb, or dereverb_denoiser)");
    }

    return options;
}

bool processing_requested(const Options& options) {
    return options.input || options.output || options.effect || options.sample_rate || options.intensity ||
           options.sdk_root;
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        const Options options = parse_options(argc, argv);

        if (options.help || argc == 1) {
            print_help();
            return 0;
        }

        if (options.version) {
            std::cout << "nvafx-audio-cli " << kVersion << "\n";
            return 0;
        }

        if (processing_requested(options)) {
            if (!options.input || !options.output || !options.effect) {
                std::cerr << "Error: --input, --output, and --effect are required for processing.\n";
                return 2;
            }

            std::cerr
                << "SDK processing is not implemented yet. NVIDIA Audio Effects SDK binaries and models "
                   "are not included in this repository.\n";
            return 3;
        }

        print_help();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}
