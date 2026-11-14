$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$sourceDir = Join-Path $repoRoot 'cmake\capture_windows'
$buildRoot = Join-Path $repoRoot 'build\cmake-guards'
$fakeInclude = Join-Path $PSScriptRoot 'wgc_negative_include'

if (Test-Path -LiteralPath $buildRoot) {
    Remove-Item -LiteralPath $buildRoot -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $buildRoot | Out-Null

if (-not (Test-Path -LiteralPath (Join-Path $fakeInclude 'windows.graphics.capture.h'))) {
    throw "Falta la cabecera falsa en $fakeInclude"
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
        throw "El caso '$Name' configuró correctamente; se esperaba un error."
    }
    if ($output -notmatch [regex]::Escape($ExpectedText)) {
        throw "El caso '$Name' falló sin el mensaje esperado: '$ExpectedText'"
    }
    Write-Host "OK: '$Name' falló con el mensaje esperado."
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
    -ExpectedText 'OneInTwo requiere Windows SDK 10.0.19041 o posterior'

$missingApiArgs = $common + @(
    '-A', 'x64',
    '-B', (Join-Path $buildRoot 'api-missing'),
    "-DCMAKE_CXX_FLAGS=/I$fakeInclude",
    '-DONEINTWO_REQUIRE_WGC=ON'
)
Invoke-ExpectedFailure -Name 'api-missing' -CMakeArguments $missingApiArgs `
    -ExpectedText 'IGraphicsCaptureSession2/IsCursorCaptureEnabled'

Write-Host 'CMake guard negative checks passed.'
