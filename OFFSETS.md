# OFFSETS

Where the FE3H touch mod finds UI elements and game state (game version 1.2.0). Collected from `src/*.h`, `HANDOFF.md` and live measurements.

**Notation**
- `main` = the game's main module base (0x8504000 in Ryujinx so far).
- `main+X` = a static or function in the game.
- `G` = the common UI object, `HUD` = the battle HUD, `battle` = the battle struct (see Base pointers). `G+X` etc. are offsets inside them.
- `vt` = a widget's vtable, given as an offset from `main`.
- Coordinates are 720p unless marked 1080p. The game stores the hand in 1080p: divide by 1.5.

## Base pointers

| Location | Description |
|---|---|
| `*(main+0x21d9ae0)` | `G`, the common UI object (lists, hand, prompts). Address seen: 0x2133974030 |
| `*(main+0x1aa77a8)` | `battle` struct |
| `*(main+0x1aca288)` | `HUD`, the battle HUD. Moves between boots; always read it from here |
| `*(main+0x1afb448)` | `M`, the monastery UI |
| `*(main+0x21d9b88)` | House picker |
| `*(*(main+0x1aa7cc8))` | Map cell grid (see Map cells) |
| `*(main+0x1ab8480)` | Battle camera |
| `*(main+0x1afb3a0)` | Free-roam camera controller (`FR_CTRL`) |
| `*(*(main+0x1aa7dd8))` | Battle HUD manager. `+0x20498`: the panel refreshed after a target change |
| `*(*(main+0x1ab00b8))` | Map marker manager: tile highlight bitmaps |

## Common UI (`G`)

| Location | Description |
|---|---|
| `G+0x2c` / `+0x30` | Pointing hand x, y (1080p) |
| `G+0x50` | Hand shown (u8) |
| `G+0x58` | Hand owner: the widget the hand is on |
| `G+0xa0` / `+0xa8` / `+0xb0` | Prompt bar label text pointers, right to left (e.g. "Back", "Ask again", "Swap display") |
| `G+0xf0` | Prompt bar button codes (u32 each), right to left. Can lag one screen behind |
| `G+0x118` | Prompt bar count |
| `G+0x120` | Message box ("Sold ... x 1", "not enough points"), vt 0x1a42b98 |
| `G+0xe00` | Yes / No box |
| `G+0x1a28` | Battle quote message window, vt 0x1a2edb0 |
| `G+0x2250` | Options list, vt 0x1a431f8 |
| `G+0x2bd0` | Options tab: 0 Battle, 1 Monastery, 2 General |
| `G+0x2cf8` | Help bar, vt 0x1a43568. UTF-8 text at `+0x3e` |
| `G+0x302e1` | Tutorial / info pop-up open (u8) |
| `G+0x30fa8` | Free-day prompt bar ("L Finish Exploring"), vt 0x1a44db8 |
| `G+0x31120` | Free-roam HUD |
| `G+0x31278` | Dialogue Log list, vt 0x1a3c630 |
| `G+0x60ab8` | Guide: category / topic list open (1 / 2) |
| `G+0xbbc64` | Cutscene message window, vt 0x1a2edb0 |
| `G+0xbcbc4` | Dialogue choices list |
| `G+0xbd7bc` | Event scene |
| `G+0xf1eb4` | Talk window, vt 0x1a2e688 |
| `G+0xf243c` | Mealtime message window, vt 0x1a2edb0 |

### Prompt bar codes (`G+0xf0`)

| Code | Description |
|---|---|
| `0x2000` | A |
| `0x4000` | B |
| `0x1000` | X |
| `0x8000` | Y |
| `0x100` | ZL |
| `0x200` | ZR |
| `0x800` | R |
| `0x8` | + |

## Common list widget fields

The same layout is used by every common list class.

