$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$tempRoot = Join-Path $root "tmp\onnxruntime-setup"
$packageRoot = Join-Path $tempRoot "packages"
$sdkRoot = Join-Path $root "3rdparty\onnxruntime-1.24.4-1.27.1"
$modelRoot = Join-Path $root "3rdparty\models"
$pythonPackages = Join-Path $tempRoot "python-packages"

New-Item -ItemType Directory -Force -Path $packageRoot, $sdkRoot, $modelRoot | Out-Null

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

$cudaArchive = Join-Path $packageRoot "onnxruntime-cuda13-1.27.1.zip"
$directmlPackage = Join-Path $packageRoot "onnxruntime-directml-1.24.4.nupkg"
$segmentationModel = Join-Path $packageRoot "clothes_segformer_b2.onnx"
$fashionClipModel = Join-Path $packageRoot "fashion_clip.onnx"

Get-VerifiedFile `
    -Uri "https://github.com/microsoft/onnxruntime/releases/download/v1.27.1/onnxruntime-win-x64-gpu_cuda13-1.27.1.zip" `
    -Path $cudaArchive `
    -Sha256 "1A88828FB1CA78C9920637ACF0F8D8CE40A854BEF874128C55193E3D1A2B9EF8"
Get-VerifiedFile `
    -Uri "https://api.nuget.org/v3-flatcontainer/microsoft.ml.onnxruntime.directml/1.24.4/microsoft.ml.onnxruntime.directml.1.24.4.nupkg" `
    -Path $directmlPackage `
    -Sha256 "57E9F11B73437BEF7A309496135D4C1F96B1A8E9DDBA60013FA27BFC1D788681"
Get-VerifiedFile `
    -Uri "https://huggingface.co/mattmdjaga/segformer_b2_clothes/resolve/main/onnx/model.onnx" `
    -Path $segmentationModel `
    -Sha256 "A93A8DAC171B5C1FCC53632A8BFC180BFD9759EA69A3E207451BB07F76ADD54F"
Get-VerifiedFile `
    -Uri "https://huggingface.co/patrickjohncyh/fashion-clip/resolve/main/onnx/model.onnx" `
    -Path $fashionClipModel `
    -Sha256 "DC4C724479E49D1DA9598969125353113A341BD4FD5A1DBC7D528D3F1545BBA9"

$cudaExtract = Join-Path $tempRoot "cuda"
$directmlExtract = Join-Path $tempRoot "directml"
Remove-Item -LiteralPath $cudaExtract, $directmlExtract -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath $cudaArchive -DestinationPath $cudaExtract
$directmlZip = Join-Path $packageRoot "onnxruntime-directml-1.24.4.zip"
Copy-Item -LiteralPath $directmlPackage -Destination $directmlZip -Force
Expand-Archive -LiteralPath $directmlZip -DestinationPath $directmlExtract

$cudaSource = Join-Path $cudaExtract "onnxruntime-win-x64-gpu_cuda13-1.27.1"
New-Item -ItemType Directory -Force -Path `
    (Join-Path $sdkRoot "include"), `
    (Join-Path $sdkRoot "runtime\cuda"), `
    (Join-Path $sdkRoot "runtime\directml") | Out-Null
Copy-Item -Path (Join-Path $directmlExtract "build\native\include\*") -Destination (Join-Path $sdkRoot "include") -Recurse -Force
Copy-Item -LiteralPath `
    (Join-Path $cudaSource "lib\onnxruntime.dll"), `
    (Join-Path $cudaSource "lib\onnxruntime_providers_cuda.dll"), `
    (Join-Path $cudaSource "lib\onnxruntime_providers_shared.dll") `
    -Destination (Join-Path $sdkRoot "runtime\cuda") -Force
Copy-Item -Path (Join-Path $directmlExtract "runtimes\win-x64\native\*.dll") `
    -Destination (Join-Path $sdkRoot "runtime\directml") -Force
Copy-Item -LiteralPath (Join-Path $directmlExtract "LICENSE") -Destination $sdkRoot -Force
Copy-Item -LiteralPath (Join-Path $directmlExtract "ThirdPartyNotices.txt") -Destination $sdkRoot -Force

Copy-Item -LiteralPath $segmentationModel -Destination (Join-Path $modelRoot "clothes_segformer_b2.onnx") -Force
New-Item -ItemType Directory -Force -Path $pythonPackages | Out-Null
$env:PYTHONPATH = $pythonPackages
if (-not (Test-Path -LiteralPath (Join-Path $pythonPackages "onnx\__init__.py"))) {
    python -m pip install --target $pythonPackages onnx==1.19.1
}
$visionModel = Join-Path $modelRoot "fashion_clip_vision.onnx"
python -c "from onnx.utils import extract_model; extract_model(r'$fashionClipModel', r'$visionModel', ['pixel_values'], ['image_embeds'], check_model=True)"
if ($LASTEXITCODE -ne 0) {
    throw "Failed to extract the FashionCLIP image encoder."
}
$visionHash = (Get-FileHash -LiteralPath $visionModel -Algorithm SHA256).Hash
if ($visionHash -ne "3A62F866D7139B45F061E7CD9ECA5BB7242A1D18ADA822B7E67FC0CBA638EA53") {
    throw "Unexpected FashionCLIP vision model hash: $visionHash"
}

Write-Host "ONNX Runtime and garment matching models are ready under 3rdparty/."
