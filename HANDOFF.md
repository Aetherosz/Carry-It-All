# InventoryBonus — handoff

Notes for picking this project up on a new PC.

## Fast path: from the external SSD (`Satisfactory-Modding` folder)
| Folder | Contents | Use |
|---|---|---|
| `1-Installers\UnrealEngine-5.6.1-CSS` | Engine installer: the .exe plus 3 .bin files | Run the .exe. It installs to `C:\Program Files\Unreal Engine - CSS` |
| `1-Installers\Wwise_2025.1.6.9117` | Copy of the installed Wwise | Copy to `C:\Audiokinetic\Wwise_2025.1.6.9117`. If the Audiokinetic Launcher doesn't pick it up, install 2025.1.6.9117 through the launcher instead |
| `2-Project\SatisfactoryModLoader` | Starter project with Wwise already integrated and compiled binaries | Copy it to a short path (see "Path length" below). It already contains `Mods\InventoryBonus` |
| `3-Builds\` | Packaged builds | Unzip into `<Satisfactory>\FactoryGame\Mods\InventoryBonus\` |
| `4-Memory` | Notes from the previous AI session | Give them to the next session as context |

You still have to install on the new PC:
1. Visual Studio 2022 or 2026 with the workloads and MSVC 14.38 listed below.
2. Git.
3. The Audiokinetic Launcher. You need to sign in to it.
4. SML through Satisfactory Mod Manager.
5. The game.

Order: Visual Studio → engine → Wwise → copy the project to a short path → generate project files → build.

If the copied binaries won't load, delete `Binaries` and `Intermediate` in the project and the mod folders, then rebuild.

The source of truth for the mod is the GitHub repo `Aetherosz/Inventory-Bonus`. Clone it over `<project>\Mods\InventoryBonus`, or replace that folder with the clone. The folder must be named `InventoryBonus` to match the plugin name.

## Status (2026-11-15)
- **v1.0.0 works in-game.** In a real save, `/invbonus` printed `18 default + 12 vanilla upgrades + 200 mod upgrades + 1000 bonus = 1230 slots`.
- **v1.1.1** (2026-11-15): fixes the menu drag bug (dragging the value down used to shrink and crate at every step) and adds the items-carried line and the low-bonus warning in the menu.
- **v1.1.0 builds and packages cleanly** (editor build and Shipping package, Steam and Epic DLLs). It has **not been tested in-game yet.** To verify:
  - A v1.0 save keeps its bonus after updating (migration from the old global value).
  - Two saves keep different bonuses.
  - The breakdown line at the top of the settings menu renders. It's an empty `UConfigPropertySection` whose title is rewritten at runtime. If SML doesn't draw it as a label, fall back to putting the breakdown in the Bonus tooltip.
  - Lowering the bonus to 0 with full trailing slots spawns a crate with the leftovers, and chat reports the count.
  - Changing the value in the pause-menu settings: nothing happens while dragging; it applies once, 2 s after the value settles (single player: after closing the menu), with one crate at most.
  - **v1.1.1:** the breakdown line shows items carried and the lowest safe bonus, and turns into a "!!! WARNING !!!" with the stack count when the pending value is too low. Check that SML redraws the label while the menu is open. If it only updates on reopening, the warning still shows on reopen, before the value is applied.
  - The main menu shows and edits the "(new saves)" value.
  - Multiplayer: a client can't change the bonus; on a dedicated server only the console can, unless `AnyPlayerCanChange` is on.
- The author is set to `Aetherosz`.
- The icon is done: `Resources/Icon128.png`.

## What the mod does
total slots = default + vanilla schematic slot unlocks + other mods' schematic slot unlocks + bonus

- There are no milestones; the bonus applies by default. It is adjustable from 0 to 20000.
- **Per save:** the bonus is stored in the save by `AInventoryBonusSubsystem` (`UPROPERTY(SaveGame)`). A save without a stored bonus takes the "new saves" value (default 1000). v1.0 used one global `Bonus`; on first start of v1.1 that value is copied into the hidden `NewSaveBonus` key, so old saves keep the bonus they had.
- **Settings menu:** in a save, the `Bonus` field edits that save's bonus and the top line shows the breakdown. In the main menu, the same field edits the "new saves" value. On a client it shows that the host's value is used.
- **Permissions:** single player and the host can always change it. On a dedicated server only the server console can, unless the server's own `AnyPlayerCanChange` is true. Clients in a hosted game can't.
- **Shrinking:**
  - Automatic resizes (load, unlocks) never drop items: occupied trailing slots are kept, and `/invbonus` warns about them.
  - A shrink the user starts (command or menu) uses a crate. Items in the removed slots are copied into a crate at the player's feet first. If the crate fails to spawn, nothing shrinks. Then the inventory shrinks and what fits is moved back from the crate. An empty crate is destroyed.
- Everything except the bonus is recomputed from purchased schematics on every resize. Schematics under `/Game/` count as vanilla; anything else counts as a mod upgrade.
- See `README.md` for the user-facing docs.

## Code map
| File | Role |
|---|---|
| `InventoryBonus.uplugin` | Plugin descriptor, v1.1.1. Depends on SML `^3.12.0`, GameVersion `>=491125` |
| `Resources/Icon128.png` | Mod icon |
| `Config/AccessTransformers.ini` | `Friend` access so the mod can read the private `AFGCharacterPlayer::mDefaultPlayerInventorySlots` |
| `Private/InventoryBonus.cpp` | All hooks, the slot math, the crate shrink, and the config sync |
| `*Subsystem.*` | `AInventoryBonusSubsystem`: per-save bonus storage (server only) |
| `*Config.*` | `UInventoryBonusConfig`: `Breakdown` (label), `Bonus`, `AnyPlayerCanChange`, `NewSaveBonus` (hidden) |
| `*Command.*` | `/invbonus` (alias `/inventorybonus`). With no args it shows the breakdown; setting a value goes through `FInventoryBonus::CanChangeBonus` |
| `*Module.*` | Module startup calls `FInventoryBonus::RegisterHooks()` |

Hooks in `InventoryBonus.cpp`. All of them except the config registration are inside `#if !WITH_EDITOR`, so **the editor build never compiles them**. Only the Shipping/Alpakit build checks them.
- `UFGInventoryComponent::Resize` (before): for a player's main inventory, replaces NewSize with the computed total.
- `AFGCharacterPlayer::BeginPlay` (virtual, after):
  - Once per world on the server: registers the subsystem, shows the save's bonus in the config, registers the chat command, and starts a 1 s poll (PollMenu). Menu edits in a save only become pending (OnConfigChanged); PollMenu applies them once unchanged for 2 s (MenuApplyDelay), because SML's number box reports every value while dragging. World timers pause with single player, so there it applies after the menu closes.
  - Applies the size now, and again at 2 s and 10 s.