| Location | Description |
|---|---|
| `+0x0` | vtable (identifies the list class) |
| `+0x8` | Focus bits. `0x3c` = has the focus, `0x30` = was just left |
| `+0x21` | Visible (u8) |
| `+0x4c` | Selected index (s32) |
| `+0x50` | Selected column (grids) |
| `+0x54` | Row count |
| `+0x64` (u16) | Row count (copy) |
| `+0x66` (u16) | Rows shown at once |
| `+0x6a` (u16) | First visible row (scroll) |

## Screens (`G` offsets)

| Location | Description |
|---|---|
| `G+0xd238` | Convoy: unit item list, vt 0x1a4a218. Item slots 0-5, then Magic rows |
| `G+0xdf80` | Convoy: Store / Trade submenu, vt 0x1a42478 |
| `G+0xe598` | Convoy: Take / Trade submenu, vt 0x1a42808 |
| `G+0xebb0` | Convoy: hand cursor, vt 0x19edb28 |
| `G+0x111d0` | Convoy: row count of the current tab |
| `G+0x111e4` | Convoy: first visible row |
| `G+0x111e8` | Convoy: selected row within the 11 visible ones |
| `G+0x111ec` | Convoy: current tab (0-6) |
| `G+0x111f0` | Convoy has focus (1), unit list has it (0) |
| `G+0x24e88` / `+0x254a8` / `+0x25bc8` | Reclass / Certifications lists. `+0x25bc8` = class list, vt 0x1a1ee00 |
| `G+0x26350` | Certifications panel page: 0 Class Description, 1 Stat Changes |
| `G+0x26368` | Certifications stat grid (5x2), vt 0x1a0db98 |
| `G+0x268f0` | Certifications class abilities list, vt 0x1a0dd50 |
| `G+0x26e70` | Certifications navigation graph, vt 0x19ee750 |
| `G+0x27398` | Support grid |
| `G+0x2c8f0` | Unit picker |
| `G+0x36e8` | Lecture results, vt 0x1a3a600 |
| `G+0x4eda8` | Status page labels {current, next, count} |
| `G+0x4ef80` | Battle: "send to convoy" item list (inventory full) |
| `G+0x568f8` | Status sheet, vt 0x1a458f8 |
| `G+0x5d060` | Status sheet tab (0-4) |
| `G+0x5d068` / `+0x5d568` | Status sheet popups: Items / Trade / Store All; Abilities / Combat Arts / Battalions / Reclass |
| `G+0x5da78` | Status sheet popup column |
| `G+0x5da80` | How the status sheet was opened |
| `G+0x60178` | Status sheet navigation graph |
| `G+0x602c8` | Status sheet current node (0 outside Details) |
| `G+0x606a0` | Fighting tournament round screen, vt 0x19f2ca8 |
| `G+0x73788` | Battalion Guild: hire unit list, vt 0x1a46d08 |
| `G+0x73e48` | Battalion Guild: available / dismiss list, vt 0x1a47098 |
| `G+0x79d8` | Battle map menu: Units list, vt 0x1a436f8 |
| `G+0x7cb70` | Battalion Guild: replenish list, vt 0x1a474e8 |
| `G+0x85418` | Shop list |
| `G+0x8b0f8` | Shop "Sell how many?" dialog |
| `G+0x8d8b0` | Storehouse "Sell how many?": Sell / Cancel buttons (1x2 grid) |
| `G+0x92434` | Storehouse category tab |
| `G+0x9a358` | Quest details, vt 0x1a21ca8 |
| `G+0x9adb0` | Calendar schedule widget |
| `G+0x9b478` | Calendar schedule details: event list, vt 0x1a24bc0 |
| `G+0x9f4c0` | Calendar animation after "Finish Exploring", vt 0x1a3c890 |
| `G+0xa1980` | Title menu → Extras list, vt 0x1a3ed18 (no hand; indicator at `G+0xa21e8`) |
| `G+0xa5e50` | Music Library: a song is playing (≠ 0) |
| `M+0x8fdd0` | Monastery book reader |
| `M+0x64f8` | Monastery map area selector, vt 0x1a1a688 |

## Battle struct (`battle`)

