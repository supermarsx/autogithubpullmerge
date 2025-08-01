$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$RootDir = Resolve-Path (Join-Path $ScriptDir '..')
Set-Location $RootDir

$BuildDir = Join-Path $RootDir 'build_gpp'
New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null

$srcFiles = Get-ChildItem -Path (Join-Path $RootDir 'src') -Filter *.cpp -Recurse | ForEach-Object { $_.FullName }
$srcList = $srcFiles -join ' '

$include = Join-Path $RootDir 'include'
$libsDir  = Join-Path $RootDir 'libs'
$includeArgs = "-I`"$include`" -I`"$libsDir\CLI11\include`" -I`"$libsDir\json\include`" -I`"$libsDir\spdlog\include`" -I`"$libsDir\yaml-cpp\include`" -I`"$libsDir\libyaml\include`" -I`"$libsDir\pdcurses`" -I`"$libsDir\curl\include`" -I`"$libsDir\sqlite`""

$libArgs = "-L`"$libsDir\curl\lib`" -L`"$libsDir\sqlite`" -L`"$libsDir\yaml-cpp\lib`" -L`"$libsDir\libyaml\lib`" -L`"$libsDir\pdcurses\lib`""

$cmd = "g++ -std=c++20 -Wall -Wextra -O2 -static -static-libgcc -static-libstdc++ -DYAML_CPP_STATIC_DEFINE $includeArgs $srcList $libArgs -lcurl -lsqlite3 -lyaml-cpp -lyaml -lpdcurses -o `"$BuildDir\autogithubpullmerge.exe`""
Invoke-Expression $cmd
