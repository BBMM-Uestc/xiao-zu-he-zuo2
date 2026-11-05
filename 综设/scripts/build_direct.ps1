# Fallback build: call cl.exe directly (no CMake configure / try_compile).
[CmdletBinding()]
param(
    [switch]$NoTests
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

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
    throw 'Visual Studio 2022 was not found. Set VSINSTALLDIR.'
}

$vs = Find-VisualStudio
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
$out = Join-Path $root 'build\manual'
New-Item -ItemType Directory -Force -Path $out | Out-Null

Get-ChildItem -LiteralPath $out -Filter '*.obj' -ErrorAction SilentlyContinue | Remove-Item -Force
Get-ChildItem -LiteralPath $out -Filter '*.exe' -ErrorAction SilentlyContinue | Remove-Item -Force
Get-ChildItem -LiteralPath $out -Filter '*.pdb' -ErrorAction SilentlyContinue | Remove-Item -Force
Get-ChildItem -LiteralPath $out -Filter '*.ilk' -ErrorAction SilentlyContinue | Remove-Item -Force

$sources = @(
    'model\src\logic_value.cpp'
    'model\src\circuit.cpp'
    'model\src\document.cpp'
    'component\src\geometry.cpp'
    'component\src\registry.cpp'
    'component\src\builtin.cpp'
    'sim\src\netlist.cpp'
    'sim\src\engine.cpp'
    'sim\src\waveform.cpp'
)

if (-not $NoTests) {
    $sources += @(
        'tests\test_main.cpp'
        'tests\logic_value_test.cpp'
        'tests\netlist_test.cpp'
        'tests\engine_test.cpp'
        'tests\sequential_test.cpp'
        'tests\diagnostic_test.cpp'
        'tests\waveform_test.cpp'
        'tests\subcircuit_test.cpp'
        'tests\stress_test.cpp'
    )
}

Push-Location $root
try {
    $compileArgs = '/nologo /std:c++20 /EHsc /utf-8 /W4 /permissive- /Zc:__cplusplus ' +
        '/I model\include /I component\include /I sim\include /I tests /c ' +
        ($sources -join ' ') + ' /Fo:build\manual\'

    Write-Host '== Compile with cl.exe'
    cmd /d /s /c "call `"$vcvars`" >nul && cl $compileArgs"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    if ($NoTests) { return }

    Write-Host '== Link'
    cmd /d /s /c "call `"$vcvars`" >nul && cl /nologo /Fe:build\manual\ll_tests.exe build\manual\*.obj /link /SUBSYSTEM:CONSOLE /DEBUG:NONE /INCREMENTAL:NO"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    Write-Host '== Run tests'
    & (Join-Path $out 'll_tests.exe')
    exit $LASTEXITCODE
} finally {
    Pop-Location
}
