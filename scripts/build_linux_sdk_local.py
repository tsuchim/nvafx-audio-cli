#!/usr/bin/env python3
"""Build and optionally install a local Linux SDK-enabled nvafx-audio-cli."""

from __future__ import annotations

import argparse
import os
import shutil
import shlex
import stat
import subprocess
import sys
import tempfile
from pathlib import Path


DEFAULT_BUILD_DIR = "build-linux-sdk"
DEFAULT_BUILD_TYPE = "RelWithDebInfo"
DEFAULT_GENERATOR = "Ninja"
DEFAULT_FEATURE = "denoiser"
DEFAULT_WRAPPER_NAME = "nvafx-audio-cli-sdk"


class HelperError(RuntimeError):
    """User-facing helper failure."""


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Configure, build, validate, and optionally install a local Linux "
            "SDK-enabled nvafx-audio-cli using user-provided SDK paths."
        )
    )
    parser.add_argument(
        "--sdk-root",
        required=True,
        type=Path,
        help="Path to the external Audio_Effects_SDK directory.",
    )
    parser.add_argument(
        "--build-dir",
        default=Path(DEFAULT_BUILD_DIR),
        type=Path,
        help=f"Build directory. Default: {DEFAULT_BUILD_DIR}",
    )
    parser.add_argument(
        "--build-type",
        default=DEFAULT_BUILD_TYPE,
        help=f"CMake build type. Default: {DEFAULT_BUILD_TYPE}",
    )
    parser.add_argument(
        "--generator",
        default=DEFAULT_GENERATOR,
        help=f"CMake generator. Default: {DEFAULT_GENERATOR}",
    )
    parser.add_argument(
        "--model",
        type=Path,
        help="Optional model path for the manual SDK denoiser smoke test.",
    )
    parser.add_argument(
        "--feature",
        default=DEFAULT_FEATURE,
        help=f"SDK feature name. Default: {DEFAULT_FEATURE}",
    )
    parser.add_argument(
        "--run-test",
        action="store_true",
        help="Run the manual Linux SDK processing CTest. Requires --model.",
    )
    parser.add_argument(
        "--install-prefix",
        type=Path,
        help="Optional local install prefix for a generated SDK-enabled wrapper.",
    )
    parser.add_argument(
        "--wrapper-name",
        default=DEFAULT_WRAPPER_NAME,
        help=f"Wrapper name under <install-prefix>/bin. Default: {DEFAULT_WRAPPER_NAME}",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print planned commands and paths without configuring, building, or installing.",
    )
    return parser.parse_args()


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def resolve_user_path(path: Path) -> Path:
    expanded = path.expanduser()
    if expanded.is_absolute():
        return expanded
    return (Path.cwd() / expanded).absolute()


def require_path(path: Path, label: str, *, is_dir: bool | None = None) -> None:
    if not path.exists():
        raise HelperError(f"{label} does not exist: {path}")
    if is_dir is True and not path.is_dir():
        raise HelperError(f"{label} is not a directory: {path}")
    if is_dir is False and not path.is_file():
        raise HelperError(f"{label} is not a file: {path}")


def feature_library_name(feature: str) -> str:
    return f"libnv_audiofx_{feature}.so"


def validate_sdk_root(sdk_root: Path, feature: str) -> None:
    require_path(sdk_root, "SDK root", is_dir=True)
    require_path(
        sdk_root / "nvafx" / "include" / "nvAudioEffects.h",
        "Missing NVIDIA AFX header",
        is_dir=False,
    )
    require_path(
        sdk_root / "nvafx" / "lib" / "libnv_audiofx.so",
        "Missing NVIDIA AFX base shared library",
        is_dir=False,
    )
    require_path(
        sdk_root / "features" / feature / "lib" / feature_library_name(feature),
        f"Missing NVIDIA AFX feature shared library for feature '{feature}'",
        is_dir=False,
    )


