$ErrorActionPreference = "Stop"

$trackedFiles = git ls-files
$blockedExtensions = @(
    ".wav", ".mp4", ".mov", ".mkv", ".avi", ".flac", ".aac", ".mp3",
    ".dll", ".exe", ".lib", ".pdb", ".ilk", ".obj", ".exp",
    ".zip", ".7z", ".tar", ".gz", ".tgz",
    ".trtpkg", ".onnx", ".engine"
)

$blockedPathPatterns = @(
    "(^|/)Audio_Effects_SDK",
    "(^|/)NVIDIA_Audio_Effects_SDK",
    "(^|/)NVIDIA-Audio-Effects-SDK",
    "(^|/)nvidia-audio-effects-sdk",
    "(^|/)nvidia_sdk",
    "(^|/)nvidia-sdk",
    "(^|/)external/nvidia",
    "(^|/)vendor/nvidia",
    "(^|/)third_party/nvidia",
    "(^|/)models(/|$)",
    "(^|/)features(/|$)",
    "(^|/)sdk(/|$)"
)

$maxTrackedBytes = 5MB
$violations = New-Object System.Collections.Generic.List[string]

foreach ($file in $trackedFiles) {
    $normalized = $file -replace "\\", "/"
    $extension = [System.IO.Path]::GetExtension($file).ToLowerInvariant()

    if ($blockedExtensions -contains $extension) {
        $violations.Add("blocked extension: $file")
    }

    foreach ($pattern in $blockedPathPatterns) {
        if ($normalized -imatch $pattern) {
            $violations.Add("blocked path pattern '$pattern': $file")
            break
        }
    }

    if (Test-Path -LiteralPath $file -PathType Leaf) {
        $length = (Get-Item -LiteralPath $file).Length
        if ($length -gt $maxTrackedBytes) {
            $violations.Add("tracked file exceeds $maxTrackedBytes bytes: $file ($length bytes)")
        }
    }
}

if ($violations.Count -gt 0) {
    Write-Error ("Repository hygiene failed:`n" + ($violations -join "`n"))
    exit 1
}

Write-Host "Repository hygiene passed: inspected $($trackedFiles.Count) tracked files."
