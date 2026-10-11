# Generic controls illustration

`generic-pixel-v2.png` is the sole controller illustration used by the game.
It is an original AI-generated static transparent bitmap, created with the
built-in imagegen tool on 2026-10-08. It is not extracted from the original ROM,
a commercial controller image, a vector drawing, a mesh or an animation.

The art direction follows the original game's low-resolution menu language:
visible pixels, muted grey plastic and simple shading. The silhouette and
unlettered buttons intentionally avoid a branded layout illustration. All
families use this same image; physical button names remain family-aware in UI
text and do not come from the illustration.

The build compiles the PNG into a 112 by 84 RGBA16 sprite using nearest sampling.
The PNG remains the authored source; the embedded texture is generated output.
No brand-specific illustration variants are distributed.
