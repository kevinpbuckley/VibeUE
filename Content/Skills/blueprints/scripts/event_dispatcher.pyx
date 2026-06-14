# event_dispatcher.pyx — Add a Blueprint Event Dispatcher (multicast delegate) with a parameter.
#
# Sample script for the blueprints skill. Run via execute_python_code.
# Use the dedicated dispatcher API — NOT add_variable(..., "EventDispatcher") (not a real type).
import unreal
bs = unreal.BlueprintService

BP_PATH = "/Game/Blueprints/BP_Player"

# 1. Create the dispatcher (skeleton recompiles inline; callable immediately)
print("dispatcher:", bs.add_event_dispatcher(BP_PATH, "OnDied"))

# 2. Add a parameter to its signature (subscribers receive it as an input)
print("param:", bs.add_event_dispatcher_parameter(BP_PATH, "OnDied", "Killer", "Actor"))

# 3. Spawn the "Call OnDied" broadcast node in the EventGraph (wire something into its execute pin)
call_id = bs.add_call_delegate_node(BP_PATH, "EventGraph", "OnDied", 1400, -700)
print("call node id:", call_id)

# 4. Compile + save
c = bs.compile_blueprint(BP_PATH)
print("compile:", c.success, "errors:", c.num_errors)
unreal.EditorAssetLibrary.save_asset(BP_PATH)
