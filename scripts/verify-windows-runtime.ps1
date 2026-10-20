[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Executable,
    [string]$ObjDump = ''
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not $ObjDump) {
    $tool = Get-Command objdump.exe -ErrorAction SilentlyContinue
    if ($tool) { $ObjDump = $tool.Source }
    else { throw 'Pass -ObjDump with the existing MinGW binutils executable path.' }
}
$executablePath = (Get-Item -LiteralPath $Executable).FullName
$objDumpPath = (Get-Item -LiteralPath $ObjDump).FullName

# The inspection tool is invoked from its existing toolchain installation;
# the child executable below gets a separate environment with OS paths only.
$peDetails = & $objDumpPath -p $executablePath
if ($LASTEXITCODE -ne 0) { throw 'Could not inspect executable PE dependencies' }
$imports = @($peDetails | ForEach-Object {
    if ($_ -match 'DLL Name:\s*(\S+)') { $Matches[1] }
} | Sort-Object -Unique)
if ($imports.Count -eq 0) { throw 'PE inspection did not find any imports' }
foreach ($dll in $imports) {
    if ($dll -notmatch '^(KERNEL32\.dll|api-ms-win-crt-[a-z0-9-]+\.dll|ucrtbase\.dll|msvcrt\.dll)$') {
        throw "Unexpected PE import outside the verified Windows/UCRT set: $dll"
    }
}

# Fresh directory: do not let DLLs left next to the build artifact hide a leak.
# No files are removed, and all generated files remain under the project root.
$isolatedRoot = Join-Path $projectRoot ('build/deployment-check/' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $isolatedRoot -Force | Out-Null
$isolatedExe = Join-Path $isolatedRoot 'mesh-tool.exe'
Copy-Item -LiteralPath $executablePath -Destination $isolatedExe
$osRoot = [Environment]::GetEnvironmentVariable('SystemRoot')
if (-not $osRoot) { throw 'SystemRoot is required for the Windows runtime check' }
$cleanPath = (Join-Path $osRoot 'System32') + ';' + $osRoot

function Invoke-Isolated {
    param([string[]]$Arguments)
    $start = New-Object System.Diagnostics.ProcessStartInfo
    $start.FileName = $isolatedExe
    $start.WorkingDirectory = $isolatedRoot
    $start.Arguments = (($Arguments | ForEach-Object { '"' + $_ + '"' }) -join ' ')
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $start.EnvironmentVariables['PATH'] = $cleanPath
    $process = [System.Diagnostics.Process]::Start($start)
    try {
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit(30000)) {
            $process.Kill()
            throw 'Isolated executable did not exit within 30 seconds'
        }
        $outputText = $stdout.GetAwaiter().GetResult()
        $errorText = $stderr.GetAwaiter().GetResult()
        if ($process.ExitCode -ne 0 -or $errorText) {
            throw "Isolated executable failed (exit $($process.ExitCode)): $errorText"
        }
        return $outputText
    } finally { $process.Dispose() }
}

$version = Invoke-Isolated @('--version')
if ($version.Trim() -ne 'Mesh Geometry Toolkit 1.0.0') { throw 'Isolated version check failed' }
$report = Invoke-Isolated @('inspect', (Join-Path $projectRoot 'examples/cube.obj'))
if ($report -notmatch '(?m)^Surface area: 6\r?$' -or $report -notmatch '(?m)^Enclosed volume: 1\r?$') {
    throw 'Isolated cube analysis failed'
}
$converted = Join-Path $isolatedRoot 'converted cube.obj'
$null = Invoke-Isolated @('convert', (Join-Path $projectRoot 'examples/cube.obj'), $converted)
$roundTrip = Invoke-Isolated @('inspect', $converted)
if ($roundTrip -cne $report) { throw 'Isolated file round trip changed the cube report' }

@("Source executable: $executablePath", "Child PATH: $cleanPath", "PE imports: $($imports -join ', ')",
  'PASS: isolated version, cube area 6 / volume 1, conversion and reinspection; no adjacent DLLs.') |
    Set-Content -LiteralPath (Join-Path $isolatedRoot 'verification.txt') -Encoding UTF8
Write-Output "Standalone Windows runtime check passed: $executablePath"
Write-Output "PE imports: $($imports -join ', ')"
Write-Output "Isolated run evidence: $isolatedRoot"
