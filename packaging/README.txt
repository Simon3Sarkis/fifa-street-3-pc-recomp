FIFA Street 3 - PC port, version 0.0.3 (pre-release, barebone port)
=====================================================================

A native PC and Steam Deck port of the Xbox 360 FIFA Street 3, made by static recompilation
(ReXGlue SDK). This is a BAREBONE PORT: the goal is to play exactly like the Xbox 360 version first.
It will improve over time.

This release is for TESTING. Please report what doesn't work like the Xbox 360 version (crashes,
picture or sound differences, slowdowns). Feature requests come later, once the port plays 1:1
with the Xbox 360 version.

You need your own copy of the game. No game files are included.

Starting it
-----------
- Windows: run fifastreet3.exe. Linux / Steam Deck: run ./fifastreet3
- The first time, pick your FIFA Street 3 disc image (.iso) with "Select ISO...". It is read as it
  is; nothing is extracted. Or pick an extracted game folder with "Folder...", or put the .iso
  next to the program, and it is found by itself.
- Only the NTSC Xbox 360 version (title 4541083B) is tested. Other regions and versions are
  allowed, but they may not start or may not work correctly. Reports welcome (attach the newest
  file from logs\, it names the version).
- With RecompLauncher: it reads recomplauncher.json, asks for your disc image and starts the game
  straight in. Saves and the shader cache then go in the recompdata folder here.

Options
-------
Before the game: the launcher screen. In the game: F1, Esc, or Start + Back together on the
controller; the game is paused while the menu is open. Tabs: LB / RB (or PgUp / PgDn).
Every option's default is the Xbox 360 picture.
- Display: display, screen mode, refresh rate, vsync, FPS counter (off / FPS / FPS + frame time /
  detailed).
- Graphics: game resolution (720p / 1080p), render scale (1x-4x), upscaling filter (bilinear,
  AMD FSR 1, AMD CAS) with sharpening, texture filtering (game default, 2x-16x anisotropic),
  anti-aliasing (off, FXAA, FXAA extreme). The frame rate stays at 60, as on the console.
In the game each row says NOW (changes at once) or RESTART (game resolution, render scale and
vsync: the next time the game starts).
- Updates: see Updating below.
Left / right change a value, A opens its list. The mouse pointer shows only in the menus.

Steam Deck: keep render scale at 1x. Measured on the Deck: 60 FPS with 99.9% of frames on time at a
15 W TDP (a short hitch about 3 times a match in the heaviest scenes); 99.6% at 10 W.

Your settings are in fifastreet3.user.toml, your saves in userdata\, logs in logs\. Unpacking a
new version over this folder keeps all three.

Updating
--------
With RecompLauncher, it updates the game itself. Otherwise: the Updates tab (launcher screen and
in-game options). At start the game asks GitHub once whether a newer version is out ("Check at
start", on by default; or press Check now). Nothing is downloaded until you press Update: it
downloads the new version, checks it, installs it over this folder and offers Restart now.
Windows only; on Linux the tab links to the release page. Your settings, saves, shader cache and
logs are kept. By hand: unzip the new version over this folder.

Known issues
------------
- Short stutters the first time each scene appears, while its shaders are built for your GPU. They
  are kept (userdata\cache) and don't come back.
- At 60 Hz, 120 Hz, 180 Hz or 240 Hz every frame stays on screen equally long (smooth 60). Other
  rates (144 Hz, 165 Hz) can't divide 60 evenly: there exclusive fullscreen at 60 or 120 Hz is
  smoother.

Source and reports: https://github.com/Simon3Sarkis/fifa-street-3-pc-recomp
Licences: licenses\ (this port is GPL-3.0; the ReXGlue SDK is BSD-3-Clause).
FIFA Street 3 is (c) Electronic Arts. This project is not affiliated with or endorsed by EA or
Microsoft.