| Location | Description |
|---|---|
| `+0x4` / `+0x8` | Map cursor x, y |
| `+0x630` / `+0x634` / `+0x638` | Battle camera mode |
| `+0x3b18`..`+0x3b24` | Map bounds (min x, min y, max x, max y) |
| `+0x3b30`..`+0x3b58` | Cursor glide target / drawn cursor (cell refs) |
| `+0x3b60` | Acting / selected unit |
| `+0x3b68` | Selected weapon / item |
| `+0x3b80`..`+0x3b90` | Attack area values, computed from acting unit + target |
| `+0x3bb8` | Map controller, vt 0x19f5de0. Its `+0x8` = sub-state: 5 heal / gambit, 6 Warp destination, 11 attack, 14 trade; `+0x1430` = 32x32 u16 cell table |
| `+0x3cc0` | Phase: 0 free, 1 unit selected, 2 action / target, 3 map menu, 8 walking, 9-11 combat / events, 14 turn change, 15 View Unit, 16 unit details, 18 overview map, 22 conditions intro, 28 prep wheel, 29 prep Units, 30 prep Map, 31 prep Inventory |
| `+0x3cdc` | Action kind (e.g. 2 attack list, 3 Warp, 0xf heal, 0x13 gambit) |
| `+0x3ce0` | Target unit |
| `+0x3ce8` / `+0x3cec` | Target tile x, y |
| `+0x3d52` | Danger Area display on |

## Map cells (`*(*(main+0x1aa7cc8))`)

| Location | Description |
|---|---|
| `+ y*0x600 + x*0x30` | Cell (x, y) |
| cell `+0x0` | Unit on the cell |
| cell `+0x26` | Flags: `0x1`/`0x2` move range, `0x10` gambit / Warp area, `0x20` attack target |
| cell `+0x27` bit 0 | In an enemy's reach (Danger Area) |

## Units

| Location | Description |
|---|---|
| `+0xd6` bit 2 | Has acted this turn |
| `+0xf1` | Army: 0 player, 1 enemy, 2 ally, 4 obstacle |
| `+0xf2` / `+0xf3` | Tile x, y |
| `+0x140` | Equipped item |
| `+0x2f3` | Inside the gambit area (u8) |

## Battle HUD (`HUD`)

| Location | Description |
|---|---|
| `HUD+0xab8` | Map menu |
| `HUD+0x1068` | Command menu (Attack, Gambit, Items ...) |
| `HUD+0x1610` | Weapon / combat art picker, vt 0x19f9678 |
| `HUD+0xee20` | Items list |
| `HUD+0x1c630` | Submenu |
| `HUD+0x1cc48` | Trade panel |
| `HUD+0x7c7c8` | Unit details: top-left list (status, HP row, class row) |
| `HUD+0x830b8` | Unit details: right-hand list, vt 0x19f9678. Page at `+0x6a4c`, unit at `+0x6ac8` |
| `HUD+0x9ab60` | Unit details navigation graph (`BD_GRAPH`) |
| `HUD+0x9c720` | Battle prep wheel, vt 0x19f4490 |

## Game functions (`main+`)

| Location | Description |
|---|---|
| `0x9e800` | SetCursor(battle, x, y, snap) |
| `0xa2810` | ShowCursor |
| `0x5de5b0` | List SetIndex (stores `+0x4c` only, no redraw) |
| `0x5de6f0` | List SetColumn |
| `0x5dd520` / `0x5dd530` | List focus on / off |
| `0x570100` | Cursor / list sound (SE 0x1b2) |
| `0x181c90` | Set / check a target (ctl, unit, enemy, -, set, x, y) |
| `0x180290` | D-pad target step |
| `0x174ba0` | Gambit area unit marks (1 draw, 0 clear) |
| `0x174e60` | Target HUD refresh (ctl, changed) |
| `0x1435c0` | Battle panel refresh |
| `0x1277a0` | Navigation graph move (graph, dir) |
| `0x40e060` | Unit data lookup (barrier ability bytes `+0x44` / `+0x45`) |
| `0xf6a60` | Terrain height |
| `0x2b2fa0` | Monastery map area select |
| `0x153290` / `0x153450` / `0x153a70` | Prep wheel state machine / stick input / direction |

