param(
    [Parameter()]
    [int]$DurationSeconds = 300,

    [Parameter()]
    [int]$SampleSeconds = 5,

    [Parameter()]
    [string]$OutputPath = "",

    [Parameter()]
    [int]$ExpectedFeeds = 8,

    [Parameter()]
    [int]$ExpectedIsoRecorders = 0,

    [Parameter()]
    [switch]$RequireObs
)

$ErrorActionPreference = "Stop"

if ($DurationSeconds -le 0) {
    throw "DurationSeconds must be greater than zero."
}
if ($SampleSeconds -le 0) {
    throw "SampleSeconds must be greater than zero."
}
if ($ExpectedFeeds -lt 0) {
    throw "ExpectedFeeds must be zero or greater."
}
if ($ExpectedIsoRecorders -lt 0) {
    throw "ExpectedIsoRecorders must be zero or greater."
}

if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $OutputPath = Join-Path (Get-Location) "artifacts\load-tests\corevideo-load-$stamp.csv"
}

$outDir = Split-Path -Parent $OutputPath
if (-not [string]::IsNullOrWhiteSpace($outDir)) {
    New-Item -Force -ItemType Directory $outDir | Out-Null
}

function Get-ProcessSnapshot {
    param([string[]]$Names)

    $rows = @()
    foreach ($name in $Names) {
        $procs = @(Get-Process -Name $name -ErrorAction SilentlyContinue)
        if ($procs.Count -eq 0) {
            $rows += [pscustomobject]@{
                ProcessName = $name
                Count = 0
                CpuSeconds = 0.0
                WorkingSetMB = 0.0
                PrivateMemoryMB = 0.0
                Handles = 0
                Threads = 0
            }
            continue
        }

        $rows += [pscustomobject]@{
            ProcessName = $name
            Count = $procs.Count
            CpuSeconds = [math]::Round((($procs | Measure-Object -Property CPU -Sum).Sum), 3)
            WorkingSetMB = [math]::Round((($procs | Measure-Object -Property WorkingSet64 -Sum).Sum / 1MB), 1)
            PrivateMemoryMB = [math]::Round((($procs | Measure-Object -Property PrivateMemorySize64 -Sum).Sum / 1MB), 1)
            Handles = (($procs | Measure-Object -Property Handles -Sum).Sum)
            Threads = (($procs | ForEach-Object { $_.Threads.Count } | Measure-Object -Sum).Sum)
        }
    }
    return $rows
}

$processNames = @("obs64", "ZoomObsEngine", "ffmpeg")
$start = Get-Date
$deadline = $start.AddSeconds($DurationSeconds)
$samples = New-Object System.Collections.Generic.List[object]
$logicalProcessors = [Math]::Max(1, [Environment]::ProcessorCount)

Write-Host "Sampling CoreVideo load for $DurationSeconds seconds every $SampleSeconds seconds."
Write-Host "Keep OBS in the target production state: 8 feeds, ISO recording, and program stream active."
Write-Host "Expected feeds: $ExpectedFeeds; expected ISO ffmpeg processes: $ExpectedIsoRecorders."

while ((Get-Date) -lt $deadline) {
    $now = Get-Date
    $elapsed = [math]::Round(($now - $start).TotalSeconds, 1)
    foreach ($snap in Get-ProcessSnapshot -Names $processNames) {
        $samples.Add([pscustomobject]@{
            Timestamp = $now.ToString("o")
            ElapsedSeconds = $elapsed
            ProcessName = $snap.ProcessName
            Count = $snap.Count
            CpuSeconds = $snap.CpuSeconds
            WorkingSetMB = $snap.WorkingSetMB
            PrivateMemoryMB = $snap.PrivateMemoryMB
            Handles = $snap.Handles
            Threads = $snap.Threads
        })
    }
    Start-Sleep -Seconds $SampleSeconds
}

$samples | Export-Csv -NoTypeInformation -Path $OutputPath

