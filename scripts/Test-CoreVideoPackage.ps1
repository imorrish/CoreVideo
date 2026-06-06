param(
    [Parameter(Mandatory = $true)]
    [string]$PackageRoot,

    [Parameter()]
    [switch]$FullRuntime
)

$ErrorActionPreference = "Stop"

$root = [System.IO.Path]::GetFullPath($PackageRoot)
if (-not (Test-Path -LiteralPath $root -PathType Container)) {
    throw "Package root does not exist: $root"
}

function Test-RequiredFile {
    param([string]$RelativePath)

    $path = Join-Path $root $RelativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Release package is incomplete. Missing: $RelativePath"
    }
    if ((Get-Item -LiteralPath $path).Length -le 0) {
        throw "Release package is incomplete. Empty file: $RelativePath"
    }
}

$required = @(
    "obs-plugins\64bit\obs-zoom-plugin.dll",
    "obs-plugins\64bit\CoreVideoOAuthCallback.exe",
    "obs-plugins\64bit\Qt6Core.dll",
    "obs-plugins\64bit\Qt6Gui.dll",
    "obs-plugins\64bit\Qt6Network.dll",
    "obs-plugins\64bit\Qt6Widgets.dll",
    "obs-plugins\64bit\plugins\platforms\qwindows.dll",
    "obs-plugins\64bit\plugins\tls\qcertonlybackend.dll",
    "obs-plugins\64bit\plugins\tls\qschannelbackend.dll",
    "data\obs-plugins\obs-zoom-plugin\locale\en-US.ini"
)

foreach ($file in $required) {
    Test-RequiredFile $file
}

$ffmpegDlls = @(
    "avcodec-62.dll",
    "avdevice-62.dll",
    "avfilter-11.dll",
    "avformat-62.dll",
    "avutil-60.dll",
    "swresample-6.dll",
    "swscale-9.dll"
)
$ffmpegPresent = $ffmpegDlls | Where-Object {
    Test-Path -LiteralPath (Join-Path $root "obs-plugins\64bit\$_")
}
if ($ffmpegPresent.Count -gt 0 -and $ffmpegPresent.Count -ne $ffmpegDlls.Count) {
    throw "Release package has a partial FFmpeg runtime. Present: $($ffmpegPresent -join ', ')"
}

$sidecarPath = Join-Path $root "obs-plugins\64bit\CoreVideoSidecar.exe"
if (Test-Path -LiteralPath $sidecarPath -PathType Leaf) {
    foreach ($file in @(
        "obs-plugins\64bit\data\templates\1-up.json",
        "obs-plugins\64bit\data\templates\8-up-grid.json",
        "obs-plugins\64bit\data\templates\speaker-screenshare.json",
        "obs-plugins\64bit\data\looks\one-up-clean.json",
        "obs-plugins\64bit\data\looks\eight-up-grid.json",
        "obs-plugins\64bit\data\looks\speaker-screenshare.json"
    )) {
        Test-RequiredFile $file
    }
}

if ($FullRuntime) {
    foreach ($file in @(
        "obs-plugins\64bit\zoom-runtime\ZoomObsEngine.exe",
        "obs-plugins\64bit\zoom-runtime\sdk.dll",
        "obs-plugins\64bit\zoom-runtime\sdkExt.dll",
        "obs-plugins\64bit\zoom-runtime\zSDK.dll",
        "obs-plugins\64bit\zoom-runtime\zVideoApp.dll",
        "obs-plugins\64bit\zoom-runtime\zVideoUI.dll",
        "obs-plugins\64bit\zoom-runtime\zNet.dll",
        "obs-plugins\64bit\zoom-runtime\libcrypto-3-zm.dll",
        "obs-plugins\64bit\zoom-runtime\libssl-3-zm.dll",
        "obs-plugins\64bit\zoom-runtime\zoom.manifest"
    )) {
        Test-RequiredFile $file
    }

    $zoomRuntime = Join-Path $root "obs-plugins\64bit\zoom-runtime"
    $zoomRuntimeFiles = @(Get-ChildItem -LiteralPath $zoomRuntime -File -ErrorAction Stop)
    if ($zoomRuntimeFiles.Count -lt 50) {
        throw "Release package is incomplete. zoom-runtime contains too few runtime files ($($zoomRuntimeFiles.Count))."
    }
}

Write-Host "CoreVideo package validation passed: $root"
