@echo off
git submodule update --init --recursive
powershell -ExecutionPolicy Bypass -File ".\Scripts\BuildProtobuf-Win64.ps1"
pause