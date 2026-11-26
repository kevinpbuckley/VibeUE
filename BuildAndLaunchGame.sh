#!/usr/bin/env bash
# Build and launch an Unreal Engine project on Linux or macOS. This script may live anywhere in the project tree.
#
# Closing the running editor (issue #697): each editor of this project is asked over MCP (curl) whether it has
# unsaved packages; a clean editor is told to quit and exits on its own, an editor with unsaved work stops the
# script with the list (exit 5) unless --force. Old logs are moved to Saved/Logs/Previous, not deleted.
#
# Exit codes: 0 done (ready, when waiting), 1 build or setup failure, 2 --wait-for-ready timed out, 3 the editor
# exited before it was ready, 4 invalid --map, 5 refused to close a running editor, 6 --wait-for-idle timed out.
set -euo pipefail

mode="Development"
engine_root="${UE_ENGINE_ROOT:-}"
clean=false
skip_build=false
strict_rebuild=false
stop_running=false
force=false
port=0
muted=false
keep_logs=false
log_generations=5
wait_ready=false
wait_idle=false
ready_timeout=180
ready_max=3600
idle_timeout=3600
quit_timeout=120

usage() {
    cat <<'EOF'
Usage: BuildAndLaunchGame.sh [options]

Options:
  --engine PATH          Unreal Engine root. Defaults to $UE_ENGINE_ROOT.
  --mode NAME            Build configuration (default: Development).
  --clean                Remove project and plugin build artifacts before building.
  --strict-rebuild       Remove this plugin's build artifacts before building.
  --skip-build           Launch without building.
  --map PATH             Map to open on launch (e.g. /Game/Maps/TrainingPool). Without it the
                         editor opens the project default map (issue #554).
  --port N               MCP port for the launched editor (-ModelContextProtocolPort=N). A second
                         editor on the machine needs its own port (both default to 8000).
  --force                Close the running editor even with unsaved packages (they are lost), and
                         kill it when it does not exit.
  --stop-editor          Close this project's running editor without asking when it cannot be
                         reached over MCP (otherwise the script asks on a terminal and refuses
                         without one). Unsaved packages still stop the script unless --force.
  --quit-timeout SEC     Wait this long for a busy editor and for its exit (default: 120).
  --keep-logs            Leave Saved/Logs as it is (default: move its files to
                         Saved/Logs/Previous/<timestamp>).
  --log-generations N    Old log generations to keep in Saved/Logs/Previous (default: 5).
  --muted                Linux (PulseAudio or PipeWire, needs pactl): mute the editor's audio
                         stream(s) for its lifetime; the engine still renders audio. Not
                         available on macOS (no per-application mute there).
  --wait-for-ready       Return once the editor's readiness signal is written (the editor keeps
                         running) instead of waiting for the editor to exit.
  --ready-timeout SEC    Give up after this many seconds WITHOUT a sign of life (CPU time, log
                         growth, health file) (default: 180).
  --ready-max SEC        Give up after this many seconds in any case (default: 3600).
  --wait-for-idle        After ready, wait for no slow task, no modal dialog, no shader jobs and no
                         assets compiling (implies --wait-for-ready).
  --idle-timeout SEC     Give up waiting for idle after this long (default: 3600).
  -h, --help             Show this help.
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
        --force) force=true; stop_running=true; shift ;;
        --port) port="$2"; shift 2 ;;
        --quit-timeout) quit_timeout="$2"; shift 2 ;;
        --keep-logs) keep_logs=true; shift ;;
        --log-generations) log_generations="$2"; shift 2 ;;
        --muted) muted=true; shift ;;
        --wait-for-ready) wait_ready=true; shift ;;
        --ready-timeout) ready_timeout="$2"; shift 2 ;;
        --ready-max) ready_max="$2"; shift 2 ;;
        --wait-for-idle) wait_idle=true; wait_ready=true; shift ;;
        --idle-timeout) idle_timeout="$2"; shift 2 ;;
        --map) map="$2"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
done

if ! [[ "$port" =~ ^[0-9]+$ ]] || ((port > 65535)); then
    echo "ERROR: --port $port is not a valid TCP port." >&2
    exit 1
