param(
  [ValidateSet("Debug","Release")]
  [string]$Config = "Release"
)

$ErrorActionPreference = "Stop"

$Root = Resolve-Path "$PSScriptRoot\.."
$Src  = Join-Path $Root "External\Protobuf"
$Out  = Join-Path $Root "External\ProtobufBuild\Win64"
$Bld  = Join-Path $Root "Intermediate\Protobuf\Win64"

New-Item -ItemType Directory -Force -Path $Bld | Out-Null
New-Item -ItemType Directory -Force -Path $Out | Out-Null

Push-Location $Bld

# Visual Studio 2022 기준. (다르면 generator만 바꿔주면 됨)
cmake -S $Src -B $Bld -G "Visual Studio 17 2022" -A x64 `
  -Dprotobuf_BUILD_TESTS=OFF `
  -Dprotobuf_BUILD_SHARED_LIBS=OFF `
  -Dprotobuf_WITH_ZLIB=OFF `
  -DCMAKE_INSTALL_PREFIX="$Out"

cmake --build $Bld --config $Config --target install

Pop-Location

Write-Host "Done. Installed to: $Out"