def existing_library_dirs(sdk_root: Path, feature: str) -> list[Path]:
    candidates: list[Path] = [
        sdk_root / "nvafx" / "lib",
        sdk_root / "external" / "cuda" / "lib",
        sdk_root / "features" / feature / "lib",
    ]
    features_root = sdk_root / "features"
    if features_root.is_dir():
        candidates.extend(sorted(path for path in features_root.glob("*/lib") if path.is_dir()))

    result: list[Path] = []
    seen: set[Path] = set()
    for candidate in candidates:
        if candidate.is_dir():
            resolved = candidate.resolve()
            if resolved not in seen:
                seen.add(resolved)
                result.append(resolved)
    return result


def require_tool(tool: str, hint: str) -> None:
    if shutil.which(tool) is None:
        raise HelperError(f"{tool} is unavailable. {hint}")


def check_tools(args: argparse.Namespace) -> None:
    if args.dry_run:
        return
    require_tool("cmake", "Install CMake and retry.")
    if args.generator.lower() == "ninja":
        require_tool("ninja", "Install ninja-build or pass --generator for an available generator.")
    if args.run_test:
        require_tool("ctest", "Install CTest with CMake and retry.")


def command_to_text(command: list[str]) -> str:
    return " ".join(shlex.quote(part) for part in command)


def merged_env_with_library_dirs(library_dirs: list[Path], extra: dict[str, str] | None = None) -> dict[str, str]:
    env = os.environ.copy()
    prefix = ":".join(str(path) for path in library_dirs)
    existing = env.get("LD_LIBRARY_PATH", "")
    if prefix and existing:
        env["LD_LIBRARY_PATH"] = f"{prefix}:{existing}"
    elif prefix:
        env["LD_LIBRARY_PATH"] = prefix
    if extra:
        env.update(extra)
    return env


def comparable_library_dir(path_text: str) -> str:
    path = Path(path_text).expanduser()
    if path.exists():
        return str(path.resolve())
    if path.is_absolute():
        return str(path)
    return str((Path.cwd() / path).absolute())


def env_without_sdk_library_dirs(library_dirs: list[Path]) -> dict[str, str]:
    env = os.environ.copy()
    sdk_dirs = {comparable_library_dir(str(path)) for path in library_dirs}
    existing = env.get("LD_LIBRARY_PATH")
    if existing is None:
        return env

    preserved_entries: list[str] = []
    for entry in existing.split(":"):
        if entry and comparable_library_dir(entry) in sdk_dirs:
            continue
        preserved_entries.append(entry)

    if preserved_entries:
        env["LD_LIBRARY_PATH"] = ":".join(preserved_entries)
    else:
        env.pop("LD_LIBRARY_PATH", None)
    return env


def run_command(
    command: list[str],
    *,
    cwd: Path,
    env: dict[str, str] | None = None,
    dry_run: bool = False,
) -> subprocess.CompletedProcess[str] | None:
    print(f"+ {command_to_text(command)}", flush=True)
    if dry_run:
        return None
    return subprocess.run(command, cwd=cwd, env=env, check=True, text=True)


