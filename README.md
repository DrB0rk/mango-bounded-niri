<div align="center">
  <img src="https://github.com/mangowm/mango/blob/main/assets/mango-transparency-256.png" alt="Mango Logo" width="120"/>

  <h1>Mango Wayland Compositor, bounded-Niri fork</h1>

  <p>A fast, feature-rich Wayland compositor built on <a href="https://codeberg.org/dwl/dwl">dwl</a></p>

<a href="https://github.com/mangowm/mango/stargazers"><img src="https://img.shields.io/github/stars/mangowm/mango?style=flat&color=orange" alt="Stars"/></a>
<a href="https://github.com/mangowm/mango/blob/main/LICENSE"><img src="https://img.shields.io/badge/license-GPL--3.0-blue?style=flat" alt="License"/></a>
<a href="https://repology.org/project/mangowm/versions"><img src="https://repology.org/badge/tiny-repos/mangowm.svg" alt="Packaged in"/></a>
<a href="https://discord.gg/CPjbDxesh5"><img src="https://img.shields.io/discord/1430889676264177687?style=flat&logo=discord&label=discord" alt="Discord"/></a>

</div>

---

https://github.com/user-attachments/assets/bb83004a-0563-4b48-ad89-6461a9b78b1f

https://github.com/user-attachments/assets/be85e13f-7798-456d-957e-f8931687392e



## What this fork adds

This fork carries the local bounded-Niri work on top of Mango 0.17.2. It keeps Mango's layouts and protocols while adding a predictable scrolling workflow for windows that should stay usable at a readable size.

- **Bounded scroller columns**: Niri-style scrolling with a configurable minimum column proportion, stack limits, automatic stacking, and horizontal or vertical scroller layouts.
- **Hover focus without viewport movement**: moving the pointer focuses a window so scroll and typing go to it. The viewport only reveals a partially visible window after an explicit click.
- **Safe click reveal**: clicking a floating window that extends beyond the usable monitor area clamps it fully into view.
- **Primary-axis navigation**: one binding works across horizontal and vertical scrollers. The direction is resolved from the active layout.
- **Reversible column maximize**: maximize a scroller column, navigate away, and restore its previous proportion later.
- **Compositor-native layout panel**: press `Super+;` to switch between Tile, Scroller, and Vertical layouts, adjust spacing, and toggle floating snapping without leaving the compositor.
- **Persistent panel settings**: panel layout, gaps, and floating snap are saved atomically under `$XDG_STATE_HOME/mango/layout-panel.conf`, or `$HOME/.local/state/mango/layout-panel.conf`.
- **Stability hardening**: monitor teardown closes the panel safely, capped stack drops roll back cleanly, compositor-owned allocations fail closed, and config parsing runs as a Meson test.

The fork is intentionally small. The changes live in the Mango source tree and the `bounded-niri/` configuration bundle, so the custom behavior can be reviewed or removed without replacing Mango's core architecture.

## Quick start with the bundled configuration

Build and validate the fork:

```bash
git clone https://github.com/DrB0rk/mango-bounded-niri.git
cd mango-bounded-niri
meson setup build --buildtype=debugoptimized
meson compile -C build
build/mango -c bounded-niri/config-bounded-niri.conf -p
```

Run Mango with the bounded-Niri configuration from a session launcher or a nested Wayland session:

```bash
build/mango -c bounded-niri/config-bounded-niri.conf
```

The bundled bindings include `Super+[` and `Super+]` for primary-axis navigation, `Super+Shift+F` for column maximize, and `Super+;` for the layout panel. The full binding and configuration notes are in [`bounded-niri/README.md`](bounded-niri/README.md) and [`bounded-niri/runbook.md`](bounded-niri/runbook.md).

Panel settings are runtime preferences, not edits to the checked-in config file. To reset them, remove the state file and restart Mango:

```bash
rm -f "${XDG_STATE_HOME:-$HOME/.local/state}/mango/layout-panel.conf"
```

