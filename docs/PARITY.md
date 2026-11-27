# 1:1 checklist (Stage A)

What PLAN.md section 6 asks for, area by area, with the evidence. "Works" means seen working in this
port; "matches" means compared against Xbox 360 reference footage. The owner signs off on the gate.

Test runs: `tests/input/parity/*.txt`, made with `tools/explore.py` (walks menus, snaps every step) and
run with `tools/run_script.ps1 -Save work\saves\fresh_profile`. Snaps: `work/snaps/<run>/`, logs:
`work/runs/<run>.log` (local only, they hold game pictures).

## Modes and menus

| Item | Status | Evidence |
|---|---|---|
| Front end: legal, title, press start, profile create, autosave notices, main menu | Works | NOTES 4 (`play_now.txt`) |
| Play Now: teams, line-ups, match settings, match | Works | NOTES 4, 6 |
| FIFA Street Challenge: challenge, team, progress, sides, line-ups, loading tip, team intros, 5-min match, post-game menu (rematch / next / continues / restart confirm) | Works | `px_challenge`, `px_fullmatch2`, `px_sides` (2026-11-27) |
| A timed match to the final whistle | Works | `px_fullmatch2`: 5:00 to 0:00, post-game menu |
| Head to Head (2 controllers): event, setup (best of 3, 5 min, arena, ball), both pick teams, home room, line-ups, match with both players' markers | Works | `px_h2h` |
| Playground Picks (2 controllers): sides, match settings, picking on the court with the countdown, both players picking | Works | `px_picks2` |
| Practice Mode: team, line-ups, arena, practice court with keeper | Works | `px_practice` |
| Xbox Live: menu opens; any online item says "You must be signed in to Xbox Live and the EA servers..." (as a console without Live) | Works | `px_live` |
| My FIFA Street: game settings, controls help, video calibration, EA Trax playlist, profile management | Works | `px_myfs` |
| Credits | Not checked yet | |

## Areas of the gate

| Area | Status | Evidence |
|---|---|---|
| Picture | Matches in the views with footage | NOTES 5-6: Samba and Rooftop vs IGN 2008 hardware footage (shadows, shading, brightness within YouTube's error); all 7 arenas load and play; Gamebreaker greyscale/glow/beams match IGN's clip. FSI render path. |
| Movies | Works | All 9 movie files open and play (`px_lang1/3/4/5/6`, `--filesystem_log_opens`): EA HD (eng, fre, spa), EA SPORTS BIG, attract (eng, fre, ger, ita, spa); each console language picks its own; the attract movie runs at 30 FPS as the game paces it. |
| Languages | Works | English, French, German, Spanish, Italian title screens (`px_lang*`). |
| Audio | Measured clean; owner's listen pending | NOTES 6 (XMA boundary log was benign), NOTES 17: recordings show 0 gaps, 0 clipping, real-time rate; 0 empty guest audio frames on the PC. |
| Timing | Matches real time | 60 FPS locked (NOTES 25: also in borderless/windowed on the PC); match clock 4:56 -> 4:24 in 32.0 s real (`px_clock`). The Deck: NOTES 15-18. |
| Input | Works | Buttons, sticks, triggers via scripts; 2 controllers (`px_h2h`, `px_picks2`). Rumble and a real controller: owner's test. |
| Gameplay | Works; reference comparison limited | Goals, tricks, Gamebreaker, instant replays, celebrations (`px_fullmatch2`, NOTES 6). With no input the controlled player (often the keeper) stands still, so the other side scores freely: normal. |
| Saves | Works | Round trip: difficulty set to Cheeky, autosaved, quit; next start loads Cheeky (`px_save1` -> `px_save2`). A real console save: owner, if they have one. |
| Stability | Running | `soak_2h`: 2 hours of Play Now matches back to back, memory sampled each minute. |
| Content | Partly | 7 arenas (NOTES 6), modes above, national teams. Street teams and other unlocks need challenge progress. |
