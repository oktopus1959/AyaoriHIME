# AGENTS.d/permissions.md

## Trusted Workspace

This repository is a trusted workspace.

### Allowed Root
- `F:\Dev\CSharp\AyaoriHIME\src`

### Rules
- No confirmation is required for commands executed under this directory.
- Only read operations are allowed.
- Commands may be executed repeatedly without re-prompting.
- As an exception for verification, `tools/deckey_sequence_probe.ps1` may be executed without re-prompting when the purpose is to inspect step-by-step output for a `deckey` sequence.
- The preferred launcher for that script is `C:\Windows\SysWOW64\WindowsPowerShell\v1.0\powershell.exe`.

### Typical Commands
- sed -n '1,200p'
- rg
- git status / diff / log
- `E:\Programs\Microsoft\VisualStudio2022\MSBuild\Current\Bin\MSBuild.exe`
- `C:\Windows\SysWOW64\WindowsPowerShell\v1.0\powershell.exe -ExecutionPolicy Bypass -File F:\Dev\CSharp\AyaoriHIME\src\tools\deckey_sequence_probe.ps1`
