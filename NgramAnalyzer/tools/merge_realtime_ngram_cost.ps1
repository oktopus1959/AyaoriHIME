param(
    [string]$BaseScoreFile = "work/wiki_hplt.all.score.0.5-0.1-10000.txt",
    [string]$RealtimePattern = "work/input/realtime.ngram.*.txt",
    [string]$OutputFile = "work/build/ngram-sys-source.txt",
    [int]$MaxRealtimeBonus = 5000,
    [int]$RealtimeBaseCost = 6000,
    [int]$RealtimeGetaBaseCost = 9500
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$GetaChar = [string][char]0x3013

function Get-RealtimeBonus([int]$count, [int]$maxCount, [int]$maxBonus) {
    if ($count -le 0 -or $maxCount -le 0) {
        return 0
    }
    if ($count -ge $maxCount) {
        return $maxBonus
    }

    $normalized = [Math]::Log(1.0 + $count) / [Math]::Log(1.0 + $maxCount)
    $bonus = [int][Math]::Round($normalized * $maxBonus)
    if ($bonus -lt 0) {
        return 0
    }
    if ($bonus -gt $maxBonus) {
        return $maxBonus
    }
    return $bonus
}

function Test-HiraganaOnly([string]$text) {
    return $text -match '^\p{IsHiragana}+$'
}

function Test-StartsWithKanji([string]$text) {
    return $text -match '^\p{IsCJKUnifiedIdeographs}'
}

function Apply-Bonus([System.Collections.Generic.Dictionary[string, int]]$dict, [string]$key, [int]$bonus, [int]$normalBaseCost, [int]$getaBaseCost, [string]$getaChar) {
    if ($dict.ContainsKey($key)) {
        $dict[$key] = [int]$dict[$key] - $bonus
        return "overwritten"
    }

    $baseCost = $normalBaseCost
    if ($key.StartsWith($getaChar)) {
        $baseCost = $getaBaseCost
    }
    $dict[$key] = $baseCost - $bonus
    return "added"
}

function New-Utf8Writer([string]$path) {
    $dir = Split-Path -Parent $path
    if ($dir) {
        New-Item -ItemType Directory -Force -Path $dir | Out-Null
    }

    $utf8NoBom = [System.Text.UTF8Encoding]::new($false)
    return [System.IO.StreamWriter]::new($path, $false, $utf8NoBom)
}

if (-not (Test-Path -LiteralPath $BaseScoreFile -PathType Leaf)) {
    throw "base score file not found: $BaseScoreFile"
}

$merged = [System.Collections.Generic.Dictionary[string, int]]::new()
$baseCount = 0
$reader = [System.IO.StreamReader]::new($BaseScoreFile)
try {
    while (($line = $reader.ReadLine()) -ne $null) {
        if ([string]::IsNullOrWhiteSpace($line) -or $line.StartsWith("#")) {
            continue
        }

        $parts = $line -split "`t", 2
        if ($parts.Length -lt 2 -or [string]::IsNullOrWhiteSpace($parts[0])) {
            continue
        }

        $cost = 0
        if (-not [int]::TryParse($parts[1], [ref]$cost)) {
            continue
        }

        $merged[$parts[0]] = $cost
        $baseCount++
    }
}
finally {
    $reader.Dispose()
}

$rtCounts = [System.Collections.Generic.Dictionary[string, int]]::new()
$maxRealtimeCount = 0
$ignoredByLength = 0
$malformedRealtime = 0

Get-ChildItem -Path $RealtimePattern -File -ErrorAction SilentlyContinue | Sort-Object Name | ForEach-Object {
    $reader = [System.IO.StreamReader]::new($_.FullName)
    try {
        while (($line = $reader.ReadLine()) -ne $null) {
            if ([string]::IsNullOrWhiteSpace($line) -or $line.StartsWith("#")) {
                continue
            }

            $parts = $line -split "`t", 2
            if ($parts.Length -lt 2 -or [string]::IsNullOrWhiteSpace($parts[0])) {
                $malformedRealtime++
                continue
            }

            $count = 0
            if (-not [int]::TryParse($parts[1], [ref]$count)) {
                $malformedRealtime++
                continue
            }

            $len = $parts[0].Length
            if ($len -lt 2 -or $len -gt 4) {
                $ignoredByLength++
                continue
            }

            if ((Test-HiraganaOnly $parts[0]) -and ($len -eq 2 -or $len -eq 4)) {
                continue
            }

            if ($count -le 1 -and $len -ge 4 -and (Test-HiraganaOnly $parts[0])) {
                continue
            }

            if ($rtCounts.ContainsKey($parts[0])) {
                $rtCounts[$parts[0]] += $count
            }
            else {
                $rtCounts[$parts[0]] = $count
            }
        }
    }
    finally {
        $reader.Dispose()
    }
}

$overwrittenCount = 0
$addedCount = 0

foreach ($count in $rtCounts.Values) {
    if ($count -gt $maxRealtimeCount) {
        $maxRealtimeCount = $count
    }
}

foreach ($entry in $rtCounts.GetEnumerator()) {
    $bonus = Get-RealtimeBonus $entry.Value $maxRealtimeCount $MaxRealtimeBonus
    if ($bonus -le 0) {
        continue
    }

    $targets = [System.Collections.Generic.List[string]]::new()
    $targets.Add($entry.Key)
    if (Test-StartsWithKanji $entry.Key) {
        $targets.Add($GetaChar + $entry.Key)
    }

    foreach ($target in $targets) {
        $result = Apply-Bonus $merged $target $bonus $RealtimeBaseCost $RealtimeGetaBaseCost $GetaChar
        if ($result -eq "overwritten") {
            $overwrittenCount++
        }
        elseif ($result -eq "added") {
            $addedCount++
        }
    }
}

$writer = New-Utf8Writer -path $OutputFile
try {
    foreach ($item in $merged.GetEnumerator()) {
        $writer.WriteLine(("{0}`t{1}" -f $item.Key, $item.Value))
    }
}
finally {
    $writer.Dispose()
}

Write-Host "[INFO] base=$baseCount realtime_used=$($rtCounts.Count) max_rt_count=$maxRealtimeCount overwritten=$overwrittenCount added=$addedCount ignored_len=$ignoredByLength malformed_rt=$malformedRealtime out=$OutputFile"
