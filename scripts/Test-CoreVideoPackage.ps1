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
}

Test-RequiredFile "obs-plugins\64bit\obs-zoom-plugin.dll"
Test-RequiredFile "obs-plugins\64bit\CoreVideoOAuthCallback.exe"
Test-RequiredFile "obs-plugins\64bit\Qt6Core.dll"
Test-RequiredFile "obs-plugins\64bit\Qt6Network.dll"
Test-RequiredFile "obs-plugins\64bit\plugins\tls\qschannelbackend.dll"

if ($FullRuntime) {
    Test-RequiredFile "obs-plugins\64bit\zoom-runtime\ZoomObsEngine.exe"
    Test-RequiredFile "obs-plugins\64bit\zoom-runtime\sdk.dll"

    $zoomRuntime = Join-Path $root "obs-plugins\64bit\zoom-runtime"
    $zoomRuntimeFiles = @(Get-ChildItem -LiteralPath $zoomRuntime -File -ErrorAction Stop)
    if ($zoomRuntimeFiles.Count -lt 3) {
        throw "Release package is incomplete. zoom-runtime contains too few runtime files."
    }
}

Write-Host "CoreVideo package validation passed: $root"
