param(
    [Parameter()]
    [int]$DurationSeconds = 300,

    [Parameter()]
    [int]$SampleSeconds = 5,

    [Parameter()]
    [string]$OutputPath = ""
)

$ErrorActionPreference = "Stop"

if ($DurationSeconds -le 0) {
    throw "DurationSeconds must be greater than zero."
}
if ($SampleSeconds -le 0) {
    throw "SampleSeconds must be greater than zero."
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

Write-Host "Sampling CoreVideo load for $DurationSeconds seconds every $SampleSeconds seconds."
Write-Host "Keep OBS in the target production state: 8 feeds, ISO recording, and program stream active."

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
        $rows = $_.Group | Where-Object { $_.Count -gt 0 }
        [pscustomobject]@{
            ProcessName = $_.Name
            Samples = $_.Group.Count
            MaxCount = ($_.Group | Measure-Object Count -Maximum).Maximum
            MaxWorkingSetMB = if ($rows) { ($rows | Measure-Object WorkingSetMB -Maximum).Maximum } else { 0 }
            MaxPrivateMemoryMB = if ($rows) { ($rows | Measure-Object PrivateMemoryMB -Maximum).Maximum } else { 0 }
            FinalCpuSeconds = if ($rows) { ($rows | Select-Object -Last 1).CpuSeconds } else { 0 }
        }
    }

$summaryPath = [System.IO.Path]::ChangeExtension($OutputPath, ".summary.csv")
$summary | Export-Csv -NoTypeInformation -Path $summaryPath
$summary | Format-Table -AutoSize

Write-Host "Wrote samples: $OutputPath"
Write-Host "Wrote summary: $summaryPath"
