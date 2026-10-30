# [MineCoach](https://github.com/megafishq/MineCoach)

**MineCoach** is a Minesweeper trainer. It plays like a competitive client (Arbiter-style controls, instant restarts, no popups), records every game you play, and then shows you exactly where you lost time: cursor jumps that left a provable cell behind, the pattern you missed, and the square you should have chorded on.

MineCoach is a fork of [LibreMines](https://github.com/Bollos00/LibreMines) by Bruno Bollos Correa, a Free/Libre and Open Source Qt Minesweeper game. All of LibreMines' features (themes, custom boards, keyboard play, no-guess mode) are still here.

![](./share/Screenshots/Screenshot4.png)

## Game analysis

Every game is recorded: each reveal, flag and chord with its timestamp, plus the mouse path over the board at ~120 Hz. After a game ends, press **Analyze Game** (Ctrl+A) or use **Trainer > Analyze Last Game**. The analysis window replays the game move by move and lists:

- **Cursor jumps**: you moved 4+ cells away while a cell next to your cursor was already provable. MineCoach names what you missed (satisfied number, filled number, a `1-1`, `1-2`, `1-2-1` or `1-2-2-1` pattern, or an advanced deduction) and highlights its numbers, the missed cell and the path your cursor took.
- **Better chord**: a nearby satisfied number would have opened everything your chord did, plus more. The optimal square to chord on is highlighted.
- **Clicks -> chord**: several single clicks around one number that a single chord (plus any missing flags) would have done in fewer clicks.
- **Rejected / wasted / low-value chords**, **unnecessary guesses** (clicking an unproven cell while a proven safe cell existed) and **wrong flags**.
- Stats: 3BV, 3BV/s, click efficiency, cursor travel and click counts.

Recordings are saved as JSON in `<app data>/trainer_recordings` (**Trainer > Open Recordings Folder**) and can be reopened with **Trainer > Open Recording...**. From the command line:

```sh
minecoach --analyze game.json   # print the text report
minecoach --review game.json    # open the analysis window
```

## Competitive (Arbiter-style) gameplay

MineCoach is tuned for fast, serious play. Every option below is on by default and can be toggled from the **Trainer** menu (settings are stored in `<app data>/trainer.ini`):

- **Arbiter style mouse controls**: flags are placed on right-button *press*; chord with left+right together or the middle button; a click acts on the cell under the cursor when the button is released, so you can press, slide and release. Left-clicking a revealed number still chords if that preference is enabled.
- **Instant restart**: **F2** or clicking the face starts a new game immediately, with no "are you sure?" dialog (Ctrl+R still works).
- **Fast board setup**: no minefield generation animation, and restarting on the same board size reuses the existing cells instead of rebuilding them. Theme pixmaps and pressed-cell images are cached, so restarts on Expert are effectively instant.
- **Auto-restart early losses**: if you die before solving a set share of the board's 3BV (default 10%, options Off/5/10/25/50%), a new game starts after a quarter of a second, with no stats, no dialog and no saved recording.
- **No score popups**: completed games are saved to the high scores silently. Turn on **Ask to Save Scores** for the original dialog; the **Save Score** button is always there.
- **End-of-game stats**: 3BV solved/total, 3BV/s, clicks and efficiency are shown in the results panel next to the usual statistics.

### Coming from LibreMines?

MineCoach stores its data in its own folder (`minecoach` instead of `libremines` in your app data directory). On the first launch it copies your LibreMines preferences, high scores, and any trainer settings and recordings across, so nothing is lost. Your original LibreMines data is left untouched.

## Compiling from the source code

### Dependencies

* [Qt Core](https://doc.qt.io/qt-6/qtcore-index.html), [Qt Gui](https://doc.qt.io/qt-6/qtgui-index.html), [Qt Widgets](https://doc.qt.io/qt-6/qtwidgets-index.html), [Qt SVG](https://doc.qt.io/qt-6/qtsvg-index.html) and [Qt Multimedia](https://doc.qt.io/qt-6/qtmultimedia-index.html) (Qt 6, or Qt 5 >= 5.12)
* [CMake](https://cmake.org/) >= 3.10

On Arch Linux and derivatives:
```sh
sudo pacman -Sy --needed base-devel qt6-base qt6-svg qt6-multimedia cmake
```

On Debian Linux and derivatives:
```sh
sudo apt-get install build-essential qt6-base-dev cmake libqt6svg6-dev qt6-multimedia-dev libgl1-mesa-dev
```

On Fedora Linux and derivatives:
```sh
sudo dnf install qt6-qtbase-devel qt6-qtsvg-devel cmake qt6-qtmultimedia-devel git
```

On FreeBSD:
```sh
su -
pkg install qt6-base qt6-svg qt6-multimedia git cmake
```

On macOS with [homebrew](https://brew.sh/):
```sh
brew install cmake ninja qt
```

On Windows, install Qt 6 for MSVC (for example with the [Qt online installer](https://download.qt.io/official_releases/online_installers/) or `aqtinstall`, including the `qtmultimedia` module) and Visual Studio 2022 or its Build Tools.

### Building

```sh
git clone https://github.com/megafishq/MineCoach.git
cd MineCoach
mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr/local ..
cmake --build . --config Release
```

On Windows pass the Qt location to CMake, e.g. `-DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64`, then run `windeployqt` on the produced `minecoach.exe`. To build with Qt 5, add `-DUSE_QT6="NO"` to the `cmake` step. Add `-DBUILD_TESTS=ON` to build the unit tests (run them with `ctest`).

The executable `minecoach` is generated in the build directory:
```sh
./minecoach
```

### Installing on the system (Unix only)

```sh
sudo make install
```

To uninstall, run this while in the `build` directory:
```sh
sudo xargs rm < install_manifest.txt
```

## How to play

### First Steps

When you start the application, you will see this screen:

![](./share/Screenshots/Screenshot0.png)

Start one of the predefined game modes (easy, medium and hard) or customize the field. The timer starts when you reveal the first cell. You can play with the mouse or with the keyboard; also see **Options** and **Trainer** on the menu bar.

### Playing with the mouse

With Arbiter style controls (default):

* **Left Click**: reveal the cell under the cursor when the button is released
* **Right Click**: flag/unflag the cell as soon as the button is pressed
* **Left + Right** or **Middle Click**: chord (reveal all neighbours of a satisfied number)
* **Left Click on a number**: chord (can be disabled in Options > Preferences)
* **F2** or **click the face**: new game

### Playing with the keyboard

To activate the keyboard controller mode, press either **W**, **A**, **S** or **D**.

* **Escape**: Exit Keyboard Controller Mode
* **A** / **S** / **D** / **W**: Move the current cell left / down / right / up
* **O**: Release Current Cell
* **P**: Flag/Unflag Current Cell
* **Space**: Locate current cell on middle of the scroll bar
* **F2** or **CTRL + R**: Restart the game
* **CTRL + SHIFT + P**: Save minefield as image

These keys can be changed from the main menu in Options > Preferences. Hold **CTRL** while moving to move faster.

## License and credits

MineCoach is free software, distributed under the same terms as LibreMines:

* The source code (everything in `src`) is licensed under the [GNU General Public License version 3](https://www.gnu.org/licenses/gpl-3.0.en.html) or any later version. See [LICENSE](./LICENSE).
* The artworks (everything in `share`) are licensed under [Creative Commons Attribution-Share Alike 4.0 International](https://creativecommons.org/licenses/by-sa/4.0/).

MineCoach is based on [LibreMines](https://github.com/Bollos00/LibreMines) 3.0.1, Copyright (C) 2020-2026 Bruno Bollos Correa, and keeps all original copyright notices. Changes made in this fork (the game recorder and analyser, the analysis window, the competitive gameplay options and the rebranding) are Copyright (C) 2026 MineCoach contributors and are released under the same licenses. The minefield themes, face reactions, icons and sounds are LibreMines' original artwork.

MineCoach is not affiliated with or endorsed by the LibreMines project. Please report MineCoach problems [here](https://github.com/megafishq/MineCoach/issues), not to LibreMines.

### Third party repositories used in this software

* [TwEmoji](https://github.com/twitter/twemoji)
* [OpenEmoji](https://github.com/hfg-gmuend/openmoji)
* [socialify](https://github.com/wei/socialify)
