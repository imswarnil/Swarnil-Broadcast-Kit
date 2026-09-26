# Installing and building

## Installing a built plugin

Quit OBS, copy the bundle into your user plugin folder, start OBS again:

```
~/Library/Application Support/obs-studio/plugins/sbk.plugin
```

**Sources → +** then lists twenty *SBK …* sources, **Filters** on any source
gains four video filters and two audio ones, **Docks → Broadcast Kit** is the
control panel, and the **Tools** menu has five *Broadcast Kit:* items.

The kit sets type in **Geist** and **Geist Mono**. Copy the files in `fonts/`
into `~/Library/Fonts`, or pick any installed font in a source's *Look → Font*.
Building from source does this for you.

### If the sources do not appear

Two different things cause this and they are easy to confuse.

**The wrong architecture.** The released bundle is **arm64**: the runner that
builds it is Apple Silicon, and so are the OBS frameworks it links. On an Intel
Mac it will not load and OBS says nothing useful about it. Check what you have:

```bash
lipo -archs ~/Library/Application\ Support/obs-studio/plugins/sbk.plugin/Contents/MacOS/sbk
uname -m    # arm64, or x86_64 for Intel
```

If they disagree, build from source instead — `./build.command` produces a
plugin for whatever machine it runs on, and an Intel build works fine. A
universal bundle is not currently shipped because it would need universal
copies of the OBS frameworks and of jansson, which Homebrew does not provide.

**The download quarantine.** macOS refuses a plugin it has flagged as coming
from the internet:

```bash
xattr -dr com.apple.quarantine ~/Library/Application\ Support/obs-studio/plugins/sbk.plugin
```

A plugin you built yourself is never quarantined, so this only ever applies to a
release you downloaded.

## The transition

**SBK Wipe** is a transition, not a source, so it is added from the
**Scene Transitions** panel's **+** button — the frontend API has no call to add
one, which is why this step is by hand for every transition plugin there has
ever been. Set the duration beside it: 300–500 ms suits the bar.

## The phone remote

```bash
./remote/serve.command
```

It prints an address to open on a phone on the same network, and the address to
type into the remote. First turn OBS's own server on: **Tools → WebSocket Server
Settings → Enable**, then *Show Connect Info* for the port and password.

It has to be plain `http` from your machine. A page over `https` cannot open the
unencrypted `ws://` connection obs-websocket speaks, so the copy at
obs.imswarnil.com/remote/ can show you the interface but will never reach your
OBS. Nothing goes through the website either way.

## The display capture

Two of the scenes — *Two up* and *Three up* — put a real display capture in the
big frame. macOS asks for **Screen Recording** permission the first time OBS
tries; until it is given the capture is black, which is the system telling you
rather than a fault in the kit. Grant it in **System Settings → Privacy &
Security → Screen Recording**, then restart OBS.

The clean self-test hides the display capture along with the camera, for the same
reason it hides the camera: a published screenshot must not carry whatever
happens to be on the desktop.

## The profile

`profile/Swarnil Broadcast Kit/basic.ini` is 1080p60, Apple hardware H.264 at
6000 kbps, 160 kbps audio, MKV recordings. `profile/install.command` copies it
into OBS's profiles folder and never overwrites one that is already there. OBS
lists it under **Profile** after a restart, and **Tools → Broadcast Kit: use the
Broadcast Kit profile** switches to it.

## Building on macOS

| | |
| --- | --- |
| OBS Studio | 30 or newer, installed in `/Applications` |
| Xcode Command Line Tools | `xcode-select --install` |
| CMake, SIMDe, jansson, Qt | `brew install cmake simde jansson qt` |

```bash
./build.command
```

It configures, compiles, and installs the plugin, the fonts and the profile.
Quit OBS first — a loaded plugin cannot be replaced underneath a running OBS,
and the script refuses to run while one is up. You can double-click it in
Finder. By hand:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
cp -R build/sbk.plugin ~/Library/Application\ Support/obs-studio/plugins/
```

SIMDe is needed because libobs's headers use it for SSE intrinsics on Apple
silicon. jansson parses what the live-data sources fetch. Qt supplies the
*headers* for the control dock — the frameworks come from OBS.app itself, at the
same version, because loading a second Qt into the process crashes on the first
widget. curl is already on every Mac. Nothing is downloaded at build time: `deps/include` holds the libobs
headers for OBS 32.2.2 (see `deps/README.md`).

## Building the documentation site

Plain Node, no dependencies, no install step:

```bash
node site/build.mjs     # → dist/
node site/check.mjs     # what CI runs: every link resolves, every screenshot exists
```

`site/content.mjs` is the only file to edit for content. The screenshots in
`docs/screens/` are real frames from the self-test below.

## Linux and Windows

The macOS branch of `CMakeLists.txt` is specific to how OBS is packaged there.
Elsewhere, install the OBS development package (`libobs-dev` and the frontend
API), `libcurl` and `libjansson`, then:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo && cmake --build build && sudo cmake --install build
```

Text is drawn by OBS's own FreeType source and the shaders are ordinary effect
files, so nothing here is macOS-specific in principle. Only macOS has been
tested, and releases ship a macOS bundle only. Reports welcome.

## Checking it loaded

**Help → Log Files → View Current Log**:

```
[sbk] v0.3.0 loaded — built for libobs 32.2.2, running 32.2.2
```

A source that draws nothing means its shader failed to compile, and the same log
says `[sbk] effects/….effect failed to compile`.

## The self-test

```bash
touch ~/Library/Application\ Support/obs-studio/.sbk-selftest         # as it really looks
touch ~/Library/Application\ Support/obs-studio/.sbk-selftest-clean   # camera hidden
```

Start OBS. Once it has loaded, the kit creates the scene collection, walks all
twenty-two scenes and takes a program screenshot of each into the profile's
recording folder. The trigger file is deleted, so it fires once.

The **clean** variant hides every camera item for the walk and puts them back
afterwards. That is the one to use for anything going on the web: an ordinary run
captures whatever the webcam is pointed at, and those images are what the
documentation site publishes.

The walk runs on a worker thread and hands each step back to the UI thread with
`obs_queue_task`. That matters: OBS writes a queued screenshot on the UI thread,
so a self-test that slept on it collapsed thirteen shots into one.

To check a QR source really scans, decode a rendered frame rather than trusting
the picture — a code can look perfect and be unreadable:

```swift
// qrdec.swift — swiftc -O -o qrdec qrdec.swift && ./qrdec frame.png
import CoreImage; import Foundation
let img = CIImage(contentsOf: URL(fileURLWithPath: CommandLine.arguments[1]))!
let det = CIDetector(ofType: CIDetectorTypeQRCode, context: nil,
                     options: [CIDetectorAccuracy: CIDetectorAccuracyHigh])!
print((det.features(in: img).first as? CIQRCodeFeature)?.messageString ?? "NONE")
```

## Uninstalling

```bash
rm -rf ~/Library/Application\ Support/obs-studio/plugins/sbk.plugin
```
