param(
    [string]$Configuration = "Release",
    [string]$Platform = "x86",
    [int[]]$DeckeySequence = @(21, 27, 28, 23),
    [string]$PrefixEdit = "きみ",
    [int]$MazeDeckey = 3039,
    [int]$LogLevel = 5,
    [string]$ExpectedMazeOut = "莫迦なの",
    [int]$ExpectedNumBackSpaces = 4,
    [string[]]$TraceMustContain = @("莫迦なの"),
    [string]$SolutionPath = "",
    [string]$ProbeScriptPath = "",
    [string]$DllPath = "",
    [string]$SettingsLogPath = "",
    [switch]$SkipBuild,
    [switch]$SkipTraceValidation
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$srcDir = Split-Path -Parent $scriptDir
$repoRoot = Split-Path -Parent $srcDir

if (-not $SolutionPath) {
    $SolutionPath = Join-Path $srcDir "AyaoriHIME.sln"
}

if (-not $ProbeScriptPath) {
    $ProbeScriptPath = Join-Path $scriptDir "maze_probe.ps1"
}

if (-not $DllPath) {
    $DllPath = Join-Path $srcDir ("bin\{0}\kw-uni.dll" -f $Configuration)
}

if (-not $SettingsLogPath) {
    $SettingsLogPath = Join-Path $srcDir "kw-uni.log"
}

$traceLogPath = Join-Path $srcDir "kw-uni.log"

function Resolve-MSBuildPath {
    $cmd = Get-Command msbuild.exe -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    $vswhere = "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $found = & $vswhere -latest -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" 2>$null |
            Select-Object -First 1
        if ($found -and (Test-Path $found)) {
            return $found
        }
    }

    $candidates = @(
        "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
    )

    foreach ($path in $candidates) {
        if (Test-Path $path) {
            return $path
        }
    }

    throw "MSBuild.exe が見つかりません。Visual Studio 2022 または Build Tools を確認してください。"
}

