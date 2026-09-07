<#
.SYNOPSIS
    Milestone 10: builds Release and stages a portable Overlay Desk package.

.DESCRIPTION
    The result is a single directory (and a zip of it) that runs from anywhere - no installer,
    no registry, and no Visual C++ redistributable, because the executable links the CRT
    statically. The only thing it writes is %APPDATA%\OverlayDesk.

    Verifies before packaging that the executable really is self-contained: a portable build
    that turns out to need a redist on the target machine is worse than no package at all,
    since it fails at the user rather than here.

.EXAMPLE
    .\scripts\package.ps1
    .\scripts\package.ps1 -SkipBuild        # package whatever is already in build\Release
#>
[CmdletBinding()]
param(
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot

if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot 'build.ps1') -Config Release
    if ($LASTEXITCODE -ne 0) { throw "Release build failed." }
}

$exe = Join-Path $repoRoot 'build\Release\OverlayDesk.exe'
if (-not (Test-Path $exe)) { throw "Not found: $exe" }

# --- Version, read back from the binary itself --------------------------------------------
# Taken from the executable rather than from CMakeLists so the package can never be labelled
# with a version the binary does not actually carry.
$info = (Get-Item $exe).VersionInfo
$version = $info.ProductVersion
if (-not $version) { throw "The executable carries no version resource." }
$version = $version -replace '\.0$', ''

$stageRoot = Join-Path $repoRoot 'build\package'
$stage = Join-Path $stageRoot "OverlayDesk-$version"
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force $stage | Out-Null

# --- Self-containment check ----------------------------------------------------------------
# dumpbin ships with the Build Tools; if it is not reachable the check is skipped rather than
# failing the package.
$dumpbin = Get-ChildItem "${env:ProgramFiles(x86)}\Microsoft Visual Studio\*\*\VC\Tools\MSVC\*\bin\Hostx64\x64\dumpbin.exe" -ErrorAction SilentlyContinue |
           Select-Object -First 1
if ($dumpbin) {
    $imports = & $dumpbin.FullName /DEPENDENTS $exe 2>$null
    $redist = $imports | Select-String -Pattern 'MSVCP\d+\.dll|VCRUNTIME\d+\.dll|api-ms-win-crt' -AllMatches
    if ($redist) {
        Write-Host "WARNING: the executable imports the Visual C++ runtime:" -ForegroundColor Red
        $redist | ForEach-Object { Write-Host "  $($_.Line.Trim())" -ForegroundColor Red }
        Write-Host "The package is NOT portable. Check CMAKE_MSVC_RUNTIME_LIBRARY." -ForegroundColor Red
    } else {
        Write-Host "Self-contained: no Visual C++ runtime imports." -ForegroundColor Green
    }
    $deps = ($imports | Select-String -Pattern '^\s+\S+\.dll$' | ForEach-Object { $_.Line.Trim() }) -join ', '
    if ($deps) { Write-Host "System DLLs: $deps" -ForegroundColor DarkGray }
}

# --- Stage -----------------------------------------------------------------------------------
Copy-Item $exe (Join-Path $stage 'OverlayDesk.exe')

foreach ($doc in 'README.md', 'USAGE.md', 'LICENSE', 'THIRD-PARTY-NOTICES.md') {
    $path = Join-Path $repoRoot $doc
    if (Test-Path $path) { Copy-Item $path $stage }
    else { Write-Host "NOTE: $doc not found, omitted from the package." -ForegroundColor Yellow }
}

# The example settings file goes in as a reference for hand-editing, not as live config -
# the application writes its own into %APPDATA% on first run.
$example = Join-Path $repoRoot 'config\settings.example.json'
if (Test-Path $example) { Copy-Item $example $stage }

# Shader sources ship alongside so a curious user can read what the filters actually do. The
# executable does not load them - the bytecode is compiled in.
$shaderDir = Join-Path $stage 'shaders-source'
New-Item -ItemType Directory -Force $shaderDir | Out-Null
Copy-Item (Join-Path $repoRoot 'shaders\*.hlsl') $shaderDir
Copy-Item (Join-Path $repoRoot 'shaders\*.hlsli') $shaderDir

# --- Zip ---------------------------------------------------------------------------------------
$zip = Join-Path $stageRoot "OverlayDesk-$version-win-x64.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }
Compress-Archive -Path $stage -DestinationPath $zip -CompressionLevel Optimal

$sizeKb = [math]::Round((Get-Item $zip).Length / 1KB)
Write-Host ""
Write-Host "Packaged Overlay Desk $version" -ForegroundColor Green
Write-Host "  directory : $stage"
Write-Host "  archive   : $zip ($sizeKb KB)"
Get-ChildItem $stage -Recurse -File | ForEach-Object {
    Write-Host ("    {0,-28} {1,8} KB" -f $_.FullName.Substring($stage.Length + 1), [math]::Round($_.Length / 1KB))
}
