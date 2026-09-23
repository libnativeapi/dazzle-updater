/**
 * The native looks the prototype is drawn in, chosen on two axes: the desktop
 * whose controls are drawn, and its light or dark appearance. macOS comes in
 * two generations: 27 Golden Gate's Liquid Glass, and 15 Sequoia's flat look
 * that came before it. Omarchy is one desktop per colour theme — the second
 * level of the Desktop choice, `omarchy-<theme>`; a theme shows as shipped in
 * its own mode, and in a mode derived from its colours in the other.
 */
export const APPEARANCES = [
  { value: 'light', title: 'Light' },
  { value: 'dark', title: 'Dark' },
] as const

/**
 * Omarchy's built-in themes, from each theme's `colors.toml` in
 * basecamp/omarchy. The windows are drawn in Omarchy's own shell style
 * (`shell/Commons/Style.qml`, `default/themed/shell.toml.tpl`), which is built
 * from these colours; the frame is Hyprland's active border — the theme's
 * `hyprland_active_border`, or its accent (`hyprland.lua.tpl`).
 */
export const OMARCHY_THEMES = [
  { value: 'tokyo-night', title: 'Tokyo Night', mode: 'dark', accent: '#7aa2f7', background: '#1a1b26', foreground: '#a9b1d6' },
  { value: 'catppuccin', title: 'Catppuccin', mode: 'dark', accent: '#89b4fa', background: '#1e1e2e', foreground: '#cdd6f4' },
  { value: 'catppuccin-latte', title: 'Catppuccin Latte', mode: 'light', accent: '#1e66f5', background: '#eff1f5', foreground: '#4c4f69' },
  { value: 'ethereal', title: 'Ethereal', mode: 'dark', accent: '#7d82d9', background: '#060b1e', foreground: '#ffcead' },
  { value: 'everforest', title: 'Everforest', mode: 'dark', accent: '#7fbbb3', background: '#2d353b', foreground: '#d3c6aa' },
  { value: 'flexoki-light', title: 'Flexoki Light', mode: 'light', accent: '#205ea6', background: '#fffcf0', foreground: '#100f0f' },
  { value: 'gruvbox', title: 'Gruvbox', mode: 'dark', accent: '#7daea3', background: '#282828', foreground: '#d4be98' },
  {
    value: 'hackerman',
    title: 'Hackerman',
    mode: 'dark',
    accent: '#82fb9c',
    background: '#0b0c16', foreground: '#ddf7ff',
    border: { colors: ['#26a269ee', '#2ec27eee'], angle: 45 },
  },
  { value: 'kanagawa', title: 'Kanagawa', mode: 'dark', accent: '#dcd7ba', background: '#1f1f28', foreground: '#dcd7ba' },
  {
    value: 'last-horizon',
    title: 'Last Horizon',
    mode: 'dark',
    accent: '#b59790',
    background: '#0c0b0c', foreground: '#fafcfb',
    border: { colors: ['#8a8588ee', '#e2dddcee'], angle: 0 },
    inactiveBorder: '#584e51aa',
  },
  { value: 'lumon', title: 'Lumon', mode: 'dark', accent: '#8bc9eb', background: '#16242d', foreground: '#d6e2ee' },
  { value: 'lupine', title: 'Lupine', mode: 'light', accent: '#3264eb', background: '#fafafa', foreground: '#212121' },
  { value: 'matte-black', title: 'Matte Black', mode: 'dark', accent: '#e68e0d', background: '#121212', foreground: '#bebebe' },
  { value: 'miasma', title: 'Miasma', mode: 'dark', accent: '#78824b', background: '#222222', foreground: '#c2c2b0' },
  { value: 'nord', title: 'Nord', mode: 'dark', accent: '#81a1c1', background: '#2e3440', foreground: '#d8dee9' },
  { value: 'osaka-jade', title: 'Osaka Jade', mode: 'dark', accent: '#509475', background: '#111c18', foreground: '#c1c497' },
  { value: 'retro-82', title: 'Retro 82', mode: 'dark', accent: '#faa968', background: '#05182e', foreground: '#f6dcac' },
  { value: 'ristretto', title: 'Ristretto', mode: 'dark', accent: '#f38d70', background: '#2c2525', foreground: '#e6d9db' },
  { value: 'rose-pine', title: 'Rosé Pine', mode: 'light', accent: '#56949f', background: '#faf4ed', foreground: '#575279' },
  {
    value: 'solitude',
    title: 'Solitude',
    mode: 'dark',
    accent: '#798186',
    background: '#101315', foreground: '#cacccc',
    border: { colors: ['#798186ee', '#caccccee'], angle: 0 },
    inactiveBorder: '#1e1e1e',
  },
  { value: 'vantablack', title: 'Vantablack', mode: 'dark', accent: '#8d8d8d', background: '#000000', foreground: '#ffffff' },
  { value: 'white', title: 'White', mode: 'light', accent: '#6e6e6e', background: '#ffffff', foreground: '#000000' },
] as const satisfies readonly OmarchyTheme[]

