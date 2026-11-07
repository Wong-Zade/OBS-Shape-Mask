# OBS Shape Mask

A filter plugin for [OBS Studio](https://obsproject.com) that masks any source — window capture, webcam, browser source, anything — into a shape, with a border, glow, drop shadow, and simple animations.
<img width="1366" height="768" alt="Screenshot (175)" src="https://github.com/user-attachments/assets/76f397a0-3c0d-44e2-a8eb-594f1530ce1d" />

![Shape Mask filter demo](docs/demo.gif)


## Features

- **Shapes** — Circle, Rectangle, Rounded Rectangle (with adjustable corner radius)
- **Independent width/height sizing** — stretch a shape instead of only scaling it uniformly
- **Rotation** — a static angle slider, plus a continuous spin option under Animation
- **Feathered edges** — soft or hard-edged mask boundary
- **Mask Invert** — show everything *outside* the shape instead of inside
- **Border** — solid border, with adjustable width, opacity, and an animated pulse
- **Drop Shadow** — offset, blurred, colored shadow that sits behind your content
- **Source Mask mode** — use another source's alpha channel as the mask instead of a geometric shape
- **Face follow (experimental, Windows)** — move the shape mask to keep a face centered in it
- **Hotkey-triggered pop animation** — bind a key to trigger a one-shot bouncy scale-in effect
- **Shape Animation** — Pulse, Breathe, Rotate, Bounce, Shake, or Scale, running continuously

## Installation

1. Go to the [Releases page](../../releases) and download the package for your OS (Windows/macOS/Linux).
2. Extract it.
3. Copy the plugin file into your OBS plugins folder:
   - **Windows:** the `.dll` goes in `obs-plugins/64bit/`, and the rest of the files go in `data/obs-plugins/obs-shape-mask/`, both inside your OBS Studio install folder.
   - **macOS/Linux:** follow the equivalent plugin folder structure for your OBS install.
4. Restart OBS.
5. Right-click any source → **Filters** → **+** → **Shape Mask**.

## Usage

- Add the filter to any source and pick a shape, size, and position.
- Turn on **Enable Border** or **Enable Glow** for an outline/halo effect.
- Turn on **Enable Drop Shadow** for a soft shadow behind your content.
- Switch **Mask Type** to **Source Mask** to use another source's alpha channel as the mask instead of a geometric shape (note: the Border effect isn't available in this mode).
- On Windows, enable **Follow Face (Experimental)** to move the shape mask opening with a face in a direct webcam/video source. Detection runs locally; scene/group sources and rendered sources are not supported by this first prototype. While face follow is on, it takes over the manual offset controls.
- Bind the **"Shape Mask: Trigger Pop Animation"** hotkey under OBS Settings → Hotkeys for a one-shot pop-in effect.

## Building from source

This plugin is built on the official [obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate) and builds automatically via GitHub Actions on every push. To build locally, follow the standard instructions in the OBS plugin template's own documentation for your platform (CMake + your OS's usual build toolchain).

### Windows installer

To build the Windows installer, install CMake 3.28-3.30, Visual Studio 2022 with the C++ desktop workload and Windows 10 SDK, PowerShell 7.2+, and Inno Setup 6. The Windows build downloads pinned ONNX Runtime and YuNet model artifacts during CMake configuration. From the repository root, run:

```powershell
$env:CI = "true"
pwsh -File .github\scripts\Build-Windows.ps1 -Target x64 -Configuration RelWithDebInfo
pwsh -File .github\scripts\Package-Windows.ps1 -Target x64 -Configuration RelWithDebInfo -Package
```

The installer and plugin archive are written to `release\`. The installer is named `obs-shape-mask-<version>-windows-x64-setup.exe`.

## Known limitations

- The Border effect doesn't apply in Source Mask mode, since it relies on shape geometry a source mask doesn't have.
- Picking the same source the filter is applied to as a Source Mask is blocked (to avoid an infinite render loop) rather than causing a crash.

## License

See [LICENSE](LICENSE).