def run_capture(command: list[str], *, cwd: Path, env: dict[str, str] | None = None) -> str:
    print(f"+ {command_to_text(command)}", flush=True)
    result = subprocess.run(
        command,
        cwd=cwd,
        env=env,
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    if result.stdout:
        print(result.stdout, end="")
    return result.stdout


def binary_path(build_dir: Path) -> Path:
    return build_dir / "nvafx-audio-cli"


def validate_manual_test_available(build_dir: Path, env: dict[str, str]) -> None:
    output = run_capture(
        ["ctest", "--test-dir", str(build_dir), "-N", "-R", "manual-linux-sdk-processing"],
        cwd=repo_root(),
        env=env,
    )
    if "manual-linux-sdk-processing" not in output:
        raise HelperError(
            "The build tree does not contain the manual SDK test "
            "'manual-linux-sdk-processing'. Reconfigure with BUILD_TESTING enabled."
        )


def make_wrapper(wrapper_path: Path, installed_binary: Path, library_dirs: list[Path]) -> None:
    ld_prefix = ":".join(str(path) for path in library_dirs)
    wrapper = "\n".join(
        [
            "#!/usr/bin/env bash",
            "set -e",
            f"LD_LIBRARY_PATH={shlex.quote(ld_prefix)}:${{LD_LIBRARY_PATH:-}} "
            f"exec {shlex.quote(str(installed_binary))} \"$@\"",
            "",
        ]
    )
    wrapper_path.write_text(wrapper, encoding="utf-8")
    wrapper_path.chmod(wrapper_path.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)


def install_local_wrapper(
    *,
    build_dir: Path,
    install_prefix: Path,
    wrapper_name: str,
    library_dirs: list[Path],
    sdk_root: Path,
    dry_run: bool,
) -> Path:
    source_binary = binary_path(build_dir)
    require_path(source_binary, "Built nvafx-audio-cli binary", is_dir=False)

    bin_dir = install_prefix / "bin"
    libexec_dir = install_prefix / "libexec" / wrapper_name
    installed_binary = libexec_dir / "nvafx-audio-cli"
    wrapper_path = bin_dir / wrapper_name

    print(f"Local wrapper install prefix: {install_prefix}")
    print(f"Project binary destination: {installed_binary}")
    print(f"Wrapper destination: {wrapper_path}")
    print("NVIDIA SDK, feature, model, and CUDA files will not be copied.")

    if dry_run:
        return wrapper_path

    bin_dir.mkdir(parents=True, exist_ok=True)
    libexec_dir.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source_binary, installed_binary)
    installed_binary.chmod(installed_binary.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
    make_wrapper(wrapper_path, installed_binary, library_dirs)

    wrapper_validation_env = env_without_sdk_library_dirs(library_dirs)
    run_command([str(wrapper_path), "--version"], cwd=repo_root(), env=wrapper_validation_env)
    run_command(
        [
            str(wrapper_path),
            "--check-sdk",
            "--api-root",
            str(sdk_root / "nvafx"),
            "--runtime-root",
            str(sdk_root),
        ],
        cwd=repo_root(),
        env=wrapper_validation_env,
    )
    print("Installed wrapper validation ran without pre-populated SDK LD_LIBRARY_PATH.")
    return wrapper_path


def run_installed_wrapper_processing_smoke(
    *,
    wrapper_path: Path,
    build_dir: Path,
    sdk_root: Path,
    model: Path,
    library_dirs: list[Path],
) -> None:
    fixture = build_dir / "wav-fixture"
    inspect = build_dir / "wav-inspect"
    require_path(fixture, "Built wav-fixture helper", is_dir=False)
    require_path(inspect, "Built wav-inspect helper", is_dir=False)

    wrapper_validation_env = env_without_sdk_library_dirs(library_dirs)
    with tempfile.TemporaryDirectory(prefix="nvafx-sdk-wrapper-") as temp_dir:
        temp_root = Path(temp_dir)
        input_wav = temp_root / "input.wav"
        output_wav = temp_root / "output.wav"
        run_command([str(fixture), str(input_wav), "48000", "1", "pcm16"], cwd=repo_root())
        run_command(
            [
                str(wrapper_path),
                "--input",
                str(input_wav),
                "--output",
                str(output_wav),
                "--effect",
                "denoiser",
                "--sample-rate",
                "48000",
                "--model",
                str(model),
                "--runtime-root",
                str(sdk_root),
            ],
            cwd=repo_root(),
            env=wrapper_validation_env,
        )
        run_command([str(inspect), str(output_wav), "48000", "1", "4"], cwd=repo_root())
    print("Installed wrapper processing smoke ran without pre-populated SDK LD_LIBRARY_PATH.")


def print_plan(
    *,
    args: argparse.Namespace,
    sdk_root: Path,
    build_dir: Path,
    model: Path | None,
    library_dirs: list[Path],
    configure_command: list[str],
    build_command: list[str],
    check_command: list[str],
) -> None:
    print(f"Repository root: {repo_root()}")
    print(f"SDK root: {sdk_root}")
    print(f"Feature: {args.feature}")
    if model:
        print(f"Model: {model}")
    print(f"Build directory: {build_dir}")
    print(f"Build type: {args.build_type}")
    print(f"Generator: {args.generator}")
    print("SDK library directories for process-local LD_LIBRARY_PATH:")
    for path in library_dirs:
        print(f"  {path}")
    if args.dry_run:
        print("Dry run: no configure, build, test, install, or wrapper commands will be executed.")
    print("Planned commands:")
    for command in (configure_command, build_command, check_command):
        print(f"  {command_to_text(command)}")
    if args.run_test:
        print("  ctest --test-dir <build-dir> -R manual-linux-sdk-processing --output-on-failure")
    if args.install_prefix:
        print(f"  install project binary and wrapper under {resolve_user_path(args.install_prefix)}")


def main() -> int:
    args = parse_args()
    try:
        sdk_root = resolve_user_path(args.sdk_root)
        build_dir = resolve_user_path(args.build_dir)
        model = resolve_user_path(args.model) if args.model else None
        install_prefix = resolve_user_path(args.install_prefix) if args.install_prefix else None

        validate_sdk_root(sdk_root, args.feature)
        if args.run_test and not model:
            raise HelperError("--run-test requires --model.")
        if model:
            require_path(model, "Model path", is_dir=False)

        check_tools(args)

        library_dirs = existing_library_dirs(sdk_root, args.feature)
        configure_command = [
            "cmake",
            "-S",
            str(repo_root()),
            "-B",
            str(build_dir),
            "-G",
            args.generator,
            "-DNVAFX_ENABLE_SDK=ON",
            f"-DNVAFX_RUNTIME_ROOT={sdk_root}",
            f"-DCMAKE_BUILD_TYPE={args.build_type}",
        ]
        build_command = ["cmake", "--build", str(build_dir)]
        check_command = [
            str(binary_path(build_dir)),
            "--check-sdk",
            "--api-root",
            str(sdk_root / "nvafx"),
            "--runtime-root",
            str(sdk_root),
        ]

        print_plan(
            args=args,
            sdk_root=sdk_root,
            build_dir=build_dir,
            model=model,
            library_dirs=library_dirs,
            configure_command=configure_command,
            build_command=build_command,
            check_command=check_command,
        )
        sys.stdout.flush()

        if args.dry_run:
            return 0

        run_command(configure_command, cwd=repo_root())
        run_command(build_command, cwd=repo_root())
        require_path(binary_path(build_dir), "Built nvafx-audio-cli binary", is_dir=False)

        env = merged_env_with_library_dirs(library_dirs)
        run_command(check_command, cwd=repo_root(), env=env)

        if args.run_test:
            assert model is not None
            test_env = merged_env_with_library_dirs(
                library_dirs,
                {
                    "NVAFX_LINUX_SDK_ROOT": str(sdk_root),
                    "NVAFX_LINUX_FEATURE_ROOT": str(sdk_root / "features" / args.feature),
                    "NVAFX_TEST_MODEL": str(model),
                },
            )
            validate_manual_test_available(build_dir, test_env)
            run_command(
                [
                    "ctest",
                    "--test-dir",
                    str(build_dir),
                    "-R",
                    "manual-linux-sdk-processing",
                    "--output-on-failure",
                ],
                cwd=repo_root(),
                env=test_env,
            )

        wrapper_path: Path | None = None
        if install_prefix:
            wrapper_path = install_local_wrapper(
                build_dir=build_dir,
                install_prefix=install_prefix,
                wrapper_name=args.wrapper_name,
                library_dirs=library_dirs,
                sdk_root=sdk_root,
                dry_run=args.dry_run,
            )

        if args.run_test and install_prefix and wrapper_path:
            assert model is not None
            run_installed_wrapper_processing_smoke(
                wrapper_path=wrapper_path,
                build_dir=build_dir,
                sdk_root=sdk_root,
                model=model,
                library_dirs=library_dirs,
            )

        print("Local Linux SDK-enabled build helper completed successfully.")
        return 0
    except HelperError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    except subprocess.CalledProcessError as exc:
        print(f"error: command failed with exit code {exc.returncode}", file=sys.stderr)
        return exc.returncode or 1


if __name__ == "__main__":
    raise SystemExit(main())
