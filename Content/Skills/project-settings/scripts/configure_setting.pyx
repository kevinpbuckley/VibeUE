# configure_setting.pyx — Discover settings classes and read/write a project setting.
#
# Sample script for the project-settings skill. Run via execute_python_code.
import unreal
ps = unreal.ProjectSettingsService

# Discover the available UDeveloperSettings categories/classes
print("classes:", [str(c) for c in ps.discover_settings_classes()][:10])

# Read a category as JSON, then write back changes with set_category_settings_from_json
# print(ps.get_category_settings_as_json("/Script/EngineSettings.GameMapsSettings"))
