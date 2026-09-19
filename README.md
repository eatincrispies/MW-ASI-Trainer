# MW: Pursuit Trainer

A pursuit ASI trainer for Need for Speed: Most Wanted (2005, PC). Switch on what
you want in one INI — never get busted, shrug off spike strips, run forever on
nitro and Speedbreaker, ghost through cops, or crank up your bounty.

Everything is off unless you turn it on. Apart from the helicopter switch, every
cheat only affects you — the AI racers you share a Blacklist race with still play
by the normal rules.

## What you need

- Most Wanted (2005) on PC, **version 1.3**
- An ASI loader, usually `dinput8.dll` from [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader)

A patched executable is fine — large address aware, widescreen and no-CD builds
all work.

## Installing

Drop `PursuitCheats.asi` and `PursuitCheats.ini` into your game's `scripts`
folder and launch. To uninstall, delete the two files.

## The cheats

Open `PursuitCheats.ini` in Notepad and set any of these to `true`. Changes
apply next launch.

| Cheat | What it does |
| --- | --- |
| `BustProof` | The busted meter never fills. You cannot be busted. |
| `SpikeProof` | Spike strips can't pop your tyres. |
| `InfiniteNitro` | Your nitro never runs out once you have some in the bottle. |
| `InfiniteSpeedbreaker` | Speedbreaker lasts as long as you hold it. |
| `TankMode` | During a pursuit you hit like a truck — cops and Rhinos get shoved out of your way. |
| `GhostCops` | You drive straight through cop cars. Traffic and walls are still solid. |
| `DisableHelicopter` | The police helicopter never shows up. |
| `CopsIgnorePlayer` | Cops won't start a pursuit on you, even if you speed, crash or ram them. Scripted pursuits in events still happen. |
| `InstantCooldown` | The moment you reach Cooldown, you've escaped. |
| `BountyMultiplier` | Multiplies the bounty you earn in pursuits. `1.0` is normal. |

A couple of combinations worth knowing:

- `GhostCops` on its own doesn't stop busts — the busted meter works on distance,
  so pair it with `BustProof` if that's what you're after.
- `BountyMultiplier` scales pursuit bounty only. Milestone and event rewards
  pay out as normal.


## Building

Only if you want to compile it yourself. Open `PursuitCheats.sln` and build
Release|Win32, or run `build.bat` with a 32-bit MinGW-w64 g++ that supports
C++20. The runtime is linked statically, so there's nothing extra to install.

## License

MIT — see [LICENSE](LICENSE).
