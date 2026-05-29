# kde-portal-platformtheme

**Qt 6 platform theme plugins for KDE appearance sync via xdg-desktop-portal, avoiding stale `kdeglobals` file binds in Flatpak.**

This repository builds two plugin variants:

- `kde-portal-plasma`: a Plasma-like KDE portal theme bridge.
- `kde-portal`: a lighter KDE portal theme bridge with direct Qt palette mapping.

## Why this exists

Qt/KDE applications normally learn KDE colors and fonts from KConfig files such
as `kdeglobals`. In Flatpak, `org.kde.Platform` may expose the host file as:

```text
--filesystem=xdg-config/kdeglobals:ro
```

That is a single-file bind mount. KDE updates `kdeglobals` by writing a new file
and replacing the old one with `rename(2)`. After that atomic replacement, the
host path points to a new inode, but the sandbox's single-file bind mount can
still point to the old inode. The application can then keep reading stale theme
data after the user switches KDE color schemes.

The same Flatpak behavior is tracked in:

https://github.com/flatpak/flatpak/issues/5370

The old workaround is to expose the whole host config directory:

```text
--nofilesystem=xdg-config/kdeglobals --filesystem=~/.config:ro
```

and then put `$HOME/.config` early in `XDG_CONFIG_DIRS`. That works because a
directory mount survives KDE's atomic file replacement, but it gives the app
read access to much more private host configuration than it actually needs.

#### Use this plugin when:

- Flatpak single-file binds can go stale after atomic rewrites.
- Qt's `xdgdesktopportal` platform theme reads the standard
- The application wants KDE appearance support without bundling the full KDE
  Frameworks stack.

As far as I know, as of May 2026, no Qt LTS release provides this behavior
unless `plasma-integration` is included. eg: **Qt 6.5 LTS, used by the Flatpak KDE
runtime 6.5, does not include `plasma-integration` either.**

**Bundling `plasma-integration` pulls in a large KDE Frameworks dependency stack.
If you want KDE appearance integration without that dependency cost, this plugin
is meant for that case.**

## What this plugin does

`kde-portal-plasma` avoids direct `kdeglobals` file reads for KDE appearance data:

- reads KDE colors, fonts, icon names, and selected UI hints from
  `org.freedesktop.portal.Settings`
- watches the portal `SettingChanged` signal and sends a Qt theme-change event
  when relevant KDE settings change
- owns `palette()`, `font()`, `colorScheme()`, and the KDE-related
  `themeHint()` values needed for live theme updates

This keeps the useful portal integrations from Qt's original Flatpak path while
avoiding the stale-inode `kdeglobals` path for colors and fonts.

This project also builds `kde-portal`, a lighter variant that keeps the same
portal-based settings path but uses direct Qt palette mapping instead of the
KColorScheme-style palette builder.

## License and source notes

This project is licensed under `LGPL-2.0-or-later`; see `LICENSE`. Files under
`kguiaddons/` and `kcolorscheme/` contain local adaptations of KDE Frameworks
logic and carry LGPL SPDX headers at the top of each file. The `portal/` code is
project glue around the portal D-Bus API.

## Flatpak integration

Copy this directory into your application's source tree, for example:

```text
qt-platformthemes/kde-portal-plasma/
```

Then add a module to the Flatpak manifest:

```yaml
modules:
  - name: kde-portal-plasma
    buildsystem: cmake-ninja
    config-opts:
      - -DCMAKE_BUILD_TYPE=Release
    sources:
      - type: dir
        path: kde-portal-plasma
```

The module installs the Plasma-like plugin as:

```text
/app/plugins/platformthemes/kde-portal-plasma.so
```

Enable it from `finish-args`:

```yaml
finish-args:
  - --env=QT_QPA_PLATFORMTHEME=kde-portal-plasma
  # or the lite one:
  #- --env=QT_QPA_PLATFORMTHEME=kde-portal
  - --env=QT_PLUGIN_PATH=/app/plugins
  - --nofilesystem=xdg-config/kdeglobals
  - --filesystem=xdg-config/kcminputrc:ro
```

`kde-portal-plasma` reads `kcminputrc` on Wayland to mirror Plasma's cursor
theme and size startup behavior. With this setup, live KDE theme sync should
not require host `~/.config` access.