$summary = $samples |
    Group-Object ProcessName |
    ForEach-Object {
        $allRows = @($_.Group)
        $rows = $_.Group | Where-Object { $_.Count -gt 0 }
        $first = if ($rows) { $rows | Select-Object -First 1 } else { $null }
        $last = if ($rows) { $rows | Select-Object -Last 1 } else { $null }
        $lastSample = if ($allRows) { $allRows | Select-Object -Last 1 } else { $null }
        $cpuDelta = if ($first -and $last) {
            [math]::Max(0.0, [double]$last.CpuSeconds - [double]$first.CpuSeconds)
        } else {
            0.0
        }
        $avgCpuPercent = if ($DurationSeconds -gt 0) {
            [math]::Round(($cpuDelta / $DurationSeconds) * 100.0 / $logicalProcessors, 1)
        } else {
            0.0
        }
        [pscustomobject]@{
            ProcessName = $_.Name
            Samples = $allRows.Count
            MinCount = if ($allRows) { ($allRows | Measure-Object Count -Minimum).Minimum } else { 0 }
            MaxCount = if ($allRows) { ($allRows | Measure-Object Count -Maximum).Maximum } else { 0 }
            FinalCount = if ($lastSample) { $lastSample.Count } else { 0 }
            ZeroCountSamples = @($allRows | Where-Object { $_.Count -eq 0 }).Count
            MaxWorkingSetMB = if ($rows) { ($rows | Measure-Object WorkingSetMB -Maximum).Maximum } else { 0 }
            MaxPrivateMemoryMB = if ($rows) { ($rows | Measure-Object PrivateMemoryMB -Maximum).Maximum } else { 0 }
            FinalCpuSeconds = if ($rows) { ($rows | Select-Object -Last 1).CpuSeconds } else { 0 }
            CpuDeltaSeconds = [math]::Round($cpuDelta, 3)
            AvgCpuPercentOfSystem = $avgCpuPercent
            MaxHandles = if ($rows) { ($rows | Measure-Object Handles -Maximum).Maximum } else { 0 }
            MaxThreads = if ($rows) { ($rows | Measure-Object Threads -Maximum).Maximum } else { 0 }
        }
    }

$summaryPath = [System.IO.Path]::ChangeExtension($OutputPath, ".summary.csv")
$summary | Export-Csv -NoTypeInformation -Path $summaryPath

$warnings = New-Object System.Collections.Generic.List[string]
$obsSummary = $summary | Where-Object { $_.ProcessName -eq "obs64" } | Select-Object -First 1
$engineSummary = $summary | Where-Object { $_.ProcessName -eq "ZoomObsEngine" } | Select-Object -First 1
$ffmpegSummary = $summary | Where-Object { $_.ProcessName -eq "ffmpeg" } | Select-Object -First 1
if ($RequireObs -and (-not $obsSummary -or $obsSummary.MaxCount -lt 1)) {
    $warnings.Add("OBS was required but obs64.exe was not observed.")
} elseif ($RequireObs -and $obsSummary.MinCount -lt 1) {
    $warnings.Add("OBS was required but obs64.exe disappeared during at least one sample.")
}
if (-not $engineSummary -or $engineSummary.MaxCount -lt 1) {
    $warnings.Add("ZoomObsEngine was not observed. The meeting engine may not have been running during the load test.")
} elseif ($engineSummary.MinCount -lt 1) {
    $warnings.Add("ZoomObsEngine disappeared during at least one sample. Check for engine crashes or restart loops.")
}
if ($ExpectedIsoRecorders -gt 0 -and
    (-not $ffmpegSummary -or $ffmpegSummary.MaxCount -lt $ExpectedIsoRecorders)) {
    $observed = if ($ffmpegSummary) { $ffmpegSummary.MaxCount } else { 0 }
    $warnings.Add("Expected at least $ExpectedIsoRecorders ffmpeg ISO recorder process(es), observed $observed.")
} elseif ($ExpectedIsoRecorders -gt 0 -and $ffmpegSummary.MinCount -lt $ExpectedIsoRecorders) {
    $warnings.Add("ffmpeg ISO recorder process count dropped below $ExpectedIsoRecorders during at least one sample.")
}
if ($ExpectedFeeds -ge 8 -and $engineSummary -and $engineSummary.AvgCpuPercentOfSystem -gt 35.0) {
    $warnings.Add("ZoomObsEngine average CPU exceeded 35% of total system CPU during an 8-feed target test.")
}

$jsonPath = [System.IO.Path]::ChangeExtension($OutputPath, ".summary.json")
$report = [pscustomobject]@{
    StartedAt = $start.ToString("o")
    FinishedAt = (Get-Date).ToString("o")
    DurationSeconds = $DurationSeconds
    SampleSeconds = $SampleSeconds
    LogicalProcessors = $logicalProcessors
    ExpectedFeeds = $ExpectedFeeds
    ExpectedIsoRecorders = $ExpectedIsoRecorders
    RequireObs = [bool]$RequireObs
    SamplesPath = (Resolve-Path $OutputPath).Path
    SummaryCsvPath = (Resolve-Path $summaryPath).Path
    Warnings = @($warnings)
    Processes = @($summary)
}
$report | ConvertTo-Json -Depth 5 | Set-Content -Path $jsonPath

$summary | Format-Table -AutoSize
if ($warnings.Count -gt 0) {
    Write-Warning "Load test completed with warnings:"
    foreach ($warning in $warnings) {
        Write-Warning " - $warning"
    }
}

Write-Host "Wrote samples: $OutputPath"
Write-Host "Wrote summary: $summaryPath"
Write-Host "Wrote JSON summary: $jsonPath"
