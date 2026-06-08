# create_sound_cue.pyx — Create a SoundCue (optionally from a wave) and add nodes.
#
# Sample script for the sound-cues skill. Run via execute_python_code.
import unreal
scs = unreal.SoundCueService

ASSET = "/Game/Audio/SC_SkillTest"
WAVE = ""   # optional: "/Game/Audio/SW_Footstep"
if unreal.EditorAssetLibrary.does_asset_exist(ASSET):
    unreal.EditorAssetLibrary.delete_asset(ASSET)

r = scs.create_sound_cue(ASSET, WAVE)   # SoundCueResult
print("create:", r)
# Add nodes (e.g. random, mixer, delay, attenuation) then connect them, e.g.:
# scs.add_delay_node(ASSET, ...)
unreal.EditorAssetLibrary.save_asset(ASSET)
