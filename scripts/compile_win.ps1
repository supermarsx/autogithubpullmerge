$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$RootDir = Join-Path $ScriptDir ".."
$BuildDir = Join-Path $RootDir "build_gpp"
New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null

$srcFiles = Get-ChildItem -Path (Join-Path $RootDir "src") -Filter *.cpp -Recurse | ForEach-Object { $_.FullName }
$include = Join-Path $RootDir "include"
$libsDir = Join-Path $RootDir "libs"
$srcList = $srcFiles -join ' '
$includeArgs = "-I`"$include`" -I`"$libsDir\CLI11\include`" -I`"$libsDir\json\include`" -I`"$libsDir\spdlog\include`" -I`"$libsDir\yaml-cpp\include`" -I`"$libsDir\libyaml\include`" -I`"$libsDir\pdcurses`" -I`"$libsDir\curl\include`" -I`"$libsDir\sqlite`""
$pkgFlags = $(pkg-config --cflags --libs yaml-cpp yaml-0.1 libcurl sqlite3 pdcurses)
if ($LASTEXITCODE -ne 0) {
    Write-Error "pkg-config failed to locate required libraries"
    exit 1
}

$cmd = "g++ -std=c++20 -Wall -Wextra -O2 -static -static-libgcc -static-libstdc++ -DYAML_CPP_STATIC_DEFINE $includeArgs $srcList $pkgFlags -o `"$BuildDir\autogithubpullmerge.exe`""
Invoke-Expression $cmd
