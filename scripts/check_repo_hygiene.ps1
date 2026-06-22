$ErrorActionPreference = "Stop"

$script = Join-Path $PSScriptRoot "check_repo_hygiene.py"
$python = Get-Command python3 -ErrorAction SilentlyContinue
if ($null -eq $python) {
    $python = Get-Command python -ErrorAction Stop
}

& $python.Source $script
exit $LASTEXITCODE
