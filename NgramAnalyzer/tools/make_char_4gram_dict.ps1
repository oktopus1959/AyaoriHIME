param(
    [string]$InputPath = "F:\Dev\Text\ngram-geta\wiki_hplt.hiraganized.4gram.2m.txt",
    [string]$OutputPath = "",
    [string]$NgramerPath = ""
)

$ErrorActionPreference = "Stop"

$ngramAnalyzerDir = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $ngramAnalyzerDir "work\bin\char-4gram.bin"
}
if ([string]::IsNullOrWhiteSpace($NgramerPath)) {
    $NgramerPath = Join-Path (Split-Path -Parent $ngramAnalyzerDir) "bin\Release\ngramer.exe"
}

if (-not (Test-Path -LiteralPath $InputPath -PathType Leaf)) {
    throw "入力ファイルが見つかりません: $InputPath"
}
if (-not (Test-Path -LiteralPath $NgramerPath -PathType Leaf)) {
    throw "ngramer.exe が見つかりません: $NgramerPath"
}

$outputDir = Split-Path -Parent $OutputPath
if (-not [string]::IsNullOrWhiteSpace($outputDir)) {
    New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
}

& $NgramerPath make-char-4gram $InputPath $OutputPath
if ($LASTEXITCODE -ne 0) {
    throw "文字4-gram辞書の生成に失敗しました: exitCode=$LASTEXITCODE"
}

Get-Item -LiteralPath $OutputPath
