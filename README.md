<!--
<p>
<img src="assets/playgb-logo-2x.png?raw=true" width="200">
</p>
-->

## CrankBoy

[![Nightly Build](https://github.com/CrankBoyHQ/crankboy-app/actions/workflows/nightly.yml/badge.svg)](https://github.com/CrankBoyHQ/crankboy-app/actions/workflows/nightly.yml)
[![Discord](https://img.shields.io/discord/675983554655551509?logo=discord&logoColor=white&color=7289DA)](https://discord.com/channels/675983554655551509/1378119815641694278)

A Game Boy emulator for Playdate. CrankBoy is a fork of [PlayGB](https://github.com/risolvipro/PlayGB)
and based on [Peanut-GB](https://github.com/deltabeard/Peanut-GB), a header-only C Gameboy emulator by
[deltabeard](https://github.com/deltabeard).

## Installing

<a href="https://github.com/CrankBoyHQ/crankboy-app/releases/latest"><img src="assets/playdate-badge-download.png?raw=true" width="200"></a>

- Download the zip for the [latest release](https://github.com/CrankBoyHQ/crankboy-app/releases/latest),
  or the [latest unstable nightly build](https://github.com/CrankBoyHQ/crankboy-app/actions/workflows/nightly.yml).
- Copy the pdx through the [Web sideload](https://play.date/account/sideload/) or USB.
- Launch the app.
- Connect Playdate to a computer, press and hold `LEFT` + `MENU` + `LOCK` at the same time in the home
  screen. Or go to Settings > System > Reboot to Data Disk.
- Place the ROMs in the app data folder, the folder name depends on the sideload method.
  - For Web sideload: `/Data/user.*.app.crankboy/games/`
  - For USB: `/Data/app.crankboy/games/`
- Filenames must end with `.gb` or `.gbc`

## Notes

- Use the crank to press Start or Select.
- To save a game you have to use the save option inside that game. A save file is automatically
  created when changing ROMs or quitting the app. After a crash, a new `(recovery).sav` file is
  created. Save files are stored in `/Data/*.crankboy/saves/`
- Audio is disabled by default. You can optionally enable it from the library screen

## Implementation

CrankBoy uses a modified version of Peanut-GB.
