#include "cli.hpp"

#include "afx_sdk.hpp"
#include "sdk_discovery.hpp"
#include "stdio_binary.hpp"
#include "wav.hpp"

#include <charconv>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

namespace nvafx {
namespace {

constexpr const char* kVersion = "0.2.0";

struct Options {
    bool help = false;
    bool version = false;
    bool dry_run = false;
    bool check_sdk = false;
    bool allow_unsafe_pipe = false;
    std::optional<std::string> input;
    std::optional<std::string> output;
    std::optional<std::string> effect;
    std::optional<int> sample_rate;
    std::optional<double> intensity;
    std::optional<std::string> sdk_root;
    std::optional<std::string> api_root;
    std::optional<std::string> runtime_root;
    std::optional<std::string> model;
};

void print_help() {
    std::cout
        << "nvafx-audio-cli " << kVersion << "\n"
        << "Small offline audio processing CLI for NVIDIA Audio Effects SDK.\n\n"
        << "Usage:\n"
        << "  nvafx-audio-cli --help\n"
        << "  nvafx-audio-cli --version\n"
        << "  nvafx-audio-cli --check-sdk [--api-root path] [--runtime-root path]\n"
        << "  nvafx-audio-cli --input in.wav --output out.wav --effect denoiser "
           "--sample-rate 48000 --intensity 1.0 --model model.trtpkg "
           "[--runtime-root path] [--dry-run]\n"
        << "  ffmpeg ... -f wav - | nvafx-audio-cli --input - --output - --effect denoiser "
           "--sample-rate 48000 --model model.trtpkg --runtime-root path | ffmpeg ...\n\n"
        << "Options:\n"
        << "  --help                 Show this help text.\n"
        << "  --version              Show version information.\n"
        << "  --input <path|->      Input WAV file, or - for stdin.\n"
        << "  --output <path|->     Output WAV file, or - for stdout.\n"
        << "  --effect <name>        denoiser, dereverb, or dereverb_denoiser.\n"
        << "  --sample-rate <hz>     16000 or 48000.\n"
        << "  --intensity <value>    Finite effect intensity from 0.0 through 1.0.\n"
        << "  --model <path>         NVIDIA AFX model file for processing.\n"
        << "  --api-root <path>      External Maxine AFX API root for --check-sdk.\n"
        << "  --runtime-root <path>  Installed NVIDIA Audio Effects SDK runtime root.\n"
        << "  --sdk-root <path>      Compatibility alias for --runtime-root.\n"
        << "  --dry-run              Validate and print the planned operation without output.\n"
        << "  --check-sdk            Check API/runtime SDK roots and expected files.\n"
        << "  --allow-unsafe-pipe    Override conservative Windows shell pipe refusal.\n\n"
        << "When --output - is used for processing, stdout contains WAV bytes only. "
           "Diagnostics go to stderr. PowerShell 7.4+ is required for native byte-stream piping; "
           "avoid 2>&1 and PowerShell cmdlets inside binary WAV pipelines.\n";
}

bool is_accepted_effect(const std::string& effect) {
    return effect == "denoiser" || effect == "dereverb" || effect == "dereverb_denoiser";
}

bool is_accepted_sample_rate(int sample_rate) {
    return sample_rate == 16000 || sample_rate == 48000;
}

bool is_stdin_path(const Options& options) {
    return options.input && *options.input == "-";
}

bool is_stdout_path(const Options& options) {
    return options.output && *options.output == "-";
}

std::string stdin_corruption_hint() {
    return "stdin did not contain a valid RIFF/WAVE stream. Unsafe PowerShell binary pipeline handling or inserted cmdlets may have corrupted the stream; use PowerShell 7.4+, cmd.exe, or temporary WAV files.";
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
    if (value.empty() || ec != std::errc() || ptr != end || !std::isfinite(parsed) || parsed < 0.0 ||
        parsed > 1.0) {
        throw std::runtime_error(option + " must be a finite number from 0.0 through 1.0");
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
        } else if (arg == "--allow-unsafe-pipe") {
            options.allow_unsafe_pipe = true;
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
        } else if (arg == "--api-root") {
            options.api_root = require_value(i, argc, argv, arg);
        } else if (arg == "--runtime-root") {
            options.runtime_root = require_value(i, argc, argv, arg);
        } else if (arg == "--model") {
            options.model = require_value(i, argc, argv, arg);
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
    return options.input || options.output || options.effect || options.sample_rate || options.intensity ||
           options.model;
}

void validate_mode(const Options& options) {
    if (options.check_sdk && options.dry_run) {
        throw std::runtime_error("--check-sdk and --dry-run cannot be used together");
    }

    if (options.check_sdk && has_processing_options(options)) {
        throw std::runtime_error("--check-sdk cannot be combined with processing options");
    }

    if ((options.sdk_root || options.api_root || options.runtime_root) && !options.check_sdk &&
        !options.dry_run && !has_processing_options(options)) {
        throw std::runtime_error("SDK root options require --check-sdk, --dry-run, or processing options");
    }
}

void require_processing_triplet(const Options& options) {
    if (!options.input || !options.output || !options.effect) {
        throw std::runtime_error("--input, --output, and --effect are required for processing");
    }
}

void enforce_pipe_safety(const Options& options) {
    if (!is_stdin_path(options) && !is_stdout_path(options)) {
        return;
    }

    const PipeSafetyCheck safety = check_pipe_safety(options.allow_unsafe_pipe);
    if (!safety.allowed) {
        throw std::runtime_error("unsafe binary WAV pipe context" +
                                 (safety.parent_process.empty() ? std::string() : " from " + safety.parent_process) +
                                 ": " + safety.message);
    }

    if (options.allow_unsafe_pipe && !safety.message.empty()) {
        std::cerr << "Warning: " << safety.message << "\n";
    }
}

AudioBuffer read_input_audio(const Options& options) {
    if (is_stdin_path(options)) {
        try {
            set_stdin_binary_mode();
            return read_wav_stream(std::cin, "<stdin>");
        } catch (const std::exception& ex) {
            throw std::runtime_error(std::string(ex.what()) + "; " + stdin_corruption_hint());
        }
    }

    return read_wav_file(*options.input);
}

void write_output_audio(const Options& options, const AudioBuffer& output) {
    if (is_stdout_path(options)) {
        set_stdout_binary_mode();
        write_float_wav_stream(std::cout, output, "<stdout>");
        return;
    }

    write_float_wav_file(*options.output, output);
}

std::ostream& dry_run_stream(const Options& options) {
    return is_stdout_path(options) ? std::cerr : std::cout;
}

void print_sdk_probe(const SdkProbeResult& probe) {
    std::cout << "API root: " << (probe.api_root.empty() ? "<not set>" : probe.api_root) << "\n";
    std::cout << "Runtime root: " << (probe.runtime_root.empty() ? "<not set>" : probe.runtime_root) << "\n";
    std::cout << "API root exists: " << (probe.api_root_exists ? "yes" : "no") << "\n";
    std::cout << "Runtime root exists: " << (probe.runtime_root_exists ? "yes" : "no") << "\n";
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

    if (!probe.libraries.empty()) {
        std::cout << "Import libraries:\n";
        for (const std::string& library : probe.libraries) {
            std::cout << "  " << library << "\n";
        }
    }

    if (!probe.runtime_dlls.empty()) {
        std::cout << "Runtime DLLs:\n";
        for (const std::string& dll : probe.runtime_dlls) {
            std::cout << "  " << dll << "\n";
        }
    }

    if (!probe.models.empty()) {
        std::cout << "Models:\n";
        for (const std::string& model : probe.models) {
            std::cout << "  " << model << "\n";
        }
    }
}

int check_sdk(const Options& options) {
    const std::string api_root = resolve_api_root(options.api_root.value_or(""));
    const std::string runtime_root =
        resolve_runtime_root(options.runtime_root.value_or(""), options.sdk_root.value_or(""));
    if (api_root.empty() && runtime_root.empty()) {
        std::cerr << "Error: SDK roots are not set. Use --api-root/--runtime-root or NVAFX_API_ROOT/"
                     "NVAFX_RUNTIME_ROOT.\n";
        return 2;
    }

    const SdkProbeResult probe = probe_sdk_roots(api_root, runtime_root);
    print_sdk_probe(probe);
    return probe.structurally_plausible ? 0 : 2;
}

int dry_run(const Options& options) {
    require_processing_triplet(options);
    enforce_pipe_safety(options);

    std::optional<AudioBuffer> audio;
    if (options.input) {
        audio = read_input_audio(options);
        if (options.sample_rate && static_cast<int>(audio->sample_rate) != *options.sample_rate) {
            throw std::runtime_error("--sample-rate does not match input WAV sample rate");
        }
    }

    const std::string runtime_root =
        resolve_runtime_root(options.runtime_root.value_or(""), options.sdk_root.value_or(""));

    std::ostream& output = dry_run_stream(options);
    output << "Dry run: SDK processing will not be called and no output will be written.\n";
    output << "Input: " << *options.input << "\n";
    output << "Output: " << *options.output << "\n";
    output << "Effect: " << *options.effect << "\n";
    output << "Sample rate option: "
           << (options.sample_rate ? std::to_string(*options.sample_rate) : "<not set>") << "\n";
    output << "Intensity: " << (options.intensity ? std::to_string(*options.intensity) : "<not set>")
           << "\n";
    output << "Model: " << (options.model ? *options.model : "<not set>") << "\n";
    output << "Runtime root: " << (runtime_root.empty() ? "<not set>" : runtime_root) << "\n";

    if (audio) {
        const std::size_t frames = audio->samples.empty() ? 0U : audio->samples.front().size();
        output << "WAV: " << audio->channels << " channel(s), " << audio->sample_rate << " Hz, "
               << frames << " frame(s)\n";
    }

    return 0;
}

int process(const Options& options) {
    require_processing_triplet(options);
    enforce_pipe_safety(options);

    if (!options.model) {
        throw std::runtime_error("--model is required for SDK processing");
    }

    if (!std::filesystem::is_regular_file(*options.model)) {
        throw std::runtime_error("model path does not exist: " + *options.model);
    }

    const AudioBuffer input = read_input_audio(options);
    if (options.sample_rate && static_cast<int>(input.sample_rate) != *options.sample_rate) {
        throw std::runtime_error("--sample-rate does not match input WAV sample rate");
    }

    const std::string runtime_root =
        resolve_runtime_root(options.runtime_root.value_or(""), options.sdk_root.value_or(""));
    if (runtime_root.empty()) {
        throw std::runtime_error("--runtime-root, --sdk-root, NVAFX_RUNTIME_ROOT, or AFX_SDK_ROOT is required for SDK processing");
    }

    const AfxProcessOptions process_options{
        *options.effect,
        *options.model,
        runtime_root,
        static_cast<float>(options.intensity.value_or(1.0)),
    };
    const AudioBuffer output = process_with_afx_sdk(input, process_options);
    write_output_audio(options, output);
    if (!is_stdout_path(options)) {
        std::cout << "Processed WAV written: " << *options.output << "\n";
    }
    return 0;
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
