# UWidgetService v2 — Font, Brush, Animation & Preview

## Motivation

### The Problem

An AI agent was asked to build a polished main-menu UI for a shipped game. After wiring the hierarchy and MVVM bindings the agent hit a wall:

1. **Attempt 1 — Font via `set_property`:** Used `SetProperty(path, "Title", "Font.Size", "48")` to set the title typeface. Font size updated. But changing the font family, weight (Bold vs Light), letter spacing, outline, or shadow requires a fully serialized `FSlateFontInfo` string that no AI can reconstruct reliably from reflection alone. Every attempt produced a malformed value string that silently no-oped.

2. **Attempt 2 — Button texture via `set_property`:** Used `SetProperty` to set a button's `BackgroundImage`. The underlying type is `FSlateBrush` — a struct with a resource object reference, tint color, draw mode, and nine-slice margins packed together. `set_property` only accepts flat string values and has no way to resolve an asset reference into a brush slot. The texture never applied.

3. **Attempt 3 — UI Animation:** The agent attempted to add a fade-in animation by using `BlueprintService` to add a custom event and manual lerp logic. This produced a working but fragile hand-rolled animation that ignored the UMG animation timeline entirely. The widget had no `UWidgetAnimation` tracks, so the animation couldn't be edited in the UMG designer and couldn't be played by name at runtime.

### Root Causes

| Gap | Description |
|-----|-------------|
| **No font API** | `set_property` cannot construct `FSlateFontInfo`. Family, weight, outline, shadow are all unreachable |
| **No brush API** | `set_property` cannot set `FSlateBrush`. Textures, 9-slice margins, draw modes, and tint on buttons/images/borders are all unreachable |
| **No animation API** | No way to create `UWidgetAnimation` assets, add property tracks, or place keyframes on the UMG timeline |
| **No visual verification** | The agent has no way to see what the widget looks like. Layout bugs go undetected until a human opens the designer |
| **No runtime testing** | No way to spawn a widget during PIE and verify its live appearance or interactive behavior |

---

## Current State: 24 Actions

| Category | Actions |
|----------|---------|
| Discovery | `list_widget_blueprints`, `get_hierarchy`, `get_root_widget`, `list_components`, `search_types`, `get_component_properties` |
| Component management | `add_component`, `remove_component` |
| Validation | `validate` |
| Property access | `get_property`, `set_property`, `list_properties` |
| Event handling | `get_available_events`, `bind_event` |
| MVVM | `list_view_models`, `add_view_model`, `remove_view_model`, `list_view_model_bindings`, `add_view_model_binding`, `remove_view_model_binding` |
| Snapshot | `get_widget_snapshot`, `get_component_snapshot` |
| Existence | `widget_blueprint_exists`, `widget_exists` |

**What's missing for production-quality UIs:**
- Zero font control beyond flat size string
- Zero brush/texture/draw-mode control
- Zero animation timeline support
- No visual preview — agent cannot see what it built
- No PIE widget lifecycle management

---

## Proposed Feature Areas

### Feature 1: Font API

**Priority: Critical**

`set_property` reaches `Font.Size` through reflection but cannot touch the rest of `FSlateFontInfo`. The entire font subsystem — family, weight, outline, letter spacing, shadow — requires a dedicated API that constructs the struct correctly.

#### New Struct: `FWidgetFontInfo`

```cpp
USTRUCT(BlueprintType)
struct FWidgetFontInfo
{
    GENERATED_BODY()

    /** Font family asset path, e.g. "/Engine/EngineFonts/Roboto" */
    UPROPERTY(BlueprintReadWrite, Category = "Widget|Font")
    FString FontFamily;

    /** Typeface name within the font family: "Regular", "Bold", "Italic", "Light", "Medium", "Black", "BoldItalic" */
    UPROPERTY(BlueprintReadWrite, Category = "Widget|Font")
    FString Typeface = TEXT("Regular");

    /** Point size */
    UPROPERTY(BlueprintReadWrite, Category = "Widget|Font")
    int32 Size = 12;

    /** Letter spacing in 1/1000 em units (0 = default) */
    UPROPERTY(BlueprintReadWrite, Category = "Widget|Font")
    int32 LetterSpacing = 0;

    /** Font color as linear RGBA string "(R=1,G=1,B=1,A=1)" */
    UPROPERTY(BlueprintReadWrite, Category = "Widget|Font")
    FString Color = TEXT("(R=1.0,G=1.0,B=1.0,A=1.0)");

    /** Shadow pixel offset "(X=1,Y=1)" — zero vector disables shadow */
    UPROPERTY(BlueprintReadWrite, Category = "Widget|Font")
    FString ShadowOffset = TEXT("(X=0.0,Y=0.0)");

    /** Shadow color "(R=0,G=0,B=0,A=0.5)" */
    UPROPERTY(BlueprintReadWrite, Category = "Widget|Font")
    FString ShadowColor = TEXT("(R=0.0,G=0.0,B=0.0,A=0.5)");

    /** Outline size in pixels (0 = no outline) */
    UPROPERTY(BlueprintReadWrite, Category = "Widget|Font")
    int32 OutlineSize = 0;

    /** Outline color "(R=0,G=0,B=0,A=1)" */
    UPROPERTY(BlueprintReadWrite, Category = "Widget|Font")
    FString OutlineColor = TEXT("(R=0.0,G=0.0,B=0.0,A=1.0)");
};
```

