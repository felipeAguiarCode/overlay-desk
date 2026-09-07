<#
.SYNOPSIS
    Configures and builds Overlay Desk.

.DESCRIPTION
    The CMake that ships inside Visual Studio Build Tools is not on PATH, so this script
    locates it through vswhere and drives the configure/build/test cycle with it.

.EXAMPLE
    .\scripts\build.ps1                    # Release build
    .\scripts\build.ps1 -Config Debug      # Debug build
    .\scripts\build.ps1 -Test              # build then run CTest
    .\scripts\build.ps1 -Clean             # wipe the build directory first
#>
[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Config = 'Release',
    [switch]$Test,
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot

function Find-Cmake {
    $onPath = Get-Command cmake -ErrorAction SilentlyContinue
    if ($onPath) { return $onPath.Source }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) {
        throw 'Neither cmake nor vswhere.exe was found. Install Visual Studio Build Tools with the C++ workload.'
    }

    $vsRoot = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vsRoot) {
        $vsRoot = & $vswhere -latest -products * -property installationPath
    }
    if (-not $vsRoot) { throw 'No Visual Studio installation found.' }

    $candidate = Join-Path $vsRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
    if (-not (Test-Path $candidate)) {
        throw "CMake was not found at $candidate. Install the 'C++ CMake tools for Windows' component."
    }
    return $candidate
}

$cmake = Find-Cmake
Write-Host "cmake: $cmake" -ForegroundColor DarkGray

$buildDir = Join-Path $repoRoot 'build'
if ($Clean -and (Test-Path $buildDir)) {
    Write-Host "Removing $buildDir" -ForegroundColor DarkGray
    Remove-Item -Recurse -Force $buildDir
}

& $cmake -S $repoRoot --preset vs
if ($LASTEXITCODE -ne 0) { throw "Configure failed ($LASTEXITCODE)." }

& $cmake --build $buildDir --config $Config
if ($LASTEXITCODE -ne 0) { throw "Build failed ($LASTEXITCODE)." }

if ($Test) {
    $ctest = Join-Path (Split-Path -Parent $cmake) 'ctest.exe'
    & $ctest --test-dir $buildDir -C $Config --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "Tests failed ($LASTEXITCODE)." }
}

$exe = Join-Path $buildDir "$Config\OverlayDesk.exe"
if (Test-Path $exe) {
    Write-Host "`nBuilt: $exe" -ForegroundColor Green
}
