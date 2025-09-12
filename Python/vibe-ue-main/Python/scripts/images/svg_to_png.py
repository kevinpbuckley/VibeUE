#!/usr/bin/env python
"""
svg_to_png.py

Standalone SVG->PNG conversion pipeline optimized for Unreal Engine UMG textures.

Features:
- Uses cairosvg for high fidelity vector rendering.
- Optional post-processing (premultiply alpha, gamma correction, resizing to power-of-two or nearest multiple of 4).
- Optional generation of a subtle blue-noise dither to reduce banding in gradients.
- Strips fully transparent pixels to pure zero for better compression.
- Allows explicit background color or transparent background.
- Can auto-generate both SRGB and linear variants (naming suffix _LIN) if requested.
- Ensures output PNGs use 8-bit RGBA (Unreal friendly) and can optionally downscale/upscale to a max dimension.

Requirements:
    pip install cairosvg pillow numpy

Usage:
    python svg_to_png.py input.svg [--out OUT.png] [--width 2048] [--height 2048]
                                 [--max-dim 4096] [--pot] [--mult4]
                                 [--bg 0,0,0,0] [--dither 0.5]
                                 [--gamma 2.2] [--premultiply]
                                 [--linear-variant]

Exit codes:
 0 success
 1 invalid args / file not found
 2 conversion failure

"""
from __future__ import annotations
import argparse, sys, os, math, io, tempfile
from pathlib import Path
from typing import Tuple, Optional

# Import required packages with fallbacks
import argparse, sys, os, math, io, tempfile
from pathlib import Path
from typing import Tuple, Optional

# Try cairosvg first, fall back to other methods
CAIROSVG_AVAILABLE = False
WAND_AVAILABLE = False

try:
    import cairosvg
    CAIROSVG_AVAILABLE = True
    print("Using CairoSVG for conversion")
except Exception as e:
    print(f"CairoSVG not available: {e}")

if not CAIROSVG_AVAILABLE:
    try:
        from wand.image import Image as WandImage
        from wand.color import Color as WandColor
        WAND_AVAILABLE = True
        print("Using Wand (ImageMagick) for conversion")
    except Exception as e:
        print(f"Wand not available: {e}")

# Always try to import PIL and numpy
try:
    from PIL import Image
    import numpy as np
except Exception as e:
    print(f"Missing PIL or numpy: {e}")
    sys.exit(1)

if not CAIROSVG_AVAILABLE and not WAND_AVAILABLE:
    print("ERROR: No SVG conversion library available!")
    print("Install either:")
    print("  1. cairosvg + cairo C library: conda install -c conda-forge cairosvg")
    print("  2. Wand + ImageMagick: pip install Wand")
    sys.exit(1)


def parse_color_rgba(s: str) -> Tuple[int,int,int,int]:
    parts = [p.strip() for p in s.split(',')]
    if len(parts) != 4:
        raise ValueError("Color must be r,g,b,a")
    vals = []
    for p in parts:
        if '.' in p:
            f = float(p)
            if not (0.0 <= f <= 1.0):
                raise ValueError("Float components must be in 0..1")
            vals.append(int(round(f * 255)))
        else:
            i = int(p)
            if not (0 <= i <= 255):
                raise ValueError("Integer components must be in 0..255")
            vals.append(i)
    return tuple(vals)  # type: ignore


def nearest_power_of_two(n: int) -> int:
    return 1 << (n - 1).bit_length()


def round_multiple(n: int, mult: int) -> int:
    return int(math.ceil(n / mult) * mult)


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="Convert SVG to PNG optimized for Unreal UMG")
    p.add_argument('input', help='Input SVG file')
    p.add_argument('--out', '-o', help='Output PNG file (default: same name .png)')
    p.add_argument('--width', type=int, help='Target raster width (overrides intrinsic)')
    p.add_argument('--height', type=int, help='Target raster height (overrides intrinsic)')
    p.add_argument('--max-dim', type=int, default=4096, help='Clamp largest dimension (default 4096)')
    p.add_argument('--pot', action='store_true', help='Force power-of-two dimensions after render')
    p.add_argument('--mult4', action='store_true', help='Round dimensions up to multiple of 4 after render')
    p.add_argument('--bg', type=str, default='0,0,0,0', help='Background RGBA (comma) or floats 0..1; default transparent')
    p.add_argument('--dither', type=float, default=0.0, help='Apply low-amplitude blue-noise style dither (0..2)')
    p.add_argument('--gamma', type=float, default=2.2, help='Gamma to assume for SRGB -> linear pre-premultiply (if premultiply)')
    p.add_argument('--premultiply', action='store_true', help='Premultiply alpha (recommended for UMG gradients)')
    p.add_argument('--linear-variant', action='store_true', help='Also emit a _LIN variant approximating linear space')
    p.add_argument('--quiet', action='store_true', help='Suppress non-error logs')
    return p


