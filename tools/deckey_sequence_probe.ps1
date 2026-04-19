param(
    [int[]]$DeckeySequence = @(21, 27, 28, 23),
    [string]$InitialEdit = "",
    [int]$LogLevel = 5,
    [string]$DllPath = "",
    [string]$SettingsLogPath = "",
    [switch]$SkipSaveTraceLog
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$srcDir = Split-Path -Parent $scriptDir
$repoRoot = Split-Path -Parent $srcDir

if ([Environment]::Is64BitProcess) {
    throw "32bit の Windows PowerShell で実行してください: C:\Windows\SysWOW64\WindowsPowerShell\v1.0\powershell.exe"
}

if (-not $SettingsLogPath) {
    $SettingsLogPath = Join-Path $srcDir "kw-uni.log"
}

if (-not $DllPath) {
    $DllPath = Join-Path $srcDir "bin\Release\kw-uni.dll"
}

$dllImportPath = $DllPath.Replace('\', '\\').Replace('"', '\"')
$outputLogPath = Join-Path $srcDir "kw-uni.log"

Push-Location $repoRoot
try {
    Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

public static class DeckeySequenceProbeNative {
    [StructLayout(LayoutKind.Sequential)]
    public struct DecoderCommandParams {
        [MarshalAs(UnmanagedType.ByValArray, ArraySubType = UnmanagedType.U2, SizeConst = 2048)]
        public char[] inOutData;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct DecoderHandleDeckeyParams {
        [MarshalAs(UnmanagedType.ByValArray, ArraySubType = UnmanagedType.U2, SizeConst = 64)]
        public char[] editBufferData;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct DecoderOutParams {
        public int numBackSpaces;
        public uint resultFlags;
        public int nextExpectedKeyType;
        public int strokeCount;
        public int nextStrokeDeckey;
        public int nextSelectPos;
        public int strokeTableNum;
        [MarshalAs(UnmanagedType.ByValArray, ArraySubType = UnmanagedType.U2, SizeConst = 100)]
        public char[] outString;
        public int layout;
        [MarshalAs(UnmanagedType.ByValArray, ArraySubType = UnmanagedType.U2, SizeConst = 32)]
        public char[] topString;
        [MarshalAs(UnmanagedType.ByValArray, ArraySubType = UnmanagedType.U2, SizeConst = 20)]
        public char[] centerString;
        [MarshalAs(UnmanagedType.ByValArray, ArraySubType = UnmanagedType.U2, SizeConst = 200)]
        public char[] faceStrings;
        [MarshalAs(UnmanagedType.ByValArray, ArraySubType = UnmanagedType.U2, SizeConst = 200)]
        public char[] candidateStrings;
    }

    [DllImport("$dllImportPath", CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr CreateDecoder(int logLevel);

    [DllImport("$dllImportPath", CallingConvention = CallingConvention.Cdecl)]
    public static extern int InitializeDecoder(IntPtr decoder, IntPtr decParams);

    [DllImport("$dllImportPath", CallingConvention = CallingConvention.Cdecl)]
    public static extern int ExecCmdDecoder(IntPtr decoder, IntPtr decParam, ref DecoderOutParams outParams);

    [DllImport("$dllImportPath", CallingConvention = CallingConvention.Cdecl)]
    public static extern int HandleDeckeyDecoder(IntPtr decoder, IntPtr decParam, int keyId, uint targetChar, int inputFlags, ref DecoderOutParams outParams);

    [DllImport("$dllImportPath", CallingConvention = CallingConvention.Cdecl)]
    public static extern int FinalizeDecoder(IntPtr decoder);
}
"@

    function New-OutParams {
        $o = New-Object DeckeySequenceProbeNative+DecoderOutParams
        $o.outString = New-Object 'char[]' 100
        $o.topString = New-Object 'char[]' 32
        $o.centerString = New-Object 'char[]' 20
        $o.faceStrings = New-Object 'char[]' 200
        $o.candidateStrings = New-Object 'char[]' 200
        return $o
    }

    function New-CmdPtr([string]$text) {
        $p = New-Object DeckeySequenceProbeNative+DecoderCommandParams
        $p.inOutData = New-Object 'char[]' 2048
        if ($text) {
            $chars = $text.ToCharArray()
            [Array]::Copy($chars, 0, $p.inOutData, 0, [Math]::Min($chars.Length, $p.inOutData.Length - 1))
        }
        $size = [Runtime.InteropServices.Marshal]::SizeOf([type][DeckeySequenceProbeNative+DecoderCommandParams])
        $ptr = [Runtime.InteropServices.Marshal]::AllocCoTaskMem($size)
        [Runtime.InteropServices.Marshal]::StructureToPtr($p, $ptr, $false)
        return $ptr
    }

    function New-DeckeyPtr([string]$text) {
        $p = New-Object DeckeySequenceProbeNative+DecoderHandleDeckeyParams
        $p.editBufferData = New-Object 'char[]' 64
        if ($text) {
            $chars = $text.ToCharArray()
            [Array]::Copy($chars, 0, $p.editBufferData, 0, [Math]::Min($chars.Length, $p.editBufferData.Length - 1))
        }
        $size = [Runtime.InteropServices.Marshal]::SizeOf([type][DeckeySequenceProbeNative+DecoderHandleDeckeyParams])
        $ptr = [Runtime.InteropServices.Marshal]::AllocCoTaskMem($size)
        [Runtime.InteropServices.Marshal]::StructureToPtr($p, $ptr, $false)
        return $ptr
    }

    function CharsToString($chars) {
        if (-not $chars) { return "" }
        $n = [Array]::IndexOf($chars, [char]0)
        if ($n -lt 0) { $n = $chars.Length }
        if ($n -eq 0) { return "" }
        return -join $chars[0..($n - 1)]
    }

    function Send-Cmd([IntPtr]$decoder, [string]$cmd, [ref]$outRef) {
        $ptr = New-CmdPtr $cmd
        try {
            [void][DeckeySequenceProbeNative]::ExecCmdDecoder($decoder, $ptr, $outRef)
        } finally {
            [Runtime.InteropServices.Marshal]::FreeCoTaskMem($ptr)
        }
    }

    function Get-SerializedDecoderSettings([string]$logPath) {
        if (-not (Test-Path $logPath)) {
            throw "settings 抽出元ログが見つかりません: $logPath"
        }
        $lines = Get-Content $logPath
        $match = $lines | Select-String 'ENTER: settings=logLevel=' | Select-Object -First 1
        $start = if ($match) { $match.LineNumber } else { $null }
        if (-not $start) {
            throw "settings ブロックが見つかりません: $logPath"
        }
        $idx = $start - 1
        $settingsLines = New-Object System.Collections.Generic.List[string]
        $settingsLines.Add(($lines[$idx] -replace '^.*settings=', ''))
        for ($i = $idx + 1; $i -lt $lines.Length; ++$i) {
            if ($lines[$i] -match '^\d{4}/\d{2}/\d{2} ') { break }
            $settingsLines.Add($lines[$i])
        }
        return [string]::Join("`n", $settingsLines)
    }

    $settings = Get-SerializedDecoderSettings $SettingsLogPath
    if ($settings -match '(^|\n)logLevel=.*?(\n|$)') {
        $settings = [regex]::Replace($settings, '(^|\n)logLevel=.*?(\n|$)', "`$1logLevel=$LogLevel`$2", 1)
    } else {
        $settings = "logLevel=$LogLevel`n$settings"
    }

    $out = New-OutParams
    $decoder = [DeckeySequenceProbeNative]::CreateDecoder($LogLevel)
    if ($decoder -eq [IntPtr]::Zero) {
        throw "CreateDecoder() に失敗しました"
    }

    try {
        $ptr = New-CmdPtr ""
        try {
            [void][DeckeySequenceProbeNative]::InitializeDecoder($decoder, $ptr)
        } finally {
            [Runtime.InteropServices.Marshal]::FreeCoTaskMem($ptr)
        }

        $payloadLimit = 1900
        $pos = 0
        $firstChunk = $true
        while ($pos -lt $settings.Length) {
            $prefix = if ($firstChunk) { "presendSettings`ttrue`t" } else { "presendSettings`tfalse`t" }
            $chunkLen = [Math]::Min($payloadLimit - $prefix.Length, $settings.Length - $pos)
            $chunk = $settings.Substring($pos, $chunkLen)
            Send-Cmd $decoder ($prefix + $chunk) ([ref]$out)
            $pos += $chunkLen
            $firstChunk = $false
        }

        Send-Cmd $decoder "initializeDecoder" ([ref]$out)
        Send-Cmd $decoder ("setLogLevel`t{0}" -f $LogLevel) ([ref]$out)

        $buffer = $InitialEdit
        foreach ($deckey in $DeckeySequence) {
            $edit = $buffer
            $out = New-OutParams
            $ptr = New-DeckeyPtr $edit
            try {
                [void][DeckeySequenceProbeNative]::HandleDeckeyDecoder($decoder, $ptr, $deckey, 0, 0, [ref]$out)
            } finally {
                [Runtime.InteropServices.Marshal]::FreeCoTaskMem($ptr)
            }

            $outStr = CharsToString $out.outString
            $numBS = [int]$out.numBackSpaces
            if ($numBS -gt 0 -and $buffer.Length -ge $numBS) {
                $buffer = $buffer.Substring(0, $buffer.Length - $numBS)
            }
            $buffer += $outStr
            Write-Output ("STEP key={0} edit='{1}' out='{2}' numBS={3} buffer='{4}'" -f $deckey, $edit, $outStr, $numBS, $buffer)
        }

        if (-not $SkipSaveTraceLog) {
            Send-Cmd $decoder "saveTraceLog" ([ref]$out)
        }

        Write-Output ("FINAL buffer='{0}'" -f $buffer)
        Write-Output ("TRACE_LOG={0}" -f $outputLogPath)
    }
    finally {
        [void][DeckeySequenceProbeNative]::FinalizeDecoder($decoder)
    }
}
finally {
    Pop-Location
}
