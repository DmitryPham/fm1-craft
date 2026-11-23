# FM-1 CRAFT

Firmware for the M-VAVE FM-1 that turns it into a controller for **FigCraft** (and later Figma).
Keys → hotkeys, knobs → relative controllers, buttons → layer navigation and pages, and the screen shows
the selected layer and the four values the knobs edit. The companion script is `fm1_figcraft.py`
(fm1-figcraft): it talks to FigCraft over its control port and to this firmware over USB-MIDI.

Built on [fm1-base](https://github.com/charlesvestal/fm1-base) (Charles Vestal) and
[felucca-bsp](https://github.com/hugelton/felucca-bsp) (Leo Kuroshita, Hügelton Instruments). MPL-2.0.

- The wire: [docs/PROTOCOL.md](docs/PROTOCOL.md)
- The app: `app/app.c`, `app/app.h`, `app/fm1.toml`
- Build on GitHub: Actions → **Build firmware** → Run workflow; the package (`craft.fwsc`) is in the run's
  `packages` artifact
- Build locally (Linux x86-64 or WSL2): JieLi toolchain + AC79 SDK as in fm1-base's README, then `./build.sh`
- Host simulator: `./build.sh --host && app/build/host/craft_host app/tests/basic.txt app/build/host`
- Install: `python3 base/tools/fm1_install.py app/build/craft.fwsc`. M-VAVE's updater or the Felucca/CTL web
  installers put another firmware back.
