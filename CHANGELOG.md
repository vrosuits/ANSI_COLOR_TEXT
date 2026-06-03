# Changelog

All notable changes to this project are documented here. The version is the
single number in `project(... VERSION)` in `CMakeLists.txt`; a release is cut by
pushing a matching `vX.Y.Z` tag (CI verifies the tag matches that version).

## [1.1.0] - 2026-06-04

### Added

- **Non-destructive rendering.** The raw ANSI source is kept per buffer;
  *Render ANSI Colors* is now a read-only color preview you can flip away from
  without losing the escape codes.
- **Raw views.** *Show Raw - Escape Codes* (real ESC bytes) and *Show Raw - \e
  Symbols* (ESC shown as `\e`, easy to edit and AI-friendly).
- **Auto-render on import / AI generate**, with a *default view* setting
  (Color / Raw codes / Raw symbols; default Color).
- **Editable per-provider AI system prompt** (with a *Load default* button),
  persisted per provider.
- **Render-font setting** (name + size) so block glyphs tile seamlessly.
- **Five new samples:** `unicode-boxes`, `truecolor-banner`, `blocks-sunset`,
  `spinner-anim`, `rainbow-wave-anim`.

### Fixed

- **Animation could not be restarted** after it finished (it replayed the
  stripped buffer and showed monochrome). It now replays from the canonical raw.
- Editing a colored render no longer destroys the escape codes; **saves always
  write real-ESC raw** regardless of the active view.
- **AI output using the literal text `ESC[`** (e.g. deepseek-chat) instead of a
  `0x1b` byte is now converted to real ESC, so it renders instead of showing the
  letters.
- Reduced the fine lines between block glyphs (zeroed Scintilla's extra line
  spacing; set a tight *Render font* for best results).

## [1.0.2] - 2026-06-03

### Bug Fixes

- **Plugin version now displays in Notepad++ Plugins Admin.** The DLL was built
  without a version resource, so the Plugins Admin "Installed" list showed a
  blank version for the plugin. Added a `VERSIONINFO` resource (generated from
  the project version) so Notepad++ now reports the version; the About dialog
  shows it as well.

### Internal

- The plugin version is now a single source of truth — `project(... VERSION)` in
  `CMakeLists.txt` flows into `Version.h`, the `VERSIONINFO` resource, and the
  About box. CI fails a `v*` tag that doesn't match it, so the version can't drift.

## [1.0.1]

- See the [v1.0.1 release notes](https://github.com/vrosuits/ANSI_COLOR_TEXT/releases/tag/v1.0.1).

## [1.0.0]

- Initial release: ANSI/SGR rendering (16 / 256 / true color), virtual-screen
  renderer, animation playback, the apply-color editor, and import/export.
