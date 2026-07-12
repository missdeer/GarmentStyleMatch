param(
    [string] $ModelRoot
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Net.Http

$root = Split-Path -Parent $PSScriptRoot
$isInstalledPackage = Test-Path -LiteralPath (Join-Path $root "GarmentStyleMatch.exe")
$modelRoot = if ($ModelRoot) {
    $ModelRoot
} elseif ($isInstalledPackage) {
    Join-Path $root "models"
} else {
    Join-Path $root "3rdparty\models"
}
$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) "GarmentStyleMatch-model-setup"
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
        [Parameter(Mandatory)] [string] $Sha256,
        [Parameter(Mandatory)] [long] $ProgressOffset,
        [Parameter(Mandatory)] [long] $ProgressTotal,
        [Parameter(Mandatory)] [string] $Label
    )

    if (Test-Path -LiteralPath $Path) {
        $actual = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
        if ($actual -eq $Sha256) {
            return
        }
        Remove-Item -LiteralPath $Path -Force
    }

    $handler = [System.Net.Http.HttpClientHandler]::new()
    $client = [System.Net.Http.HttpClient]::new($handler)
    $client.Timeout = [TimeSpan]::FromHours(1)
    $response = $null
    $sourceStream = $null
    $destinationStream = $null
    try {
        $response = $client.GetAsync(
            $Uri,
            [System.Net.Http.HttpCompletionOption]::ResponseHeadersRead
        ).GetAwaiter().GetResult()
        $response.EnsureSuccessStatusCode()
        $sourceStream = $response.Content.ReadAsStreamAsync().GetAwaiter().GetResult()
        $destinationStream = [System.IO.File]::Open(
            $Path,
            [System.IO.FileMode]::Create,
            [System.IO.FileAccess]::Write,
            [System.IO.FileShare]::None
        )
        $buffer = New-Object byte[] (1024 * 1024)
        $downloaded = 0L
        $lastPercent = -1
        while (($read = $sourceStream.Read($buffer, 0, $buffer.Length)) -gt 0) {
            $destinationStream.Write($buffer, 0, $read)
            $downloaded += $read
            $percent = [Math]::Min(
                100,
                [int] [Math]::Floor(($ProgressOffset + $downloaded) * 100.0 / $ProgressTotal)
            )
            if ($percent -ne $lastPercent) {
                Write-Output "GSM_PROGRESS|$Label|$percent"
                $lastPercent = $percent
            }
        }
    } catch {
        if (Test-Path -LiteralPath $Path) {
            Remove-Item -LiteralPath $Path -Force
        }
        throw
    } finally {
        if ($destinationStream) { $destinationStream.Dispose() }
        if ($sourceStream) { $sourceStream.Dispose() }
        if ($response) { $response.Dispose() }
        $client.Dispose()
        $handler.Dispose()
    }

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
    -Sha256 "A93A8DAC171B5C1FCC53632A8BFC180BFD9759EA69A3E207451BB07F76ADD54F" `
    -ProgressOffset 0 `
    -ProgressTotal 512778784 `
    -Label "clothes_segformer_b2.onnx"
Get-VerifiedFile `
    -Uri "https://huggingface.co/patrickjohncyh/fashion-clip/resolve/main/onnx/model.onnx" `
    -Path $fashionClipModel `
    -Sha256 "DC4C724479E49D1DA9598969125353113A341BD4FD5A1DBC7D528D3F1545BBA9" `
    -ProgressOffset 110039290 `
    -ProgressTotal 512778784 `
    -Label "fashion_clip.onnx"

Copy-Item -LiteralPath $segmentationModel -Destination $installedSegmentationModel -Force
Write-Output "正在准备 FashionCLIP 图像模型..."
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
