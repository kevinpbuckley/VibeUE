# create_animation.pyx — Author a widget animation with a track and keyframes, then verify.
#
# Sample script for the umg-widgets skill. Run via execute_python_code.
# Order matters: create animation -> add track on a REAL property -> add keyframes.
import unreal

WIDGET_PATH = "/Game/Blueprints/TestWidget"
ANIM_NAME = "IntroFade"
TARGET_WIDGET = "HeaderTitle"
TARGET_PROPERTY = "RenderOpacity"  # real property / slot alias (e.g. RenderOpacity, Position X)
KEYFRAMES = [(0.0, "0.0"), (0.35, "1.0")]  # (time_seconds, value_string)

unreal.WidgetService.create_animation(WIDGET_PATH, ANIM_NAME)
unreal.WidgetService.add_animation_track(WIDGET_PATH, ANIM_NAME, TARGET_WIDGET, TARGET_PROPERTY)

for t, v in KEYFRAMES:
    key = unreal.WidgetAnimKeyframe()
    key.time = t
    key.value = v
    key.interpolation = "Linear"
    unreal.WidgetService.add_keyframe(WIDGET_PATH, ANIM_NAME, TARGET_WIDGET, TARGET_PROPERTY, key)

unreal.EditorAssetLibrary.save_asset(WIDGET_PATH)

# Verify the animation exists with at least one track.
for anim in unreal.WidgetService.list_animations(WIDGET_PATH):
    print(anim.animation_name, "duration:", anim.duration, "tracks:", anim.track_count)
