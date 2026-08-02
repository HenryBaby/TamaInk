$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$out = Join-Path $PSScriptRoot 'rom_tests.exe'
if (-not (Get-Command g++ -ErrorAction SilentlyContinue)) {
  Write-Error 'g++ compiler not found on PATH; host ROM tests were not run.'
}
try {
  g++ -std=c++17 -Wall -Wextra -Werror -I (Join-Path $root 'include') (Join-Path $root 'src/tamaink_rom.cpp') (Join-Path $PSScriptRoot 'test_rom.cpp') -o $out
  & $out
  if ($LASTEXITCODE -ne 0) { throw "ROM host tests failed with exit code $LASTEXITCODE" }
} finally {
  if (Test-Path $out) { Remove-Item -Force $out }
}
