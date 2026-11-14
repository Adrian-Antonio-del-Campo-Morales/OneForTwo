[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Executable,

    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Assert-Condition {
    param(
        [bool]$Condition,
        [string]$Message
    )
    if (-not $Condition) {
        throw $Message
    }
}

function New-SolidPng {
    param(
        [string]$Path,
        [int]$Size
    )
    $bitmap = New-Object System.Drawing.Bitmap($Size, $Size)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.Clear([System.Drawing.Color]::FromArgb(0, 120, 212))
    } finally {
        $graphics.Dispose()
    }
    try {
        $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $bitmap.Dispose()
    }
}

$manifestSource = Join-Path $PSScriptRoot "AppxManifest.xml"
$output = [System.IO.Path]::GetFullPath($OutputDirectory)
$layout = Join-Path $output "layout"
$assets = Join-Path $layout "Assets"
$manifestPath = Join-Path $layout "AppxManifest.xml"
$msixPath = Join-Path $output "OneInTwo-NativeCheck.msix"
$cerPath = Join-Path $output "OneInTwo-NativeCheck.cer"
$pfxPath = $null
$certificate = $null

try {
    Assert-Condition (Test-Path -LiteralPath $manifestSource) "Falta AppxManifest.xml junto al script."
    $executablePath = (Resolve-Path -LiteralPath $Executable).Path
    $exeName = Split-Path -Leaf $executablePath
    Assert-Condition ($exeName -eq "oneintwo_notifications_native_windows.exe") `
        "El ejecutable no es el recorrido nativo esperado: $exeName"

    if (Test-Path -LiteralPath $output) {
        Remove-Item -LiteralPath $output -Recurse -Force
    }
    New-Item -ItemType Directory -Path $assets -Force | Out-Null
    Copy-Item -LiteralPath $manifestSource -Destination $manifestPath
    Copy-Item -LiteralPath $executablePath -Destination (Join-Path $layout $exeName)

    Add-Type -AssemblyName System.Drawing
    New-SolidPng -Path (Join-Path $assets "StoreLogo.png") -Size 50
    New-SolidPng -Path (Join-Path $assets "Square150x150Logo.png") -Size 150
    New-SolidPng -Path (Join-Path $assets "Square44x44Logo.png") -Size 44

    [xml]$manifest = Get-Content -LiteralPath $manifestPath -Raw
    $package = $manifest.DocumentElement
    Assert-Condition ($package.LocalName -eq "Package") "El manifiesto no tiene un elemento Package."

    $identity = $package.SelectSingleNode("*[local-name()='Identity']")
    Assert-Condition ($null -ne $identity) "El manifiesto no declara Identity."
    $packageName = $identity.GetAttribute("Name")
    $publisher = $identity.GetAttribute("Publisher")
    $version = $identity.GetAttribute("Version")
    $architecture = $identity.GetAttribute("ProcessorArchitecture")
    Assert-Condition ($packageName -eq "OneInTwo.Notifications.NativeCheck") `
        "Identidad de paquete inesperada: $packageName"
    Assert-Condition ($publisher -eq "CN=OneInTwo Development") `
        "Publisher inesperado: $publisher"
    Assert-Condition ($version -match '^\d+\.\d+\.\d+\.\d+$') "Versión MSIX inválida: $version"
    Assert-Condition ($architecture -eq "x64") "Arquitectura MSIX inesperada: $architecture"

    $capabilities = @($package.SelectNodes("*[local-name()='Capabilities']/*"))
    $listener = @($capabilities | Where-Object { $_.GetAttribute("Name") -eq "userNotificationListener" })
    Assert-Condition ($listener.Count -eq 1) "Falta la capacidad userNotificationListener."
    Assert-Condition ($listener[0].NamespaceURI -eq "http://schemas.microsoft.com/appx/manifest/uap/windows10/3") `
        "userNotificationListener debe declararse en el namespace uap3."
    $fullTrust = @($capabilities | Where-Object { $_.GetAttribute("Name") -eq "runFullTrust" })
    Assert-Condition ($fullTrust.Count -eq 1) "Falta la capacidad runFullTrust."
    Assert-Condition ($fullTrust[0].NamespaceURI -eq "http://schemas.microsoft.com/appx/manifest/foundation/windows10/restrictedcapabilities") `
        "runFullTrust debe declararse en el namespace rescap."

    $applications = @($package.SelectNodes("*[local-name()='Applications']/*[local-name()='Application']"))
    Assert-Condition ($applications.Count -eq 1) "El manifiesto debe declarar una sola aplicación."
    $application = $applications[0]
    Assert-Condition ($application.GetAttribute("Executable") -eq $exeName) `
        "El ejecutable del manifiesto no coincide con $exeName."
    Assert-Condition ($application.GetAttribute("EntryPoint") -eq "Windows.FullTrustApplication") `
        "La aplicación debe usar Windows.FullTrustApplication."
    $visual = $application.SelectSingleNode("*[local-name()='VisualElements']")
    Assert-Condition ($null -ne $visual) "Faltan los elementos visuales de la aplicación."
    foreach ($attribute in @("Square150x150Logo", "Square44x44Logo")) {
        $relative = $visual.GetAttribute($attribute)
        Assert-Condition (-not [string]::IsNullOrWhiteSpace($relative)) "Falta $attribute."
        Assert-Condition (Test-Path -LiteralPath (Join-Path $layout $relative)) `
            "No existe el recurso $relative."
    }
    $storeLogo = $package.SelectSingleNode("*[local-name()='Properties']/*[local-name()='Logo']")
    Assert-Condition ($null -ne $storeLogo -and -not [string]::IsNullOrWhiteSpace($storeLogo.InnerText)) `
        "Falta el logotipo de la ficha de la tienda."
    Assert-Condition (Test-Path -LiteralPath (Join-Path $layout $storeLogo.InnerText)) `
        "No existe el recurso $($storeLogo.InnerText)."

    $family = $package.SelectSingleNode("*[local-name()='Dependencies']/*[local-name()='TargetDeviceFamily']")
    Assert-Condition ($null -ne $family) "Falta TargetDeviceFamily."
    Assert-Condition ($family.GetAttribute("Name") -eq "Windows.Desktop") `
        "TargetDeviceFamily debe ser Windows.Desktop."

    $sdkRoot = Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10\bin"
    Assert-Condition (Test-Path -LiteralPath $sdkRoot) "No se encontró el SDK de Windows."
    $sdk = Get-ChildItem -LiteralPath $sdkRoot -Directory |
        Where-Object {
            $_.Name -match '^\d+(\.\d+){1,3}$' -and
            (Test-Path -LiteralPath (Join-Path $_.FullName "x64\makeappx.exe"))
        } |
        Sort-Object { [version]$_.Name } -Descending |
        Select-Object -First 1
    Assert-Condition ($null -ne $sdk) "No se encontró MakeAppx.exe del SDK."
    $makeappx = Join-Path $sdk.FullName "x64\makeappx.exe"
    $signtool = Join-Path $sdk.FullName "x64\signtool.exe"
    Assert-Condition (Test-Path -LiteralPath $signtool) "No se encontró signtool.exe del SDK."

    Write-Host "Empaquetando $packageName $version ($architecture)"
    & $makeappx pack /d $layout /p $msixPath /o 2>&1 |
        Tee-Object -FilePath (Join-Path $output "makeappx-pack.log")
    Assert-Condition ($LASTEXITCODE -eq 0) "MakeAppx pack falló con código $LASTEXITCODE."

    $password = [Guid]::NewGuid().ToString("N")
    $securePassword = ConvertTo-SecureString -String $password -Force -AsPlainText
    $certificate = New-SelfSignedCertificate `
        -Type Custom `
        -Subject $publisher `
        -KeyUsage DigitalSignature `
        -KeyExportPolicy Exportable `
        -KeyAlgorithm RSA `
        -KeyLength 2048 `
        -HashAlgorithm SHA256 `
        -FriendlyName "OneInTwo Native Check (CI)" `
        -CertStoreLocation "Cert:\CurrentUser\My" `
        -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3", "2.5.29.19={text}")
    $pfxPath = Join-Path $output "OneInTwo-NativeCheck.pfx"
    Export-PfxCertificate -Cert $certificate -FilePath $pfxPath -Password $securePassword | Out-Null
    Export-Certificate -Cert $certificate -FilePath $cerPath | Out-Null

    & $signtool sign /fd SHA256 /f $pfxPath /p $password $msixPath 2>&1 |
        Tee-Object -FilePath (Join-Path $output "signtool-sign.log")
    Assert-Condition ($LASTEXITCODE -eq 0) "signtool sign falló con código $LASTEXITCODE."

    # El certificado es efímero de comprobación; se confía solo en este runner.
    Import-Certificate -FilePath $cerPath -CertStoreLocation "Cert:\CurrentUser\TrustedPeople" | Out-Null
    Import-Certificate -FilePath $cerPath -CertStoreLocation "Cert:\CurrentUser\Root" | Out-Null
    & $signtool verify /pa /v $msixPath 2>&1 |
        Tee-Object -FilePath (Join-Path $output "signtool-verify.log")
    Assert-Condition ($LASTEXITCODE -eq 0) "signtool verify falló con código $LASTEXITCODE."

    $unpacked = Join-Path $output "unpacked"
    & $makeappx unpack /p $msixPath /d $unpacked /o 2>&1 |
        Tee-Object -FilePath (Join-Path $output "makeappx-unpack.log")
    Assert-Condition ($LASTEXITCODE -eq 0) "MakeAppx unpack falló con código $LASTEXITCODE."
    Assert-Condition (Test-Path -LiteralPath (Join-Path $unpacked "AppxManifest.xml")) `
        "El MSIX no contiene AppxManifest.xml."
    Assert-Condition (Test-Path -LiteralPath (Join-Path $unpacked $exeName)) `
        "El MSIX no contiene $exeName."

    $hash = (Get-FileHash -LiteralPath $msixPath -Algorithm SHA256).Hash
    $validation = @(
        "runner=$env:ImageOS"
        "package=$packageName"
        "publisher=$publisher"
        "version=$version"
        "architecture=$architecture"
        "capability=userNotificationListener"
        "capability=runFullTrust"
        "executable=$exeName"
        "msix=$([System.IO.Path]::GetFileName($msixPath))"
        "msix_sha256=$hash"
        "certificate_thumbprint=$($certificate.Thumbprint)"
        "signature=verified"
        "unpacked=ok"
        "result=ok"
    )
    $validation | Set-Content -LiteralPath (Join-Path $output "validation.txt") -Encoding UTF8

    $install = @'
OneInTwo Native Notifications Check
==================================

Paquete MSIX de comprobación, no de distribución. El certificado es autofirmado
y solo sirve para probar esta compilación.

1. Confiar en el certificado (sin elevación, tienda del usuario actual):
   Import-Certificate -FilePath OneInTwo-NativeCheck.cer -CertStoreLocation Cert:\CurrentUser\TrustedPeople

2. Instalar el paquete:
   Add-AppxPackage -Path OneInTwo-NativeCheck.msix

3. Abrir la comprobación nativa:
   $pkg = Get-AppxPackage OneInTwo.Notifications.NativeCheck
   Start-Process "shell:AppsFolder\$($pkg.PackageFamilyName)!NativeCheck"

4. Aceptar el consentimiento de acceso a notificaciones y seguir la lista de
   comprobación de notification_support/README.md.

Para retirar la compilación:
   Get-AppxPackage OneInTwo.Notifications.NativeCheck | Remove-AppxPackage
'@
    Set-Content -LiteralPath (Join-Path $output "install.txt") -Value $install -Encoding UTF8

    Write-Host "MSIX validado y firmado: $msixPath"
} catch {
    Write-Error $_
    exit 1
} finally {
    if ($pfxPath -and (Test-Path -LiteralPath $pfxPath)) {
        Remove-Item -LiteralPath $pfxPath -Force -ErrorAction SilentlyContinue
    }
    if ($certificate) {
        Remove-Item -LiteralPath "Cert:\CurrentUser\My\$($certificate.Thumbprint)" -Force -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath "Cert:\CurrentUser\TrustedPeople\$($certificate.Thumbprint)" -Force -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath "Cert:\CurrentUser\Root\$($certificate.Thumbprint)" -Force -ErrorAction SilentlyContinue
    }
}