#### New Functions: `SetFont` / `GetFont`

```cpp
/**
 * Set the font on a TextBlock, RichTextBlock, EditableTextBox, or any widget
 * with an FSlateFontInfo property.
 *
 * @param WidgetPath  - Full path to the Widget Blueprint
 * @param ComponentName - Name of the text widget
 * @param FontInfo    - Font configuration
 * @param PropertyName - Target property name (default "Font", override for e.g. "ButtonTextStyle.Font")
 * @return True if successful
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|Widgets|Font")
static bool SetFont(
    const FString& WidgetPath,
    const FString& ComponentName,
    const FWidgetFontInfo& FontInfo,
    const FString& PropertyName = TEXT("Font"));

/**
 * Read the current font configuration from a text widget.
 *
 * @param WidgetPath  - Full path to the Widget Blueprint
 * @param ComponentName - Name of the text widget
 * @param PropertyName - Source property name (default "Font")
 * @return Populated FWidgetFontInfo (default/empty on failure)
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|Widgets|Font")
static FWidgetFontInfo GetFont(
    const FString& WidgetPath,
    const FString& ComponentName,
    const FString& PropertyName = TEXT("Font"));
```

#### Python Usage

```python
import unreal

path = "/Game/UI/WBP_MainMenu"

font = unreal.WidgetFontInfo()
font.set_editor_property("font_family", "/Engine/EngineFonts/Roboto")
font.set_editor_property("typeface", "Bold")
font.set_editor_property("size", 48)
font.set_editor_property("letter_spacing", -10)
font.set_editor_property("color", "(R=0.9,G=0.9,B=0.9,A=1.0)")
font.set_editor_property("shadow_offset", "(X=2.0,Y=2.0)")
font.set_editor_property("shadow_color", "(R=0.0,G=0.0,B=0.0,A=0.6)")
font.set_editor_property("outline_size", 1)
font.set_editor_property("outline_color", "(R=0.0,G=0.0,B=0.0,A=1.0)")

unreal.WidgetService.set_font(path, "TitleText", font)
unreal.EditorAssetLibrary.save_asset(path)
```

#### Built-in Font Families

| Name | Path |
|------|------|
| Roboto | `/Engine/EngineFonts/Roboto` |
| DroidSansMono | `/Engine/EngineFonts/DroidSansMono` |
| NotoSans | `/Engine/EngineFonts/NotoSans` |
| Custom | Any `/Game/` font asset path |

#### Typeface Names

`Regular` · `Bold` · `Italic` · `BoldItalic` · `Light` · `LightItalic` · `Medium` · `MediumItalic` · `Black` · `BlackItalic`

Not every font family includes every typeface. Missing typefaces silently fall back to Regular.

---

### Feature 2: Brush API

**Priority: Critical**

`FSlateBrush` is the struct behind every visual surface in UMG — button states, image resources, border backgrounds, progress bar fills. It cannot be written through `set_property` because it holds a `UObject*` resource pointer alongside draw configuration. A dedicated API is required.

#### Brush Targets

Each widget type exposes named brush slots:

| Widget | Brush Slots |
|--------|-------------|
| Image | `Brush` |
| Button | `Normal`, `Hovered`, `Pressed`, `Disabled` |
| Border | `Background` |
| ProgressBar | `BackgroundImage`, `FillImage`, `MarqueeImage` |
| CheckBox | `CheckedImage`, `UncheckedImage`, `CheckedHoveredImage`, `UncheckedHoveredImage`, `CheckedPressedImage`, `UncheckedPressedImage` |
| Slider | `BarImage`, `ThumbImage` |

