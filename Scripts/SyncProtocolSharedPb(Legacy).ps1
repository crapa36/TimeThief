$ErrorActionPreference = "Stop"

$Root = Resolve-Path "$PSScriptRoot\.."

$Src = Join-Path $Root "External\ProtocolShared\cpp"
$Dst = Join-Path $Root "Source\ProtocolSharedUE\Private\Generated"

if (!(Test-Path $Src)) { throw "Source not found: $Src" }

New-Item -ItemType Directory -Force -Path $Dst | Out-Null

# 목적: pb.h/pb.cc만 미러링 (기존 파일 삭제 후 복사)
Get-ChildItem -Path $Dst -Recurse -File | Remove-Item -Force

Copy-Item -Path (Join-Path $Src "*") -Destination $Dst -Recurse -Force

Write-Host "Synced ProtocolShared pb files:"
Write-Host "  from: $Src"
Write-Host "  to  : $Dst"