def log(msg: str, quiet: bool):
    if not quiet:
        print(msg)


def generate_blue_noise(shape: Tuple[int,int], amplitude: float) -> np.ndarray:
    # Simple approach: start white noise then blur via FFT falloff to favor mid frequencies.
    h, w = shape
    noise = np.random.rand(h, w).astype(np.float32) - 0.5
    if amplitude <= 0:
        return np.zeros_like(noise)
    # Frequency domain shaping
    f = np.fft.rfft2(noise)
    yy = np.fft.fftfreq(h)[:,None]
    xx = np.fft.rfftfreq(w)[None,:]
    radius = np.sqrt(xx*xx + yy*yy)
    # Emphasize band-ish mid frequencies, suppress very low & very high.
    falloff = np.exp(- (radius*2.5)**2)  # gaussian emphasizing center; invert pattern slightly
    shaped = f * (1.0 - falloff)
    noise2 = np.fft.irfft2(shaped)
    noise2 = noise2 / (np.max(np.abs(noise2)) + 1e-6)
    return noise2 * amplitude


def apply_post(image: Image.Image, args) -> Image.Image:
    # Convert to RGBA explicitly
    img = image.convert('RGBA')
    w, h = img.size

    arr = np.array(img, dtype=np.uint8)

    # Strip fully transparent garbage (set RGB=0 where A=0) for better compression.
    mask_zero = arr[...,3] == 0
    arr[mask_zero, 0:3] = 0

    # Optional premultiply (convert to float first)
    if args.premultiply:
        rgb = arr[...,0:3].astype(np.float32) / 255.0
        alpha = arr[...,3:4].astype(np.float32) / 255.0
        # approximate gamma removal then premultiply then restore gamma.
        gamma = max(0.01, args.gamma)
        rgb_lin = np.power(rgb, gamma)
        rgb_lin *= alpha
        rgb = np.power(np.clip(rgb_lin, 0, 1), 1.0/gamma)
        arr[...,0:3] = (rgb * 255.0 + 0.5).astype(np.uint8)

    # Dither (adds subtle noise on RGB only where alpha>0)
    if args.dither > 0.0:
        amp = min(2.0, max(0.0, args.dither)) / 255.0 * 8.0  # scale
        noise = generate_blue_noise((h, w), amp)
        rgbf = arr[...,0:3].astype(np.float32)/255.0
        alpha = arr[...,3:4].astype(np.float32)/255.0
        rgbf += noise[...,None] * alpha  # scale by alpha so transparent stays clean
        rgbf = np.clip(rgbf, 0, 1)
        arr[...,0:3] = (rgbf*255.0 + 0.5).astype(np.uint8)

    return Image.fromarray(arr, 'RGBA')


def resize_dims(w: int, h: int, args) -> Tuple[int,int]:
    # Clamp max dim
    scale = 1.0
    max_dim = args.max_dim
    if max_dim and max(w,h) > max_dim:
        scale = max_dim / float(max(w,h))
    if args.width and args.height:
        return args.width, args.height
    if args.width:
        scale = args.width / float(w)
    elif args.height:
        scale = args.height / float(h)
    nw = int(round(w * scale))
    nh = int(round(h * scale))
    nw = max(1, nw)
    nh = max(1, nh)
    if args.pot:
        nw = nearest_power_of_two(nw)
        nh = nearest_power_of_two(nh)
    if args.mult4:
        nw = round_multiple(nw, 4)
        nh = round_multiple(nh, 4)
    return nw, nh


def convert_svg_to_png_bytes(svg_path: Path, target_w: int, target_h: int) -> bytes:
    """Convert SVG to PNG bytes using available backend."""
    
    if CAIROSVG_AVAILABLE:
        try:
            return cairosvg.svg2png(
                url=str(svg_path),
                output_width=target_w,
                output_height=target_h,
                background_color=None
            )
        except Exception as e:
            print(f"CairoSVG conversion failed: {e}")
            # Fall through to next method
    
    if WAND_AVAILABLE:
        try:
            with WandImage() as img:
                img.format = 'svg'
                img.background_color = WandColor('transparent')
                with open(svg_path, 'rb') as f:
                    img.blob = f.read()
                img.resize(target_w, target_h)
                img.format = 'png'
                return img.blob
        except Exception as e:
            print(f"Wand conversion failed: {e}")
            # Fall through to manual method
    
    # Last resort: try using subprocess with inkscape if available
    try:
        import subprocess
        temp_png = tempfile.mktemp(suffix='.png')
        cmd = [
            'inkscape',
            '--export-type=png',
            f'--export-width={target_w}',
            f'--export-height={target_h}',
            f'--export-filename={temp_png}',
            str(svg_path)
        ]
        result = subprocess.run(cmd, capture_output=True, timeout=30)
        if result.returncode == 0 and os.path.exists(temp_png):
            with open(temp_png, 'rb') as f:
                data = f.read()
            os.unlink(temp_png)
            return data
    except Exception as e:
        print(f"Inkscape conversion failed: {e}")
    
