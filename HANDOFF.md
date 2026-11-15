# Carry It All (mod ID `CarryItAll`) — handoff

Notes for picking this project up on a new PC.

## Fast path: from the external SSD (`Satisfactory-Modding` folder)
| Folder | Contents | Use |
|---|---|---|
| `1-Installers\UnrealEngine-5.6.1-CSS` | Engine installer: the .exe plus 3 .bin files | Run the .exe. It installs to `C:\Program Files\Unreal Engine - CSS` |
| `1-Installers\Wwise_2025.1.6.9117` | Copy of the installed Wwise | Copy to `C:\Audiokinetic\Wwise_2025.1.6.9117`. If the Audiokinetic Launcher doesn't pick it up, install 2025.1.6.9117 through the launcher instead |
| `2-Project\SatisfactoryModLoader` | Starter project with Wwise already integrated and compiled binaries | Copy it to a short path (see "Path length" below). It contains `Mods\InventoryBonus` from before the rename: delete it and put the repo at `Mods\CarryItAll` |
| `3-Builds\` | Packaged builds | Unzip into `<Satisfactory>\FactoryGame\Mods\CarryItAll\` (the 1.0.0 zip there is the old ID) |
| `4-Memory` | Notes from the previous AI session | Give them to the next session as context |

You still have to install on the new PC:
1. Visual Studio 2022 or 2026 with the workloads and MSVC 14.38 listed below.
2. Git.
3. The Audiokinetic Launcher. You need to sign in to it.
4. SML through Satisfactory Mod Manager.
5. The game.

Order: Visual Studio → engine → Wwise → copy the project to a short path → generate project files → build.

If the copied binaries won't load, delete `Binaries` and `Intermediate` in the project and the mod folders, then rebuild.

The source of truth for the mod is the GitHub repo `Aetherosz/Carry-It-All` (formerly Inventory-Bonus). Clone it to `<project>\Mods\CarryItAll`, or replace that folder with the clone. The folder must be named `CarryItAll` to match the plugin name.

## Status (2026-11-15)
- **1.0.0 is the first public release** on ficsit.app: https://ficsit.app/mod/CMqTrzFafewRRg (mod reference `CarryItAll`, owner Aetherosz). Public since 2026-11-15 (tested via an SMM install first).
  - Windows client only. No WindowsServer/LinuxServer builds yet, so dedicated servers are not supported (the description says so).
  - AI disclosure on ficsit.app: "Generative AI was used", described as "Coded with AI assistance. The design, decisions and in-game testing are mine. No AI-generated art, models or audio."
- Private history before the public release: builds 1.0.0-1.3.1 under the old ID `InventoryBonus`, then 2.0.0 (rename to `CarryItAll`), renumbered to public 1.0.0. Tested in-game along the way: slot math, per-save bonus, menu layout and drag debounce (one crate per change), exit without crash. The 1.0.0 build itself (new ID) still needs one in-game check.
- The author is set to `Aetherosz`.
- The icon is done: `Resources/Icon128.png`.

## What the mod does
total slots = default + vanilla schematic slot unlocks + other mods' schematic slot unlocks + bonus

- There are no milestones; the bonus applies by default. It is adjustable from 0 to 1000.
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
| `CarryItAll.uplugin` | Plugin descriptor, v1.0.0 (integer `Version` must equal the SemVer major, or ficsit.app rejects the upload). Display name "Carry It All"; mod ID `CarryItAll` (was `InventoryBonus`; C++ class and file names kept the old name). Depends on SML `^3.12.0`, GameVersion `>=491125` |
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
9. **Put this repo at** `<project>\Mods\CarryItAll`. Either clone it there, or clone it elsewhere and create a junction into `Mods`.

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
& "C:\Program Files\Unreal Engine - CSS\Engine\Build\BatchFiles\RunUAT.bat" -ScriptsForProject="<project>\FactoryGame.uproject" PackagePlugin -project="<project>\FactoryGame.uproject" -clientconfig=Shipping -serverconfig=Shipping -utf8output -DLCName=CarryItAll -build -platform=Win64 -nocompileeditor -installed
```
The output is `<project>\Saved\ArchivedPlugins\CarryItAll\CarryItAll-Windows.zip`. It contains DLLs for both Steam and Epic.

**Don't** run `Build.bat FactoryGame Win64 Shipping` directly. It tries to compile engine modules and fails with `CoreNetTypes.generated.h` not found. Use the RunUAT command above.

## Installing and testing
1. Close Satisfactory Mod Manager completely, including the tray icon. It can remove mods it doesn't manage.
2. Unzip the package into `<Satisfactory>\FactoryGame\Mods\CarryItAll\`.
3. Launch the game from Steam.
4. In a save, press Enter and type `/invbonus`.
5. Log: `%LOCALAPPDATA%\FactoryGame\Saved\Logs\FactoryGame.log`. Search it for `CarryItAll` and `InventoryBonus` (the log category is still LogInventoryBonus).

## Possible next steps
- Watch the ficsit.app page for comments and compatibility reports.
- New versions on ficsit.app: bump `SemVersion`/`VersionName` and keep `Version` = major; upload a zip without .pdb files (the browser upload limit is 10 MB) with forward-slash paths.
- Dedicated server support: package WindowsServer and LinuxServer targets (LinuxServer needs Epic's Linux cross-compile toolchain) and test on a real server.
- **Next mod idea (Aetherosz's):** auto-supports.
  - Any buildable automatically extends supports down to the terrain, including through water.
  - Also buildable water blocks.
  - Neither exists on ficsit.app as of 2026-11. The closest is Structural Solutions.
