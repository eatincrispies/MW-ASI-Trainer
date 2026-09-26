<div align="center">

[![Status](https://img.shields.io/badge/Status-Paused-critical?style=flat-square)]()
[![License](https://img.shields.io/badge/License-MIT-blue?style=flat-square)](LICENSE)

</div>

---

> [!IMPORTANT]
> **Projects Temporarily Paused**
> All updates, bug fixes, and support for this project are temporarily paused until further notice due to family issues and personal matters. 

![Thumbnail](dunce.png)

# MWCheats

> **Work in progress.** This mod is still being built, and it will be
> finished when it is ready. Patience is key.

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
| `UnlockAllCars` | Every car in the car lot is for sale, plus the traffic cars, cop cars, the police helicopter and the AI racers' preset cars. |
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
| `MaxHelicopters` | How many police helicopters can chase you at once. `1` is normal. Once the first one arrives, the rest join a few seconds apart, and each one gets its own minimap icon. |
| `InfiniteHelicopterFuel` | The helicopter never runs dry and flies off. |
| `DisableRoadblocks` | Cops never set up roadblocks ahead of you. |
| `DisableReinforcements` | No Rhino heavy units, and no Cross showing up to join the chase. |
| `CopsIgnorePlayer` | Cops won't start a pursuit on you. Scripted pursuits in events still happen. |
| `InstantCooldown` | The moment you reach Cooldown, you've escaped. |
| `BountyMultiplier` | Multiplies the bounty you earn in pursuits. `1.0` is normal. |
| `FreezeHeatLevel` | Locks your heat at `SetHeatLevel` so it never rises or drops. |
| `TouchOfDeathCops` | Any cop car you hit is wrecked on the spot. |
| `PursuitBreakerNuke` | Knock down one pursuit breaker and every cop car on the map is wrecked, and the helicopters head home. |
| `PaperWeightCops` | Cop cars weigh almost nothing, so the smallest tap sends them flying. |

### Misc

| Setting | What it does |
| --- | --- |
| `LoadedPopup` | The achievement popup when the game starts. On unless you set it to `false`. |
| `FOVSlider` | Widens (or narrows) the view in every car you drive, in degrees. `0` is stock, `20` is noticeably wider. |

A few things worth knowing:

- `InfiniteCash` and `InfiniteJunkmanParts` put real money and parts in your
  career. They stay in your save after you turn the cheats off.
- `UnlockAllCars` adds the extra cars to your profile, so they stay in your
  save too. It leaves room for your own cars, so if your garage is very full
  not every preset will fit. Back up your save before trying it the first time.
- Preset cars you buy in the car lot keep their body kits and paint.
- The helicopter shows up in the car lot too, but it was never built to be
  driven. Racing with it can behave oddly or crash the game.
- `UnlockAllBlacklist` doesn't skip the rivals themselves. You still beat them
  one at a time, you just don't have to earn the right first.
- The game was built around a single helicopter, so with `MaxHelicopters` above
  `1` the extra ones share the radio chatter of the first. They do each get
  their own minimap icon. `DisableHelicopter` wins if both are set.
- `PursuitBreakerNuke` doesn't end the pursuit. With every cop wrecked you'll
  usually slip into cooldown, but new units can still be called in.
- `InfiniteGrip` takes the slide out of handbrake turns too.
- `GhostCops` on its own doesn't stop busts — the busted meter works on
  distance, so pair it with `BustProof` if that's what you're after.

## The popup

When the game starts, an Xbox 360 style "Achievement unlocked" popup slides
out at the top of the screen with the achievement chime, then fades away. It
shows up once all your cheats are applied, so it's a quick way to know the mod
loaded. Set `LoadedPopup = false` under `[Misc]` if you'd rather not see it.

## The log

Every launch writes `MWCheats.log` next to the ASI:

```
Mod injected and applied to v1.3 and C0516B485065FABDD69579816B5DF763
```

That hash is your own `speed.exe`, so a patched exe will show something
different and that's normal. If another mod has already changed the same part
of the game, the cheat that couldn't be applied is listed as skipped instead of
being forced in.

## Building

Only if you want to compile it yourself. Open `MWCheats.sln` and build
Release|Win32, or run `build.bat` with a 32-bit MinGW-w64 g++ that supports
C++20. The runtime is linked statically, so there's nothing extra to install.

## Credits

`PaperWeightCops` was KingVector11's idea. Thanks!

## License

MIT — see [LICENSE](LICENSE).
