# Prototype

Interaction prototypes for `UpdaterUI` ([specs/updater-ui.md](../specs/updater-ui.md)),
hosted in Storybook. The window's metrics follow Sparkle's. All data and behavior are
simulated locally: no release index is fetched and nothing is downloaded or
installed.

```sh
pnpm install
pnpm dev
pnpm build
pnpm typecheck
```

Run these commands from `prototype/`. Open http://127.0.0.1:6006 and select
**Examples / Host App / Check for Updates**. The toolbar switches the desktop
(macOS 27, macOS 15, Windows 11, Ubuntu, Debian, Fedora, KDE Plasma, and Omarchy as
one entry per built-in theme — `omarchy-<theme>`, colours taken from each theme's
`colors.toml`), its appearance (light, dark; an Omarchy theme in the mode it does
not ship is derived from its colours) and the window's language (English,
Simplified Chinese).

## Architecture

See [Prototype architecture](../specs/prototype-architecture.md) for directory
responsibilities, naming, dependency boundaries, Storybook grouping and
validation rules.

- `src/features/updater-ui/`: the state machine and the user's preferences
  (`updater-ui-view.tsx`), its windows — `components/update-available-window.tsx`
  for Available and Downloaded, `components/update-status-window.tsx` for every
  other state, `components/update-permission-window.tsx` before the first silent
  check — the Reference / Sparkle stories (`sparkle-reference.stories.tsx`), the screen they
  stand on and the switch between them (`components/desktop.tsx`), the release notes
  (`components/markdown-view.tsx`, `markdown.ts`), the wording in English and
  Simplified Chinese (`strings.ts`), example data and simulated Updater calls.
- `src/components/ui/`: native controls — window, push button, progress bar,
  app icon — drawn from the tokens in `src/styles/native.css`.
- `src/styles/`: the native tokens per platform and appearance, global styles,
  and switching desktop and appearance.
- `.storybook/`: Storybook configuration, the desktop, appearance and language toolbars, and sidebar ordering.
- `stories/`: introductory MDX documentation.

## Relation to the spec and to Sparkle

The states, their transitions, the buttons and the wording come from
`specs/updater-ui.md`. The two windows are split the way most update frameworks
split them — Sparkle's `SUUpdateAlert` / `SUStatusController`, NetSparkle's
`UpdateAvailableWindow` / `DownloadProgressWindow`, AutoUpdater.NET's
`UpdateForm` / `DownloadUpdateDialog` — and named after the common one; `strings.ts` is the prototype's copy of
`src/updater_ui_strings.*`. The layout metrics — the 64pt icon, 20pt insets,
the 6pt-radius release notes box, the 400pt compact window with its bar in a
20pt frame, the 532 × 370pt window with notes — follow Sparkle's
`SUUpdateAlert.xib` and `SUStatusController.m`. Sparkle's extra
parts (Skip This Version, the automatic-updates checkbox, separate alerts) are
not used.
