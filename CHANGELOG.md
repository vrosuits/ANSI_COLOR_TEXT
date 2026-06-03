# Changelog

All notable changes to this project are documented here. The version is the
single number in `project(... VERSION)` in `CMakeLists.txt`; a release is cut by
pushing a matching `vX.Y.Z` tag (CI verifies the tag matches that version).

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