## List classes (vtables)

| vt | Description |
|---|---|
| `0x19ed970` | Battle map menu → Auto-Battle orders |
| `0x19ee290` | Marketplace menu |
| `0x19f0e48` | Prep wheel: System → Options / Guide |
| `0x1a07bd8` | Calendar menu |
| `0x1a083f0` | Gardening menu; also one-row world prompts |
| `0x1a0dd50` | Certifications class abilities |
| `0x1a0db98` | Certifications stat grid |
| `0x1a0eaf0` | Lecture: Group Tasks |
| `0x1a1a4d0` | Monastery map → Locate list |
| `0x1a1a820` | Lost Items; tea party Gifts (hand hidden there) |
| `0x1a1ab88` | Tea party: tea leaves |
| `0x1a1b0b0` | Dining hall: Today's Special unit list |
| `0x1a1b850` / `0x1a1ba18` | Cooking Together: unit list / recipes |
| `0x1a1c878` | Tournament organizer: unit list |
| `0x1a1cd88` / `0x1a1d1b8` | Gardening: seeds / cultivation methods |
| `0x1a1d528` / `0x1a1d6f8` | Tea party: topics / final responses (1x3 buttons) |
| `0x1a1db48` | Lecture: answers to a student's question |
| `0x1a1ddc8` / `0x1a1e048` | Instruct: topics / unit list |
| `0x1a1e2d8` | Lecture: "Update goals?" Yes / No |
| `0x1a1e648` | Seminar lecturer list |
| `0x1a1ea90` / `0x1a1ec48` / `0x1a1ee00` | Reclass: unit list / class grid / class list |
| `0x1a1f240` / `0x1a1f3f8` | Saint Statue: statue list / rewards |
| `0x1a0ee60` | Professor Level: renown list |
| `0x1a213f0` / `0x1a215b0` | Skill Levels: skill list / renown list |
| `0x1a21930` / `0x1a21770` | Class Abilities: tabs / class list (tabs - 0x620) |
| `0x1a1f768` | Group Tasks: unit list |
| `0x1a20ce8` / `0x1a20eb0` / `0x1a21070` / `0x1a21230` | Unit Appearance: units / options / attire lists / variants |
| `0x1a221c0` | Quest Accept / Decline |
| `0x1a22378` | Quest list |
| `0x1a249f8` / `0x1a24bc0` | Calendar days / schedule events |
| `0x1a24d78` / `0x1a24f40` / `0x1a250f8` | Goals: units / focus / custom skills |
| `0x1a25620` | DLC Supplies |
| `0x1a25c88` | Shop Sell / Cancel popup |
| `0x1a42478` / `0x1a42808` | Unequip / Trade and Equip / Trade popups (also convoy Store / Take) |
| `0x1a436f8` | Battle map menu → Units |
| `0x1a43a68` | Support levels |
| `0x1a44138` | Support unit grid |
| `0x1a44878` | Inventory unit picker |
| `0x1a44a48` | Calendar → Battle mission list |
| `0x1a45588` | Battle: full-inventory item list |
| `0x1a462e8` / `0x1a46658` | Blacksmith: Forge / Repair lists / Forge Selection |
| `0x1a46d08` / `0x1a47098` / `0x1a474e8` | Battalion Guild: hire units / available / replenish |
| `0x1a47be8` / `0x1a47f78` | Shops: unit list / Buy-Sell item list |
| `0x1a483c8` / `0x1a48aa8` | Storehouse: Sell popup / item list |
| `0x1a48738` | Pause menu |
| `0x1a4a218` | Convoy unit item list |
| `0x1a4ddf0` / `0x1a4e660` | Extras: Music Library / Movie Gallery |
| `0x19ffdd8` | Tea party Extra Time menu (Observe / Gifts / End) |
| `0x1a00298` / `0x19fff68` | Tea party: "Offer some tea" / "A Talk!" buttons |
