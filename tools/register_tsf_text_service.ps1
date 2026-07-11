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
    throw "TSF text service の profile/category 登録には管理者権限が必要です。管理者として PowerShell を開き直してから、このスクリプトを実行してください。"
}

if (!(Test-Path $x86Dll)) { throw "32bit TSF DLL not found: $x86Dll" }
if (!(Test-Path $x64Dll)) { throw "64bit TSF DLL not found: $x64Dll" }

function Invoke-Regsvr32 {
    param(
        [Parameter(Mandatory=$true)][string]$Regsvr32Path,
        [Parameter(Mandatory=$true)][string]$DllPath,
        [Parameter(Mandatory=$true)][string]$Label
    )

    $proc = Start-Process -FilePath $Regsvr32Path -ArgumentList @("/s", $DllPath) -Wait -PassThru -WindowStyle Hidden
    if ($proc.ExitCode -ne 0) {
        throw "$Label regsvr32 failed: exitCode=$($proc.ExitCode), dll=$DllPath"
    }
}

Invoke-Regsvr32 "$env:WINDIR\SysWOW64\regsvr32.exe" $x86Dll "32bit"
Invoke-Regsvr32 "$env:WINDIR\System32\regsvr32.exe" $x64Dll "64bit"

Write-Host "Registered AyaoriHIME TSF text service."