> See all layouts in action at [mangowm.github.io](https://mangowm.github.io/)

## Why Mango?

Mango starts where dwl ends. It keeps the lightweight, fast-build philosophy while adding the features that make a compositor actually usable day-to-day — without the bloat.

- **Lightweight & fast** — as lean as dwl, no functionality compromised
- **Excellent xwayland support** — run X11 apps without friction(Supports scale without blurring)
- **Tags, not workspaces** — each tag maintains its own independent window layout
- **Smooth animations** — window open/move/close, tag transitions, layer surfaces
- **Flexible layouts** — scroller, master-stack, monocle, dwindle, grid, and more
- **Rich window states** — swallow, minimize, maximize, global, overlay, fakefullscreen
- **Window effects** — blur, shadow, corner radius, opacity (via scenefx)
- **Excellent input method support** — text-input v2/v3
- **Sway-like scratchpad** — named scratchpad support included
- **Hycov-style overview** — see all windows at a glance
- **IPC** — send/receive messages from external programs
- **Hot-reload config** — no restart needed for keybinding changes
- **Zero flickering** — every frame is correct

## Vision

**Stability first.** After months of testing, Mango is solid enough for daily use. Breaking changes will be minimal.

**Practicality over novelty.** Features get added when they genuinely improve daily workflows — not for the sake of completeness.

**Focused scope.** Niche requests are evaluated by community interest. Significant upvotes move things forward.

## Installation

[![Packaging status](https://repology.org/badge/vertical-allrepos/mangowm.svg)](https://repology.org/project/mangowm/versions)

### Arch Linux

```bash
yay -S mangowm-git
```
#### use my config
- install dependencies
```
yay -S rofi foot xdg-desktop-portal-wlr swaybg waybar wl-clip-persist cliphist wl-clipboard wlsunset xfce-polkit swaync pamixer wlr-dpms sway-audio-idle-inhibit-git swayidle dimland-git brightnessctl swayosd wlr-randr grim slurp satty swaylock-effects-git wlogout sox
```
- clone config
```
git clone https://github.com/DreamMaoMao/mango-config.git ~/.config/mango
```

### Other distributions

See the [Installation Guide](https://mangowm.github.io/docs/installation) for Fedora, Gentoo, Guix, NixOS, openSUSE, PikaOS, AerynOS, and building from source.

## Documentation

- **[mangowm.github.io](https://mangowm.github.io/)** — website docs with configuration reference, keybindings, layouts, IPC, and more
- **[GitHub Wiki](https://github.com/mangowm/mango/wiki/)** — community-maintained wiki

## Community

Join us on **[Discord](https://discord.gg/CPjbDxesh5)**

## Acknowledgements

- [wlroots](https://gitlab.freedesktop.org/wlroots/wlroots) — Wayland protocol implementation
- [dwl](https://codeberg.org/dwl/dwl) — the foundation Mango builds on
- [scenefx](https://github.com/wlrfx/scenefx) — window effects library
- [owl](https://github.com/dqrk0jeste/owl) — animation groundwork
- [sway](https://github.com/swaywm/sway) — protocol reference

## Sponsor

If Mango makes your desktop better, consider supporting its development.

Thanks to everyone who has sponsored this project:

<table>
  <tr>
    <!-- add new sponsors here: copy the <td>...</td> block below -->
    <td align="center">
      <a href="https://github.com/dl09r">
        <img src="https://unavatar.io/github/dl09r" width="48" style="border-radius:50%"/><br/>
        <sub>dl09r</sub>
      </a>
    </td>
    <td align="center">
      <a href="https://github.com/tonybanters">
        <img src="https://unavatar.io/github/tonybanters" width="48" style="border-radius:50%"/><br/>
        <sub>tonybanters</sub>
      </a>
    </td>
    <td align="center">
      <a href="https://github.com/vinthara">
        <img src="https://unavatar.io/github/vinthara" width="48" style="border-radius:50%"/><br/>
        <sub>vinthara</sub>
      </a>
    </td>
    <td align="center">
      <a href="https://github.com/stepbrobd">
        <img src="https://unavatar.io/github/stepbrobd" width="48" style="border-radius:50%"/><br/>
        <sub>stepbrobd</sub>
      </a>
    </td>
  </tr>
</table>

Crypto donations accepted:

<table>
  <tr>
    <td valign="middle">
      <strong>Network:</strong> BEP20 (BSC)<br/>
      <strong>Address:</strong> <code>0xf9cda472f2556671d2504afc4c35340ec5615da1</code>
    </td>
    <td valign="middle">
      <img width="120" alt="sponsor QR" src="assets/crypto_sponserme_qrcode.png" />
    </td>
  </tr>
</table>
