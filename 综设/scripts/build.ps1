# Build logisim-lite with Visual Studio 2022 + CMake + Ninja.
[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string]$Configuration = 'Release',
    [switch]$NoTests
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root 'build'

function Find-VisualStudio {
    if ($env:VSINSTALLDIR -and (Test-Path (Join-Path $env:VSINSTALLDIR 'VC\Auxiliary\Build\vcvars64.bat'))) {
        return $env:VSINSTALLDIR
    }
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path $vswhere) {
        $found = & $vswhere -latest -products * -property installationPath | Select-Object -First 1
        if ($found) { return $found.Trim() }
    }
    if (Test-Path 'D:\vs2022\VC\Auxiliary\Build\vcvars64.bat') { return 'D:\vs2022' }
    if (Test-Path 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat') {
        return 'C:\Program Files\Microsoft Visual Studio\2022\Community'
    }
    throw 'Visual Studio 2022 was not found. Set VSINSTALLDIR or edit scripts/build.ps1.'
}

$vs = Find-VisualStudio
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
$cmake = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
if (-not (Test-Path $cmake)) {
    $cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
    if (-not $cmakeCommand) { throw 'cmake was not found. Install CMake or use scripts/build_direct.ps1.' }
    $cmake = $cmakeCommand.Source
}

$ctest = Join-Path (Split-Path -Parent $cmake) 'ctest.exe'
if (-not (Test-Path $ctest)) {
    $ctestCommand = Get-Command ctest -ErrorAction SilentlyContinue
    if ($ctestCommand) { $ctest = $ctestCommand.Source }
}

$testsArg = if ($NoTests) { '-DLL_BUILD_TESTS=OFF' } else { '-DLL_BUILD_TESTS=ON' }
Write-Host "== VS      : $vs"
Write-Host "== CMake   : $cmake"
Write-Host "== Build   : $build ($Configuration)"

$command = "call `"$vcvars`" >nul && `"$cmake`" -S `"$root`" -B `"$build`" -G Ninja -DCMAKE_BUILD_TYPE=$Configuration $testsArg"
$command += " && `"$cmake`" --build `"$build`" --parallel"

if (-not $NoTests -and (Test-Path $ctest)) {
    $command += " && `"$ctest`" --test-dir `"$build`" --output-on-failure"
}

cmd /d /s /c $command
exit $LASTEXITCODE
