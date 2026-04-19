param(
    [Parameter(Mandatory = $true)]
    [string]$Target,
    [string[]]$DumpPaths = @(
        "F:\Dev\CSharp\AyaoriHIME\src\tables\dump-table1.txt",
        "F:\Dev\CSharp\AyaoriHIME\src\tables\dump-table2.txt"
    ),
    [switch]$RunProbe
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$srcDir = Split-Path -Parent $scriptDir
$probeScript = Join-Path $scriptDir "deckey_sequence_probe.ps1"
$x86PowerShell = "C:\Windows\SysWOW64\WindowsPowerShell\v1.0\powershell.exe"

function Normalize-EntryText([string]$text) {
    if ($null -eq $text) { return "" }
    return $text.Replace("/", "")
}

function Convert-PathToDeckeys([string]$pathText) {
    return @($pathText -split ":" | Where-Object { $_ -ne "" } | ForEach-Object { [int]$_ })
}

function Get-DeckeySequenceLiteral([int[]]$deckeys) {
    return "@(" + (($deckeys | ForEach-Object { "$_" }) -join ",") + ")"
}

function New-StringEntry([string]$tableName, [string]$pathText, [string]$rawText, [string]$lineText) {
    $deckeys = Convert-PathToDeckeys $pathText
    $normalized = Normalize-EntryText $rawText
    $alternatives = @($normalized -split "\|")
    [pscustomobject]@{
        Table = $tableName
        Type = "string"
        Path = $pathText
        Deckeys = $deckeys
        DeckeySequence = Get-DeckeySequenceLiteral $deckeys
        Prev = ""
        Out = $rawText
        Next = ""
        Normalized = $normalized
        Alternatives = $alternatives
        RawLine = $lineText
    }
}

function New-RewriteEntry([string]$tableName, [string]$head, [string]$outText, [string]$nextText, [string]$lineText) {
    if ($head -notmatch '^(.*?)(\d+(?::\d+)*)$') {
        return $null
    }

    $prevText = $Matches[1]
    $pathText = $Matches[2]
    $deckeys = Convert-PathToDeckeys $pathText
    $normalized = Normalize-EntryText ($outText + $nextText)

    [pscustomobject]@{
        Table = $tableName
        Type = "rewrite"
        Path = $pathText
        Deckeys = $deckeys
        DeckeySequence = Get-DeckeySequenceLiteral $deckeys
        Prev = $prevText
        Out = $outText
        Next = $nextText
        Normalized = $normalized
        Alternatives = @($normalized)
        RawLine = $lineText
    }
}

function Get-MatchesFromDump([string]$dumpPath, [string]$targetText) {
    if (-not (Test-Path $dumpPath)) {
        throw "dump ファイルが見つかりません: $dumpPath"
    }

    $tableName = Split-Path -Leaf $dumpPath
    $results = New-Object System.Collections.Generic.List[object]

    foreach ($line in Get-Content $dumpPath) {
        if ([string]::IsNullOrWhiteSpace($line)) { continue }

        $columns = @($line -split "`t")
        if ($columns.Count -lt 2) { continue }

        $head = $columns[0]
        $body = $columns[1]

        if ($body.StartsWith("@")) { continue }

        $entry = $null
        if ($body.StartsWith('"') -and $body.EndsWith('"')) {
            $rawText = $body.Substring(1, $body.Length - 2)
            $entry = New-StringEntry -tableName $tableName -pathText $head -rawText $rawText -lineText $line
        }
        elseif ($columns.Count -ge 3) {
            $entry = New-RewriteEntry -tableName $tableName -head $head -outText $columns[1] -nextText $columns[2] -lineText $line
        }

        if ($null -eq $entry) { continue }

        if ($entry.Alternatives -contains $targetText -or $entry.Normalized -eq $targetText) {
            $results.Add($entry)
        }
    }

    return $results
}

$allMatches = New-Object System.Collections.Generic.List[object]
foreach ($dumpPath in $DumpPaths) {
    foreach ($entry in Get-MatchesFromDump -dumpPath $dumpPath -targetText $Target) {
        $allMatches.Add($entry)
    }
}

if ($allMatches.Count -eq 0) {
    throw "対象文字列に一致する候補が見つかりませんでした: $Target"
}

$allMatches |
    Sort-Object Table, Type, Path |
    ForEach-Object {
        if ($_.Type -eq "string") {
            Write-Output ("MATCH table={0} type={1} path={2} deckeys={3} out='{4}'" -f $_.Table, $_.Type, $_.Path, $_.DeckeySequence, $_.Out)
        }
        else {
            Write-Output ("MATCH table={0} type={1} path={2} deckeys={3} prev='{4}' out='{5}' next='{6}'" -f $_.Table, $_.Type, $_.Path, $_.DeckeySequence, $_.Prev, $_.Out, $_.Next)
        }
    }

if ($RunProbe) {
    if (-not (Test-Path $probeScript)) {
        throw "probe スクリプトが見つかりません: $probeScript"
    }
    if (-not (Test-Path $x86PowerShell)) {
        throw "32bit PowerShell が見つかりません: $x86PowerShell"
    }

    foreach ($entry in $allMatches) {
        if ($entry.Type -ne "string") { continue }

        $command = "& '$probeScript' -DeckeySequence $($entry.DeckeySequence)"
        Write-Output ("PROBE table={0} path={1} deckeys={2}" -f $entry.Table, $entry.Path, $entry.DeckeySequence)
        & $x86PowerShell -ExecutionPolicy Bypass -Command $command
    }
}
