$ErrorActionPreference = 'Stop'
# These CMake cases are expected to fail. Continue so the script can check
# the exit code and diagnostic.
$PSNativeCommandUseErrorActionPreference = $false

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$sourceDir = Join-Path $repoRoot 'cmake\capture_windows'
$buildRoot = Join-Path $repoRoot 'build\cmake-guards'
$fakeInclude = Join-Path $PSScriptRoot 'wgc_negative_include'

if (Test-Path -LiteralPath $buildRoot) {
    Remove-Item -LiteralPath $buildRoot -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $buildRoot | Out-Null

if (-not (Test-Path -LiteralPath (Join-Path $fakeInclude 'windows.graphics.capture.h'))) {
    throw "The test header is missing from $fakeInclude"
}

function Stop-WithDiagnostic {
    param([Parameter(Mandatory = $true)][string]$Message)
    $encoded = $Message -replace "`r", "" -replace "`n", "%0A"
    Write-Output "::error title=CMake guard failed::$encoded"
    throw $Message
}

function Invoke-ExpectedFailure {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string[]]$CMakeArguments,
        [Parameter(Mandatory = $true)][string]$ExpectedText
    )

    $log = Join-Path $buildRoot "$Name.log"
    & cmake @CMakeArguments *> $log
    $exitCode = $LASTEXITCODE
    $output = Get-Content -LiteralPath $log -Raw
    Write-Host $output

    if ($exitCode -eq 0) {
        Stop-WithDiagnostic "The '$Name' case configured successfully. An error was expected.`n$output"
    }
    if ($output -notmatch [regex]::Escape($ExpectedText)) {
        Stop-WithDiagnostic "The '$Name' case failed without the expected message: '$ExpectedText'. Output:`n$output"
    }
    Write-Host "OK: '$Name' failed with the expected message."
}

$common = @(
    '-S', $sourceDir,
    '-G', 'Visual Studio 17 2022',
    '-DONEINTWO_CAPTURE_QT=OFF',
    '-DONEINTWO_BUILD_TESTS=OFF'
)

$oldSdkArgs = $common + @(
    '-A', 'x64,version=10.0.17763.0',
    '-B', (Join-Path $buildRoot 'sdk-old')
)
Invoke-ExpectedFailure -Name 'sdk-old' -CMakeArguments $oldSdkArgs `
    -ExpectedText 'OneInTwo requires Windows SDK 10.0.19041 or later'

$missingApiArgs = $common + @(
    '-A', 'x64',
    '-B', (Join-Path $buildRoot 'api-missing'),
    "-DCMAKE_CXX_FLAGS=/I$fakeInclude",
    '-DONEINTWO_REQUIRE_WGC=ON'
)
Invoke-ExpectedFailure -Name 'api-missing' -CMakeArguments $missingApiArgs `
    -ExpectedText 'IGraphicsCaptureSession2/IsCursorCaptureEnabled'

Write-Host 'CMake guard negative checks passed.'
# The expected CMake failures leave $LASTEXITCODE at 1. Report guard success explicitly.
exit 0
