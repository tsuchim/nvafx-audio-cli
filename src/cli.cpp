#include "cli.hpp"

#include "sdk_discovery.hpp"
#include "wav.hpp"

#include <charconv>
#include <cmath>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

namespace nvafx {
namespace {

constexpr const char* kVersion = "0.1.0";

struct Options {
    bool help = false;
    bool version = false;
    bool dry_run = false;
    bool check_sdk = false;
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
        << "  nvafx-audio-cli --check-sdk [--sdk-root path]\n"
        << "  nvafx-audio-cli --input in.wav --output out.wav --effect denoiser "
           "--sample-rate 48000 --intensity 1.0 [--sdk-root path] [--dry-run]\n\n"
        << "Options:\n"
        << "  --help                 Show this help text.\n"
        << "  --version              Show version information.\n"
        << "  --input <path>         Input WAV file.\n"
        << "  --output <path>        Output WAV file.\n"
        << "  --effect <name>        denoiser, dereverb, or dereverb_denoiser.\n"
        << "  --sample-rate <hz>     16000 or 48000.\n"
        << "  --intensity <value>    Non-negative finite effect intensity.\n"
        << "  --sdk-root <path>      NVIDIA Audio Effects SDK root or use AFX_SDK_ROOT.\n"
        << "  --dry-run              Validate and print the planned operation without output.\n"
        << "  --check-sdk            Check the SDK root path and expected subpaths.\n";
}

bool is_accepted_effect(const std::string& effect) {
    return effect == "denoiser" || effect == "dereverb" || effect == "dereverb_denoiser";
}

bool is_accepted_sample_rate(int sample_rate) {
    return sample_rate == 16000 || sample_rate == 48000;
}

std::string require_value(int& index, int argc, char* argv[], const std::string& option) {
    if (index + 1 >= argc) {
        throw std::runtime_error(option + " requires a value");
    }

    ++index;
    return argv[index];
}

int parse_int(const std::string& value, const std::string& option) {
    int parsed = 0;
    const char* begin = value.data();
    const char* end = value.data() + value.size();
    const auto [ptr, ec] = std::from_chars(begin, end, parsed);
    if (value.empty() || ec != std::errc() || ptr != end || parsed <= 0) {
        throw std::runtime_error(option + " must be a positive integer");
    }

    if (!is_accepted_sample_rate(parsed)) {
        throw std::runtime_error(option + " must be 16000 or 48000");
    }

    return parsed;
}

double parse_double(const std::string& value, const std::string& option) {
    double parsed = 0.0;
    const char* begin = value.data();
    const char* end = value.data() + value.size();
    const auto [ptr, ec] = std::from_chars(begin, end, parsed);
    if (value.empty() || ec != std::errc() || ptr != end || !std::isfinite(parsed) || parsed < 0.0) {
        throw std::runtime_error(option + " must be a non-negative finite number");
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
        } else if (arg == "--dry-run") {
            options.dry_run = true;
        } else if (arg == "--check-sdk") {
            options.check_sdk = true;
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

bool has_processing_options(const Options& options) {
    return options.input || options.output || options.effect || options.sample_rate || options.intensity;
}

void validate_mode(const Options& options) {
    if (options.check_sdk && options.dry_run) {
        throw std::runtime_error("--check-sdk and --dry-run cannot be used together");
    }

    if (options.check_sdk && has_processing_options(options)) {
        throw std::runtime_error("--check-sdk cannot be combined with processing options");
    }

    if (options.sdk_root && !options.check_sdk && !options.dry_run && !has_processing_options(options)) {
        throw std::runtime_error("--sdk-root requires --check-sdk, --dry-run, or processing options");
    }
}

void require_processing_triplet(const Options& options) {
    if (!options.input || !options.output || !options.effect) {
        throw std::runtime_error("--input, --output, and --effect are required for processing");
    }
}

void print_sdk_probe(const SdkProbeResult& probe) {
    std::cout << "SDK root: " << (probe.root.empty() ? "<not set>" : probe.root) << "\n";
    std::cout << "Path exists: " << (probe.exists ? "yes" : "no") << "\n";
    std::cout << "SDK-like structure: " << (probe.structurally_plausible ? "plausible" : "incomplete")
              << "\n";

    if (!probe.present_subpaths.empty()) {
        std::cout << "Present SDK-like subpaths:";
        for (const std::string& subpath : probe.present_subpaths) {
            std::cout << " " << subpath;
        }
        std::cout << "\n";
    }

    if (!probe.missing_subpaths.empty()) {
        std::cout << "Missing SDK-like subpaths:";
        for (const std::string& subpath : probe.missing_subpaths) {
            std::cout << " " << subpath;
        }
        std::cout << "\n";
    }
}

int check_sdk(const Options& options) {
    const std::string root = resolve_sdk_root(options.sdk_root.value_or(""));
    if (root.empty()) {
        std::cerr << "Error: SDK root is not set. Use --sdk-root or AFX_SDK_ROOT.\n";
        return 2;
    }

    const SdkProbeResult probe = probe_sdk_root(root);
    print_sdk_probe(probe);
    return probe.structurally_plausible ? 0 : 2;
}

int dry_run(const Options& options) {
    require_processing_triplet(options);

    std::optional<AudioBuffer> audio;
    if (options.input) {
        audio = read_wav_file(*options.input);
        if (options.sample_rate && static_cast<int>(audio->sample_rate) != *options.sample_rate) {
            throw std::runtime_error("--sample-rate does not match input WAV sample rate");
        }
    }

    const std::string sdk_root = resolve_sdk_root(options.sdk_root.value_or(""));

    std::cout << "Dry run: SDK processing is not implemented and will not be called.\n";
    std::cout << "Input: " << *options.input << "\n";
    std::cout << "Output: " << *options.output << "\n";
    std::cout << "Effect: " << *options.effect << "\n";
    std::cout << "Sample rate option: "
              << (options.sample_rate ? std::to_string(*options.sample_rate) : "<not set>") << "\n";
    std::cout << "Intensity: " << (options.intensity ? std::to_string(*options.intensity) : "<not set>")
              << "\n";
    std::cout << "SDK root: " << (sdk_root.empty() ? "<not set>" : sdk_root) << "\n";

    if (audio) {
        const std::size_t frames = audio->samples.empty() ? 0U : audio->samples.front().size();
        std::cout << "WAV: " << audio->channels << " channel(s), " << audio->sample_rate << " Hz, "
                  << frames << " frame(s)\n";
    }

    return 0;
}

int process(const Options& options) {
    require_processing_triplet(options);

    std::cerr
        << "SDK processing is not implemented yet. NVIDIA Audio Effects SDK binaries and models "
           "are not included in this repository.\n";
    return 3;
}

}  // namespace

int run_cli(int argc, char* argv[]) {
    try {
        const Options options = parse_options(argc, argv);
        validate_mode(options);

        if (options.help) {
            print_help();
            return 0;
        }

        if (options.version) {
            std::cout << "nvafx-audio-cli " << kVersion << "\n";
            return 0;
        }

        if (argc == 1) {
            print_help();
            return 0;
        }

        if (options.check_sdk) {
            return check_sdk(options);
        }

        if (options.dry_run) {
            return dry_run(options);
        }

        if (has_processing_options(options)) {
            return process(options);
        }

        print_help();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}

}  // namespace nvafx
