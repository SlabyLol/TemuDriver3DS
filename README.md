# TemuDriver3DS

**Be a Temu Driver!** Deliver packages, upgrade your car, drift through traffic and earn money on your Nintendo 3DS.

![Logo](logo.png)

## Features
- **Drive** as a Temu delivery driver
- **Circle Pad** to steer left / right
- **Drift** by holding **Y** or **B** while turning
- **Upgrades**: Speed, Handling, Reward multiplier
- **Customize** your car color
- **Levels** with increasing difficulty
- More money for **faster deliveries** and **fewer crashes**
- Automatic **update check** on startup
- Fully in **English**

## Controls
| Button       | Action                  |
|--------------|-------------------------|
| Circle Pad   | Steer left / right      |
| Y or B       | Drift                   |
| A            | Confirm / Start         |
| X            | Open Garage             |
| START        | Back / Exit / Abort     |

## How to play
1. Put `TemuDriver3DS.3dsx` (and optional `.smdh`) into the `/3ds/` folder on your SD card.
2. Launch it from the Homebrew Launcher.
3. Deliver packages as fast as possible while avoiding other cars.
4. Spend money in the Garage to upgrade and paint your car.

## Building
This project is built automatically with **GitHub Actions** using the official `devkitpro/devkitarm` Docker image.

To build locally you need [devkitPro](https://devkitpro.org/) with the 3DS packages installed:

```bash
export DEVKITPRO=/opt/devkitpro
export DEVKITARM=$DEVKITPRO/devkitARM
make
```

## Credits
- Made for the Nintendo 3DS homebrew community
- Uses libctru + citro2d

Repo: https://github.com/SlabyLol/TemuDriver3DS
