#!/usr/bin/env bash
# Build and launch an Unreal Engine project on Linux. This script may live anywhere in the project tree.
set -euo pipefail

mode="Development"
engine_root="${UE_ENGINE_ROOT:-}"
clean=false
skip_build=false
strict_rebuild=false

usage() {
    cat <<'EOF'
Usage: BuildAndLaunchGame.sh [options]

Options:
  --engine PATH        Unreal Engine root. Defaults to $UE_ENGINE_ROOT.
  --mode NAME          Build configuration (default: Development).
  --clean              Remove project and plugin build artifacts before building.
  --strict-rebuild     Remove this plugin's build artifacts before building.
  --skip-build         Launch without building.
  -h, --help           Show this help.
EOF
}

while (($#)); do
    case "$1" in
        --engine) engine_root="$2"; shift 2 ;;
        --mode) mode="$2"; shift 2 ;;
        --clean) clean=true; shift ;;
        --strict-rebuild) strict_rebuild=true; shift ;;
        --skip-build) skip_build=true; shift ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
done

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
search_dir="$script_dir"
project_path=""
while [[ "$search_dir" != "/" ]]; do
    shopt -s nullglob
    projects=("$search_dir"/*.uproject)
    shopt -u nullglob
    if ((${#projects[@]})); then
        project_path="${projects[0]}"
        break
    fi
    search_dir="$(dirname -- "$search_dir")"
done

if [[ -z "$project_path" ]]; then
    echo "ERROR: No .uproject file found above $script_dir" >&2
    exit 1
fi

if [[ -z "$engine_root" ]]; then
    echo "ERROR: Set UE_ENGINE_ROOT or pass --engine /path/to/UE5." >&2
    exit 1
fi

engine_root="$(cd -- "$engine_root" && pwd -P)"
build_script="$engine_root/Engine/Build/BatchFiles/Linux/Build.sh"
editor_bin="$engine_root/Engine/Binaries/Linux/UnrealEditor"
if [[ ! -x "$build_script" || ! -x "$editor_bin" ]]; then
    echo "ERROR: $engine_root does not contain executable Linux Build.sh and UnrealEditor binaries." >&2
    exit 1
fi

project_root="$(dirname -- "$project_path")"
project_name="$(basename -- "${project_path%.uproject}")"

stop_editor() {
    local pids
    pids="$(pgrep -f -- "UnrealEditor.*$project_path" || true)"
    [[ -z "$pids" ]] && return

    echo "Stopping the running editor for $project_name..."
    kill $pids 2>/dev/null || true
    for _ in {1..15}; do
        sleep 1
        pids="$(pgrep -f -- "UnrealEditor.*$project_path" || true)"
        [[ -z "$pids" ]] && return
    done
    echo "Editor did not exit gracefully; terminating it."
    kill -KILL $pids 2>/dev/null || true
}

remove_artifacts() {
    local path
    for path in "$@"; do
        [[ -e "$path" ]] && rm -rf -- "$path"
    done
}

echo "=== $project_name Linux Build and Launch ==="
echo "Project: $project_path"
echo "Engine:  $engine_root"
echo "Mode:    $mode"

stop_editor

if [[ "$clean" == true ]]; then
    remove_artifacts "$project_root/Binaries" "$project_root/Intermediate"
    while IFS= read -r -d '' plugin_dir; do
        remove_artifacts "$plugin_dir/Binaries" "$plugin_dir/Intermediate"
    done < <(find "$project_root/Plugins" -mindepth 1 -maxdepth 1 -type d -print0 2>/dev/null)
elif [[ "$strict_rebuild" == true ]]; then
    remove_artifacts "$script_dir/Binaries" "$script_dir/Intermediate"
fi

if [[ "$skip_build" == false ]]; then
    "$build_script" "${project_name}Editor" Linux "$mode" "$project_path" -waitmutex
fi

exec "$editor_bin" "$project_path"
