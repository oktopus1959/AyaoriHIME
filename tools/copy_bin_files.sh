#!/bin/bash

. ~/bin/debug_util.sh

RUN_CMD -m "mkdir -p ../bin/tsf/{Win32,x64}"
RUN_CMD -m "mkdir -p ../publish/bin/tsf/{Win32,x64}"
RUN_CMD -m "cp -p tools/register_tsf_dll.bat ../bin/"
RUN_CMD -m "cp -p tools/register_tsf_dll.bat ../publish/bin/"

RUN_CMD -m "cd bin/Release"

RUN_CMD -m "cp -p AyaoriHIME.exe* DyMazinLib.dll kw-uni.dll Utils.dll ../../../bin/"
RUN_CMD -m "cp -p tsf/Win32/AyaoriHimeTsfTextService.dll ../../../bin/tsf/Win32"
RUN_CMD -m "cp -p tsf/x64/AyaoriHimeTsfTextService.dll ../../../bin/tsf/x64"

RUN_CMD -m "cp -p AyaoriHIME.exe* DyMazinLib.dll kw-uni.dll Utils.dll ../../../publish/bin/"
RUN_CMD -m "cp -p tsf/Win32/AyaoriHimeTsfTextService.dll ../../../publish/bin/tsf/Win32"
RUN_CMD -m "cp -p tsf/x64/AyaoriHimeTsfTextService.dll ../../../publish/bin/tsf/x64"

RUN_CMD -m "cd ../.."
