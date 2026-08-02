$ErrorActionPreference = 'Stop'
g++ -std=c++17 -Wall -Wextra -Werror -I include test/host/wake_diagnostic_test.cpp -o "$env:TEMP\tamaink-wake-tests.exe"
& "$env:TEMP\tamaink-wake-tests.exe"
