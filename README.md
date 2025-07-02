## CrankBoy

[![Nightly Build](https://github.com/CrankBoyHQ/crankboy-app/actions/workflows/nightly.yml/badge.svg)](https://github.com/CrankBoyHQ/crankboy-app/actions/workflows/nightly.yml)
[![Forum Thread](https://img.shields.io/badge/Forum_Thread-yellow?logo=discourse&label=PlayDate)](https://devforum.play.date/t/60-fps-gameboy-emulation/22865)
[![Discord](https://img.shields.io/discord/675983554655551509?logo=discord&logoColor=white&color=7289DA)](https://discord.com/channels/675983554655551509/1378119815641694278)


A full-speed Game Boy emulator for Playdate. CrankBoy is a fork of [PlayGB](https://github.com/risolvipro/PlayGB)
and based on [Peanut-GB](https://github.com/deltabeard/Peanut-GB), a header-only C Gameboy emulator by
[deltabeard](https://github.com/deltabeard).

<p align="center">
<img src="Source/launcher/card.png?raw=true">
</p>

## Features
- Stable, full-speed Game Boy emulation (on both Rev A and Rev B devices)
- Cartridge data saves automatically
- 44.1 kHz audio; not perfectly accurate but this shouldn't matter for most games
- Settings to fine-tune performance, visual appearance, and crank controls
- Multiple Save State slots per game (note: not currently available if game has its own cartridge data)
- You can download cover art for your library.
- Checks for an update once a day (you can disable this by revoking the networking permission for CrankBoy in your Playdate's settings)
- ROMs can access Playdate features [via IO registers](gb-extensions) and are also [scriptable with Lua](./lua-docs.md) -- you can add native crank controls to a game if you have the technical know-how.

## Installing

<a href="https://github.com/CrankBoyHQ/crankboy-app/releases/latest"><img src="assets/playdate-badge-download.png?raw=true" width="200"></a>

**Currently, we do not provide stable releases, since CrankBoy is still under heavy development.**

- Download the zip for the [latest release](https://github.com/CrankBoyHQ/crankboy-app/releases/latest),
  or the [latest unstable nightly build](https://github.com/CrankBoyHQ/crankboy-app/actions/workflows/nightly.yml).
- Copy the pdx through the [Web sideload](https://play.date/account/sideload/) or USB.
- Launch the app.
- Connect Playdate to a computer, press and hold `LEFT` + `MENU` + `LOCK` at the same time in the home screen. Or go to `Settings > System > Reboot to Data Disk`.
- Place the ROMs in the app data folder; the folder name depends on the sideload method.
    - For Web sideload: `/Data/user.*.app.crankboyhq.crankboy/games/`
    - For USB: `/Data/app.crankboyhq.crankboy/games/`
- ROM filenames must end with `.gb` or `.gbc`
- Cover art can be placed manually in the `covers/` directory. The file name should match that of the corresponding ROM except for the file extension, which should be one of `.png`, `.jpg`, or `.bmp`. The resolution should be 240x240 pixels. CrankBoy will automatically convert the image to a Playdate-format `.pdi` image the next time it is launched.

## Contributions

Come chat with us on the [Playdate Developer Forum](https://devforum.play.date/t/60-fps-gameboy-emulation/22865) or on [Discord](https://discord.com/channels/675983554655551509/1378119815641694278). Even if you're not an expert at emulation coding, we could still use some visual assets, UI, UX, and so on to make the app feel more cute and at-home on a cozy device like Playdate.

CrankBoy uses a heavily modified version of Peanut-GB. Various [advanced optimization techniques](https://devforum.play.date/t/dirty-optimization-secrets-c-for-playdate/23011) were used to tailor the performance to the Playdate. If you wish to work on adding features to the emulator core itself, you may want to glance at those optimization techniques since it explains some of the unusual design choices made.

### Project Setup

After cloning the repository, please enable the clang-format git hook by running this command from the project root:

```bash
git config core.hooksPath githooks
```
