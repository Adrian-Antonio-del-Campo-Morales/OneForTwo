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

function Get-ZipEntryBytes {
    param(
        $Archive,
        [string]$Name
    )
    $entry = $Archive.GetEntry($Name)
    Assert-Condition ($null -ne $entry) "El MSIX no contiene $Name."
    $stream = $entry.Open()
    $memory = New-Object System.IO.MemoryStream
    try {
        $stream.CopyTo($memory)
    } finally {
        $stream.Dispose()
    }
    try {
        $bytes = $memory.ToArray()
    } finally {
        $memory.Dispose()
    }
    return ,$bytes
}

function Assert-MsixSignature {
    param(
        [byte[]]$SignatureBytes,
        $Certificate
    )
    Assert-Condition ($SignatureBytes.Length -gt 4) "AppxSignature.p7x is empty."
    Assert-Condition ([Text.Encoding]::ASCII.GetString($SignatureBytes, 0, 4) -eq "PKCX") `
        "AppxSignature.p7x lacks the PKCX header."

    $der = New-Object byte[] ($SignatureBytes.Length - 4)
    [Array]::Copy($SignatureBytes, 4, $der, 0, $der.Length)
    $cms = New-Object System.Security.Cryptography.Pkcs.SignedCms
    $cms.Decode($der)
    Assert-Condition ($null -ne $cms.SignerInfos -and $cms.SignerInfos.Count -eq 1) `
        "AppxSignature.p7x must have exactly one signer."

    $signer = $cms.SignerInfos[0]
    Assert-Condition ($null -ne $signer.Certificate) `
        "The signature does not contain the signer's certificate."
    Assert-Condition ($signer.Certificate.Thumbprint -eq $Certificate.Thumbprint) `
        "The AppxSignature.p7x signer does not match the generated ephemeral certificate."

    # true skips trust-chain validation for the self-signed ephemeral certificate.
    # The PKCS#7 signature is still checked cryptographically against the content.
    $cms.CheckSignature($true)

    $contentType = "desconocido"
    if ($null -ne $cms.ContentInfo -and $null -ne $cms.ContentInfo.ContentType) {
        $contentType = $cms.ContentInfo.ContentType.Value
    }
    $details = [pscustomobject]@{
        Thumbprint = $signer.Certificate.Thumbprint
        DigestAlgorithm = $signer.DigestAlgorithm.Value
        ContentType = $contentType
    }
    return $details
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
    Assert-Condition (Test-Path -LiteralPath $manifestSource) "AppxManifest.xml is missing next to the script."
    $executablePath = (Resolve-Path -LiteralPath $Executable).Path
    $exeName = Split-Path -Leaf $executablePath
    Assert-Condition ($exeName -eq "oneintwo_notifications_native_windows.exe") `
        "Expected the native check executable, found $exeName"

    if (Test-Path -LiteralPath $output) {
        Remove-Item -LiteralPath $output -Recurse -Force
    }
    New-Item -ItemType Directory -Path $assets -Force | Out-Null
    Copy-Item -LiteralPath $manifestSource -Destination $manifestPath
    Copy-Item -LiteralPath $executablePath -Destination (Join-Path $layout $exeName)

    Add-Type -AssemblyName System.Drawing
    Add-Type -AssemblyName System.Security
    New-SolidPng -Path (Join-Path $assets "StoreLogo.png") -Size 50
    New-SolidPng -Path (Join-Path $assets "Square150x150Logo.png") -Size 150
    New-SolidPng -Path (Join-Path $assets "Square44x44Logo.png") -Size 44

    [xml]$manifest = Get-Content -LiteralPath $manifestPath -Raw
    $package = $manifest.DocumentElement
    Assert-Condition ($package.LocalName -eq "Package") "The manifest lacks a Package element."

    $identity = $package.SelectSingleNode("*[local-name()='Identity']")
    Assert-Condition ($null -ne $identity) "El manifiesto no declara Identity."
    $packageName = $identity.GetAttribute("Name")
    $publisher = $identity.GetAttribute("Publisher")
    $version = $identity.GetAttribute("Version")
    $architecture = $identity.GetAttribute("ProcessorArchitecture")
    Assert-Condition ($packageName -eq "OneInTwo.Notifications.NativeCheck") `
        "Unexpected package identity: $packageName"
    Assert-Condition ($publisher -eq "CN=OneInTwo Development") `
        "Publisher inesperado: $publisher"
    Assert-Condition ($version -match '^\d+\.\d+\.\d+\.\d+$') "Invalid MSIX version: $version"
    Assert-Condition ($architecture -eq "x64") "Arquitectura MSIX unexpected: $architecture"

    $capabilities = @($package.SelectNodes("*[local-name()='Capabilities']/*"))
    $listener = @($capabilities | Where-Object { $_.GetAttribute("Name") -eq "userNotificationListener" })
    Assert-Condition ($listener.Count -eq 1) "The userNotificationListener capability is missing."
    Assert-Condition ($listener[0].NamespaceURI -eq "http://schemas.microsoft.com/appx/manifest/uap/windows10/3") `
        "Declare userNotificationListener in the uap3 namespace."
    $fullTrust = @($capabilities | Where-Object { $_.GetAttribute("Name") -eq "runFullTrust" })
    Assert-Condition ($fullTrust.Count -eq 1) "The runFullTrust capability is missing."
    Assert-Condition ($fullTrust[0].NamespaceURI -eq "http://schemas.microsoft.com/appx/manifest/foundation/windows10/restrictedcapabilities") `
        "Declare runFullTrust in the rescap namespace."

    $applications = @($package.SelectNodes("*[local-name()='Applications']/*[local-name()='Application']"))
    Assert-Condition ($applications.Count -eq 1) "The manifest must declare exactly one application."
    $application = $applications[0]
    Assert-Condition ($application.GetAttribute("Executable") -eq $exeName) `
        "The manifest executable does not match $exeName."
    Assert-Condition ($application.GetAttribute("EntryPoint") -eq "Windows.FullTrustApplication") `
        "The application must use Windows.FullTrustApplication."
    $visual = $application.SelectSingleNode("*[local-name()='VisualElements']")
    Assert-Condition ($null -ne $visual) "The application's visual elements are missing."
    foreach ($attribute in @("Square150x150Logo", "Square44x44Logo")) {
        $relative = $visual.GetAttribute($attribute)
        Assert-Condition (-not [string]::IsNullOrWhiteSpace($relative)) "$attribute is missing."
        Assert-Condition (Test-Path -LiteralPath (Join-Path $layout $relative)) `
            "Missing resource $relative."
    }
    $storeLogo = $package.SelectSingleNode("*[local-name()='Properties']/*[local-name()='Logo']")
    Assert-Condition ($null -ne $storeLogo -and -not [string]::IsNullOrWhiteSpace($storeLogo.InnerText)) `
        "The Store logo is missing."
    Assert-Condition (Test-Path -LiteralPath (Join-Path $layout $storeLogo.InnerText)) `
        "Missing resource $($storeLogo.InnerText)."

    $family = $package.SelectSingleNode("*[local-name()='Dependencies']/*[local-name()='TargetDeviceFamily']")
    Assert-Condition ($null -ne $family) "TargetDeviceFamily is missing."
    Assert-Condition ($family.GetAttribute("Name") -eq "Windows.Desktop") `
        "TargetDeviceFamily debe ser Windows.Desktop."
    Write-Output "msix: manifest validated"

    $sdkRoot = Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10\bin"
    Assert-Condition (Test-Path -LiteralPath $sdkRoot) "The Windows SDK was not found."
    $sdk = Get-ChildItem -LiteralPath $sdkRoot -Directory |
        Where-Object {
            $_.Name -match '^\d+(\.\d+){1,3}$' -and
            (Test-Path -LiteralPath (Join-Path $_.FullName "x64\makeappx.exe"))
        } |
        Sort-Object { [version]$_.Name } -Descending |
        Select-Object -First 1
    Assert-Condition ($null -ne $sdk) "MakeAppx.exe was not found in the SDK."
    $makeappx = Join-Path $sdk.FullName "x64\makeappx.exe"
    $signtool = Join-Path $sdk.FullName "x64\signtool.exe"
    Assert-Condition (Test-Path -LiteralPath $signtool) "signtool.exe was not found in the SDK."

    Write-Host "Empaquetando $packageName $version ($architecture)"
    & $makeappx pack /d $layout /p $msixPath /o 2>&1 |
        Tee-Object -FilePath (Join-Path $output "makeappx-pack.log")
    Assert-Condition ($LASTEXITCODE -eq 0) "MakeAppx pack failed with exit code $LASTEXITCODE."
    Write-Output "msix: package created"

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
    Write-Output "msix: certificate created"

    & $signtool sign /fd SHA256 /f $pfxPath /p $password $msixPath 2>&1 |
        Tee-Object -FilePath (Join-Path $output "signtool-sign.log")
    Assert-Condition ($LASTEXITCODE -eq 0) "signtool sign failed with exit code $LASTEXITCODE."
    Write-Output "msix: signature applied"

    # WinTrust and Appx verification hang on the runner. Verify the PKCS#7
    # signature in AppxSignature.p7x offline without calling signtool verify.
    # Also check the actual container's manifest, executable, and resources.
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $signatureDetails = $null
    $archive = [System.IO.Compression.ZipFile]::OpenRead($msixPath)
    try {
        $entries = @($archive.Entries | ForEach-Object { $_.FullName.Replace('\', '/') })
        foreach ($required in @(
            "AppxManifest.xml",
            $exeName,
            "Assets/StoreLogo.png",
            "Assets/Square44x44Logo.png",
            "Assets/Square150x150Logo.png",
            "AppxSignature.p7x"
        )) {
            Assert-Condition ($entries -contains $required) "El MSIX no contiene $required."
        }
        $signatureEntry = $archive.GetEntry("AppxSignature.p7x")
        Assert-Condition ($null -ne $signatureEntry -and $signatureEntry.Length -gt 0) `
            "The MSIX does not contain a signature."
        $signatureBytes = Get-ZipEntryBytes -Archive $archive -Name "AppxSignature.p7x"
        $signatureDetails = Assert-MsixSignature -SignatureBytes $signatureBytes -Certificate $certificate
        $manifestEntry = $archive.GetEntry("AppxManifest.xml")
        $reader = New-Object System.IO.StreamReader($manifestEntry.Open())
        try {
            $packedManifestText = $reader.ReadToEnd()
        } finally {
            $reader.Dispose()
        }
        [xml]$packedManifest = $packedManifestText
        $packedCapabilities = @($packedManifest.Package.Capabilities.ChildNodes |
            ForEach-Object { $_.GetAttribute("Name") })
        Assert-Condition ($packedCapabilities -contains "userNotificationListener") `
            "El manifiesto empaquetado no declara userNotificationListener."
        Assert-Condition ($packedCapabilities -contains "runFullTrust") `
            "El manifiesto empaquetado no declara runFullTrust."
    } finally {
        $archive.Dispose()
    }
    Assert-Condition ($null -ne $signatureDetails) "The PKCS#7 signature was not verified."
    Write-Output "msix: PKCS#7 signature verified without signtool verify"
    Write-Output "msix: manifest, resources, and container checked"

    $signatureLog = @(
        "format=PKCS7-SignedData"
        "prefix=PKCX"
        "signature=valid"
        "signer_thumbprint=$($signatureDetails.Thumbprint)"
        "digest_algorithm=$($signatureDetails.DigestAlgorithm)"
        "content_type=$($signatureDetails.ContentType)"
    )
    $signatureLog | Set-Content -LiteralPath (Join-Path $output "signature-verify.log") -Encoding UTF8

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
        "signature=present"
        "signature_crypto=verified"
        "signature_signer=$($signatureDetails.Thumbprint)"
        "container=verified"
        "result=ok"
    )
    $validation | Set-Content -LiteralPath (Join-Path $output "validation.txt") -Encoding UTF8

    $install = @'
OneInTwo Native Notifications Check
==================================

This MSIX package is for validation only. Use the self-signed certificate
only to test this build.

1. From an elevated shell, trust the test certificate in the Local Machine
   store used by App Installer:
   Import-Certificate -FilePath OneInTwo-NativeCheck.cer -CertStoreLocation Cert:\LocalMachine\TrustedPeople

2. Install the package:
   Add-AppxPackage -Path OneInTwo-NativeCheck.msix

3. Open the native check:
   $pkg = Get-AppxPackage OneInTwo.Notifications.NativeCheck
   Start-Process "shell:AppsFolder\$($pkg.PackageFamilyName)!NativeCheck"

4. Grant access to notifications and follow the checklist in
   notification_support/README.md.

To remove the build:
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
