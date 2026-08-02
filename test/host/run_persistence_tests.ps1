$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$out = Join-Path $PSScriptRoot 'persistence_tests.exe'
if (-not (Get-Command g++ -ErrorAction SilentlyContinue)) {
  Write-Error 'g++ compiler not found on PATH; host persistence tests were not run.'
}
try {
  g++ -std=c++17 -Wall -Wextra -Werror -I (Join-Path $root 'include') (Join-Path $root 'src/tamaink_persistence.cpp') (Join-Path $root 'src/tamaink_emulator_state.cpp') (Join-Path $PSScriptRoot 'test_persistence.cpp') -o $out
  & $out
} finally {
  if (Test-Path $out) { Remove-Item -Force $out }
}
