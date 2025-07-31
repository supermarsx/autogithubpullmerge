$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$RootDir = Join-Path $ScriptDir ".."
$BuildDir = Join-Path $RootDir "build_gpp"
New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null

$srcFiles = Get-ChildItem -Path (Join-Path $RootDir "src") -Filter *.cpp -Recurse | ForEach-Object { $_.FullName }
$include = Join-Path $RootDir "include"
$srcList = $srcFiles -join ' '

$cmd = "g++ -std=c++20 -Wall -Wextra -O2 -I`"$include`" $srcList -lcurl -lsqlite3 -lyaml-cpp -lspdlog -lncurses -o `"$BuildDir\autogithubpullmerge.exe`""
Invoke-Expression $cmd
