#!/usr/bin/env bash
# Build and launch an Unreal Engine project on Linux or macOS. This script may live anywhere in the project tree.
set -euo pipefail

mode="Development"
engine_root="${UE_ENGINE_ROOT:-}"
clean=false
skip_build=false
strict_rebuild=false
stop_running=false

usage() {
    cat <<'EOF'
Usage: BuildAndLaunchGame.sh [options]

Options:
  --engine PATH        Unreal Engine root. Defaults to $UE_ENGINE_ROOT.
  --mode NAME          Build configuration (default: Development).
  --clean              Remove project and plugin build artifacts before building.
  --strict-rebuild     Remove this plugin's build artifacts before building.
  --skip-build         Launch without building.
  --stop-editor        Close this project's running editor without asking. Without it the script
                       asks on a terminal and refuses otherwise. Unsaved changes are lost.
  --map PATH           Map to open on launch (e.g. /Game/Maps/TrainingPool). Without it the
                       editor opens the project default map (issue #554).
  -h, --help           Show this help.
EOF
}

map=""
while (($#)); do
    case "$1" in
        --engine) engine_root="$2"; shift 2 ;;
        --mode) mode="$2"; shift 2 ;;
        --clean) clean=true; shift ;;
        --strict-rebuild) strict_rebuild=true; shift ;;
        --skip-build) skip_build=true; shift ;;
        --stop-editor) stop_running=true; shift ;;
        --map) map="$2"; shift 2 ;;
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
case "$(uname -s)" in
    Linux)
        platform="Linux"
        build_script="$engine_root/Engine/Build/BatchFiles/Linux/Build.sh"
        editor_bin="$engine_root/Engine/Binaries/Linux/UnrealEditor"
        ;;
    Darwin)
        platform="Mac"
        build_script="$engine_root/Engine/Build/BatchFiles/Mac/Build.sh"
        editor_bin="$engine_root/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
        ;;
    *)
        echo "ERROR: BuildAndLaunchGame.sh supports Linux and macOS only." >&2
        exit 1
        ;;
esac
if [[ ! -x "$build_script" || ! -x "$editor_bin" ]]; then
    echo "ERROR: $engine_root does not contain executable $platform Build.sh and UnrealEditor binaries." >&2
    exit 1
fi

project_root="$(dirname -- "$project_path")"
project_name="$(basename -- "${project_path%.uproject}")"

# Resolve the editor target from Source/*Editor.Target.cs, like BuildAndLaunchGame.ps1. A Blueprint-only
# project has no target of its own: its plugins build through the engine's UnrealEditor target.
shopt -s nullglob
target_files=("$project_root"/Source/*Editor.Target.cs)
shopt -u nullglob
editor_target="${project_name}Editor"
target_args=("$project_path")
if ((${#target_files[@]} == 0)); then
    editor_target="UnrealEditor"
    target_args=(-Project="$project_path")
fi
for target_file in "${target_files[@]}"; do
    if [[ "$(<"$target_file")" =~ class[[:space:]]+([A-Za-z0-9_]+)EditorTarget[[:space:]]*:[[:space:]]*TargetRules ]]; then
        editor_target="${BASH_REMATCH[1]}Editor"
        break
    fi
done
build_manifest="$project_root/Saved/VibeUE/last-build.json"

write_build_manifest() {
    local status="$1" exit_code="${2:-null}" diagnostic="${3:-}"
    mkdir -p -- "$(dirname -- "$build_manifest")"
    local completed="null"
    [[ "$status" == "succeeded" || "$status" == "failed" || "$status" == "skipped" ]] && completed="\"$(date -u +%Y-%m-%dT%H:%M:%SZ)\""
    local temp="$build_manifest.tmp"
    printf '{"schema":"vibeue.build.v1","status":"%s","projectFile":"%s","engineRoot":"%s","target":"%s","platform":"%s","configuration":"%s","completedAtIso":%s,"exitCode":%s,"verdict":"%s","logPath":"","diagnostic":"%s"}\n' \
        "$status" "$project_path" "$engine_root" "$editor_target" "$platform" "$mode" "$completed" "$exit_code" "$status" "$diagnostic" > "$temp"
    mv -f -- "$temp" "$build_manifest"
}

stop_editor() {
    local pids
    pids="$(pgrep -f "UnrealEditor.*$project_path" || true)"
    [[ -z "$pids" ]] && return

    if [[ "$stop_running" != true ]]; then
        local reply=""
        if [[ -t 0 ]]; then
            read -r -p "An editor for $project_name is running (PID $(echo $pids)). Close it? Unsaved changes are lost. [y/N] " reply
        fi
        if [[ "$reply" != [yY]* ]]; then
            echo "ERROR: an editor for $project_name is running (PID $(echo $pids)). Close it, or pass --stop-editor." >&2
            exit 1
        fi
    fi

    echo "Stopping the running editor for $project_name..."
    while IFS= read -r pid; do
        [[ -n "$pid" ]] && kill "$pid" 2>/dev/null || true
    done <<< "$pids"
    for _ in {1..15}; do
        sleep 1
        pids="$(pgrep -f "UnrealEditor.*$project_path" || true)"
        [[ -z "$pids" ]] && return
    done
    echo "Editor did not exit gracefully; terminating it."
    while IFS= read -r pid; do
        [[ -n "$pid" ]] && kill -KILL "$pid" 2>/dev/null || true
    done <<< "$pids"
}

remove_artifacts() {
    local path
    for path in "$@"; do
        [[ -e "$path" ]] && rm -rf -- "$path"
    done
}

echo "=== $project_name $platform Build and Launch ==="
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
    write_build_manifest running null
    if "$build_script" "$editor_target" "$platform" "$mode" "${target_args[@]}" -waitmutex; then
        write_build_manifest succeeded 0
    else
        code=$?
        write_build_manifest failed "$code" "UnrealBuildTool returned a non-zero exit code."
        exit "$code"
    fi
else
    write_build_manifest skipped null "Build was skipped by caller; this is not compile verification."
fi

if [[ -n "$map" ]]; then
    "$editor_bin" "$project_path" "$map" &
else
    "$editor_bin" "$project_path" &
fi
editor_pid=$!

# Same stale-signal guard as BuildAndLaunchGame.ps1: a crashed Editor can leave a readiness signal
# named for a PID the OS later reuses, and RegisterToolsets() only clears it late in startup.
rm -f -- "$project_root/Saved/VibeUE/Signals/editor-$editor_pid-"*.json* 2>/dev/null || true

echo "Editor-PID=$editor_pid"

wait "$editor_pid"
