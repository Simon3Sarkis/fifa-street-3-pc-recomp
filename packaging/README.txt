FIFA Street 3 - PC port, version 0.0.1 (pre-release, barebone port)
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
  is; nothing is extracted. Or put the .iso next to the program, and it is found by itself.
- With RecompLauncher: add this folder (it holds recomplauncher.json); the launcher asks for the
  game and starts it.

Options
-------
Before the game: the launcher screen. In the game: F1, Esc, or Start + Back together on the
controller. Display, screen mode, refresh rate, vsync, game resolution (720p / 1080p), render scale
(1x-4x). The frame rate stays at 60, as on the console. Game resolution, render scale and vsync
apply the next time the game starts.

Steam Deck: keep render scale at 1x. Measured on the Deck at 720p: 60 FPS, and about 99.8% of
frames on time even at a 10 W TDP.

Your settings are in fifastreet3.user.toml, your saves in userdata\, logs in logs\. Unpacking a
new version over this folder keeps all three.

Known issues
------------
- Short stutters the first time each scene appears, while its shaders are built for your GPU. They
  are kept (userdata\cache) and don't come back.
- The game keeps running while the options menu is open.
- Windowed mode on Windows can run below 60 FPS (the desktop compositor); fullscreen is smooth.
- On a high refresh rate display a few frames per minute can be held a refresh too long.

Source and reports: https://github.com/Simon3Sarkis/fifa-street-3-pc-recomp
Licences: licenses\ (this port is GPL-3.0; the ReXGlue SDK is BSD-3-Clause).
FIFA Street 3 is (c) Electronic Arts. This project is not affiliated with or endorsed by EA or
Microsoft.