def convert(svg_path: Path, out_path: Optional[Path], args) -> Tuple[Path, Optional[Path]]:
    if not svg_path.exists():
        raise FileNotFoundError(svg_path)
    if out_path is None:
        out_path = svg_path.with_suffix('.png')

    # Get target dimensions
    target_w, target_h = resize_dims(1024, 1024, args)  # Default size if SVG doesn't specify
    
    # Convert SVG to PNG bytes
    png_bytes = convert_svg_to_png_bytes(svg_path, target_w, target_h)
    
    base_img = Image.open(io.BytesIO(png_bytes))
    orig_w, orig_h = base_img.size

    # Resize/adjust dims if needed
    final_w, final_h = resize_dims(orig_w, orig_h, args)
    if (final_w, final_h) != (orig_w, orig_h):
        base_img = base_img.resize((final_w, final_h), Image.LANCZOS)

    # Background apply if requested (non-transparent)
    bg_rgba = parse_color_rgba(args.bg)
    if bg_rgba[3] > 0:  # has alpha
        bg = Image.new('RGBA', base_img.size, bg_rgba)
        # alpha composite
        base_img = Image.alpha_composite(bg, base_img.convert('RGBA'))

    processed = apply_post(base_img, args)

    # Save SRGB variant
    processed.save(out_path, format='PNG', optimize=True)

    linear_variant_path = None
    if args.linear_variant:
        # derive a rough linear variant (assumes current is srgb-premult or not; just gamma expand) 
        arr = np.array(processed, dtype=np.uint8)
        rgb = arr[...,0:3].astype(np.float32)/255.0
        lin = np.power(rgb, 2.2)  # simple approach
        arr[...,0:3] = (np.clip(lin,0,1)*255.0+0.5).astype(np.uint8)
        lin_img = Image.fromarray(arr, 'RGBA')
        linear_variant_path = out_path.with_name(out_path.stem + '_LIN.png')
        lin_img.save(linear_variant_path, format='PNG', optimize=True)

    return out_path, linear_variant_path
    if not svg_path.exists():
        raise FileNotFoundError(svg_path)
    if out_path is None:
        out_path = svg_path.with_suffix('.png')

    # Render initial raster to bytes
    # Let cairosvg compute intrinsic size unless width/height provided.
    png_bytes = cairosvg.svg2png(url=str(svg_path),
                                 output_width=args.width if args.width else None,
                                 output_height=args.height if args.height else None,
                                 background_color=None)  # keep alpha

    base_img = Image.open(io.BytesIO(png_bytes))
    orig_w, orig_h = base_img.size

    # Resize/adjust dims
    target_w, target_h = resize_dims(orig_w, orig_h, args)
    if (target_w, target_h) != (orig_w, orig_h):
        base_img = base_img.resize((target_w, target_h), Image.LANCZOS)

    # Background apply if requested (non-transparent)
    bg_rgba = parse_color_rgba(args.bg)
    if bg_rgba[3] > 0:  # has alpha
        bg = Image.new('RGBA', base_img.size, bg_rgba)
        # alpha composite
        base_img = Image.alpha_composite(bg, base_img.convert('RGBA'))

    processed = apply_post(base_img, args)

    # Save SRGB variant
    processed.save(out_path, format='PNG', optimize=True)

    linear_variant_path = None
    if args.linear_variant:
        # derive a rough linear variant (assumes current is srgb-premult or not; just gamma expand) 
        arr = np.array(processed, dtype=np.uint8)
        rgb = arr[...,0:3].astype(np.float32)/255.0
        lin = np.power(rgb, 2.2)  # simple approach
        arr[...,0:3] = (np.clip(lin,0,1)*255.0+0.5).astype(np.uint8)
        lin_img = Image.fromarray(arr, 'RGBA')
        linear_variant_path = out_path.with_name(out_path.stem + '_LIN.png')
        lin_img.save(linear_variant_path, format='PNG', optimize=True)

    return out_path, linear_variant_path


def main():
    parser = build_parser()
    args = parser.parse_args()
    try:
        svg = Path(args.input).resolve()
        out = Path(args.out).resolve() if args.out else None
        final_png, lin_png = convert(svg, out, args)
        payload = {
            "status": "ok",
            "input": str(svg),
            "output": str(final_png),
            "linear_variant": str(lin_png) if lin_png else None,
            "width": Image.open(final_png).width,
            "height": Image.open(final_png).height,
            "pot": args.pot,
            "multiple_of_4": args.mult4,
            "premultiplied": args.premultiply,
            "dither": args.dither,
        }
        if not args.quiet:
            print(payload)
        sys.exit(0)
    except FileNotFoundError as e:
        print(f"[ERROR] File not found: {e}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"[ERROR] Conversion failed: {e}", file=sys.stderr)
        sys.exit(2)

if __name__ == '__main__':
    main()
