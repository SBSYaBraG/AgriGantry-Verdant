[CmdletBinding()]
param(
  [string]$LogPath,
  [string]$OutputPath
)

$projectRoot = Split-Path -Parent $PSScriptRoot

if ([string]::IsNullOrWhiteSpace($LogPath)) {
  $logsDirectory = Join-Path $projectRoot "logs"
  $latestLog = Get-ChildItem -LiteralPath $logsDirectory -File |
    Where-Object {
      $_.Name -like "device-monitor-*.log" -or
      $_.Name -like "platformio-device-monitor-*.log"
    } |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1

  if ($null -eq $latestLog) {
    throw "No PlatformIO monitor log was found in $logsDirectory. Run the Serial Monitor first."
  }

  $LogPath = $latestLog.FullName
} elseif (-not [System.IO.Path]::IsPathRooted($LogPath)) {
  $LogPath = Join-Path $projectRoot $LogPath
}

if (-not (Test-Path -LiteralPath $LogPath -PathType Leaf)) {
  throw "Serial log not found: $LogPath"
}

$logItem = Get-Item -LiteralPath $LogPath
$sessionDate = $logItem.LastWriteTime.ToString("yyyy-MM-dd")
if ($logItem.BaseName -match "(?:device-monitor|platformio-device-monitor)-(?<date>\d{6})") {
  $sessionDate = [DateTime]::ParseExact(
    $Matches.date,
    "yyMMdd",
    [System.Globalization.CultureInfo]::InvariantCulture
  ).ToString("yyyy-MM-dd")
}

$fallbackTimestamp = $logItem.LastWriteTime.ToString("yyyy-MM-ddTHH:mm:ss.fff")
if ([string]::IsNullOrWhiteSpace($OutputPath)) {
  $OutputPath = Join-Path $projectRoot ("readings_enc_{0:yyyyMMdd_HHmmss}.csv" -f $logItem.LastWriteTime)
} elseif (-not [System.IO.Path]::IsPathRooted($OutputPath)) {
  $OutputPath = Join-Path $projectRoot $OutputPath
}

$header = "test_timestamp_local,timestamp_source,sequence,capture_label,type,direction,command_pulses,target_pps,duration_ms,encoder_before,encoder_after,encoder_delta,expected_magnitude,magnitude_error,index_before,index_after"
$rows = @(
  Get-Content -LiteralPath $LogPath |
    ForEach-Object {
      $line = $_.Trim()
      $timestamp = $fallbackTimestamp
      $timestampSource = "session_end_fallback"

      if ($line -match "^(?<time>\d{2}:\d{2}:\d{2}\.\d{3})\s+>\s*(?<content>.*)$") {
        $timestamp = "$sessionDate" + "T" + $Matches.time
        $timestampSource = "monitor"
        $line = $Matches.content.Trim()
      }

      if ($line.StartsWith("ENC_LOG,")) {
        "$timestamp,$timestampSource," + $line.Substring(8)
      }
    }
)

if ($rows.Count -eq 0) {
  throw "No completed ENC_LOG rows were found in $LogPath."
}

@($header) + $rows | Set-Content -LiteralPath $OutputPath -Encoding utf8
Write-Host "Wrote $($rows.Count) encoder summary row(s) to $OutputPath"
