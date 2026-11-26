# Pokrytí repa PicoLibSDK

Stav k 2026-11-26, základ commitu bd2e191 (větev m3-coverage, runner přidává následující commit). Spuštěno `tools/coverage/run.cmake` (viz README, sekce Pokrytí repa): 41 her z `PicoPad/GAME` a `PicoPad/TINYGAME` postaveno beze změny zdrojáků, každá spuštěna headless se skriptem `tools/coverage/smoke.txt` (snímek ve 3,0 s, `quit` ve 4,5 s).

**built=41 ran=41 nonuniform=41 total=41** – všech 41 her se postaví, doběhne s kódem 0 (`end reason=quit`, `stalls=0 pending=0`) a snímek má aspoň 2 barvy. Tento test je jen smoke: ukazuje, že hra startuje a kreslí, ne že se dá hrát (vstup do her skript neposílá). Snímky se do repa necommitují (vznikají v `build/coverage/shots`, přehledový obrázek `build/coverage/sheet.png`).

Zvuk: GAME = PWM (zařízení Picopad má `USE_PWMSND 4` v `_devices/picopad/_config.h`, hry `config.h` nepřepisují), TINYGAME = GPIO (všechny mají `USE_PWMSND 0` a `PICOKIT.cpp`/`ELECTRO.cpp` bit-bangují `PWMSND_GPIO`, emulátor to vzorkuje). Sloupec „WAV (špička)“ je špička vzorků WAV z téhož smoke běhu (runner předává `--wav smoke.wav`, `tests/wav_tone.py`): `ticho` = špička 0 za celých 4,5 s virtuálního času, jinak `peak=N`; ticho znamená jen, že hra v menu/intru zatím nehraje, ne že zvuk nemá. SD: jen PICTOR volá `DiskAutoMount()`/`FileOpen()` (žebříček `/GAME/PICTOR.TOP`, `src/gamemenu.cpp`); ostatní hry kartu nepoužívají. PICTOR zapisuje `/GAME/PICTOR.TOP` a adresář `GAME` nevytváří, takže obraz karty musí složku `GAME/` obsahovat (např. `picopad-sdimg create card.img --from dir`, kde `dir` obsahuje podadresář `GAME/`).

