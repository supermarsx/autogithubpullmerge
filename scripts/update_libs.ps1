$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$LibsDir = Join-Path $ScriptDir "..\libs"
New-Item -ItemType Directory -Path $LibsDir -Force | Out-Null

function CloneOrUpdate($repo, $dir) {
    $target = Join-Path $LibsDir $dir
    if (Test-Path (Join-Path $target ".git")) {
        git -C $target pull --ff-only
    } else {
        git clone --depth 1 $repo $target
    }
}

CloneOrUpdate "https://github.com/CLIUtils/CLI11.git" "CLI11"
CloneOrUpdate "https://github.com/jbeder/yaml-cpp.git" "yaml-cpp"
CloneOrUpdate "https://github.com/yaml/libyaml.git" "libyaml"
CloneOrUpdate "https://github.com/nlohmann/json.git" "json"
CloneOrUpdate "https://github.com/gabime/spdlog.git" "spdlog"
CloneOrUpdate "https://github.com/curl/curl.git" "curl"

# Download SQLite amalgamation containing sqlite3.c and sqlite3.h
$sqliteVer = "3430000"
$sqliteYear = "2023"
$sqliteZip = "sqlite-amalgamation-$sqliteVer.zip"
$sqliteDir = Join-Path $LibsDir "sqlite"
New-Item -ItemType Directory -Path $sqliteDir -Force | Out-Null
if (-not (Test-Path (Join-Path $sqliteDir "sqlite3.h"))) {
    $zipPath = Join-Path $sqliteDir $sqliteZip
    Invoke-WebRequest -Uri "https://sqlite.org/$sqliteYear/$sqliteZip" -OutFile $zipPath
    Expand-Archive -Path $zipPath -DestinationPath $sqliteDir -Force
    Move-Item -Path (Join-Path $sqliteDir "sqlite-amalgamation-$sqliteVer\*") -Destination $sqliteDir
    Remove-Item -Recurse -Force (Join-Path $sqliteDir "sqlite-amalgamation-$sqliteVer")
    Remove-Item $zipPath
}

CloneOrUpdate "https://github.com/wmcbrine/PDCurses.git" "pdcurses"