function Invoke-KwUniBuild {
    param(
        [string]$MsbuildPath,
        [string]$Solution,
        [string]$Config,
        [string]$Plat
    )

    Write-Output ("BUILD solution='{0}' configuration='{1}' platform='{2}'" -f $Solution, $Config, $Plat)

    $binDir = Join-Path $srcDir ("bin\{0}" -f $Config)
    $dymazinReleaseDir = Join-Path $srcDir "DyMazin\DyMazinLib\Release"
    $kwUniReleaseDir = Join-Path $srcDir "kw-uni\Release"
    $staleOutputs = @(
        (Join-Path $binDir "DyMazinLib.dll"),
        (Join-Path $binDir "DyMazinLib.lib"),
        (Join-Path $binDir "DyMazinLib.exp"),
        (Join-Path $binDir "kw-uni.dll"),
        (Join-Path $binDir "kw-uni.lib"),
        (Join-Path $binDir "kw-uni.exp"),
        (Join-Path $dymazinReleaseDir "DyMazinLib.iobj"),
        (Join-Path $dymazinReleaseDir "DyMazinLib.ipdb"),
        (Join-Path $kwUniReleaseDir "kw-uni.iobj"),
        (Join-Path $kwUniReleaseDir "kw-uni.ipdb")
    )
    $gitBashPath = "C:\Program Files\Git\bin\bash.exe"
    $gitRmPath = "C:\Program Files\Git\usr\bin\rm.exe"
    foreach ($path in $staleOutputs) {
        if (Test-Path $path) {
            try {
                Remove-Item -Force $path
                Write-Output ("CLEAN removed='{0}'" -f $path)
            }
            catch {
                $removedByGitTools = $false

                if (Test-Path $gitRmPath) {
                    & $gitRmPath -f $path
                    if ($LASTEXITCODE -eq 0 -and -not (Test-Path $path)) {
                        $removedByGitTools = $true
                        Write-Output ("CLEAN removed-by-git-rm='{0}'" -f $path)
                    }
                }

                if (-not $removedByGitTools -and -not (Test-Path $gitBashPath)) {
                    throw "ビルド前削除に失敗しました: $path"
                }

                if (-not $removedByGitTools) {
                    $bashTarget = $path.Replace('\', '/')
                    & $gitBashPath -lc "rm -f '$bashTarget'"
                }

                if (Test-Path $path) {
                    throw "ビルド前削除に失敗しました: $path"
                }
                if (-not $removedByGitTools) {
                    Write-Output ("CLEAN removed-by-git-bash='{0}'" -f $path)
                }
            }
        }
    }

    $mergedPath = @(
        [System.Environment]::GetEnvironmentVariable("Path", "Machine"),
        [System.Environment]::GetEnvironmentVariable("Path", "User")
    ) | Where-Object { $_ } | Select-Object -Unique
    $normalizedPath = ($mergedPath -join ";")

    $envMap = @{
        Path = $normalizedPath
    }

    foreach ($name in @("SystemRoot", "ComSpec", "TEMP", "TMP", "PATHEXT", "NUMBER_OF_PROCESSORS", "PROCESSOR_ARCHITECTURE")) {
        $value = [System.Environment]::GetEnvironmentVariable($name, "Process")
        if ($value) {
            $envMap[$name] = $value
        }
    }

    $projectBuilds = @(
        @{
            Label = "DyMazinLib"
            ProjectPath = Join-Path $srcDir "DyMazin\DyMazinLib\DyMazinLib.vcxproj"
            Platform = "Win32"
        },
        @{
            Label = "NgramCoreLib"
            ProjectPath = Join-Path $srcDir "NgramAnalyzer\NgramCoreLib\NgramCoreLib.vcxproj"
            Platform = "Win32"
        },
        @{
            Label = "kw-uni"
            ProjectPath = Join-Path $srcDir "kw-uni\kw-uni.vcxproj"
            Platform = "Win32"
        }
    )

    $originalPath = [System.Environment]::GetEnvironmentVariable("Path", "Process")
    $originalUpperPath = [System.Environment]::GetEnvironmentVariable("PATH", "Process")
    try {
        [System.Environment]::SetEnvironmentVariable("Path", $normalizedPath, "Process")
        [System.Environment]::SetEnvironmentVariable("PATH", $null, "Process")

        foreach ($build in $projectBuilds) {
            Write-Output ("BUILD-PROJECT name='{0}' project='{1}'" -f $build.Label, $build.ProjectPath)
            $args = @(
                $build.ProjectPath,
                "/nologo",
                "/p:Configuration=$Config",
                "/p:Platform=$($build.Platform)"
            )
            & $MsbuildPath @args
            if ($LASTEXITCODE -ne 0) {
                throw "kw-uni.dll のビルドに失敗しました。project=$($build.Label) exitCode=$LASTEXITCODE"
            }
        }
    }
    finally {
        [System.Environment]::SetEnvironmentVariable("Path", $originalPath, "Process")
        [System.Environment]::SetEnvironmentVariable("PATH", $originalUpperPath, "Process")
    }
}

function Invoke-MazeProbe {
    param(
        [string]$ProbePath,
        [string]$Dll,
        [string]$SettingsPath,
        [int[]]$Deckeys,
        [string]$Prefix,
        [int]$MazeKey,
        [int]$Level
    )

    $x86PowerShell = "C:\Windows\SysWOW64\WindowsPowerShell\v1.0\powershell.exe"
    if (-not (Test-Path $x86PowerShell)) {
        throw "32bit PowerShell が見つかりません: $x86PowerShell"
    }

    $deckeyCsv = ($Deckeys | ForEach-Object { "$_" }) -join ","
    $command = "& '$ProbePath' -LogLevel $Level -DllPath '$Dll' -MazeDeckey $MazeKey -PrefixEdit '$Prefix' -SettingsLogPath '$SettingsPath' -DeckeySequence $deckeyCsv"
    $args = @(
        "-ExecutionPolicy", "Bypass",
        "-Command", $command
    )

    Write-Output ("PROBE script='{0}'" -f $ProbePath)
    $output = & $x86PowerShell @args 2>&1
    $exitCode = $LASTEXITCODE
    foreach ($line in $output) {
        Write-Output $line
    }
    if ($exitCode -ne 0) {
        throw "maze_probe.ps1 の実行に失敗しました。exitCode=$exitCode"
    }

    return ,$output
}

function Assert-ProbeResult {
    param(
        [string[]]$OutputLines,
        [string]$ExpectedOut,
        [int]$ExpectedBs,
        [string]$TracePath,
        [string[]]$TraceNeedles,
        [switch]$SkipTraceCheck
    )

    $mazeLine = $OutputLines | Where-Object { $_ -like "MAZE key=*" } | Select-Object -Last 1
    if (-not $mazeLine) {
        throw "maze_probe.ps1 の出力に MAZE 行が見つかりません。"
    }
    if ($mazeLine -notmatch "^MAZE key=\d+ edit='.*' out='(.*)' numBS=(\d+)$") {
        throw "MAZE 行の形式が想定外です: $mazeLine"
    }

    $actualOut = $Matches[1]
    $actualBs = [int]$Matches[2]

    if ($actualOut -ne $ExpectedOut) {
        throw "MazeConversion の出力が想定と異なります。expected='$ExpectedOut' actual='$actualOut'"
    }
    if ($actualBs -ne $ExpectedBs) {
        throw "MazeConversion の numBS が想定と異なります。expected=$ExpectedBs actual=$actualBs"
    }

    $traceLine = $OutputLines | Where-Object { $_ -like "TRACE_LOG=*" } | Select-Object -Last 1
    if (-not $traceLine) {
        throw "maze_probe.ps1 の出力に TRACE_LOG 行が見つかりません。"
    }

    if (-not (Test-Path $TracePath)) {
        throw "トレースログが見つかりません: $TracePath"
    }

    if (-not $SkipTraceCheck) {
        foreach ($needle in $TraceNeedles) {
            if (-not (Select-String -Path $TracePath -Pattern $needle -SimpleMatch -Quiet)) {
                throw "トレースログに期待文字列が見つかりません: '$needle'"
            }
        }
    }

    Write-Output ("ASSERT mazeOut='{0}' numBS={1} trace='{2}'" -f $actualOut, $actualBs, $TracePath)
}

if (-not (Test-Path $SolutionPath)) {
    throw "solution が見つかりません: $SolutionPath"
}
if (-not (Test-Path $ProbeScriptPath)) {
    throw "probe script が見つかりません: $ProbeScriptPath"
}
if (-not (Test-Path $SettingsLogPath)) {
    throw "settings 抽出元ログが見つかりません: $SettingsLogPath"
}

if (-not $SkipBuild) {
    $msbuildPath = Resolve-MSBuildPath
    Invoke-KwUniBuild -MsbuildPath $msbuildPath -Solution $SolutionPath -Config $Configuration -Plat $Platform
}

if (-not (Test-Path $DllPath)) {
    throw "kw-uni.dll が見つかりません: $DllPath"
}

$probeOutput = Invoke-MazeProbe -ProbePath $ProbeScriptPath -Dll $DllPath -SettingsPath $SettingsLogPath -Deckeys $DeckeySequence -Prefix $PrefixEdit -MazeKey $MazeDeckey -Level $LogLevel

Assert-ProbeResult -OutputLines $probeOutput -ExpectedOut $ExpectedMazeOut -ExpectedBs $ExpectedNumBackSpaces -TracePath $traceLogPath -TraceNeedles $TraceMustContain -SkipTraceCheck:$SkipTraceValidation

Write-Output "DONE build_and_probe_kw_uni"