- `AFGSchematicManager::GiveAccessToSchematic(s)` (after): re-applies the size to all players.
- `UConfigManager::MarkConfigurationDirty` (after): routes the menu value to the save, or to "new saves" in the main menu.
- `FWorldDelegates::OnStartGameInstance`: registers the config, so it shows in the main menu.
- `FWorldDelegates::OnWorldCleanup`: switches the menu back to the "new saves" value.

## Rebuilding the toolchain on a new PC
Follow https://docs.ficsit.app/satisfactory-modding/latest/Development/BeginnersGuide/dependencies.html. Exact versions used:

1. **Visual Studio 2022 or 2026** with these workloads:
   - Game dev with C++
   - Desktop dev with C++
   - .NET desktop

   Also add the individual component **MSVC v143 14.38 (x64/x86)**. Not the ARM64 one. Visual Studio 2026 works: UBT picks up MSVC 14.38 and reports it as the VS2022 toolchain.
2. **GitHub ↔ Epic link** is already done for the account `Aetherosz`, and it is per account. On the new PC you only need to sign in to GitHub. **Unreal Linker** (https://linker.ficsit.app/link) is already done too.
3. **Unreal Engine 5.6.1-CSS:**
   - Download the 4 files from https://github.com/satisfactorymodding/UnrealEngine/releases/tag/5.6.1-css-83: `UnrealEngine-CSS-Editor-Win64.exe` and `-1.bin`, `-2.bin`, `-3.bin`.
   - Run the .exe. It installs to `C:\Program Files\Unreal Engine - CSS`.
4. **Wwise 2025.1.6.9117**, exactly this version and not 2026.x. Install it through the Audiokinetic Launcher:
   - Components: Authoring, SDK (C++).
   - Platforms: Windows (VS2022) and Linux.
   - **No plugins.** Untick even the two the launcher selects by default.
5. **Starter project:**
   ```
   git clone https://github.com/satisfactorymodding/SatisfactoryModLoader.git
   ```
   Use the `master` branch, which is SML 3.12.0 with EngineAssociation `5.6.1-CSS`.
6. **Path length (required).** UBT fails with "action paths are longer than 260 characters" if the project path is long. The longest relative build path is about 196 characters, so keep the folder containing `FactoryGame.uproject` at **45 characters or less**. Either clone to a short folder (e.g. `C:\SML`), or create a junction and always build through it:
   ```powershell
   New-Item -ItemType Junction -Path C:\SML -Target "<where you cloned SatisfactoryModLoader>"
   ```
7. **Integrate Wwise** in the Audiokinetic Launcher (you must be signed in):
   - Open the Unreal Engine tab and choose Open other → `<project>\FactoryGame.uproject`.
   - Choose Integrate Wwise in Project.
   - Set Integration Version to All → `2025.1.6.9117`.
   - Under Wwise Project, choose ▸ New project.
   - Click Integrate.
8. **Generate soundbanks:**
   - Open `SatisfactoryModLoader_WwiseProject.wproj` in Wwise.
   - In SoundBanks, right-click the top folder and choose Generate for all platforms.
9. **Put this repo at** `<project>\Mods\InventoryBonus`. Either clone it there, or clone it elsewhere and create a junction into `Mods`.

## Build commands (PowerShell)
`<project>` is the short folder containing `FactoryGame.uproject`, e.g. `C:\SML`. Close the Unreal Editor first: Live Coding blocks UBT.

Generate the project files:
```powershell
& "C:\Program Files\Unreal Engine - CSS\Engine\Build\BatchFiles\Build.bat" -projectfiles -project="<project>\FactoryGame.uproject" -game -rocket -progress
```
Build the editor. The first build takes about 10 minutes. It is needed before you open the editor or use Alpakit:
```powershell
& "C:\Program Files\Unreal Engine - CSS\Engine\Build\BatchFiles\Build.bat" FactoryEditor Win64 Development -project="<project>\FactoryGame.uproject" -WaitMutex -FromMsBuild
```
Package the mod. This is the same UAT call Alpakit Dev makes. It compiles the Shipping DLLs, including the hooks. The first run takes about 25 minutes:
```powershell
& "C:\Program Files\Unreal Engine - CSS\Engine\Build\BatchFiles\RunUAT.bat" -ScriptsForProject="<project>\FactoryGame.uproject" PackagePlugin -project="<project>\FactoryGame.uproject" -clientconfig=Shipping -serverconfig=Shipping -utf8output -DLCName=InventoryBonus -build -platform=Win64 -nocompileeditor -installed
```
The output is `<project>\Saved\ArchivedPlugins\InventoryBonus\InventoryBonus-Windows.zip`. It contains DLLs for both Steam and Epic.

**Don't** run `Build.bat FactoryGame Win64 Shipping` directly. It tries to compile engine modules and fails with `CoreNetTypes.generated.h` not found. Use the RunUAT command above.

## Installing and testing
1. Close Satisfactory Mod Manager completely, including the tray icon. It can remove mods it doesn't manage.
2. Unzip the package into `<Satisfactory>\FactoryGame\Mods\InventoryBonus\`.
3. Launch the game from Steam.
4. In a save, press Enter and type `/invbonus`.
5. Log: `%LOCALAPPDATA%\FactoryGame\Saved\Logs\FactoryGame.log`. Search it for `InventoryBonus`.

## Possible next steps
- Test v1.1.0 and v1.1.1 in-game (see Status).
- Publish to ficsit.app:
  1. Run Alpakit Release, which builds the multi-target zip including the Linux dedicated server.
  2. Upload the zip.
  3. Clients and server both need the mod.
- **Next mod idea (Aetherosz's):** auto-supports.
  - Any buildable automatically extends supports down to the terrain, including through water.
  - Also buildable water blocks.
  - Neither exists on ficsit.app as of 2026-11. The closest is Structural Solutions.
