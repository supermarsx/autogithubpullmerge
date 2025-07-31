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
CloneOrUpdate "https://github.com/nlohmann/json.git" "json"
CloneOrUpdate "https://github.com/gabime/spdlog.git" "spdlog"
