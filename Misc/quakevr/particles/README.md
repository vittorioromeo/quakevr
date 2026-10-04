# Explosion particle texture

`particle_explosion_source.png` is the full-resolution source generated with the built-in imagegen tool. The runtime asset is `quakevr/textures/particle_explosion.tga`, a 128 x 128 RGBA TGA with straight alpha. The particle atlas premultiplies it on upload.

The neutral luminance lets the engine's particle colors tint the fire without multiplying baked brown/orange colors. Explosions use a warm orange tint instead of the sparks' bright yellow palette entry. Broad flame folds and a soft irregular outline remain readable at the atlas resolution. This cell is also used for fire trails and lava effects.

To export with Pillow, convert the source to RGBA, resize to 128 x 128 with `Image.Resampling.LANCZOS`, and save as TGA. Do not premultiply the exported pixels.

Generation prompt:

```text
Use case: stylized-concept. Asset type: game VFX texture, one explosion/fire puff billboard for Quake VR, ultimately downsampled to 128x128. Generate a NEW replacement sprite. Single compact roughly spherical but organically asymmetric turbulent fireball, centered with generous completely transparent padding (outer 12 percent on all sides empty). Broad rolling flame lobes, bright ivory-white hot center, light neutral gray midtone folds, soft wispy irregular outer edges fading smoothly to true transparency. GRAYSCALE / neutral white luminance texture designed for engine tinting: NO baked orange, yellow, red or brown color. Strong readable large-scale structure at 128 pixels, no tiny gritty speckles. Volumetric combustion texture, expanding outward in all directions, no upright flame silhouette, no mushroom cloud, no dark opaque smoke, no ring, no starburst rays, no sharp circular cutout, no square edge, no background, no text, no watermark. Straight-alpha transparency, not premultiplied. This is a production game texture, not a scene or concept sheet.
```

## Fire particle texture

`particle_fire_source.png` was generated with the built-in imagegen tool. `quakevr/textures/particle_fire.tga` is its 128 x 128 straight-alpha RGBA export, used for rising fire tongues and explosion debris trails. The neutral texture receives warm engine tinting.

Generation prompt:

```text
Use case: stylized-concept. Asset type: one game VFX flame particle sprite for Quake VR, used over low-poly Quake flame models, downsampled to 128x128. Generate a single detached twisting tongue of fire, roughly teardrop with asymmetrical curled tip, broad simple folds and a luminous core, soft feathered edges. Neutral white / grayscale luminance only: the engine supplies warm orange fire color. Airy, wispy, translucent, readable at small size, stylized to complement gritty 1990s Quake art, not a dense photoreal fire cloud. Centered within square canvas with 15 percent truly transparent padding on ALL edges, tip pointing upwards. No smoke cloud, no embers scattered elsewhere, no scenery, no ground, no text, no framing. True transparent background and straight alpha.
```
