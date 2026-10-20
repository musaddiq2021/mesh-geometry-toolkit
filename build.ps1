[CmdletBinding()]
param(
    [string]$Compiler = '',
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug'
)

$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
$buildRoot = Join-Path $projectRoot ('build/' + $Configuration.ToLowerInvariant())
$tempRoot = Join-Path $buildRoot 'tmp'
New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null

if (-not $Compiler) {
    $foundCompiler = Get-Command 'g++.exe' -ErrorAction SilentlyContinue
    if ($foundCompiler) { $Compiler = $foundCompiler.Source }
    else { throw 'A C++20 GCC compiler is required. Pass -Compiler with its path; no tools are installed by this script.' }
}
$Compiler = (Get-Item -LiteralPath $Compiler).FullName
$previousPath = $env:PATH
$previousTemp = $env:TEMP
$previousTmp = $env:TMP
$env:PATH = (Split-Path -Parent $Compiler) + ';' + $env:PATH
$env:TEMP = $tempRoot
$env:TMP = $tempRoot

function Invoke-Checked {
    param([string]$Program, [string[]]$Arguments)
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed with exit code $LASTEXITCODE" }
}

try {
    $flags = @('-std=c++20', '-Wall', '-Wextra', '-Wpedantic', '-Wconversion', '-Wshadow', '-Werror',
               '-I', (Join-Path $projectRoot 'include'))
    if ($Configuration -eq 'Debug') { $flags += @('-O0', '-g', '-D_GLIBCXX_ASSERTIONS') }
    else { $flags += @('-O2', '-DNDEBUG') }
    $targetTriple = & $Compiler -dumpmachine
    if ($LASTEXITCODE -ne 0) { throw 'Could not inspect the GCC target' }
    if ($targetTriple -match 'mingw') { $flags += @('-static', '-static-libgcc', '-static-libstdc++') }
    $librarySources = @('geometry.cpp', 'obj.cpp', 'triangulation.cpp', 'volume.cpp') | ForEach-Object { Join-Path $projectRoot ('src/' + $_) }
    $tool = Join-Path $buildRoot 'mesh-tool.exe'
    $tests = Join-Path $buildRoot 'mesh-tests.exe'
    Invoke-Checked $Compiler ($flags + $librarySources + @((Join-Path $projectRoot 'src/main.cpp'), '-o', $tool))
    Invoke-Checked $Compiler ($flags + $librarySources + @((Join-Path $projectRoot 'tests/test_main.cpp'), '-o', $tests))
    Invoke-Checked $tests @()
    Invoke-Checked $tool @('--help')
    $example = Join-Path $projectRoot 'examples/tetrahedron.obj'
    $summary = & $tool inspect $example
    if ($LASTEXITCODE -ne 0 -or $summary -notcontains 'Triangles: 4' -or
        $summary -notcontains 'Closed with consistent edge winding: yes') {
        throw 'CLI inspection did not report the expected tetrahedron topology'
    }
    $summary | Write-Output
    $roundTrip = Join-Path $buildRoot 'round-trip.obj'
    Invoke-Checked $tool @('convert', $example, $roundTrip)
    $roundTripSummary = & $tool inspect $roundTrip
    if ($LASTEXITCODE -ne 0 -or (Compare-Object $summary $roundTripSummary)) {
        throw 'CLI file round trip changed mesh inspection results'
    }
    $invalidInput = Join-Path $projectRoot 'tests/fixtures/invalid.obj'
    # Capture native stderr via a process so PowerShell 5 does not convert it into
    # a terminating error before we can assert the program's exit status.
    function Expect-Failure {
        param([string[]]$Arguments, [int]$ExpectedExit)
        $start = New-Object System.Diagnostics.ProcessStartInfo
        $start.FileName = $tool
        $start.Arguments = (($Arguments | ForEach-Object { '"' + $_ + '"' }) -join ' ')
        $start.UseShellExecute = $false
        $start.CreateNoWindow = $true
        $start.RedirectStandardError = $true
        $start.RedirectStandardOutput = $true
        $process = [System.Diagnostics.Process]::Start($start)
        try {
            $errorText = $process.StandardError.ReadToEnd()
            $outputText = $process.StandardOutput.ReadToEnd()
            $process.WaitForExit()
            if ($process.ExitCode -ne $ExpectedExit -or -not $errorText) {
                throw "Expected CLI exit $ExpectedExit and diagnostic; got $($process.ExitCode): $outputText $errorText"
            }
        } finally { $process.Dispose() }
    }
    Expect-Failure @('inspect', $invalidInput) 1
    Expect-Failure @('inspect', (Join-Path $buildRoot 'missing.obj')) 1
    Expect-Failure @('unsupported-command') 2
    # Invalid conversion must leave an existing destination intact.
    $before = [System.IO.File]::ReadAllText($roundTrip)
    Expect-Failure @('convert', $invalidInput, $roundTrip) 1
    if ([System.IO.File]::ReadAllText($roundTrip) -cne $before) { throw 'Invalid input changed the destination file' }
    Write-Output "Build and CLI integration checks passed ($Configuration). Artifacts: $buildRoot"
} finally {
    $env:PATH = $previousPath
    $env:TEMP = $previousTemp
    $env:TMP = $previousTmp
}
