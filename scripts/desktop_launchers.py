"""Fixed native launcher templates for versioned, writable game data."""
import plistlib
from pathlib import Path

MAC_LAUNCHER = '''#!/bin/sh
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
DATA=${KAIRO_DATA_HOME:-"$HOME/Library/Application Support/Kairo/Signal Vault/0.1.0"}
mkdir -p -- "$DATA"
if [ ! -e "$DATA/project" ]; then
    LOCK="$DATA/.first-launch-lock"
    mkdir -- "$LOCK" || { echo "First-launch lock exists: $LOCK" >&2; exit 1; }
    trap 'rm -rf -- "$LOCK"' EXIT
    trap 'exit 1' HUP INT TERM
    if [ ! -e "$DATA/project" ]; then
        cp -R -- "$HERE/../Resources/project" "$LOCK/project"
        chmod -R u+rwX -- "$LOCK/project"
        mv -- "$LOCK/project" "$DATA/project"
    fi
    rmdir -- "$LOCK"
    trap - EXIT HUP INT TERM
fi
[ -f "$DATA/project/Project.kproject" ] || { echo 'Invalid user project' >&2; exit 1; }
exec "$HERE/KairoPlayer" "$DATA/project/Project.kproject" "$@"
'''

WINDOWS_LAUNCHER = r'''param([switch]$Validate)
$ErrorActionPreference = 'Stop'
$Data = if ($env:KAIRO_DATA_HOME) { $env:KAIRO_DATA_HOME } else { Join-Path $env:LOCALAPPDATA 'Kairo\Signal Vault\Data\0.1.0' }
$Project = Join-Path $Data 'project'
[IO.Directory]::CreateDirectory($Data) | Out-Null
if (-not (Test-Path -LiteralPath $Project)) {
    $Lock = Join-Path $Data '.first-launch-lock'
    New-Item -ItemType Directory -Path $Lock -ErrorAction Stop | Out-Null
    try {
        if (-not (Test-Path -LiteralPath $Project)) {
            Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'project') -Destination (Join-Path $Lock 'project') -Recurse
            [IO.Directory]::Move((Join-Path $Lock 'project'), $Project)
        }
    } finally { Remove-Item -LiteralPath $Lock -Recurse -Force }
}
$Descriptor = Join-Path $Project 'Project.kproject'
if (-not (Test-Path -LiteralPath $Descriptor -PathType Leaf)) { throw 'Invalid user project' }
$Player = Join-Path $PSScriptRoot 'bin\KairoPlayer.exe'
if ($Validate) { & $Player $Descriptor --validate } else { & $Player $Descriptor }
exit $LASTEXITCODE
'''

WINDOWS_INSTALLER = r'''param([string]$Destination = (Join-Path $env:LOCALAPPDATA 'Kairo\Signal Vault\Apps\0.1.0'))
$ErrorActionPreference = 'Stop'
$Destination = [IO.Path]::GetFullPath($Destination)
if (Test-Path -LiteralPath $Destination) { throw 'Installation already exists; no overwrite' }
$Source = Join-Path $PSScriptRoot 'Payload'
$Parent = Split-Path -Parent $Destination
[IO.Directory]::CreateDirectory($Parent) | Out-Null
$Stage = Join-Path $Parent ('.kairo-stage-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $Stage -ErrorAction Stop | Out-Null
try {
    $Records = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'payload-hashes.json') -Raw | ConvertFrom-Json
    foreach ($Record in $Records) {
        $Name = [string]$Record.path
        if ($Name -notmatch '^[A-Za-z0-9_. /-]+$' -or ($Name.Split('/') | Where-Object { $_ -in @('', '.', '..') })) { throw 'Unsafe payload path' }
        $File = Join-Path $Source $Name
        if ((Get-Item -LiteralPath $File).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Linked payload' }
        if ((Get-FileHash -LiteralPath $File -Algorithm SHA256).Hash.ToLowerInvariant() -ne $Record.sha256) { throw 'Payload hash mismatch' }
        $Target = Join-Path $Stage $Name
        [IO.Directory]::CreateDirectory((Split-Path -Parent $Target)) | Out-Null
        Copy-Item -LiteralPath $File -Destination $Target
        if ((Get-FileHash -LiteralPath $Target -Algorithm SHA256).Hash.ToLowerInvariant() -ne $Record.sha256) { throw 'Copied hash mismatch' }
    }
    [IO.Directory]::Move($Stage, $Destination)
} finally { if (Test-Path -LiteralPath $Stage) { Remove-Item -LiteralPath $Stage -Recurse -Force } }
Write-Output "Installed: $Destination"
'''


def write_mac_launcher(app):
    contents = Path(app) / 'Contents'
    executable = contents / 'MacOS/SignalVault'
    executable.parent.mkdir(parents=True, exist_ok=True)
    executable.write_text(MAC_LAUNCHER, encoding='utf-8', newline='\n')
    executable.chmod(0o755)
    with (contents / 'Info.plist').open('wb') as stream:
        plistlib.dump({'CFBundleIdentifier': 'org.kairo.signalvault', 'CFBundleName': 'Signal Vault',
                       'CFBundleExecutable': 'SignalVault', 'CFBundlePackageType': 'APPL',
                       'CFBundleShortVersionString': '0.1.0', 'CFBundleVersion': '1',
                       'NSHighResolutionCapable': True}, stream)
    return executable