#### New Enum: `EWidgetBrushDrawAs` (as FString in API)

| Value | Description |
|-------|-------------|
| `Image` | Stretch the texture to fill the rect (default) |
| `Box` | 9-slice: stretch center, tile/clamp edges using margin |
| `Border` | 9-slice: render only the border region, transparent center |
| `RoundedBox` | Render as rounded rectangle (UE5.1+) |
| `NoDrawType` | Invisible — slot reserves space but draws nothing |

#### New Struct: `FWidgetBrushInfo`

```cpp
USTRUCT(BlueprintType)
struct FWidgetBrushInfo
{
    GENERATED_BODY()

    /**
     * Asset path to a Texture2D or Material, e.g. "/Game/Textures/T_ButtonNormal".
     * Empty string = solid color only.
     */
    UPROPERTY(BlueprintReadWrite, Category = "Widget|Brush")
    FString ResourcePath;

    /** Tint/solid color "(R=1,G=1,B=1,A=1)". Used as solid color when ResourcePath is empty. */
    UPROPERTY(BlueprintReadWrite, Category = "Widget|Brush")
    FString TintColor = TEXT("(R=1.0,G=1.0,B=1.0,A=1.0)");

    /** How to draw: "Image", "Box", "Border", "RoundedBox", "NoDrawType" */
    UPROPERTY(BlueprintReadWrite, Category = "Widget|Brush")
    FString DrawAs = TEXT("Image");

    /**
     * Image size in pixels "(X=64,Y=64)".
     * Used for "Image" draw mode to set the desired rendered size.
     * Zero = use widget slot size.
     */
    UPROPERTY(BlueprintReadWrite, Category = "Widget|Brush")
    FString ImageSize = TEXT("(X=0.0,Y=0.0)");

    /**
     * Nine-slice margin "(Left=8,Top=8,Right=8,Bottom=8)".
     * Only used when DrawAs is "Box" or "Border".
     * Values are in texture-space pixels.
     */
    UPROPERTY(BlueprintReadWrite, Category = "Widget|Brush")
    FString Margin = TEXT("(Left=0.0,Top=0.0,Right=0.0,Bottom=0.0)");

    /**
     * Corner radius for "RoundedBox" draw mode "(TopLeft=8,TopRight=8,BottomRight=8,BottomLeft=8)".
     */
    UPROPERTY(BlueprintReadWrite, Category = "Widget|Brush")
    FString CornerRadius = TEXT("(TopLeft=0.0,TopRight=0.0,BottomRight=0.0,BottomLeft=0.0)");
};
```

#### New Functions: `SetBrush` / `GetBrush`

```cpp
/**
 * Set a brush slot on a widget component.
 * Resolves the ResourcePath to a UTexture2D or UMaterialInterface via the Asset Registry.
 *
 * @param WidgetPath    - Full path to the Widget Blueprint
 * @param ComponentName - Name of the widget component
 * @param SlotName      - Brush slot: "Brush", "Normal", "Hovered", "Background", etc.
 * @param BrushInfo     - Brush configuration
 * @return True if successful
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|Widgets|Brush")
static bool SetBrush(
    const FString& WidgetPath,
    const FString& ComponentName,
    const FString& SlotName,
    const FWidgetBrushInfo& BrushInfo);

/**
 * Read the current brush from a widget component slot.
 *
 * @param WidgetPath    - Full path to the Widget Blueprint
 * @param ComponentName - Name of the widget component
 * @param SlotName      - Brush slot name
 * @return Populated FWidgetBrushInfo (default/empty on failure)
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|Widgets|Brush")
static FWidgetBrushInfo GetBrush(
    const FString& WidgetPath,
    const FString& ComponentName,
    const FString& SlotName);
```

#### Python Usage

