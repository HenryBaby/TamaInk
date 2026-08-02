$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$out = Join-Path $PSScriptRoot 'autosave_tests.exe'
try {
  if (-not (Get-Command g++ -ErrorAction SilentlyContinue)) { throw 'g++ compiler not found on PATH' }
  g++ -std=c++17 -Wall -Wextra -Werror -I (Join-Path $root 'include') (Join-Path $PSScriptRoot 'test_autosave.cpp') -o $out
  & $out
} finally { if (Test-Path $out) { Remove-Item -Force $out } }