export interface OmarchyTheme {
  value: string
  title: string
  mode: 'light' | 'dark'
  accent: string
  background: string
  foreground: string
  /** `hyprland_active_border` when the theme sets one: Hyprland gradient colours and angle. */
  border?: { colors: readonly string[]; angle: number }
  /** `hyprland_inactive_border` when the theme sets one; Hyprland's `rgba(595959aa)` otherwise. */
  inactiveBorder?: string
}

/** The desktops whose controls are drawn; `data-desktop` carries one of them. */
export type Desktop = 'macos27' | 'macos' | 'windows' | 'ubuntu' | 'debian' | 'fedora' | 'kde' | 'omarchy'

/**
 * The Desktop toolbar: one entry per desktop, and Omarchy's second level —
 * one entry per theme, `omarchy-<theme>` — since a theme is what Omarchy looks
 * like, not a setting beside it.
 */
export const DESKTOPS: readonly { value: string; title: string }[] = [
  { value: 'macos27', title: 'macOS 27 Golden Gate' },
  { value: 'macos', title: 'macOS 15 Sequoia' },
  { value: 'windows', title: 'Windows 11' },
  { value: 'ubuntu', title: 'Ubuntu' },
  { value: 'debian', title: 'Debian (GNOME)' },
  { value: 'fedora', title: 'Fedora (GNOME)' },
  { value: 'kde', title: 'KDE Plasma' },
  ...OMARCHY_THEMES.map(theme => ({ value: `omarchy-${theme.value}`, title: `Omarchy · ${theme.title}` })),
]
export type Appearance = (typeof APPEARANCES)[number]['value']
export type Platform = 'macos' | 'windows' | 'linux'

export const DEFAULT_DESKTOP: Desktop = 'macos27'
export const DEFAULT_APPEARANCE: Appearance = 'light'

export function platformOf(desktop: Desktop): Platform {
  if (desktop === 'macos' || desktop === 'macos27') return 'macos'
  return desktop === 'windows' ? 'windows' : 'linux'
}

/** Hyprland draws a gradient border from its angle; as a CSS gradient, 0deg runs left to right. */
function borderGradient(theme: OmarchyTheme): string {
  const colors = theme.border?.colors ?? [theme.accent, theme.accent]
  const angle = 90 - (theme.border?.angle ?? 0)
  return `linear-gradient(${angle}deg, ${colors.join(', ')})`
}

/** A Desktop toolbar value as the desktop it draws and, for Omarchy, its theme. */
function resolveDesktop(value: string): { desktop: Desktop; omarchy: OmarchyTheme } {
  const theme = value.startsWith('omarchy-') ? OMARCHY_THEMES.find(entry => `omarchy-${entry.value}` === value) : undefined
  if (theme) return { desktop: 'omarchy', omarchy: theme }
  const known = DESKTOPS.some(entry => entry.value === value) && value !== 'omarchy'
  return { desktop: known ? (value as Desktop) : DEFAULT_DESKTOP, omarchy: OMARCHY_THEMES[0] }
}

/**
 * The theme's colours in the appearance asked for. In its own mode a theme is
 * exactly as Omarchy ships it. Asked for the other mode — which Omarchy's
 * themes do not have — the prototype derives one from the theme's own colours:
 * the foreground becomes the background (lifted towards white, or pressed
 * towards black), the background becomes the text, the accent is pressed or
 * lifted to stay legible, and Hyprland's border stays as it is, so the theme is
 * still recognisable. The derived half is the
 * prototype's, not Omarchy's.
 */
function omarchyPalette(theme: OmarchyTheme, appearance: Appearance) {
  if (theme.mode === appearance) {
    return { background: theme.background, foreground: theme.foreground, accent: theme.accent }
  }
  // The accent is kept but pressed or lifted, so text drawn in it still reads.
  return appearance === 'light'
    ? {
        background: `color-mix(in srgb, ${theme.foreground} 22%, #ffffff)`,
        foreground: theme.background,
        accent: `color-mix(in srgb, ${theme.accent} 62%, #000000)`,
      }
    : {
        background: `color-mix(in srgb, ${theme.foreground} 45%, #000000)`,
        foreground: theme.background,
        accent: `color-mix(in srgb, ${theme.accent} 62%, #ffffff)`,
      }
}

/**
 * Stamps `data-platform`, `data-desktop` and `data-appearance` on `root`, and
 * under Omarchy sets the theme's colours, in the appearance asked for, as
 * `--omarchy-*` custom properties.
 */
export function applyTheme(root: HTMLElement, desktopValue: string, appearanceValue: string) {
  const { desktop: resolved, omarchy } = resolveDesktop(desktopValue)
  const appearance: Appearance = appearanceValue === 'dark' ? 'dark' : 'light'
  const palette = omarchyPalette(omarchy, appearance)
  root.dataset.platform = platformOf(resolved)
  root.dataset.desktop = resolved
  root.dataset.appearance = appearance
  root.style.setProperty('--omarchy-border', borderGradient(omarchy))
  root.style.setProperty('--omarchy-inactive-border', omarchy.inactiveBorder ?? '#595959aa')
  root.style.setProperty('--omarchy-accent', palette.accent)
  root.style.setProperty('--omarchy-background', palette.background)
  root.style.setProperty('--omarchy-foreground', palette.foreground)
}
