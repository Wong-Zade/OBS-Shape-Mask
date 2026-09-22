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
- **Border** — solid or dashed, with adjustable width, opacity, and an animated pulse
- **Glow** — a separate soft halo effect around the shape edge, with optional pulsing
- **Drop Shadow** — offset, blurred, colored shadow that sits behind your content
- **Source Mask mode** — use another source's alpha channel as the mask instead of a geometric shape
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
- Bind the **"Shape Mask: Trigger Pop Animation"** hotkey under OBS Settings → Hotkeys for a one-shot pop-in effect.

## Building from source

This plugin is built on the official [obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate) and builds automatically via GitHub Actions on every push. To build locally, follow the standard instructions in the OBS plugin template's own documentation for your platform (CMake + your OS's usual build toolchain).

## Known limitations

- The Border effect doesn't apply in Source Mask mode, since it relies on shape geometry a source mask doesn't have.
- Picking the same source the filter is applied to as a Source Mask is blocked (to avoid an infinite render loop) rather than causing a crash.

## License

See [LICENSE](LICENSE).
