param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration,
    [ValidateSet("Win32", "x64")]
    [string]$Platform,
    [string]$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
)

$ErrorActionPreference = "Stop"

$configurationSpecified = $PSBoundParameters.ContainsKey("Configuration")
$platformSpecified = $PSBoundParameters.ContainsKey("Platform")
$platforms = if ($PSBoundParameters.ContainsKey("Platform")) { @($Platform) } else { @("Win32", "x64") }

foreach ($targetPlatform in $platforms) {
    $relativeDllPath = if ($configurationSpecified) {
        "bin\$Configuration\tsf\$targetPlatform\AyaoriHimeTsfTextService.dll"
    } else {
        "bin\tsf\$targetPlatform\AyaoriHimeTsfTextService.dll"
    }
    $dllPath = Join-Path $Root $relativeDllPath
    Write-Host "[$targetPlatform] $dllPath"

    if (!(Test-Path $dllPath)) {
        if ($platformSpecified) {
            throw "TSF DLL not found: $dllPath"
        }
        Write-Host "TSF DLL が見つかりません。"
        Write-Host
        continue
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
        Write-Host "TSF DLL をロードしている確認可能なプロセスはありません。"
    }
    Write-Host
}