```python
import unreal

path = "/Game/UI/WBP_MainMenu"

# Solid color button (no texture)
btn_brush = unreal.WidgetBrushInfo()
btn_brush.set_editor_property("tint_color", "(R=0.1,G=0.4,B=0.9,A=1.0)")
btn_brush.set_editor_property("draw_as", "RoundedBox")
btn_brush.set_editor_property("corner_radius", "(TopLeft=8,TopRight=8,BottomRight=8,BottomLeft=8)")
unreal.WidgetService.set_brush(path, "PlayButton", "Normal", btn_brush)

# Nine-slice textured border
border_brush = unreal.WidgetBrushInfo()
border_brush.set_editor_property("resource_path", "/Game/UI/Textures/T_PanelBorder")
border_brush.set_editor_property("draw_as", "Box")
border_brush.set_editor_property("margin", "(Left=12,Top=12,Right=12,Bottom=12)")
border_brush.set_editor_property("tint_color", "(R=1.0,G=1.0,B=1.0,A=0.85)")
unreal.WidgetService.set_brush(path, "ContentBorder", "Background", border_brush)

# Image widget with a texture
img_brush = unreal.WidgetBrushInfo()
img_brush.set_editor_property("resource_path", "/Game/UI/Icons/T_MenuLogo")
img_brush.set_editor_property("image_size", "(X=256.0,Y=128.0)")
unreal.WidgetService.set_brush(path, "LogoImage", "Brush", img_brush)

unreal.EditorAssetLibrary.save_asset(path)
```

---

### Feature 3: Widget Animation API

**Priority: High**

UMG Widget Blueprints hold `UWidgetAnimation` objects as properties. Each animation contains a `UMovieScene` with property tracks — one track per widget-property pair — and keyframes on each track. This is the correct UMG-native way to drive property changes over time. Hand-rolling lerp logic in Blueprint events is not a substitute.

#### Concepts

| Concept | UE Type | Description |
|---------|---------|-------------|
| Animation | `UWidgetAnimation` | Named timeline asset owned by the WBP |
| Track | `UMovieScenePropertyTrack` | Drives one property path on one widget |
| Section | `UMovieSceneSection` | Time range within a track |
| Keyframe | `FMovieSceneFloatValue` etc. | Value at a specific time |

#### New Struct: `FWidgetAnimKeyframe`

```cpp
USTRUCT(BlueprintType)
struct FWidgetAnimKeyframe
{
    GENERATED_BODY()

    /** Time in seconds */
    UPROPERTY(BlueprintReadWrite, Category = "Widget|Animation")
    float Time = 0.0f;

    /** Value as string — same format as set_property (e.g. "0.0", "(R=1,G=0,B=0,A=1)") */
    UPROPERTY(BlueprintReadWrite, Category = "Widget|Animation")
    FString Value;

    /** Interpolation: "Linear", "Cubic", "Constant" */
    UPROPERTY(BlueprintReadWrite, Category = "Widget|Animation")
    FString Interpolation = TEXT("Linear");
};
```

#### New Struct: `FWidgetAnimInfo`

```cpp
USTRUCT(BlueprintType)
struct FWidgetAnimInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Widget|Animation")
    FString AnimationName;

    /** Total duration in seconds */
    UPROPERTY(BlueprintReadWrite, Category = "Widget|Animation")
    float Duration = 0.0f;

    /** Number of property tracks */
    UPROPERTY(BlueprintReadWrite, Category = "Widget|Animation")
    int32 TrackCount = 0;
};
```

#### New Functions

```cpp
/**
 * List all UWidgetAnimation objects on a Widget Blueprint.
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|Widgets|Animation")
static TArray<FWidgetAnimInfo> ListAnimations(const FString& WidgetPath);

/**
 * Create a new UWidgetAnimation on a Widget Blueprint.
 *
 * @param AnimationName - Unique name for the animation (e.g. "FadeIn", "SlideOut")
 * @param Duration      - Total timeline length in seconds
 * @return True if created successfully
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|Widgets|Animation")
static bool CreateAnimation(
    const FString& WidgetPath,
    const FString& AnimationName,
    float Duration = 1.0f);

/**
 * Remove a UWidgetAnimation from a Widget Blueprint.
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|Widgets|Animation")
static bool RemoveAnimation(
    const FString& WidgetPath,
    const FString& AnimationName);

/**
 * Add a property track to an animation.
 * The track drives PropertyName on ComponentName over time.
 *
 * @param AnimationName  - Name of the target animation
 * @param ComponentName  - Widget component to animate (e.g. "FadePanel")
 * @param PropertyName   - Property to animate: "RenderOpacity", "ColorAndOpacity",
 *                         "Position X", "Position Y", "Size X", "Size Y"
 * @return True if track was added (or already existed)
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|Widgets|Animation")
static bool AddAnimationTrack(
    const FString& WidgetPath,
    const FString& AnimationName,
    const FString& ComponentName,
    const FString& PropertyName);

/**
 * Add a keyframe to an existing animation track.
 * The track must already exist (call add_animation_track first).
 *
 * @param AnimationName  - Name of the target animation
 * @param ComponentName  - Widget component (must match a track)
 * @param PropertyName   - Property name (must match a track)
 * @param Keyframe       - Time, value, and interpolation
 * @return True if keyframe was placed
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|Widgets|Animation")
static bool AddKeyframe(
    const FString& WidgetPath,
    const FString& AnimationName,
    const FString& ComponentName,
    const FString& PropertyName,
    const FWidgetAnimKeyframe& Keyframe);
```