fi
if [[ -n "$map" && ( "$map" != /* || "$map" == *:* || "$map" == *\\* ) ]]; then
    echo "ERROR: --map '$map' is not a valid Unreal package path (expected e.g. /Game/Maps/L_Foo)." >&2
    exit 4
fi

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
signals_dir="$project_root/Saved/VibeUE/Signals"
logs_dir="$project_root/Saved/Logs"

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

# ---------------------------------------------------------------------------------------------------------------
# Signals and MCP (issue #697)
# ---------------------------------------------------------------------------------------------------------------
file_mtime() { stat -c %Y -- "$1" 2>/dev/null || stat -f %m -- "$1" 2>/dev/null || echo 0; }

# One field of a signal JSON file (pretty-printed or compact): the value without quotes.
json_field() {
    local file="$1" key="$2"
    [[ -f "$file" ]] || return 0
    tr -d '\n\r\t' < "$file" | sed -n "s/.*\"$key\"[[:space:]]*:[[:space:]]*\(\"[^\"]*\"\|[^,}]*\).*/\1/p" | sed 's/^"//; s/"$//; s/[[:space:]]*$//'
}

# The health file of a process when it was rewritten in the last 20 s (it is rewritten every ~5 s), else nothing.
fresh_health_file() {
    local file="$1/editor-$2-health.json"
    [[ -f "$file" ]] || return 0
    (( $(date +%s) - $(file_mtime "$file") <= 20 )) && echo "$file"
    return 0
}

process_command_line() {
    if [[ -r "/proc/$1/cmdline" ]]; then tr '\0' ' ' < "/proc/$1/cmdline"; else ps -o command= -p "$1" 2>/dev/null || true; fi
}

# The MCP port an editor serves: its fresh health file, else its command line, else --port, else its project config,
# else 8000. A wrong guess is harmless: the Python checks the pid before it does anything.
editor_mcp_port() {
    local pid="$1" cmd health p
    health="$(fresh_health_file "$signals_dir" "$pid")"
    if [[ -n "$health" ]]; then
        p="$(json_field "$health" mcpPort)"
        if [[ "$p" =~ ^[0-9]+$ ]] && ((p > 0)); then echo "$p"; return; fi
    fi
    cmd="$(process_command_line "$pid")"
    if [[ "$cmd" =~ -ModelContextProtocolPort=([0-9]+) ]]; then echo "${BASH_REMATCH[1]}"; return; fi
    if ((port > 0)); then echo "$port"; return; fi
    local ini
    for ini in "$project_root/Saved/Config/LinuxEditor/EditorPerProjectUserSettings.ini" \
               "$project_root/Saved/Config/MacEditor/EditorPerProjectUserSettings.ini" \
               "$project_root/Config/DefaultEditorPerProjectUserSettings.ini"; do
        if [[ -f "$ini" ]]; then
            p="$(sed -n 's/^[[:space:]]*ServerPortNumber[[:space:]]*=[[:space:]]*\([0-9][0-9]*\).*/\1/p' "$ini" | tail -n 1)"
            if [[ -n "$p" ]]; then echo "$p"; return; fi
        fi
    done
    echo 8000
}

# Asks editor $1 (MCP port $2) for its unsaved packages and, when there are none (or $3 = true), to quit. The Python
# checks its own pid first, so a wrong port never closes another editor. Prints the decoded answer, one
# "key=value" per line (pid=, quitting=0|1, dirty=<package> ...); returns 1 when the editor could not be asked.
request_editor_quit() {
    local pid="$1" mcp_port="$2" quit_dirty="False"
    [[ "$3" == true ]] && quit_dirty="True"
    command -v curl >/dev/null 2>&1 || return 1
    local url="http://127.0.0.1:$mcp_port/mcp" headers session body answer token
    headers="$(mktemp)"
    local common=(-sS --noproxy '*' -H 'Content-Type: application/json' -H 'Accept: application/json, text/event-stream' -H 'MCP-Protocol-Version: 2025-06-18')
    if ! curl "${common[@]}" -m 15 -D "$headers" -o /dev/null \
            --data '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{},"clientInfo":{"name":"BuildAndLaunchGame","version":"1.0"}}}' "$url"; then
        rm -f -- "$headers"; return 1
    fi
    session="$(grep -i '^mcp-session-id:' "$headers" | head -n 1 | cut -d: -f2- | tr -d ' \r')"
    rm -f -- "$headers"
    [[ -n "$session" ]] && common+=(-H "Mcp-Session-Id: $session")
    curl "${common[@]}" -m 15 -o /dev/null --data '{"jsonrpc":"2.0","method":"notifications/initialized"}' "$url" || true

    # The script uses single quotes only, so it goes into the JSON body with only its newlines escaped. The quit is
    # deferred by half a second so the MCP answer leaves before the engine loop ends.
    local py
    py="import base64\nimport os\nimport time\nexpected_pid = $pid\nquit_even_if_dirty = $quit_dirty\npid = os.getpid()\n"
    py+="try:\n    dirty = [str(p) for p in unreal.PythonTools.get_dirty_packages()]\nexcept AttributeError:\n"
    py+="    utils = unreal.EditorLoadingAndSavingUtils\n"
    py+="    dirty = sorted(set(p.get_name() for p in list(utils.get_dirty_content_packages()) + list(utils.get_dirty_map_packages())))\n"
    py+="quitting = pid == expected_pid and (not dirty or quit_even_if_dirty)\n"
    py+="if quitting:\n    started = time.monotonic()\n"
    py+="    def _vibeue_deferred_quit(delta_seconds):\n"
    py+="        if time.monotonic() - started >= 0.5 and not getattr(_vibeue_deferred_quit, 'done', False):\n"
    py+="            _vibeue_deferred_quit.done = True\n            unreal.SystemLibrary.quit_editor()\n"
    py+="    unreal.register_slate_post_tick_callback(_vibeue_deferred_quit)\n"
    py+="lines = ['pid=%d' % pid, 'quitting=%d' % int(quitting)] + ['dirty=' + d for d in dirty]\n"
    py+="'VIBEUE697:' + base64.urlsafe_b64encode(chr(10).join(lines).encode('utf-8')).decode('ascii') + ':END'"
    body="{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/call\",\"params\":{\"name\":\"call_tool\",\"arguments\":{\"toolset_name\":\"editor_toolset.toolsets.python_scripting.PythonScriptingTools\",\"tool_name\":\"execute_python\",\"arguments\":{\"script\":{\"language\":\"Python\",\"content\":\"$py\"}}}}}"
    answer="$(curl "${common[@]}" -m 60 --data "$body" "$url")" || return 1
    token="$(printf '%s' "$answer" | grep -o 'VIBEUE697:[A-Za-z0-9_=-]*:END' | head -n 1)"
    [[ -n "$token" ]] || return 1
    token="${token#VIBEUE697:}"
    token="${token%:END}"
    token="$(printf '%s' "$token" | tr '_-' '/+')"
    while (( ${#token} % 4 )); do token+="="; done
    printf '%s' "$token" | { base64 -d 2>/dev/null || base64 -D; }
    echo
}

wait_for_exit() {
    local pid="$1" seconds="$2" waited=0
    while kill -0 "$pid" 2>/dev/null && ((waited < seconds)); do sleep 1; waited=$((waited + 1)); done
    ! kill -0 "$pid" 2>/dev/null
}

# Closes one editor; returns 1 when the script must refuse (reason printed).
close_editor() {
    local pid="$1" health waited=0 mcp_port answer answer_pid quitting dirty

    # Busy editors refuse MCP calls: wait for a slow task or a modal dialog to end first.
    while health="$(fresh_health_file "$signals_dir" "$pid")"; [[ -n "$health" ]]; do
        [[ "$(json_field "$health" slowTask)" == true || "$(json_field "$health" modal)" == true ]] || break
        ((waited >= quit_timeout)) && break
        echo "  PID $pid is busy (slow task or modal dialog); waiting ($waited/$quit_timeout s)..."
        sleep 5; waited=$((waited + 5))
    done
    kill -0 "$pid" 2>/dev/null || return 0

    mcp_port="$(editor_mcp_port "$pid")"
    if answer="$(request_editor_quit "$pid" "$mcp_port" "$force")"; then
        answer_pid="$(printf '%s\n' "$answer" | sed -n 's/^pid=//p')"
        quitting="$(printf '%s\n' "$answer" | sed -n 's/^quitting=//p')"
        dirty="$(printf '%s\n' "$answer" | sed -n 's/^dirty=//p')"
        if [[ "$answer_pid" != "$pid" ]]; then
            echo "  MCP port $mcp_port is served by PID $answer_pid, not by PID $pid: ignoring that answer."
        elif [[ "$quitting" != 1 ]]; then
            echo "REFUSED: the editor (PID $pid) has unsaved package(s):" >&2
            printf '    %s\n' $dirty >&2
            echo "  Save or discard them, then run this script again; --force closes it anyway and loses them." >&2
            return 1
        else
            [[ -n "$dirty" ]] && echo "  --force: discarding unsaved package(s): $(echo $dirty)"
            echo "  Asked PID $pid to quit over MCP (port $mcp_port); waiting for it to exit..."
            if wait_for_exit "$pid" "$quit_timeout"; then
                echo "  PID $pid exited by itself: no kill."
                return 0
            fi
            echo "  PID $pid accepted the quit but is still running after $quit_timeout s; it is shutting down with nothing to save, killing it."
            kill -KILL "$pid" 2>/dev/null || true
            return 0
        fi
    else
        echo "  No answer over MCP on port $mcp_port."
    fi

    # Fallback: the pre-#697 behaviour, with consent.
    if [[ "$stop_running" != true ]]; then
        local reply=""
        if [[ -t 0 ]]; then
            read -r -p "An editor for $project_name is running (PID $pid) and cannot be asked about unsaved work. Close it? Unsaved changes are lost. [y/N] " reply
        fi
        if [[ "$reply" != [yY]* ]]; then
            echo "REFUSED: an editor for $project_name is running (PID $pid). Close it, or pass --stop-editor / --force." >&2
            return 1
        fi
    fi
    echo "Stopping the running editor for $project_name (PID $pid)..."
    kill "$pid" 2>/dev/null || true
    if wait_for_exit "$pid" 15; then return 0; fi
    echo "Editor did not exit gracefully; terminating it."
    kill -KILL "$pid" 2>/dev/null || true
    return 0
}

stop_editors() {
    local pids pid
    pids="$(pgrep -f "UnrealEditor.*$project_path" || true)"
    [[ -z "$pids" ]] && return 0
    while IFS= read -r pid; do
        [[ -n "$pid" ]] || continue
        echo "Closing the editor for $project_name (PID $pid)..."
        close_editor "$pid" || exit 5
    done <<< "$pids"
}

remove_artifacts() {
    local path
    for path in "$@"; do
        [[ -e "$path" ]] && rm -rf -- "$path"
    done
}

# Old logs go to Saved/Logs/Previous/<timestamp>; the newest $log_generations generations are kept.
rotate_logs() {
    if [[ "$keep_logs" == true ]]; then
        echo "--keep-logs: leaving Saved/Logs as it is."
        return
    fi
    [[ -d "$logs_dir" ]] || return 0
    local generation="$logs_dir/Previous/$(date +%Y%m%d-%H%M%S)" moved=0 file
    shopt -s nullglob
    local files=("$logs_dir"/*) conversations=("$logs_dir"/AgentConversations/*)
    shopt -u nullglob
    for file in "${files[@]}"; do
        [[ -f "$file" ]] || continue
        mkdir -p -- "$generation"
        mv -f -- "$file" "$generation/" && moved=$((moved + 1))
    done
    for file in "${conversations[@]}"; do
        [[ -f "$file" ]] || continue
        mkdir -p -- "$generation/AgentConversations"
        mv -f -- "$file" "$generation/AgentConversations/" && moved=$((moved + 1))
    done
    ((moved > 0)) && echo "Moved $moved log file(s) to $generation"
    if [[ -d "$logs_dir/Previous" ]]; then
        local old
        while IFS= read -r old; do
            [[ -n "$old" ]] && rm -rf -- "$logs_dir/Previous/$old" && echo "  Removed the oldest log generation $old (keeping $log_generations)"
        done < <(ls -1 "$logs_dir/Previous" | sort -r | tail -n +"$((log_generations + 1))")
    fi
}

# Audio (Linux only): pactl lists each stream ("sink input") with application.process.id. PulseAudio's and
# PipeWire's stream-restore remember the mute per application, so a later launch without --muted un-mutes the editor
# once and forgets the entry (same rule as the .ps1).
muted_list="${XDG_DATA_HOME:-$HOME/.local/share}/VibeUE/muted-editor-executables.txt"
set_sink_input_mute() {
    local pid="$1" state="$2" found=1 idx
    for idx in $(pactl list sink-inputs 2>/dev/null | awk -v want="$pid" '
            /^Sink Input #/ { sub(/^Sink Input #/, ""); cur = $0 }
            /application.process.id = / { gsub(/"/, "", $3); if ($3 == want) print cur }'); do
        pactl set-sink-input-mute "$idx" "$state" && found=0
    done
    return $found
}
start_audio_watcher() {
    local pid="$1" audio_mode="$2"
    if [[ "$platform" != Linux ]]; then
        echo "NOTE: --muted is not available on macOS (no per-application mute); the editor plays sound." >&2
        return
    fi
    if ! command -v pactl >/dev/null 2>&1; then
        echo "NOTE: --muted needs pactl (PulseAudio or PipeWire); the editor plays sound." >&2
        return
    fi
    mkdir -p -- "$(dirname -- "$muted_list")"
    if [[ "$audio_mode" == mute ]]; then
        grep -qxF -- "$editor_bin" "$muted_list" 2>/dev/null || echo "$editor_bin" >> "$muted_list"
        ( while kill -0 "$pid" 2>/dev/null; do set_sink_input_mute "$pid" 1 || true; sleep 1; done ) >/dev/null 2>&1 &
        echo "Muting the editor's audio stream(s) for its lifetime (watcher PID $!)."
    else
        ( for _ in $(seq 1 900); do
              kill -0 "$pid" 2>/dev/null || exit 0
              if set_sink_input_mute "$pid" 0; then
                  grep -vxF -- "$editor_bin" "$muted_list" > "$muted_list.tmp" || true
                  mv -f -- "$muted_list.tmp" "$muted_list"
                  exit 0
              fi
              sleep 1
          done ) >/dev/null 2>&1 &
        echo "An earlier --muted launch left the editor muted: un-muting this one (watcher PID $!)."
    fi
    disown "$!" 2>/dev/null || true
}

echo "=== $project_name $platform Build and Launch ==="
echo "Project: $project_path"
echo "Engine:  $engine_root"
echo "Mode:    $mode"

stop_editors

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

rotate_logs

editor_args=("$project_path")
[[ -n "$map" ]] && editor_args+=("$map")
((port > 0)) && editor_args+=("-ModelContextProtocolPort=$port")
launched_at="$(date +%s)"
"$editor_bin" "${editor_args[@]}" &
editor_pid=$!

# Same stale-signal guard as BuildAndLaunchGame.ps1: a crashed Editor can leave a readiness signal
# named for a PID the OS later reuses, and RegisterToolsets() only clears it late in startup.
rm -f -- "$signals_dir/editor-$editor_pid-"*.json* 2>/dev/null || true

echo "Editor-PID=$editor_pid"

if [[ "$muted" == true ]]; then
    start_audio_watcher "$editor_pid" mute
elif [[ -f "$muted_list" ]] && grep -qxF -- "$editor_bin" "$muted_list"; then
    start_audio_watcher "$editor_pid" unmute
fi

if [[ "$wait_ready" != true ]]; then
    wait "$editor_pid"
    exit $?
fi

ready_signal="$signals_dir/editor-$editor_pid-true.json"
editor_log="$logs_dir/$project_name.log"
echo "Waiting for editor readiness signal (gives up after ${ready_timeout}s without a sign of life, ${ready_max}s at most)..."
started="$(date +%s)"
last_life="$started"
last_cpu=""
last_log_size=-1
last_health=0
while [[ ! -f "$ready_signal" ]]; do
    if ! kill -0 "$editor_pid" 2>/dev/null; then
        wait "$editor_pid" && code=0 || code=$?
        echo "Editor process exited (code $code) before signaling ready." >&2
        exit 3
    fi
    now="$(date +%s)"
    cpu="$(ps -o time= -p "$editor_pid" 2>/dev/null | tr -d ' ' || true)"
    if [[ -n "$cpu" && "$cpu" != "$last_cpu" ]]; then last_cpu="$cpu"; last_life="$now"; fi
    if [[ -f "$editor_log" ]] && (( $(file_mtime "$editor_log") >= launched_at )); then
        size="$(wc -c < "$editor_log" | tr -d ' ')"
        if [[ "$size" != "$last_log_size" ]]; then last_log_size="$size"; last_life="$now"; fi
    fi
    health="$(fresh_health_file "$signals_dir" "$editor_pid")"
    if [[ -n "$health" ]] && (( $(file_mtime "$health") != last_health )); then last_health="$(file_mtime "$health")"; last_life="$now"; fi
    if ((now - last_life >= ready_timeout)); then
        echo "Editor did not signal ready and showed no sign of life for ${ready_timeout}s." >&2
        exit 2
    fi
    if ((now - started >= ready_max)); then
        echo "Editor did not signal ready within ${ready_max}s (it may still be loading)." >&2
        exit 2
    fi
    sleep 1
done
echo "Editor is ready (signaled after $(( $(date +%s) - started ))s)."

if [[ -n "$map" ]]; then
    current_map=""
    for _ in $(seq 1 20); do
        current_map="$(json_field "$ready_signal" currentMap)"
        current_map="${current_map%%.*}"
        [[ -n "$current_map" && "$current_map" != /Temp/* ]] && break
        sleep 1
    done
    if [[ -z "$current_map" || "$current_map" == /Temp/* ]]; then
        echo "NOTE: level still loading at signal time (currentMap '$current_map'); verify currentMap before world edits."
    elif [[ "${current_map,,}" != "${map,,}" ]]; then
        echo "WARNING: requested --map '$map' but the editor reports currentMap '$current_map'." >&2
    fi
fi

signal_port="$(json_field "$ready_signal" mcpPort)"
if ((port > 0)) && [[ "$signal_port" != "$port" ]]; then
    echo "WARNING: --port $port was asked for but the editor reports MCP port $signal_port." >&2
fi

if [[ "$wait_idle" == true ]]; then
    echo "Waiting for the editor to be idle (no slow task, shaders and assets compiled; timeout ${idle_timeout}s)..."
    idle_started="$(date +%s)"
    quiet=0
    last_beat=""
    while true; do
        if ! kill -0 "$editor_pid" 2>/dev/null; then
            echo "Editor process exited while waiting for idle." >&2
            exit 3
        fi
        health="$(fresh_health_file "$signals_dir" "$editor_pid")"
        if [[ -n "$health" ]]; then
            beat="$(json_field "$health" updatedUtc)"
            if [[ "$beat" != "$last_beat" ]]; then
                last_beat="$beat"
                shaders="$(json_field "$health" shaderJobs)"; shaders="${shaders:-0}"
                assets="$(json_field "$health" assetCompilations)"; assets="${assets:-0}"
                if [[ "$(json_field "$health" slowTask)" == true || "$(json_field "$health" modal)" == true || "$shaders" != 0 || "$assets" != 0 ]]; then
                    quiet=0
                    echo "  busy: shader jobs $shaders, assets compiling $assets"
                else
                    quiet=$((quiet + 1))
                fi
                if ((quiet >= 2)); then
                    echo "Editor is idle (after $(( $(date +%s) - idle_started ))s more)."
                    break
                fi
            fi
        fi
        if (( $(date +%s) - idle_started >= idle_timeout )); then
            echo "Editor was not idle within ${idle_timeout}s." >&2
            exit 6
        fi
        sleep 1
    done
fi

echo "=== Launch complete: the editor (PID $editor_pid) keeps running ==="
