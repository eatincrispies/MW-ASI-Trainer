# MWCheats WIP

A cheat trainer for Need for Speed: Most Wanted (2005, PC) that lives in one
INI file. Turn on what you want: endless nitro, a car that never loses grip,
every car and part unlocked, free impound, a trip to the Blacklist without the
grind, or pursuits where the cops can't touch you.

Everything is off unless you turn it on, and most cheats only affect you. The
AI keeps playing by the normal rules unless a cheat says otherwise.

## What you need

- Most Wanted (2005) on PC, **version 1.3**
- An ASI loader, usually `dinput8.dll` from [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader)

A patched executable is fine — large address aware, widescreen and no-CD builds
all work.

## Installing

Drop `MWCheats.asi` and `MWCheats.ini` into your game's `scripts` folder and
launch. To uninstall, delete the two files.

## The cheats

Open `MWCheats.ini` in Notepad and set any of these to `true`. Changes apply
next launch.

### Main

| Cheat | What it does |
| --- | --- |
| `InfiniteNitro` | Your nitro never runs out once you have some in the bottle. |
| `InfiniteSpeedbreaker` | Speedbreaker lasts as long as you hold it. |
| `EnginePowerMultiplier` | More (or less) power from your engine. `1.0` is stock, `2.0` is double. |
| `InfiniteGrip` | Your tyres never break loose, so the car stays planted through every corner. |
| `DisableRubberbanding` | Racers stop getting a speed boost when they fall behind you. |
| `FreezeAI` | Your opponents stay stopped on the grid while you race. |
| `DisableTraffic` | No civilian cars anywhere, in free roam or in races. |
| `HideHUD` | Hides the whole HUD for clean screenshots and videos. |

### Career

| Cheat | What it does |
| --- | --- |
| `InfiniteCash` | Your wallet stays full, and buying things never costs you. |
| `CashMultiplier` | Multiplies the prize money from races. `1.0` is normal. |
| `UnlockAllCars` | Every car in the car lot is for sale. |
| `UnlockAllPerformanceParts` | Every performance upgrade is available in the shop. |
| `UnlockAllVisualParts` | Every body kit, rim, spoiler and paint is available. |
| `InfiniteJunkmanParts` | You always have Junkman performance parts to install. |
| `UnlockAllBlacklist` | Each Blacklist rival can be challenged right away. No races, milestones or bounty needed first. |
| `AlwaysWinPinkSlip` | The first card you pick after beating a Blacklist rival is always their pink slip. |
| `FreeImpoundBail` | Getting your car out of the impound costs nothing. |
| `NeverImpoundCar` | Busts still count as strikes, but your car is never taken. |
| `InfiniteTollboothTime` | The clock stops running in Tollbooth events. |

### Pursuit

| Cheat | What it does |
| --- | --- |
| `BustProof` | The busted meter never fills. You cannot be busted. |
| `SpikeProof` | Spike strips can't pop your tyres. |
| `TankMode` | During a pursuit you hit like a truck — cops and Rhinos get shoved out of your way. |
| `GhostCops` | You drive straight through cop cars. Traffic and walls are still solid. |
| `DisableHelicopter` | The police helicopter never shows up. |
| `MaxHelicopters` | How many police helicopters can chase you at once. `1` is normal. |
| `InfiniteHelicopterFuel` | The helicopter never runs dry and flies off. |
| `DisableRoadblocks` | Cops never set up roadblocks ahead of you. |
| `DisableReinforcements` | No Rhino heavy units, and no Cross showing up to join the chase. |
| `CopsIgnorePlayer` | Cops won't start a pursuit on you. Scripted pursuits in events still happen. |
| `InstantCooldown` | The moment you reach Cooldown, you've escaped. |
| `BountyMultiplier` | Multiplies the bounty you earn in pursuits. `1.0` is normal. |
| `FreezeHeatLevel` | Locks your heat at `SetHeatLevel` so it never rises or drops. |
| `TouchOfDeathCops` | Any cop car you hit is wrecked on the spot. |

A few things worth knowing:

- `InfiniteCash` and `InfiniteJunkmanParts` put real money and parts in your
  career. They stay in your save after you turn the cheats off.
- `UnlockAllBlacklist` doesn't skip the rivals themselves. You still beat them
  one at a time, you just don't have to earn the right first.
- The game was built around a single helicopter, so with `MaxHelicopters` above
  `1` the extra ones may not get their own minimap icon or radio chatter.
  `DisableHelicopter` wins if both are set.
- `InfiniteGrip` takes the slide out of handbrake turns too.
- `GhostCops` on its own doesn't stop busts — the busted meter works on
  distance, so pair it with `BustProof` if that's what you're after.

## Building

Only if you want to compile it yourself. Open `MWCheats.sln` and build
Release|Win32, or run `build.bat` with a 32-bit MinGW-w64 g++ that supports
C++20. The runtime is linked statically, so there's nothing extra to install.

## License

MIT — see [LICENSE](LICENSE).