#### Python Usage — Fade-In Animation

```python
import unreal

path = "/Game/UI/WBP_MainMenu"

# Step 1: create the animation timeline (0.5 seconds)
unreal.WidgetService.create_animation(path, "FadeIn", 0.5)

# Step 2: add a track — animate RenderOpacity on the root canvas
unreal.WidgetService.add_animation_track(path, "FadeIn", "RootCanvas", "RenderOpacity")

# Step 3: place keyframes
kf_start = unreal.WidgetAnimKeyframe()
kf_start.set_editor_property("time", 0.0)
kf_start.set_editor_property("value", "0.0")
kf_start.set_editor_property("interpolation", "Cubic")
unreal.WidgetService.add_keyframe(path, "FadeIn", "RootCanvas", "RenderOpacity", kf_start)

kf_end = unreal.WidgetAnimKeyframe()
kf_end.set_editor_property("time", 0.5)
kf_end.set_editor_property("value", "1.0")
kf_end.set_editor_property("interpolation", "Cubic")
unreal.WidgetService.add_keyframe(path, "FadeIn", "RootCanvas", "RenderOpacity", kf_end)

unreal.EditorAssetLibrary.save_asset(path)
```

#### Python Usage — Slide-In Animation (position + opacity)

```python
import unreal

path = "/Game/UI/WBP_MainMenu"

unreal.WidgetService.create_animation(path, "SlideIn", 0.4)

# Track 1: position X (slide from -200 to 0)
unreal.WidgetService.add_animation_track(path, "SlideIn", "MenuPanel", "Position X")
for t, x in [(0.0, "-200.0"), (0.4, "0.0")]:
    kf = unreal.WidgetAnimKeyframe()
    kf.set_editor_property("time", t)
    kf.set_editor_property("value", x)
    kf.set_editor_property("interpolation", "Cubic")
    unreal.WidgetService.add_keyframe(path, "SlideIn", "MenuPanel", "Position X", kf)

# Track 2: opacity
unreal.WidgetService.add_animation_track(path, "SlideIn", "MenuPanel", "RenderOpacity")
for t, v in [(0.0, "0.0"), (0.4, "1.0")]:
    kf = unreal.WidgetAnimKeyframe()
    kf.set_editor_property("time", t)
    kf.set_editor_property("value", v)
    kf.set_editor_property("interpolation", "Linear")
    unreal.WidgetService.add_keyframe(path, "SlideIn", "MenuPanel", "RenderOpacity", kf)

unreal.EditorAssetLibrary.save_asset(path)
```

#### Animatable Properties

| Property | Value Format | Notes |
|----------|-------------|-------|
| `RenderOpacity` | `"0.0"` – `"1.0"` | All widgets |
| `ColorAndOpacity` | `"(R=1,G=1,B=1,A=1)"` | TextBlock, Image |
| `Position X` | `"100.0"` | Canvas slot only |
| `Position Y` | `"100.0"` | Canvas slot only |
| `Size X` | `"200.0"` | Canvas slot only |
| `Size Y` | `"50.0"` | Canvas slot only |
| `Percent` | `"0.0"` – `"1.0"` | ProgressBar |

The implementation resolves canvas slot position/size properties to the `UCanvasPanelSlot` object, not the widget itself, which requires resolving the slot in the movie scene binding target.

---

### Feature 4: Widget Preview Capture

**Priority: High**

The agent has no way to see what it built. Layout bugs — wrong anchors, clipped text, overlapping panels — go undetected until a human opens the UMG designer. A render-to-texture preview closes this loop without requiring PIE.

#### Mechanism

