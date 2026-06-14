# set_cvars.pyx — Read scalability and set a console variable (EngineSettingsService).
#
# Sample script for the engine-settings skill. Run via execute_python_code.
import unreal
es = unreal.EngineSettingsService

print("scalability:", es.get_scalability_settings())
print("set cvar:", es.set_console_variable("r.ScreenPercentage", "100"))
# Inspect a setting category as JSON:
# print(es.get_category_settings_as_json("Rendering"))
