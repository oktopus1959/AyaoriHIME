param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    [ValidateSet("Win32", "x64")]
    [string]$Platform = "x64",
    [string]$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
)

$ErrorActionPreference = "Stop"

$dllPath = Join-Path $Root "bin\$Configuration\tsf\$Platform\AyaoriHimeTsfTextService.dll"
if (!(Test-Path $dllPath)) {
    throw "TSF DLL not found: $dllPath"
}

$resolvedDllPath = (Resolve-Path $dllPath).Path
$holders = foreach ($process in Get-Process) {
    try {
        foreach ($module in $process.Modules) {
            if ([string]::Equals($module.FileName, $resolvedDllPath, [StringComparison]::OrdinalIgnoreCase)) {
                [PSCustomObject]@{
                    ProcessName = $process.ProcessName
                    Id          = $process.Id
                    Module      = $module.FileName
                }
                break
            }
        }
    } catch {
        # 権限不足などでモジュール一覧を取得できないプロセスは確認対象外とする。
    }
}

if ($holders) {
    $holders | Sort-Object ProcessName, Id | Format-Table -AutoSize
} else {
    Write-Host "TSF DLL をロードしている確認可能なプロセスはありません: $resolvedDllPath"
}