Unreal's `FWidgetRenderer` renders a `UUserWidget` instance off-screen to a `UTextureRenderTarget2D` at a specified resolution. This is the same path used by UMG designer thumbnails. The render target can be exported to PNG and returned as a file path.

#### New Struct: `FWidgetPreviewResult`

```cpp
USTRUCT(BlueprintType)
struct FWidgetPreviewResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Widget|Preview")
    bool bSuccess = false;

    /** Absolute file path of the exported PNG, e.g. "C:/Project/Saved/WidgetPreviews/WBP_MainMenu.png" */
    UPROPERTY(BlueprintReadWrite, Category = "Widget|Preview")
    FString OutputPath;

    UPROPERTY(BlueprintReadWrite, Category = "Widget|Preview")
    int32 Width = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Widget|Preview")
    int32 Height = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Widget|Preview")
    FString ErrorMessage;
};
```

#### New Function: `CapturePreview`

```cpp
/**
 * Render a Widget Blueprint to a PNG file for visual verification.
 * Uses FWidgetRenderer to render off-screen — no PIE required.
 * Output is written to <ProjectDir>/Saved/WidgetPreviews/<AssetName>.png.
 *
 * @param WidgetPath - Full path to the Widget Blueprint
 * @param Width      - Render width in pixels (default 1920)
 * @param Height     - Render height in pixels (default 1080)
 * @return Result with output path and dimensions
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|Widgets|Preview")
static FWidgetPreviewResult CapturePreview(
    const FString& WidgetPath,
    int32 Width = 1920,
    int32 Height = 1080);
```

#### Python Usage

```python
import unreal

result = unreal.WidgetService.capture_preview("/Game/UI/WBP_MainMenu", 1920, 1080)

if result.b_success:
    print(f"Preview saved: {result.output_path}")
    # The agent can now read this image file to verify layout
else:
    print(f"Preview failed: {result.error_message}")
```

#### Limitations

- `FWidgetRenderer` requires a world context. The implementation creates a transient `UWorld` (game world type) for the render pass and destroys it immediately after.
- Animated states render at time 0. To preview a mid-animation frame, call `add_keyframe` to set the desired state as a static property, preview, then revert.
- Custom fonts require the font assets to be cooked/loaded in the editor. Fonts not loaded at preview time fall back to the engine default.

---

### Feature 5: PIE Widget Lifecycle

**Priority: Medium**

Some UI issues only manifest at runtime — focus handling, scroll behavior, dynamic text overflow. A PIE lifecycle API lets the agent launch Play-In-Editor, spawn a widget into the viewport, query its live state, and stop PIE — all without human interaction.

#### New Struct: `FPIEWidgetHandle`

```cpp
USTRUCT(BlueprintType)
struct FPIEWidgetHandle
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Widget|PIE")
    bool bValid = false;

    /** Instance identifier for subsequent get/set calls */
    UPROPERTY(BlueprintReadWrite, Category = "Widget|PIE")
    FString InstanceId;

    UPROPERTY(BlueprintReadWrite, Category = "Widget|PIE")
    FString ErrorMessage;
};
```

#### New Functions

```cpp
/**
 * Start Play-In-Editor (simulated, not standalone).
 * No-op if PIE is already running.
 * @return True if PIE started or was already running
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|Widgets|PIE")
static bool StartPIE();

/**
 * Stop Play-In-Editor.
 * No-op if PIE is not running.
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|Widgets|PIE")
static bool StopPIE();

/**
 * Check if PIE is currently running.
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|Widgets|PIE")
static bool IsPIERunning();

/**
 * Spawn a Widget Blueprint into the PIE viewport and add it to the viewport stack.
 * PIE must be running.
 *
 * @param WidgetPath - Full path to the Widget Blueprint
 * @param ZOrder     - Viewport Z order (higher = on top)
 * @return Handle for subsequent get/set calls
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|Widgets|PIE")
static FPIEWidgetHandle SpawnWidgetInPIE(
    const FString& WidgetPath,
    int32 ZOrder = 0);

/**
 * Get a property value from a live PIE widget instance.
 * Use the InstanceId from SpawnWidgetInPIE.
 *
 * @param Handle        - Handle from spawn_widget_in_pie
 * @param ComponentName - Widget component name
 * @param PropertyName  - Property to read
 * @return Current runtime value as string (empty on failure)
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|Widgets|PIE")
static FString GetLiveProperty(
    const FPIEWidgetHandle& Handle,
    const FString& ComponentName,
    const FString& PropertyName);

/**
 * Remove a live PIE widget from the viewport.
 * Does not stop PIE.
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|Widgets|PIE")
static bool RemoveWidgetFromPIE(const FPIEWidgetHandle& Handle);
```

