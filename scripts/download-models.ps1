$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$isInstalledPackage = Test-Path -LiteralPath (Join-Path $root "GarmentStyleMatch.exe")
$modelRoot = if ($isInstalledPackage) {
    Join-Path $root "models"
} else {
    Join-Path $root "3rdparty\models"
}
$tempRoot = Join-Path $root "tmp\model-setup"
$packageRoot = Join-Path $tempRoot "packages"
$pythonPackages = Join-Path $tempRoot "python-packages"

New-Item -ItemType Directory -Force -Path $packageRoot, $modelRoot, $pythonPackages | Out-Null

$installedSegmentationModel = Join-Path $modelRoot "clothes_segformer_b2.onnx"
$installedVisionModel = Join-Path $modelRoot "fashion_clip_vision.onnx"
if ((Test-Path -LiteralPath $installedSegmentationModel) -and
    (Test-Path -LiteralPath $installedVisionModel) -and
    (Get-FileHash -LiteralPath $installedSegmentationModel -Algorithm SHA256).Hash -eq
        "A93A8DAC171B5C1FCC53632A8BFC180BFD9759EA69A3E207451BB07F76ADD54F" -and
    (Get-FileHash -LiteralPath $installedVisionModel -Algorithm SHA256).Hash -eq
        "3A62F866D7139B45F061E7CD9ECA5BB7242A1D18ADA822B7E67FC0CBA638EA53") {
    Write-Host "Garment matching models are already ready under $modelRoot."
    return
}

function Get-VerifiedFile {
    param(
        [Parameter(Mandatory)] [string] $Uri,
        [Parameter(Mandatory)] [string] $Path,
        [Parameter(Mandatory)] [string] $Sha256
    )

    if (Test-Path -LiteralPath $Path) {
        $actual = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
        if ($actual -eq $Sha256) {
            return
        }
        Remove-Item -LiteralPath $Path -Force
    }

    Invoke-WebRequest -Uri $Uri -OutFile $Path -TimeoutSec 3600
    $actual = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
    if ($actual -ne $Sha256) {
        throw "SHA-256 mismatch for $Path. Expected $Sha256, got $actual."
    }
}

$segmentationModel = Join-Path $packageRoot "clothes_segformer_b2.onnx"
$fashionClipModel = Join-Path $packageRoot "fashion_clip.onnx"

Get-VerifiedFile `
    -Uri "https://huggingface.co/mattmdjaga/segformer_b2_clothes/resolve/main/onnx/model.onnx" `
    -Path $segmentationModel `
    -Sha256 "A93A8DAC171B5C1FCC53632A8BFC180BFD9759EA69A3E207451BB07F76ADD54F"
Get-VerifiedFile `
    -Uri "https://huggingface.co/patrickjohncyh/fashion-clip/resolve/main/onnx/model.onnx" `
    -Path $fashionClipModel `
    -Sha256 "DC4C724479E49D1DA9598969125353113A341BD4FD5A1DBC7D528D3F1545BBA9"

Copy-Item -LiteralPath $segmentationModel -Destination $installedSegmentationModel -Force
$env:PYTHONPATH = $pythonPackages
if (-not (Test-Path -LiteralPath (Join-Path $pythonPackages "onnx\__init__.py"))) {
    python -m pip install --target $pythonPackages onnx==1.19.1
}
python -c "from onnx.utils import extract_model; extract_model(r'$fashionClipModel', r'$installedVisionModel', ['pixel_values'], ['image_embeds'], check_model=True)"
if ($LASTEXITCODE -ne 0) {
    throw "Failed to extract the FashionCLIP image encoder."
}
$visionHash = (Get-FileHash -LiteralPath $installedVisionModel -Algorithm SHA256).Hash
if ($visionHash -ne "3A62F866D7139B45F061E7CD9ECA5BB7242A1D18ADA822B7E67FC0CBA638EA53") {
    throw "Unexpected FashionCLIP vision model hash: $visionHash"
}

Write-Host "Garment matching models are ready under $modelRoot."