| hra | stav | snímek ve 3 s | zvuk | WAV (špička) | SD | poznámka |
|---|---|---|---|---|---|---|
| G_ANTS | ok (68 barev) | úvodní obrazovka ANTS s popisem a ikonami hráčů | PWM | ticho | – | |
| G_ATOMS | ok (7) | menu Exploding Atoms, volba hráčů | PWM | ticho | – | |
| G_CHESS | ok (3) | menu CHESS (strana, úroveň PRO) | PWM | ticho | – | |
| G_EGGS | ok (16) | úvod „EGGS“ (pánev s vejcem) | PWM | peak=18453 | – | |
| G_FIFTEEN | ok (10) | rozmíchaná hrací deska 4×4 | PWM | peak=32743 | – | |
| G_FIFTEEN2 | ok (3) | výběr obrázku (Digits, Girl with Kitten, …) | PWM | ticho | – | |
| G_FLAPPY | ok (4) | výběr sady scén | PWM | ticho | – | |
| G_GHOSTRAC | ok (286) | závodní hra, auto na silnici | PWM | ticho | – | |
| G_INVADERS | ok (7) | invaders, vetřelci a štíty, skóre 0 | PWM | peak=22538 | – | |
| G_LIFE | ok (12) | hra Life, svislé buňky na fialové mřížce | PWM | ticho | – | |
| G_MAZE | ok (6) | tmavá šumová plocha s malým barevným kurzorem (ve 3 i v 9 s stejně) | PWM | ticho | – | |
| G_MULTISET | ok (244) | menu s dlaždicemi všech 16 miniher | PWM | ticho | – | |
| G_PACMAN | ok (11) | titulní obrazovka PAC-MAN, duchové | PWM | ticho | – | |
| G_PICTOR | ok (255) | titulní obrazovka Pictor s postavou a nápovědou kláves | PWM | ticho | PICTOR.TOP | bez `--sd` karta „není vložena“, hra běží |
| G_RAPTOR | ok (50) | letadlo nad krajinou | PWM | ticho | – | |
| G_SOKOBAN | ok (15) | titulní obrazovka SOKOBAN z cihel | PWM | ticho | – | |
| G_TETRIS | ok (80) | Tetris s cihlovým rámem, pauza | PWM | ticho | – | |
| G_TRAIN | ok (14) | hrací plocha s vláčkem, stromy, jablky | PWM | peak=32709 | – | |
| G_TVTENNIS | ok (4) | menu TV Tennis | PWM | ticho | – | |
| G_VEGASLOT | ok (647) | herní automat VEGASLOT | PWM | ticho | – | |
| G_ZOOM | ok (257) | úvod Buck Rogers: Planet of Zoom | PWM | ticho | – | |
| T_AMANIA | ok (2) | bílý obdélník na černém | GPIO | peak=16383 | – | viz odchylky |
| T_ARKANOID | ok (65) | titulní obrazovka Tiny Arkanoid | GPIO | ticho | – | |
| T_DOWNLAND | ok (3) | zelený rámeček „DOWNLAND“ (načítání) | GPIO | ticho | – | v 9 s „OK“, hra běží dál; viz odchylky |
| T_INVADERS | ok (29) | titulek Tiny Invaders | GPIO | ticho | – | |
| T_NOHZDYVE | ok (3) | „PLEASE WAIT NOHZDYVE IS LOADING“ | GPIO | peak=15111 | – | v 9 s ACCEPT/REFUSE; viz odchylky |
| T_TBOMBER | ok (3) | Tiny Bomber, mřížka bloků | GPIO | ticho | – | |
| T_TDDUG | ok (6) | logo Tiny DDUG | GPIO | ticho | – | |
| T_TGILBERT | ok (33) | titulek Tiny Gilbert | GPIO | peak=16201 | – | |
| T_TINYBERT | ok (66) | Tiny-Bert, „Press (B) to START“ | GPIO | ticho | – | |
| T_TINYBIKE | ok (2) | logo Tiny Bike s motorkářem (azurová na černé) | GPIO | ticho | – | hra je z podstaty dvoubarevná |
| T_TINYDOC | ok (3) | titulek Tiny Doc | GPIO | ticho | – | |
| T_TINYPAC | ok (65) | bludiště TinyPac | GPIO | ticho | – | |
| T_TINYPIPE | ok (33) | titulek Tiny-Pipe, Start Game | GPIO | ticho | – | |
| T_TINYTRIS | ok (65) | titulek Tiny-Tris | GPIO | ticho | – | |
| T_TMISSILE | ok (18) | Tiny Missile, INSERT COIN | GPIO | ticho | – | |
| T_TMORPION | ok (38) | piškvorky, volba obtížnosti | GPIO | ticho | – | |
| T_TPINBALL | ok (2) | titulek Tiny Pinball s pálkou | GPIO | peak=16200 | – | hra je z podstaty dvoubarevná |
| T_TPLAQUE | ok (3) | červené okraje hrací plochy a skóre | GPIO | ticho | – | hra se kreslí po částech |
| T_TSQUEST | ok (4) | titulek Tiny-Squest, OXYGEN | GPIO | ticho | – | |
| T_TTRICK | ok (42) | titulek Tiny Trick | GPIO | ticho | – | |

## Známé odchylky

- T_AMANIA: bílá obrazovka ve 3 s je vlastní falešné „načítání disku“ hry (`BootIntro`/`FakeLoad`, `main.cpp:441-457`, bílo od 2,2 do ~3,5 s), ne chyba emulátoru. Dvoubarevný bílo-černý snímek proto prochází smoke testem (≥ 2 barvy). Skutečná titulní obrazovka je v 6 s; ta je v testu `repo_smoke_T_AMANIA` (`tests/headless/amania.txt`).
- T_DOWNLAND a T_NOHZDYVE: ve 3 s ještě běží vlastní úvod hry („načítání“); v 9 s je u NOHZDYVE volba ACCEPT/REFUSE, u DOWNLAND zelené okno s „OK“. Smoke test s časem 3 s tedy zachytí úvod, ne hru.
- Smoke test neposílá hrám klávesy, takže neověřuje hratelnost; ruční ověření je v README, sekce Ověřeno.
- Měřítko času v headless režimu: hry, které čekají bez volání SDK (např. PACMAN, SOKOBAN, VEGASLOT), běží pod hlídačem pomaleji než reálný čas (viz README, Známá omezení). Smoke test je kvůli tomu jen orientační, obraz ve 3 s virtuálního času může být o něco dřív v animaci, než by byl na zařízení.
