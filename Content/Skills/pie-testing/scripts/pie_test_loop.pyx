# pie_test_loop.pyx — Clean PIE start/stop loop for runtime validation.
#
# Sample script for the pie-testing skill. Run via execute_python_code.
# start_pie() is ASYNCHRONOUS: the world isn't live in the same call. Run the start step,
# then do your runtime checks / log inspection in a LATER call, then stop.
import unreal
ws = unreal.WidgetService

# Step 1 (its own call): start from a clean state, then request play.
if ws.is_pie_running():
    ws.stop_pie()
ws.start_pie()
print("PIE requested; running now:", ws.is_pie_running(),
      "-> if False, re-check in a later call before acting in-world")

# Step 2 (a later call, once is_pie_running() is True): inspect runtime state via other services
#   e.g. read_logs to confirm a Blueprint event fired, or WidgetService.spawn_widget_in_pie(...)

# Step 3 (final call): always stop so later edits/recompiles don't fail
# ws.stop_pie()
