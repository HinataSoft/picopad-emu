<#
.SYNOPSIS
  Builds a Picopad game that lives outside the PicoLibSDK tree into <GameDir>\<NAME>.uf2.
.EXAMPLE
  powershell -NoProfile -ExecutionPolicy Bypass -File tools\build_device.ps1 -GameDir examples\HELLO -Device picopad10
.NOTES
  TARGET = name of GameDir. Nothing is written into the SDK tree (no copy to !PicoPad10).
  Needs arm-none-eabi-gcc (prepended to PATH from Program Files if not found).
#>
param(
    [Parameter(Mandatory = $true)][string]$GameDir,
    [string]$Device = 'picopad10',
    [string]$SdkDir = (Join-Path $PSScriptRoot '..\..\PicoLibSDK')
)
$ErrorActionPreference = 'Stop'

# DEVICE/DEVCLASS as in the SDK _setup.bat
$devclass = switch ($Device) {
    'picopad10' { 'picopad' }
    'picopad08' { 'picopad' }
    'picopad20' { 'picopad' }
    default { throw "unsupported device '$Device' (use picopad10)" }
}

$sdk = (Resolve-Path $SdkDir).Path
$game = (Resolve-Path $GameDir).Path
$target = Split-Path $game -Leaf
$make = Join-Path $sdk '_tools\make.exe'
$crc = Join-Path $sdk '_tools\PicoPadLoaderCrc\LoaderCrc.exe'
foreach ($f in @($make, $crc)) { if (-not (Test-Path $f)) { throw "missing $f" } }

if (-not (Get-Command arm-none-eabi-gcc -ErrorAction SilentlyContinue)) {
    $gcc = Get-ChildItem 'C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\*\bin\arm-none-eabi-gcc.exe' -ErrorAction SilentlyContinue |
        Sort-Object FullName -Descending | Select-Object -First 1
    if (-not $gcc) { throw 'arm-none-eabi-gcc not found' }
    $env:PATH = (Split-Path $gcc.FullName) + ';' + $env:PATH
}

$env:TARGET = $target
$env:GRPDIR = 'GAME'
$env:DEVICE = $Device
$env:DEVCLASS = $devclass
$env:MEMMAP = ''

Push-Location $game
try {
    foreach ($ext in 'uf2', 'bin') { Remove-Item "$target.$ext" -ErrorAction SilentlyContinue }
    $root = $sdk.Replace([string][char]92, '/')
    & $make "PICO_ROOT_PATH=$root" all
    if ($LASTEXITCODE -ne 0) { throw "make failed ($LASTEXITCODE)" }
    if (-not (Test-Path "$target.bin")) { throw "$target.bin was not created" }
    & $crc "$target.bin" "$target.uf2"
    if ($LASTEXITCODE -ne 0) { throw "LoaderCrc failed ($LASTEXITCODE)" }
    if (-not (Test-Path "$target.uf2")) { throw "$target.uf2 was not created" }
    Write-Host "OK: $(Join-Path $game "$target.uf2")"
}
finally { Pop-Location }
