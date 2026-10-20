[CmdletBinding()]
param(
    [string]$CMake = '',
    [string]$Compiler = '',
    [string]$Make = '',
    [switch]$Clean
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not $Compiler) {
    $tool = Get-Command g++.exe -ErrorAction SilentlyContinue
    if ($tool) { $Compiler = $tool.Source }
    else { throw 'Pass -Compiler with the path of an existing MinGW GCC compiler.' }
}
if (-not $Make) { $Make = Join-Path (Split-Path -Parent $Compiler) 'mingw32-make.exe' }
if (-not $CMake) {
    $installed = Get-Command cmake -ErrorAction SilentlyContinue
    if ($installed) { $CMake = $installed.Source }
    else { $CMake = Join-Path $projectRoot '.tools/cmake-3.31.6-windows-x86_64/bin/cmake.exe' }
}
$CMake = (Get-Item -LiteralPath $CMake).FullName
$ctest = Join-Path (Split-Path -Parent $CMake) 'ctest.exe'
$previousPath = $env:PATH
$previousTemp = $env:TEMP
$previousTmp = $env:TMP
$tempRoot = Join-Path $projectRoot 'build/tmp'
New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null
$env:PATH = (Split-Path -Parent $Compiler) + ';' + $env:PATH
$env:TEMP = $tempRoot
$env:TMP = $tempRoot
function Checked {
    param([string]$Program, [string[]]$Arguments)
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed with exit code $LASTEXITCODE" }
}
try {
    foreach ($configuration in @('Debug', 'Release')) {
        $buildRoot = Join-Path $projectRoot ('build/cmake-' + $configuration.ToLowerInvariant())
        Checked $CMake @('-S', $projectRoot, '-B', $buildRoot, '-G', 'MinGW Makefiles',
            "-DCMAKE_MAKE_PROGRAM=$Make", "-DCMAKE_CXX_COMPILER=$Compiler", "-DCMAKE_BUILD_TYPE=$configuration", '-DBUILD_TESTING=ON', '-DMESH_STATIC_MINGW_RUNTIME=ON')
        $buildArguments = @('--build', $buildRoot, '--parallel', '2')
        if ($Clean) { $buildArguments += '--clean-first' }
        Checked $CMake $buildArguments
        $buildPath = $env:PATH
        try {
            # Test application and test-runner startup without compiler DLL paths.
            $env:PATH = (Join-Path $env:SystemRoot 'System32') + ';' + $env:SystemRoot
            Checked $ctest @('--test-dir', $buildRoot, '--output-on-failure', '--no-tests=error')
        } finally { $env:PATH = $buildPath }
        & (Join-Path $PSScriptRoot 'verify-windows-runtime.ps1') -Executable (Join-Path $buildRoot 'mesh-tool.exe') `
            -ObjDump (Join-Path (Split-Path -Parent $Compiler) 'objdump.exe')
    }
    Write-Output 'CMake Debug and Release builds and CTest checks passed.'
} finally {
    $env:PATH = $previousPath
    $env:TEMP = $previousTemp
    $env:TMP = $previousTmp
}
