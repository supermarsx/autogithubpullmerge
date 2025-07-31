$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$RootDir = Join-Path $ScriptDir ".."
$BuildDir = Join-Path $RootDir "build_gpp"
New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null

$srcFiles = Get-ChildItem -Path (Join-Path $RootDir "src") -Filter *.cpp -Recurse | ForEach-Object { $_.FullName }
$include = Join-Path $RootDir "include"
$libsDir = Join-Path $RootDir "libs"
$srcList = $srcFiles -join ' '
$includeArgs = "-I`"$include`" -I`"$libsDir\CLI11\include`" -I`"$libsDir\json\include`" -I`"$libsDir\spdlog\include`" -I`"$libsDir\yaml-cpp\include`""

$cmd = "g++ -std=c++20 -Wall -Wextra -O2 $includeArgs $srcList -lcurl -lsqlite3 -lyaml-cpp -lncurses -o `"$BuildDir\autogithubpullmerge.exe`""
Invoke-Expression $cmd
