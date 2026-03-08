#!/bin/bash

. ~/bin/debug_util.sh

RUN_CMD -m "cd bin/Release"

RUN_CMD -m "cp -p AyaoriHIME.exe* DyMazinLib.dll kw-uni.dll Utils.dll ../../../bin/"
RUN_CMD -m "cp -p AyaoriHIME.exe* DyMazinLib.dll kw-uni.dll Utils.dll ../../../publish/bin/"
