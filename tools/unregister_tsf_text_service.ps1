param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [string]$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
)

$ErrorActionPreference = "Stop"

$x86Dll = Join-Path $Root "bin\$Configuration\tsf\Win32\AyaoriHimeTsfTextService.dll"
$x64Dll = Join-Path $Root "bin\$Configuration\tsf\x64\AyaoriHimeTsfTextService.dll"

function Test-IsAdministrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

if (!(Test-IsAdministrator)) {
    throw "TSF text service の profile/category 登録解除には管理者権限が必要です。管理者として PowerShell を開き直してから、このスクリプトを実行してください。"
}

function Invoke-Regsvr32Unregister {
    param(
        [Parameter(Mandatory=$true)][string]$Regsvr32Path,
        [Parameter(Mandatory=$true)][string]$DllPath,
        [Parameter(Mandatory=$true)][string]$Label
    )

    $proc = Start-Process -FilePath $Regsvr32Path -ArgumentList @("/s", "/u", $DllPath) -Wait -PassThru -WindowStyle Hidden
    if ($proc.ExitCode -ne 0) {
        throw "$Label regsvr32 /u failed: exitCode=$($proc.ExitCode), dll=$DllPath"
    }
}

function Get-DllHolders {
    param([Parameter(Mandatory=$true)][string[]]$DllPaths)

    $resolvedPaths = @($DllPaths | Where-Object { Test-Path $_ } | ForEach-Object { (Resolve-Path $_).Path })
    if (!$resolvedPaths) { return @() }

    $holders = foreach ($process in Get-Process) {
        try {
            foreach ($module in $process.Modules) {
                if ($resolvedPaths | Where-Object { [string]::Equals($module.FileName, $_, [StringComparison]::OrdinalIgnoreCase) }) {
                    [PSCustomObject]@{
                        ProcessName = $process.ProcessName
                        Id = $process.Id
                        Module = $module.FileName
                    }
                    break
                }
            }
        } catch {
            # 管理者でも保護プロセスのモジュール一覧は取得できない場合がある。
        }
    }
    return @($holders)
}

$holders = @(Get-DllHolders @($x86Dll, $x64Dll))
if ($holders.Count -gt 0) {
    $details = ($holders | Sort-Object ProcessName, Id | ForEach-Object {
        "{0} (PID {1}): {2}" -f $_.ProcessName, $_.Id, $_.Module
    }) -join [Environment]::NewLine
    throw "TSF DLL はプロセスにロード中のため、安全のため登録解除を中止しました。対象アプリを終了してから再実行してください。`n$details"
}

if (Test-Path $x86Dll) {
    Invoke-Regsvr32Unregister "$env:WINDIR\SysWOW64\regsvr32.exe" $x86Dll "32bit"
}

if (Test-Path $x64Dll) {
    Invoke-Regsvr32Unregister "$env:WINDIR\System32\regsvr32.exe" $x64Dll "64bit"
}

Write-Host "Unregistered AyaoriHIME TSF text service."
