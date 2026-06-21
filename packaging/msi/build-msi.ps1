param(
    [Parameter(Mandatory = $true)]
    [string] $InputDirectory,

    [Parameter(Mandatory = $true)]
    [string] $OutputDirectory,

    [Parameter(Mandatory = $true)]
    [string] $Version,

    [ValidateSet('x64')]
    [string] $Architecture = 'x64',

    [string] $WixPath = 'wix'
)

$ErrorActionPreference = 'Stop'

function Resolve-ExistingDirectory([string] $Path, [string] $Name) {
    if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
        throw "$Name does not exist or is not a directory: $Path"
    }
    return (Resolve-Path -LiteralPath $Path).Path
}

function Assert-RequiredFiles([string] $PayloadDir) {
    $required = @(
        'nvafx-audio-cli.exe',
        'README.md',
        'LICENSE',
        'THIRD_PARTY_NOTICES.md',
        'BUILD_INFO.txt'
    )

    foreach ($file in $required) {
        $path = Join-Path $PayloadDir $file
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Required payload file is missing: $file"
        }
    }

    $actual = Get-ChildItem -LiteralPath $PayloadDir -File | Select-Object -ExpandProperty Name
    $extra = $actual | Where-Object { $_ -notin $required }
    if ($extra) {
        throw "Unexpected payload file(s): $($extra -join ', ')"
    }
}

function Assert-NoForbiddenArtifacts([string] $PayloadDir) {
    $forbiddenPatterns = @(
        'NVAudioEffects.dll',
        '*.trtpkg',
        '*.lib',
        '*.h',
        '*.onnx',
        '*.engine',
        '*.wav',
        '*.mp4',
        '*.mov',
        '*.mkv',
        '*.avi',
        '*.flac',
        '*.aac',
        '*.mp3',
        '*.zip',
        '*.7z',
        '*.tar',
        '*.tar.gz',
        'Audio_Effects_SDK*',
        'NVIDIA_Audio_Effects_SDK*',
        'models',
        'features',
        'sdk'
    )

    $hits = New-Object System.Collections.Generic.List[string]
    foreach ($pattern in $forbiddenPatterns) {
        Get-ChildItem -LiteralPath $PayloadDir -Recurse -Force -ErrorAction SilentlyContinue -Filter $pattern |
            ForEach-Object { [void] $hits.Add($_.FullName) }
    }

    Get-ChildItem -LiteralPath $PayloadDir -Recurse -Force -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match '[\\/]third_party[\\/]nvidia([\\/]|$)' } |
        ForEach-Object { [void] $hits.Add($_.FullName) }

    if ($hits.Count -gt 0) {
        throw "Forbidden artifact(s) found in payload: $($hits -join ', ')"
    }
}

if ($Version -notmatch '^\d+\.\d+\.\d+$') {
    throw "Version must use Windows Installer numeric format major.minor.patch: $Version"
}

$payloadDir = Resolve-ExistingDirectory $InputDirectory 'InputDirectory'
$outputDirFull = [System.IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $outputDirFull | Out-Null

Assert-RequiredFiles $payloadDir
Assert-NoForbiddenArtifacts $payloadDir

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$wxsPath = Join-Path $scriptDir 'Product.wxs'
$msiName = "nvafx-audio-cli-v$Version-windows-$Architecture.msi"
$msiPath = Join-Path $outputDirFull $msiName

Remove-Item -LiteralPath $msiPath -Force -ErrorAction SilentlyContinue

$wixArgs = @(
    'build',
    $wxsPath,
    '-arch', $Architecture,
    '-d', "ProductVersion=$Version",
    '-d', "PayloadDir=$payloadDir",
    '-out', $msiPath
)

& $WixPath @wixArgs
if ($LASTEXITCODE -ne 0) {
    throw "WiX failed with exit code $LASTEXITCODE"
}

if (-not (Test-Path -LiteralPath $msiPath -PathType Leaf)) {
    throw "MSI was not created: $msiPath"
}

Write-Output $msiPath
