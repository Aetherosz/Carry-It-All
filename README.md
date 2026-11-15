# Carry It All

Adds **+X inventory slots** on top of everything else. There are no milestones or unlocks. Each save has its own bonus, and you can change it in-game whenever you like.

```
total slots = default + vanilla upgrades + other mods' upgrades + bonus
```

| Part | Where it comes from |
|---|---|
| Default | The game's starting slot count (`mDefaultPlayerInventorySlots`) |
| Vanilla upgrades | Every purchased vanilla schematic that unlocks slots: HUB milestones, MAM research, hard drive alternates |
| Mod upgrades | Every purchased schematic from a mod that unlocks slots, e.g. `InventorySpace2` (+50) or `InventorySpace3` (+150) |
| Bonus | This mod. New saves start at **+1000**. Change it in the mod settings menu or with `/invbonus` |

Every part except the bonus is recalculated from the game's own data on spawn, on load, and whenever any schematic is unlocked. Buying upgrades or adding and removing other inventory mods keeps working.

## Settings menu
Open **Mods → Mod Configuration → Carry It All**.
- **In a save** (pause menu): the first line shows your breakdown, e.g. `Default 18 / Unlocked 12 / Bonus 1000 = 1030 slots` (a `Mods` part appears only if another inventory mod adds slots). **Bonus inventory slots (this save)** changes that save's bonus. The second line shows how many items you carry and the lowest bonus that keeps them all. While you change the value nothing happens yet: it's applied once it has stayed the same for 2 seconds (in single player, when you close the pause menu, because the game is paused). If the new value is too low for what you carry, the second line turns into a loud **!!! WARNING !!!** with roughly how many stacks will go into a crate.
- **In the main menu**: **Bonus inventory slots (new saves)** is the bonus a save starts with the first time it's loaded with this mod.
- Dedicated servers have one more setting, `AnyPlayerCanChange`, which is only in the config file (a server has no menu); see Multiplayer below.
- This uses SML's built-in mod config system, so no other settings mod is required.

## In-game commands (chat)
| Command | Effect |
|---|---|
| `/invbonus` | Show your breakdown |
| `/invbonus 500` | Set this save's bonus to +500 |
| `/invbonus +200` | Add 200 |
| `/invbonus -200` | Remove 200 |
| `/invbonus 0` | No bonus (back to the normal game) |

The range is 0–1000.

## Lowering the bonus never deletes items
When you lower the bonus (with `/invbonus` or in the settings menu), items in the slots being removed are moved into free slots. Whatever doesn't fit goes into a **crate at your feet**, and chat tells you how many items went in. The crate is created before anything is removed; if it can't be created, the inventory isn't shrunk.

Automatic resizes (loading a save, unlocking an upgrade) never move items: slots that still hold items are kept until you lower the bonus yourself.

## Each save has its own bonus
The bonus is stored in the save file.

## Multiplayer
- The bonus applies to every player. Everyone needs the mod installed.
- **Single player / hosting:** only the host can change it.
- **Dedicated server:** by default, only the server console can change the bonus (players can still type `/invbonus` to see their breakdown). To let every player change it, set `"AnyPlayerCanChange": true` in the server's `FactoryGame/Configs/CarryItAll.cfg`. Only the server's setting counts.

## Uninstalling
Before you remove the mod, run `/invbonus 0` in each save you care about. Anything that no longer fits goes into a crate, so no items are left in slots that disappear.

## Files
```
CarryItAll/
  CarryItAll.uplugin
  Resources/Icon128.png                 mod icon
  Config/AccessTransformers.ini         grants read access to the private default-slot field
  Source/CarryItAll/
    CarryItAll.Build.cs                 from the official Alpakit C++ template
    Public/InventoryBonus.h             slot math, crate shrink, config sync
    Private/InventoryBonus.cpp          hooks: Resize, BeginPlay, GiveAccessToSchematic(s), config changes
    Public|Private/InventoryBonusSubsystem.*  stores the bonus in the save file
    Public|Private/InventoryBonusConfig.*    Mods -> Mod Configuration entry (SML config)
    Public|Private/InventoryBonusCommand.*   /invbonus chat command
    Public|Private/InventoryBonusModule.*    module entry point
```

## Building (one-time toolchain setup)
C++ mods must be compiled with Coffee Stain's custom Unreal Engine. Follow the official guide:
https://docs.ficsit.app/satisfactory-modding/latest/Development/BeginnersGuide/dependencies.html

1. **Accounts:** GitHub, an Epic Games account linked to GitHub, and the ficsit.app Unreal Linker for access to the modding engine. You also need an Audiokinetic account for Wwise.
2. **Visual Studio 2022** (17.14) or 2026, with the C++ game dev, C++ desktop, and .NET desktop workloads, plus MSVC v143 14.38.
3. **Unreal Engine 5.6.1-CSS** from the modding engine release.
4. **Wwise 2025.1.6.9117** (Authoring + SDK).
5. **Starter project**:
   ```bash
   git clone https://github.com/satisfactorymodding/SatisfactoryModLoader.git SatisfactoryModLoader
   ```
   Keep it at a short path (e.g. `C:\SML`): the build fails if any path goes over 260 characters.

## Compile and install
1. Copy this `CarryItAll` folder into `SatisfactoryModLoader/Mods/`.
2. Right-click `FactoryGame.uproject`, choose **Generate Visual Studio project files**, then build **Development Editor / Win64**.
3. Open the editor. In **Alpakit**, set the game path to your Satisfactory install folder, tick **CarryItAll**, and click **Alpakit Dev**.
4. Launch the game and check **Mods → Mod Configuration → Carry It All**, or type `/invbonus` in chat.

## Limitations
- Mods that add slots through schematics are counted, which is the normal way and how `InventorySpace2`/`3` work. A mod that resizes the inventory directly in code is overridden by this mod's total. If that happens, raise the bonus to compensate.
- If you uninstall the mod without lowering the bonus first, the inventory may shrink back to the vanilla size, and it isn't yet known what happens to items in the extra slots. Run `/invbonus 0` first (see Uninstalling above), and back up your save.
