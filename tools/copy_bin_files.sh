#!/bin/bash

. ~/bin/debug_util.sh

RUN_CMD -m "cd bin/Release"

RUN_CMD -m "cp -p AyaoriHIME.exe* DyMazinLib.dll kw-uni.dll NgramLib.dll Utils.dll ../../../publish/bin/"
