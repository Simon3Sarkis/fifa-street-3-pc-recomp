# 0.1.0 "PC options" - release notes (draft, not released)

Every new option defaults to the Xbox 360 picture; nothing changes until you pick something else.

- **The game pauses while the options menu is open** (F1, Esc, Start + Back). It continues exactly
  where it was when you close it. Switch: `pause_in_options_menu` (default on).
- **Menu tabs**: Display, Graphics, Audio, Controls (LB / RB, PgUp / PgDn). Each in-game row says
  NOW (applies at once) or RESTART (applies the next time the game starts).
- **Upscaling filter** (Graphics, NOW): Bilinear (default, as before), AMD FSR 1, AMD CAS.
  `present_effect`.
- **Sharpening** (Graphics, NOW, with FSR 1 or CAS): five levels; FSR's default is 4, CAS's is 1.
  `present_fsr_sharpness_reduction`, `present_cas_additional_sharpness`.
- **Texture filtering** (Graphics, NOW): Game default (default), 2x / 4x / 8x / 16x anisotropic.
  `anisotropic_override`.
- **Anti-aliasing** (Graphics, NOW): Off (default), FXAA, FXAA (extreme). `swap_post_effect`.
- **FPS counter** (Display, NOW): Off (default), FPS, FPS + frame time, Detailed.
  `show_fps_counter`, `show_fps_counter_detail`.
- Unchanged: screen mode, display, refresh rate, vsync (RESTART), game resolution 720p / 1080p
  (RESTART), render scale 1x-4x (RESTART).
- RecompLauncher: the same options in its Display and Graphics groups.