#### Python Usage

```python
import unreal

# Start PIE and test the main menu
unreal.WidgetService.start_pie()

handle = unreal.WidgetService.spawn_widget_in_pie("/Game/UI/WBP_MainMenu", 0)
if handle.b_valid:
    opacity = unreal.WidgetService.get_live_property(handle, "TitleText", "RenderOpacity")
    print(f"TitleText opacity at runtime: {opacity}")

    visibility = unreal.WidgetService.get_live_property(handle, "PlayButton", "Visibility")
    print(f"PlayButton visibility: {visibility}")

    unreal.WidgetService.remove_widget_from_pie(handle)

unreal.WidgetService.stop_pie()
```

#### PIE Rules

- **PIE must be running** before `spawn_widget_in_pie` — call `is_pie_running()` first and start if needed
- **Handles are invalidated** when PIE stops — always call `stop_pie` before ending a test session
- **`get_live_property` reads from the running instance**, not the asset. Values reflect any runtime changes (animations, Blueprint logic)
- **One PIE world per session** — VibeUE always uses the first PIE world

---

## Summary: Functions Added in v2

| Function | Category |
|----------|---------|
| `set_font` | Font |
| `get_font` | Font |
| `set_brush` | Brush |
| `get_brush` | Brush |
| `list_animations` | Animation |
| `create_animation` | Animation |
| `remove_animation` | Animation |
| `add_animation_track` | Animation |
| `add_keyframe` | Animation |
| `capture_preview` | Preview |
| `start_pie` | PIE |
| `stop_pie` | PIE |
| `is_pie_running` | PIE |
| `spawn_widget_in_pie` | PIE |
| `get_live_property` | PIE |
| `remove_widget_from_pie` | PIE |

Total: **16 new functions**, bringing WidgetService from 24 to **40 actions**.

---

## Implementation Notes

### Font

`FSlateFontInfo` is a struct on `UTextBlock::Font`, `UEditableTextBox::WidgetStyle.Font`, and similar. Set it via `FProperty::ImportText_Direct` after constructing the struct in C++. The outline is a nested `FFontOutlineSettings` struct inside `FSlateFontInfo` — it must be written as part of the same import, not separately.

### Brush

`FSlateBrush` is set on `UImage::Brush`, `UButton::WidgetStyle.Normal/Hovered/Pressed/Disabled`, `UBorder::Background`, `UProgressBar::BackgroundImage/FillImage`. The resource object is set via `FSlateBrush::SetResourceObject(UObject*)` after loading the asset with `StaticLoadObject`. Nine-slice margins are `FSlateBrush::Margin` (a `FMargin`). Draw mode is `ESlateBrushDrawType::Type`.

### Animation

`UWidgetAnimation` is a `UObject` property on `UWidgetBlueprint`. Create it with `NewObject<UWidgetAnimation>(WidgetBP)` and register it in `WidgetBP->Animations`. Add a `UMovieScene` to it, then add `UMovieSceneWidgetMaterialTrack` or `FMovieSceneFloatChannel` tracks. The binding target maps widget names via `FWidgetAnimationBinding` entries on the `UWidgetAnimation`. Compile the blueprint after modifying animations.

### Preview

`FWidgetRenderer` requires `UUserWidget::Initialize(FLocalPlayerContext(...))` before rendering. Use `UGameplayStatics::GetPlayerController(GEditor->PlayWorld ?? GEditor->GetEditorWorldContext().World(), 0)` for the context. Export the `UTextureRenderTarget2D` to PNG with `FImageUtils::ExportRenderTargetTexture2DToFile`.

### PIE

`GEditor->RequestPlaySession(FRequestPlaySessionParams(...))` starts PIE. `GEditor->RequestEndPlayMap()` stops it. The PIE world is `GEditor->PlayWorld`. Widget instances in PIE are `UUserWidget*` objects obtained via `UGameplayStatics::GetAllActorsOfClass` is not applicable — instead use `UWidgetBlueprintLibrary::Create` on the PIE world and `UGameInstance::GetSubsystem` pattern to track handles.
