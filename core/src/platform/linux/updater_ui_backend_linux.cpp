#include "../../foundation/updater_ui_backend.h"
#include "../../updater_ui_state.h"
#include "updater_ui_notes_linux.h"

#include <gtk/gtk.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <map>
#include <string>
#include <vector>

// UpdaterUI's windows on Linux: the three of ../../../specs/updater-ui.md, built from
// GTK 4's own controls (GtkWindow with a header bar, GtkLabel, GtkButton,
// GtkCheckButton, GtkProgressBar, and a GtkTextView for the release notes).
//
// GTK is not thread-safe and may only be used by the thread that initialized it — the
// application's main thread, which is also the thread running the main loop. Every entry
// point here therefore copies what it needs and posts a closure to the default main
// context with g_main_context_invoke_full(): the state machine's worker keeps producing
// states, and they are drawn in order on the main thread. A GTK application has done
// gtk_init() (through gtk_application_run()) long before it offers "Check for Updates…",
// which is why IsAvailable() is false in a process that is not a GTK application at all.
//
// The backend never reports an event from inside Draw/Hide/BringToFront/QuitApplication
// (the state machine holds its lock there): the user's own gestures arrive later, on the
// main thread.
//
// Two details are Linux's own, and the spec records them: the windows belong to the
// application's main loop rather than to a thread of the module's, and the compositor
// places a new window, so the flow does not centre it on the one it replaces.

namespace {

using dazzle_updater::internal::IsUpdateWindow;
using dazzle_updater::internal::UiButton;
using dazzle_updater::internal::UiCheckbox;
using dazzle_updater::internal::UiContent;
using dazzle_updater::internal::UiEvent;
using dazzle_updater::internal::UiEventKind;
using dazzle_updater::internal::UiProgress;
using dazzle_updater::internal::UiState;
using dazzle_updater::internal::UiWindow;

// --- metrics (../../../specs/updater-ui.md, "外观") -----------------------------------
//
// The prototype's spacing and type sizes, in GTK's own units. Linux has no Sparkle
// window to match, so a window is as wide as the spec says and as tall as its content
// needs — except the update-available window, which opens at the spec's own size and
// scrolls its notes instead of growing with them.

constexpr int kMargin = 20;             // 四周 20 pt 边距
constexpr int kIconSize = 64;           // 64 pt 应用图标
constexpr int kIconGap = 8;             // the icon and the text beside it
constexpr int kGap = 12;                // between groups of controls
constexpr int kTextGap = 8;             // between a heading and its detail
constexpr int kNotesMinHeight = 200;    // 更新说明区域最小高 200 pt
constexpr int kAvailableWidth = 532;    // 内容区默认 532 × 370 pt
constexpr int kAvailableHeight = 370;
constexpr int kAvailableMinWidth = 462;
constexpr int kAvailableMinHeight = 150;
constexpr int kStatusWidth = 400;       // 内容区宽 400 pt
constexpr int kPermissionWidth = 437;   // 内容区宽 437 pt
constexpr int kButtonMinWidth = 100;
/// The text columns, in pixels: the window's content width minus its margins, the icon and
/// the gap beside it (../specs/updater-ui.md, "外观").
constexpr int kAvailableTextWidth = kAvailableWidth - 2 * kMargin - kIconSize - kIconGap;
constexpr int kStatusTextWidth = kStatusWidth - 2 * kMargin - kIconSize - kIconGap;
constexpr int kPermissionTextWidth = kPermissionWidth - 2 * kMargin - kIconSize - 16;
constexpr int kStatusBodyWidth = kStatusWidth - 2 * kMargin;
constexpr int kPulseIntervalMs = 100;

const char* const kStylesheet =
    ".dazzle_updater-heading { font-weight: bold; }\n"
    ".dazzle_updater-detail { }\n"
    ".dazzle_updater-status { font-size: 11px; opacity: 0.65; }\n"
    ".dazzle_updater-notes { border: 1px solid alpha(currentColor, 0.15); border-radius: 6px; }\n"
    ".dazzle_updater-notes text { background-color: transparent; }\n"
    ".dazzle_updater-buttons button { min-width: 100px; }\n";

// --- state shared with the main thread, main thread only ------------------------------

struct LinuxWindow {
  GtkWidget* window = nullptr;
  UiWindow kind = UiWindow::kNone;
  UiState state = UiState::kChecking;
  /// The widgets a redraw of the same state updates instead of rebuilding the window.
  GtkWidget* heading = nullptr;
  GtkWidget* detail = nullptr;
  GtkWidget* status = nullptr;
  GtkWidget* progress = nullptr;
  GtkWidget* checkbox = nullptr;
  /// The state's default button, which is given the keyboard focus: Return has to press
  /// the bold button, not whichever control GTK happened to focus first.
  GtkWidget* default_button = nullptr;
  /// Set while the module itself flips the check box, so the change is not reported back.
  bool updating_checkbox = false;
  guint pulse_source = 0;
  /// The notes' link targets, indexed by the tag names the renderer created.
  std::vector<std::string> notes_urls;
};

LinuxWindow g_window;
std::function<void(const UiEvent&)> g_sink;

void ReportEvent(UiEventKind kind, bool flag = false) {
  if (!g_sink) {
    return;
  }
  UiEvent event;
  event.kind = kind;
  event.flag = flag;
  g_sink(event);
}

/// Runs `work` on the thread that owns the default main context — the application's GTK
/// thread — and destroys the closure afterwards. It is deliberately always *deferred*
/// (an idle source) rather than run inline when the caller happens to be that thread: a
/// button's own handler reaches here, and drawing the next window inside it would destroy
/// the window whose signal is still on the stack.
void OnMainThread(std::function<void()> work) {
  auto* job = new std::function<void()>(std::move(work));
  g_idle_add_full(
      G_PRIORITY_HIGH_IDLE,
      [](gpointer data) -> gboolean {
        (*static_cast<std::function<void()>*>(data))();
        return G_SOURCE_REMOVE;
      },
      job, [](gpointer data) { delete static_cast<std::function<void()>*>(data); });
}

void EnsureStyles() {
  static bool installed = false;
  if (installed) {
    return;
  }
  installed = true;
  GtkCssProvider* provider = gtk_css_provider_new();
#if GTK_CHECK_VERSION(4, 12, 0)
  gtk_css_provider_load_from_string(provider, kStylesheet);
#else
  gtk_css_provider_load_from_data(provider, kStylesheet, -1);
#endif
  gtk_style_context_add_provider_for_display(gdk_display_get_default(),
                                             GTK_STYLE_PROVIDER(provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref(provider);
}

// --- Omarchy ---------------------------------------------------------------------------
//
// On Omarchy (a Hyprland desktop, not GNOME) GTK is told only light or dark:
// omarchy-theme-set writes Adwaita or Adwaita-dark into gsettings and nothing more, so a
// plain GTK 4 window comes out in Adwaita's greys and blue while the bar, the launcher and
// the terminals around it wear the theme. The theme's palette is a file —
// ~/.local/state/omarchy/current/theme/colors.toml, ~/.config/omarchy/current/theme/ on
// releases before 3.x — that Omarchy's own templates read, and the windows read the same
// file and dress themselves from it, in the roles Omarchy gives its own surfaces
// (shell.toml: a control is the foreground at 4 % over the background with a 40 %
// border, 8 % on hover, 22 % pressed; a chosen control is the accent with the background
// as its text; a sunken view is dark_background). The CSS is scoped to this module's
// windows (`window.dazzle_updater`), so the host application's own windows are left to
// their theme. Under GNOME nothing is read and nothing changes: there the desktop's theme
// already reaches the controls.
//
// DAZZLE_OMARCHY_COLORS=<path to a colors.toml> uses that palette anywhere, for previews.

struct OmarchyPalette {
  std::string background;
  std::string dark_background;
  std::string foreground;
  std::string dark_foreground;
  std::string bright_foreground;
  std::string accent;
  std::string selection;
};

bool IsHexColor(const std::string& value) {
  if (value.size() != 7 || value[0] != '#') {
    return false;
  }
  for (size_t index = 1; index < value.size(); ++index) {
    if (!std::isxdigit(static_cast<unsigned char>(value[index]))) {
      return false;
    }
  }
  return true;
}

std::string Trim(const std::string& text) {
  const size_t first = text.find_first_not_of(" \t\r");
  if (first == std::string::npos) {
    return "";
  }
  const size_t last = text.find_last_not_of(" \t\r");
  return text.substr(first, last - first + 1);
}

/// colors.toml is flat `key = "value"` lines. They are read the way omarchy-theme-color
/// reads them: the value is what sits between the first pair of quotes (so a comment after
/// it is ignored), an unquoted value is trimmed, and keys are kept as written.
std::map<std::string, std::string> ReadColorsFile(const std::string& path) {
  std::map<std::string, std::string> values;
  gchar* contents = nullptr;
  gsize length = 0;
  if (!g_file_get_contents(path.c_str(), &contents, &length, nullptr)) {
    return values;
  }
  const std::string text(contents, length);
  g_free(contents);
  size_t position = 0;
  while (position < text.size()) {
    size_t newline = text.find('\n', position);
    if (newline == std::string::npos) {
      newline = text.size();
    }
    const std::string line = text.substr(position, newline - position);
    position = newline + 1;
    const size_t equals = line.find('=');
    if (equals == std::string::npos) {
      continue;
    }
    const std::string key = Trim(line.substr(0, equals));
    if (key.empty() || key[0] == '#' || key[0] == '[') {
      continue;
    }
    std::string value = Trim(line.substr(equals + 1));
    const size_t quote = value.find_first_of("\"'");
    if (quote != std::string::npos) {
      const size_t close = value.find(value[quote], quote + 1);
      value = value.substr(quote + 1, close == std::string::npos ? std::string::npos
                                                                 : close - quote - 1);
    }
    for (char& character : value) {
      character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    values[key] = value;
  }
  return values;
}

/// The first of `keys` the file defines as a colour, or "": the canonical name first, then
/// the legacy short name and the ANSI slot omarchy-theme-color falls back to.
std::string PickColor(const std::map<std::string, std::string>& values,
                      std::initializer_list<const char*> keys) {
  for (const char* key : keys) {
    const auto found = values.find(key);
    if (found != values.end() && IsHexColor(found->second)) {
      return found->second;
    }
  }
  return "";
}

int HexChannel(const std::string& hex, size_t index) {
  return static_cast<int>(std::strtol(hex.substr(1 + 2 * index, 2).c_str(), nullptr, 16));
}

/// `from` blended `amount` of the way towards `to`, as a hex colour.
std::string MixHex(const std::string& from, const std::string& to, double amount) {
  char buffer[8];
  int channels[3];
  for (size_t index = 0; index < 3; ++index) {
    const double value = HexChannel(from, index) * (1.0 - amount) + HexChannel(to, index) * amount;
    channels[index] = static_cast<int>(value + 0.5);
  }
  std::snprintf(buffer, sizeof buffer, "#%02x%02x%02x", channels[0], channels[1], channels[2]);
  return buffer;
}

/// A hex colour at an opacity, spelled out: GTK 4.16 deprecates alpha() and mix() in CSS.
std::string Rgba(const std::string& hex, double alpha) {
  char buffer[40];
  std::snprintf(buffer, sizeof buffer, "rgba(%d,%d,%d,%.2f)", HexChannel(hex, 0),
                HexChannel(hex, 1), HexChannel(hex, 2), alpha);
  return buffer;
}

bool LoadOmarchyPalette(const std::string& path, OmarchyPalette* palette) {
  const std::map<std::string, std::string> values = ReadColorsFile(path);
  palette->background = PickColor(values, {"background", "bg", "color0"});
  palette->foreground = PickColor(values, {"foreground", "fg", "color7"});
  if (palette->background.empty() || palette->foreground.empty()) {
    return false;  // not a palette: the windows keep the theme GTK gives them
  }
  palette->dark_background = PickColor(values, {"dark_background", "dark_bg"});
  if (palette->dark_background.empty()) {
    palette->dark_background = MixHex(palette->background, "#000000", 0.25);
  }
  palette->dark_foreground = PickColor(values, {"dark_foreground", "dark_fg", "color8"});
  if (palette->dark_foreground.empty()) {
    palette->dark_foreground = palette->foreground;
  }
  palette->bright_foreground = PickColor(values, {"bright_foreground", "bright_fg", "color15"});
  if (palette->bright_foreground.empty()) {
    palette->bright_foreground = palette->foreground;
  }
  palette->accent = PickColor(values, {"accent", "blue", "color4"});
  if (palette->accent.empty()) {
    palette->accent = palette->foreground;
  }
  palette->selection = PickColor(values, {"selection", "selection_background", "color8"});
  if (palette->selection.empty()) {
    palette->selection = MixHex(palette->background, palette->foreground, 0.2);
  }
  return true;
}

/// Hyprland's `decoration:rounding`, which the Omarchy shell mirrors as the corner radius
/// of every control (0 on a stock Omarchy: square). Asked once per process; hyprctl needs
/// the instance variable the session exports, and without it the answer is 0.
int HyprlandRounding() {
  static const int rounding = [] {
    if (g_getenv("HYPRLAND_INSTANCE_SIGNATURE") == nullptr) {
      return 0;
    }
    const gchar* argv[] = {"hyprctl", "getoption", "decoration:rounding", "-j", nullptr};
    gchar* output = nullptr;
    gint status = 0;
    const gboolean ran = g_spawn_sync(
        nullptr, const_cast<gchar**>(argv), nullptr,
        static_cast<GSpawnFlags>(G_SPAWN_SEARCH_PATH | G_SPAWN_STDERR_TO_DEV_NULL), nullptr,
        nullptr, &output, nullptr, &status, nullptr);
    int value = 0;
    if (ran && output != nullptr) {
      const char* key = std::strstr(output, "\"int\":");
      if (key != nullptr) {
        value = std::atoi(key + 6);
      }
    }
    g_free(output);
    return std::max(0, value);
  }();
  return rounding;
}

/// The palette as CSS for this module's windows and nothing else, in the language of the
/// Omarchy shell's own kit (/usr/share/omarchy/shell: Commons/Style.qml, Ui/Button.qml,
/// Ui/Toggle.qml, Ui/ToggleSwitch.qml, and the [controls] of shell.toml):
///
/// - everything is set in the `monospace` alias at 12 px (the shell's base size), a
///   heading at 16 px, a toggle's label at 13 px bold, dim text at 11 px;
/// - a control at rest is the foreground at 4 % over the window with a 1 px border at
///   40 %; under the pointer or the keyboard 8 % and 25 %; pressed 22 %. The chosen one
///   (the default button) is 18 % with its text in the accent, bold;
/// - corners follow Hyprland's rounding; a switch is a 42 × 22 track with a 16 px knob,
///   a pill when corners are round and square when they are not;
/// - there is no focus ring: the focus state is the control's own fill and border.
std::string OmarchyStylesheet(const OmarchyPalette& palette) {
  const std::string& bg = palette.background;
  const std::string& fg = palette.foreground;
  const std::string fill_normal = Rgba(fg, 0.04);
  const std::string fill_hover = Rgba(fg, 0.08);
  const std::string fill_selected = Rgba(fg, 0.18);
  const std::string fill_pressed = Rgba(fg, 0.22);
  const std::string border_normal = Rgba(fg, 0.4);
  const std::string border_hover = Rgba(fg, 0.25);
  const std::string hairline = Rgba(fg, 0.12);
  const std::string knob_off = MixHex(fg, "#000000", 0.2);  // Qt.darker(foreground, 1.25)
  const int rounding = HyprlandRounding();
  const std::string radius = std::to_string(rounding) + "px";
  const std::string switch_radius = rounding > 0 ? "11px" : "0px";
  const std::string knob_radius = rounding > 0 ? "8px" : "0px";
  const std::string w = "window.dazzle_updater ";
  const std::string hover = "{ background-color: " + fill_hover + "; border-color: " + border_hover + "; }\n";
  std::string css;
  css += "window.dazzle_updater { background-color: " + bg + "; color: " + fg +
         "; font-family: monospace; font-size: 12px; }\n";
  css += w + ".dazzle_updater-heading { color: " + palette.bright_foreground +
         "; font-size: 16px; font-weight: bold; }\n";
  css += w + ".dazzle_updater-status { color: " + palette.dark_foreground +
         "; opacity: 1; font-size: 11px; }\n";
  css += w + "*:focus-visible { outline: none; }\n";
  // The notes: a sunken view on dark_background with the control border, the theme's
  // selection colours inside it.
  css += w + ".dazzle_updater-notes { background-color: " + palette.dark_background +
         "; border: 1px solid " + border_normal + "; border-radius: " + radius + "; }\n";
  css += w + ".dazzle_updater-notes textview, " + w +
         ".dazzle_updater-notes textview text { background-color: transparent; color: " + fg +
         "; }\n";
  css += w + "textview text selection { background-color: " + palette.selection +
         "; color: " + palette.bright_foreground + "; }\n";
  // Buttons: 28 px tall (14 of content, 6 of padding and 1 of border each way), 10 px
  // of padding at the sides.
  css += w + "button { background-image: none; background-color: " + fill_normal + "; color: " + fg +
         "; border: 1px solid " + border_normal + "; border-radius: " + radius +
         "; box-shadow: none; text-shadow: none; padding: 6px 10px; min-height: 14px; "
         "font-weight: normal; }\n";
  css += w + "button:hover, " + w + "button:focus-visible " + hover;
  css += w + "button:active, " + w + "button:checked { background-color: " + fill_pressed + "; }\n";
  css += w + "button.suggested-action { background-color: " + fill_selected + "; color: " +
         palette.accent + "; font-weight: bold; }\n";
  css += w + "button.suggested-action:hover, " + w + "button.suggested-action:focus-visible " + hover;
  css += w + "button.suggested-action:active { background-color: " + fill_pressed + "; }\n";
  // The toggle row: a control of its own, the label in bold, the switch at its end.
  css += w + ".dazzle_updater-toggle { background-color: " + fill_normal + "; border: 1px solid " +
         border_normal + "; border-radius: " + radius + "; padding: 16px 12px; }\n";
  css += w + ".dazzle_updater-toggle:hover " + hover;
  css += w + ".dazzle_updater-toggle label { font-weight: bold; font-size: 13px; color: " + fg + "; }\n";
  css += w + "switch { background-image: none; background-color: " + fill_normal +
         "; border: 1px solid " + border_normal + "; border-radius: " + switch_radius +
         "; min-width: 40px; min-height: 20px; box-shadow: none; }\n";
  css += w + "switch:hover, " + w + "switch:focus-visible " + hover;
  css += w + "switch:checked { background-color: " + fill_selected + "; border-color: transparent; }\n";
  css += w + "switch slider { background-image: none; background-color: " + knob_off +
         "; border: none; box-shadow: none; border-radius: " + knob_radius +
         "; min-width: 16px; min-height: 16px; margin: 2px; }\n";
  css += w + "switch:checked slider { background-color: " + fg + "; }\n";
  css += w + "switch image { color: transparent; }\n";
  // Progress: the accent on the hairline track.
  css += w + "progressbar trough { background-color: " + hairline +
         "; border-color: transparent; border-radius: " + radius + "; }\n";
  css += w + "progressbar progress { background-color: " + palette.accent + "; border-color: " +
         palette.accent + "; border-radius: " + radius + "; }\n";
  css += w + "scrollbar slider { background-color: " + Rgba(fg, 0.35) + "; }\n";
  css += w + "scrollbar slider:hover { background-color: " + Rgba(fg, 0.5) + "; }\n";
  return css;
}

struct OmarchyStyles {
  GtkCssProvider* provider = nullptr;
  std::string css;   // what the provider holds; "" when the palette is not in use
  std::string link;  // the accent, for the notes' links
};

OmarchyStyles g_omarchy;

/// The palette file to read, or "" when the windows stay with the GTK theme.
std::string OmarchyColorsPath() {
  if (const char* forced = g_getenv("DAZZLE_OMARCHY_COLORS")) {
    return forced;
  }
  if (const char* desktop = g_getenv("XDG_CURRENT_DESKTOP")) {
    // "GNOME", "ubuntu:GNOME", "GNOME-Classic:GNOME": the theme reaches GTK by itself.
    std::string name = desktop;
    for (char& character : name) {
      character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
    }
    if (name.find("GNOME") != std::string::npos) {
      return "";
    }
  }
  std::string state_home;
  if (const char* xdg_state = g_getenv("XDG_STATE_HOME"); xdg_state != nullptr && *xdg_state) {
    state_home = xdg_state;
  } else {
    state_home = std::string(g_get_home_dir()) + "/.local/state";
  }
  const std::string candidates[] = {
      state_home + "/omarchy/current/theme/colors.toml",
      std::string(g_get_user_config_dir()) + "/omarchy/current/theme/colors.toml",
  };
  for (const std::string& candidate : candidates) {
    if (g_file_test(candidate.c_str(), G_FILE_TEST_IS_REGULAR)) {
      return candidate;
    }
  }
  return "";
}

/// Reads the palette whenever a window is drawn — the file is a few hundred bytes, and
/// omarchy-theme-set replaces the theme directory in place — and touches GTK only when it
/// changed, so a theme switched while the application runs reaches the next window.
void EnsureOmarchyStyles() {
  OmarchyPalette palette;
  std::string css;
  const std::string path = OmarchyColorsPath();
  if (!path.empty() && LoadOmarchyPalette(path, &palette)) {
    css = OmarchyStylesheet(palette);
  }
  if (css == g_omarchy.css) {
    return;
  }
  g_omarchy.css = css;
  g_omarchy.link = palette.accent;
  if (g_omarchy.provider == nullptr) {
    if (css.empty()) {
      return;
    }
    g_omarchy.provider = gtk_css_provider_new();
    // Above the module's own sheet, which sits at the application priority.
    gtk_style_context_add_provider_for_display(gdk_display_get_default(),
                                               GTK_STYLE_PROVIDER(g_omarchy.provider),
                                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1);
  }
#if GTK_CHECK_VERSION(4, 12, 0)
  gtk_css_provider_load_from_string(g_omarchy.provider, css.c_str());
#else
  gtk_css_provider_load_from_data(g_omarchy.provider, css.c_str(), -1);
#endif
}

/// Whether the windows are dressed from an Omarchy palette right now.
bool OmarchyActive() { return !g_omarchy.css.empty(); }

/// The colour the notes draw their links in, or nullptr for GTK's own.
const char* OmarchyLinkColor() { return g_omarchy.css.empty() ? nullptr : g_omarchy.link.c_str(); }

/// The application's own icon, as the icon theme knows it: the application id, then the
/// update icon the desktop themes carry, then a last generic one. The other two platforms
/// take the icon from the bundle or the executable; on Linux it lives in the theme.
const char* AppIconName() {
  static std::string name;
  if (!name.empty()) {
    return name.c_str();
  }
  GtkIconTheme* theme = gtk_icon_theme_get_for_display(gdk_display_get_default());
  GApplication* application = g_application_get_default();
  const char* id = application != nullptr ? g_application_get_application_id(application) : nullptr;
  if (id != nullptr && gtk_icon_theme_has_icon(theme, id)) {
    name = id;
    return name.c_str();
  }
  // What the application told GTK its windows' icon is (gtk_window_set_default_icon_name),
  // for a process without a GApplication.
  const char* default_name = gtk_window_get_default_icon_name();
  if (default_name != nullptr && gtk_icon_theme_has_icon(theme, default_name)) {
    name = default_name;
    return name.c_str();
  }
  for (const char* fallback :
       {"software-update-available", "system-software-update", "dialog-information"}) {
    if (gtk_icon_theme_has_icon(theme, fallback)) {
      name = fallback;
      return name.c_str();
    }
  }
  name = "dialog-information";
  return name.c_str();
}

GtkWidget* MakeIcon() {
  GtkWidget* image = gtk_image_new_from_icon_name(AppIconName());
  gtk_image_set_pixel_size(GTK_IMAGE(image), kIconSize);
  gtk_widget_set_size_request(image, kIconSize, kIconSize);
  gtk_widget_set_valign(image, GTK_ALIGN_START);
  return image;
}

/// A wrapping label's natural width is the whole text on one line, which would make the
/// window as wide as its longest sentence. GTK can cap it in characters, so the cap is
/// found by measuring: shrink the character cap until the measured natural width fits the
/// column. Measuring rather than guessing keeps Chinese wording (whose characters are
/// wider) from pushing the window open.
void CapLabelWidth(GtkWidget* label, int pixel_width) {
  if (pixel_width <= 0) {
    return;
  }
  int chars = 240;
  gtk_label_set_max_width_chars(GTK_LABEL(label), chars);
  for (int attempt = 0; attempt < 24; ++attempt) {
    int minimum = 0;
    int natural = 0;
    int minimum_baseline = 0;
    int natural_baseline = 0;
    gtk_widget_measure(label, GTK_ORIENTATION_HORIZONTAL, -1, &minimum, &natural,
                       &minimum_baseline, &natural_baseline);
    if (natural <= pixel_width || chars <= 6) {
      return;
    }
    // A little past the target, because the cap is what the natural width may be, and a
    // cap that lands exactly on the measured width still draws one line too wide.
    int next = chars * pixel_width / natural * 95 / 100;
    if (next >= chars) {
      next = chars - 1;
    }
    chars = next < 6 ? 6 : next;
    gtk_label_set_max_width_chars(GTK_LABEL(label), chars);
  }
}

GtkWidget* MakeLabel(const std::string& text, const char* css_class) {
  GtkWidget* label = gtk_label_new(text.c_str());
  gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
  gtk_label_set_wrap(GTK_LABEL(label), TRUE);
  if (css_class != nullptr) {
    gtk_widget_add_css_class(label, css_class);
  }
  return label;
}

/// Opens a notes link with whatever the desktop uses for that scheme. GtkUriLauncher is
/// GTK's own way in (GTK 4.10 and later); older GTK asks GIO directly.
void OpenUri(const std::string& url) {
#if GTK_CHECK_VERSION(4, 10, 0)
  GtkUriLauncher* launcher = gtk_uri_launcher_new(url.c_str());
  gtk_uri_launcher_launch(launcher, g_window.window != nullptr ? GTK_WINDOW(g_window.window) : nullptr,
                          nullptr, nullptr, nullptr);
  g_object_unref(launcher);
#else
  g_app_info_launch_default_for_uri(url.c_str(), nullptr, nullptr);
#endif
}

void OnNotesClicked(GtkGestureClick* gesture, int press_count, double x, double y,
                    gpointer user_data) {
  (void)gesture;
  (void)press_count;
  GtkWidget* view = GTK_WIDGET(user_data);
  GtkTextIter iter;
  if (!gtk_text_view_get_iter_at_location(GTK_TEXT_VIEW(view), &iter, static_cast<int>(x),
                                          static_cast<int>(y))) {
    return;
  }
  GSList* tags = gtk_text_iter_get_tags(&iter);
  for (GSList* item = tags; item != nullptr; item = item->next) {
    // The renderer stores each link's number on its tag; a tag without one is not a link.
    const gpointer value =
        g_object_get_data(G_OBJECT(item->data), "dazzle_updater-link-index");
    if (value == nullptr) {
      continue;
    }
    const int index = GPOINTER_TO_INT(value) - 1;
    if (index >= 0 && index < static_cast<int>(g_window.notes_urls.size())) {
      OpenUri(g_window.notes_urls[index]);
    }
    break;
  }
  g_slist_free(tags);
}

GtkWidget* MakeNotesBox(const UiContent& content) {
  GtkWidget* scroller = gtk_scrolled_window_new();
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller), GTK_POLICY_NEVER,
                                 GTK_POLICY_AUTOMATIC);
  gtk_widget_set_size_request(scroller, -1, kNotesMinHeight);
  // The text view's own natural height is how tall the notes are, which would make the
  // window as tall as the release notes; the spec gives the box 200 pt and scrolls the
  // rest, so the natural height is capped there.
  gtk_scrolled_window_set_max_content_height(GTK_SCROLLED_WINDOW(scroller), kNotesMinHeight);
  gtk_widget_set_vexpand(scroller, TRUE);
  gtk_widget_add_css_class(scroller, "dazzle_updater-notes");

  GtkWidget* view = gtk_text_view_new();
  gtk_text_view_set_editable(GTK_TEXT_VIEW(view), FALSE);
  gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(view), FALSE);
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view), GTK_WRAP_WORD_CHAR);
  gtk_text_view_set_left_margin(GTK_TEXT_VIEW(view), 10);
  gtk_text_view_set_right_margin(GTK_TEXT_VIEW(view), 10);
  gtk_text_view_set_top_margin(GTK_TEXT_VIEW(view), 8);
  gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(view), 8);

  g_window.notes_urls.clear();
  dazzle_updater::internal::RenderNotesInto(gtk_text_view_get_buffer(GTK_TEXT_VIEW(view)),
                                    content.notes_markdown, &g_window.notes_urls,
                                    OmarchyLinkColor());

  GtkGesture* click = gtk_gesture_click_new();
  gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click), GDK_BUTTON_PRIMARY);
  g_signal_connect(click, "released", G_CALLBACK(OnNotesClicked), view);
  gtk_widget_add_controller(view, GTK_EVENT_CONTROLLER(click));

  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), view);
  return scroller;
}

void OnButtonClicked(GtkButton* button, gpointer user_data) {
  (void)user_data;
  const int tag = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "dazzle_updater-event"));
  ReportEvent(static_cast<UiEventKind>(tag - 1));
}

void OnCheckboxToggled(GtkCheckButton* checkbox, gpointer user_data) {
  (void)user_data;
  if (g_window.updating_checkbox) {
    return;
  }
  ReportEvent(UiEventKind::kCheckboxChanged, gtk_check_button_get_active(checkbox) == TRUE);
}

void OnSwitchToggled(GObject* toggle, GParamSpec* property, gpointer user_data) {
  (void)property;
  (void)user_data;
  if (g_window.updating_checkbox) {
    return;
  }
  ReportEvent(UiEventKind::kCheckboxChanged, gtk_switch_get_active(GTK_SWITCH(toggle)) == TRUE);
}

/// A click anywhere on the toggle row flips its switch, as Omarchy's Toggle does; a
/// click on the switch itself is the switch's own and is left to it.
void OnToggleRowReleased(GtkGestureClick* gesture, int press_count, double x, double y,
                         gpointer user_data) {
  (void)press_count;
  (void)user_data;
  GtkWidget* row = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture));
  GtkWidget* hit = gtk_widget_pick(row, x, y, GTK_PICK_DEFAULT);
  if (g_window.checkbox == nullptr || !GTK_IS_SWITCH(g_window.checkbox) ||
      hit == g_window.checkbox ||
      (hit != nullptr && gtk_widget_is_ancestor(hit, g_window.checkbox))) {
    return;
  }
  gtk_switch_set_active(GTK_SWITCH(g_window.checkbox),
                        !gtk_switch_get_active(GTK_SWITCH(g_window.checkbox)));
}

/// Sets the value of whichever control holds it, without reporting it back.
void SetCheckboxActive(bool active) {
  if (g_window.checkbox == nullptr) {
    return;
  }
  g_window.updating_checkbox = true;
  if (GTK_IS_SWITCH(g_window.checkbox)) {
    gtk_switch_set_active(GTK_SWITCH(g_window.checkbox), active);
  } else {
    gtk_check_button_set_active(GTK_CHECK_BUTTON(g_window.checkbox), active);
  }
  g_window.updating_checkbox = false;
}

/// The boolean option of the Available window and the permission prompt. Returns the
/// widget to place and leaves the control that holds the value in g_window.checkbox:
/// GTK's own check button, or — under an Omarchy palette, whose kit has no check box —
/// a Toggle row (shell/Ui/Toggle.qml): the label in bold at the left, a switch at the
/// right, the row drawn as a control and clickable anywhere.
GtkWidget* MakeCheckbox(const UiCheckbox& checkbox) {
  if (OmarchyActive()) {
    GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, kGap);
    gtk_widget_add_css_class(row, "dazzle_updater-toggle");
    GtkWidget* label = gtk_label_new(checkbox.label.c_str());
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(label), TRUE);
    gtk_widget_set_hexpand(label, TRUE);
    gtk_widget_set_valign(label, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(row), label);
    GtkWidget* toggle = gtk_switch_new();
    gtk_widget_set_valign(toggle, GTK_ALIGN_CENTER);
    g_window.checkbox = toggle;
    SetCheckboxActive(checkbox.checked);
    g_signal_connect(toggle, "notify::active", G_CALLBACK(OnSwitchToggled), nullptr);
    gtk_box_append(GTK_BOX(row), toggle);
    GtkGesture* click = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click), GDK_BUTTON_PRIMARY);
    g_signal_connect(click, "released", G_CALLBACK(OnToggleRowReleased), nullptr);
    gtk_widget_add_controller(row, GTK_EVENT_CONTROLLER(click));
    return row;
  }
  GtkWidget* widget = gtk_check_button_new_with_label(checkbox.label.c_str());
  g_window.checkbox = widget;
  SetCheckboxActive(checkbox.checked);
  g_signal_connect(widget, "toggled", G_CALLBACK(OnCheckboxToggled), nullptr);
  gtk_widget_set_halign(widget, GTK_ALIGN_START);
  return widget;
}

/// The state's action row, at the trailing edge — except Skip This Version, which sits at
/// the leading edge on its own ("跳过此版本 ··· 以后再说 · 安装更新", as in SUUpdateAlert and
/// the prototype), with Later and Install equal in width. The default button is the
/// window's default widget, so Return presses it; the window's own Escape handling reports
/// the same event a close does, which is what the spec asks of both.
GtkWidget* MakeButtons(const UiContent& content) {
  bool has_skip = false;
  for (const UiButton& button : content.buttons) {
    has_skip = has_skip || button.event == UiEventKind::kSkip;
  }
  GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, kGap);
  gtk_widget_set_halign(row, has_skip ? GTK_ALIGN_FILL : GTK_ALIGN_END);
  gtk_widget_add_css_class(row, "dazzle_updater-buttons");
  GtkWidget* trailing = row;
  if (has_skip) {
    trailing = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, kGap);
    gtk_box_set_homogeneous(GTK_BOX(trailing), TRUE);
    gtk_widget_set_halign(trailing, GTK_ALIGN_END);
    gtk_widget_set_hexpand(trailing, TRUE);
  }
  for (const UiButton& button : content.buttons) {
    GtkWidget* widget = gtk_button_new_with_label(button.label.c_str());
    gtk_widget_set_size_request(widget, kButtonMinWidth, -1);
    g_object_set_data(G_OBJECT(widget), "dazzle_updater-event",
                      GINT_TO_POINTER(static_cast<int>(button.event) + 1));
    g_signal_connect(widget, "clicked", G_CALLBACK(OnButtonClicked), nullptr);
    if (button.is_default) {
      gtk_widget_add_css_class(widget, "suggested-action");
      gtk_window_set_default_widget(GTK_WINDOW(g_window.window), widget);
      g_window.default_button = widget;
    }
    gtk_box_append(GTK_BOX(button.event == UiEventKind::kSkip ? row : trailing), widget);
  }
  if (trailing != row) {
    gtk_box_append(GTK_BOX(row), trailing);
  }
  return row;
}

gboolean OnPulse(gpointer user_data) {
  (void)user_data;
  if (g_window.progress == nullptr) {
    g_window.pulse_source = 0;
    return G_SOURCE_REMOVE;
  }
  gtk_progress_bar_pulse(GTK_PROGRESS_BAR(g_window.progress));
  return G_SOURCE_CONTINUE;
}

void SyncProgress(const UiProgress& progress) {
  if (g_window.progress == nullptr) {
    return;
  }
  if (progress.determinate) {
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(g_window.progress), progress.fraction);
    if (g_window.pulse_source != 0) {
      g_source_remove(g_window.pulse_source);
      g_window.pulse_source = 0;
    }
  } else {
    gtk_progress_bar_pulse(GTK_PROGRESS_BAR(g_window.progress));
    if (g_window.pulse_source == 0) {
      g_window.pulse_source = g_timeout_add(kPulseIntervalMs, OnPulse, nullptr);
    }
  }
}

/// The UpdateAvailableWindow: the icon and the version lines, the notes, the automatic
/// downloads check box and the state's buttons.
GtkWidget* MakeAvailableBody(const UiContent& content) {
  GtkWidget* root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_size_request(root, kAvailableWidth, -1);
  gtk_widget_set_margin_start(root, kMargin);
  gtk_widget_set_margin_end(root, kMargin);
  gtk_widget_set_margin_top(root, kMargin);
  gtk_widget_set_margin_bottom(root, kMargin);

  GtkWidget* title_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, kIconGap);
  gtk_box_append(GTK_BOX(title_row), MakeIcon());
  GtkWidget* text_column = gtk_box_new(GTK_ORIENTATION_VERTICAL, kTextGap);
  gtk_widget_set_hexpand(text_column, TRUE);
  gtk_widget_set_valign(text_column, GTK_ALIGN_START);
  g_window.heading = MakeLabel(content.heading, "dazzle_updater-heading");
  g_window.detail = MakeLabel(content.detail, "dazzle_updater-detail");
  CapLabelWidth(g_window.heading, kAvailableTextWidth);
  CapLabelWidth(g_window.detail, kAvailableTextWidth);
  gtk_box_append(GTK_BOX(text_column), g_window.heading);
  gtk_box_append(GTK_BOX(text_column), g_window.detail);
  gtk_box_append(GTK_BOX(title_row), text_column);
  gtk_box_append(GTK_BOX(root), title_row);

  if (!content.notes_markdown.empty()) {
    GtkWidget* notes = MakeNotesBox(content);
    gtk_widget_set_margin_top(notes, kGap);
    gtk_box_append(GTK_BOX(root), notes);
  } else if (!content.notes_link_label.empty()) {
    // No notes, but a page that has them: one link, nothing to scroll.
    GtkWidget* link = gtk_link_button_new_with_label(content.notes_url.c_str(),
                                                     content.notes_link_label.c_str());
    gtk_widget_set_halign(link, GTK_ALIGN_START);
    gtk_widget_set_margin_top(link, kGap);
    gtk_box_append(GTK_BOX(root), link);
  }

  if (content.checkbox.visible) {
    GtkWidget* option = MakeCheckbox(content.checkbox);
    gtk_widget_set_margin_top(option, kGap);
    gtk_box_append(GTK_BOX(root), option);
  }

  GtkWidget* buttons = MakeButtons(content);
  gtk_widget_set_margin_top(buttons, kGap);
  gtk_box_append(GTK_BOX(root), buttons);
  return root;
}

/// The UpdateStatusWindow: the icon, one line of text or a progress bar, the state's
/// buttons, and the progress's own status text on the buttons' line.
GtkWidget* MakeStatusBody(const UiContent& content) {
  GtkWidget* root = gtk_box_new(GTK_ORIENTATION_VERTICAL, kGap);
  gtk_widget_set_size_request(root, kStatusWidth, -1);
  gtk_widget_set_margin_start(root, kMargin);
  gtk_widget_set_margin_end(root, kMargin);
  gtk_widget_set_margin_top(root, kMargin);
  gtk_widget_set_margin_bottom(root, kMargin);

  GtkWidget* body_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, kIconGap);
  gtk_box_append(GTK_BOX(body_row), MakeIcon());
  GtkWidget* text_column = gtk_box_new(GTK_ORIENTATION_VERTICAL, kTextGap);
  gtk_widget_set_hexpand(text_column, TRUE);
  gtk_widget_set_valign(text_column, GTK_ALIGN_CENTER);
  g_window.heading = MakeLabel(content.heading, "dazzle_updater-heading");
  CapLabelWidth(g_window.heading, kStatusTextWidth);
  gtk_box_append(GTK_BOX(text_column), g_window.heading);
  if (content.progress.visible) {
    g_window.progress = gtk_progress_bar_new();
    gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(g_window.progress), FALSE);
    SyncProgress(content.progress);
    gtk_box_append(GTK_BOX(text_column), g_window.progress);
  } else {
    g_window.detail = MakeLabel(content.detail, "dazzle_updater-detail");
    CapLabelWidth(g_window.detail, kStatusTextWidth);
    gtk_box_append(GTK_BOX(text_column), g_window.detail);
  }
  gtk_box_append(GTK_BOX(body_row), text_column);
  gtk_box_append(GTK_BOX(root), body_row);

  GtkWidget* footer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, kGap);
  g_window.status = MakeLabel(content.progress.status_text, "dazzle_updater-status");
  gtk_label_set_wrap(GTK_LABEL(g_window.status), FALSE);
  gtk_label_set_selectable(GTK_LABEL(g_window.status), FALSE);
  gtk_widget_set_valign(g_window.status, GTK_ALIGN_CENTER);
  gtk_widget_set_hexpand(g_window.status, TRUE);
  gtk_box_append(GTK_BOX(footer), g_window.status);
  gtk_box_append(GTK_BOX(footer), MakeButtons(content));
  gtk_box_append(GTK_BOX(root), footer);

  if (content.error_message.empty()) {
    return root;
  }
  // The raw message goes under the footer, in the small monospace the spec asks for, so
  // a user can copy it into a bug report.
  GtkWidget* message = MakeLabel(content.error_message, "dazzle_updater-status");
  gtk_widget_add_css_class(message, "monospace");
  CapLabelWidth(message, kStatusBodyWidth);
  gtk_box_append(GTK_BOX(root), message);
  return root;
}

/// The UpdatePermissionWindow: the one-time question, its check box and its two answers.
GtkWidget* MakePermissionBody(const UiContent& content) {
  GtkWidget* root = gtk_box_new(GTK_ORIENTATION_VERTICAL, kGap);
  gtk_widget_set_size_request(root, kPermissionWidth, -1);
  gtk_widget_set_margin_start(root, kMargin);
  gtk_widget_set_margin_end(root, kMargin);
  gtk_widget_set_margin_top(root, kMargin);
  gtk_widget_set_margin_bottom(root, kMargin);

  GtkWidget* title_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
  gtk_box_append(GTK_BOX(title_row), MakeIcon());
  GtkWidget* text_column = gtk_box_new(GTK_ORIENTATION_VERTICAL, kTextGap);
  gtk_widget_set_hexpand(text_column, TRUE);
  gtk_widget_set_valign(text_column, GTK_ALIGN_CENTER);
  g_window.heading = MakeLabel(content.heading, "dazzle_updater-heading");
  g_window.detail = MakeLabel(content.detail, "dazzle_updater-detail");
  CapLabelWidth(g_window.heading, kPermissionTextWidth);
  CapLabelWidth(g_window.detail, kPermissionTextWidth);
  gtk_box_append(GTK_BOX(text_column), g_window.heading);
  gtk_box_append(GTK_BOX(text_column), g_window.detail);
  gtk_box_append(GTK_BOX(title_row), text_column);
  gtk_box_append(GTK_BOX(root), title_row);

  if (content.checkbox.visible) {
    gtk_box_append(GTK_BOX(root), MakeCheckbox(content.checkbox));
  }

  gtk_box_append(GTK_BOX(root), MakeButtons(content));
  return root;
}

gboolean OnCloseRequest(GtkWindow* window, gpointer user_data) {
  (void)window;
  (void)user_data;
  // The state machine decides what a close means in this state; Hide() closes the window.
  ReportEvent(UiEventKind::kClose);
  return TRUE;
}

gboolean OnKeyPressed(GtkEventControllerKey* controller, guint keyval, guint keycode,
                      GdkModifierType state, gpointer user_data) {
  (void)controller;
  (void)keycode;
  (void)state;
  (void)user_data;
  if (keyval != GDK_KEY_Escape) {
    return FALSE;
  }
  // Esc is Later, Cancel or Don't Check; a close is what the state machine reads as all
  // three (spec: "Esc 与关闭窗口等同于").
  ReportEvent(UiEventKind::kClose);
  return TRUE;
}

void SyncContent(const UiContent& content) {
  if (g_window.heading != nullptr) {
    gtk_label_set_text(GTK_LABEL(g_window.heading), content.heading.c_str());
  }
  if (g_window.detail != nullptr) {
    gtk_label_set_text(GTK_LABEL(g_window.detail), content.detail.c_str());
  }
  if (g_window.status != nullptr) {
    gtk_label_set_text(GTK_LABEL(g_window.status), content.progress.status_text.c_str());
  }
  if (g_window.progress != nullptr) {
    SyncProgress(content.progress);
  }
  SetCheckboxActive(content.checkbox.checked);
}

void ForgetWidgets() {
  g_window.default_button = nullptr;
  g_window.heading = nullptr;
  g_window.detail = nullptr;
  g_window.status = nullptr;
  g_window.progress = nullptr;
  g_window.checkbox = nullptr;
  g_window.notes_urls.clear();
}

/// Gives the keyboard focus to the state's default button, so Return presses it — the
/// spec's "粗体为默认按钮（回车触发）". Without this, GTK focuses the first control that
/// takes focus, and Return would press that one instead: in the Ready window, "Later".
void FocusDefaultButton() {
  if (g_window.default_button != nullptr) {
    gtk_widget_grab_focus(g_window.default_button);
  }
}

/// Asks a fixed window for its size once more, a moment after its content changed. A fixed
/// window is as tall as its content, and new content may be taller (an error with its
/// message under the buttons); but when the content changes right after the window was
/// presented — a check that fails at once turns Checking into Error — the display server's
/// answer to the first size arrives after the second request and wins, and the message is
/// clipped. Asking again once that answer is in settles on the content's own height.
void ResizeAgainSoon() {
  GtkWidget* window = g_window.window;
  if (window == nullptr || gtk_window_get_resizable(GTK_WINDOW(window))) {
    return;
  }
  g_timeout_add_full(
      G_PRIORITY_DEFAULT, 150,
      [](gpointer data) -> gboolean {
        GtkWidget* target = GTK_WIDGET(data);
        if (target == g_window.window) {
          gtk_widget_queue_resize(target);
        }
        return G_SOURCE_REMOVE;
      },
      g_object_ref(window), g_object_unref);
}

/// The content widget for `content`'s window kind. The window it will be put into must
/// already be g_window.window: the default button registers itself on it as it is built.
GtkWidget* BuildBody(const UiContent& content) {
  switch (content.window) {
    case UiWindow::kUpdateAvailable:
      return MakeAvailableBody(content);
    case UiWindow::kUpdatePermission:
      return MakePermissionBody(content);
    case UiWindow::kUpdateStatus:
    case UiWindow::kNone:
      break;
  }
  return MakeStatusBody(content);
}

/// Draws `content`; runs on the main thread.
void DrawOnMain(const UiContent& content) {
  EnsureStyles();
  EnsureOmarchyStyles();

  const bool have_window = g_window.window != nullptr;
  const bool same_window =
      have_window && IsUpdateWindow(content.window) && g_window.kind == content.window;

  if (same_window && g_window.state == content.state) {
    // The same state again: a progress tick, or a first frame. Only the values change, so
    // the window is not rebuilt (spec: 同一窗口内的状态转换只更新窗口内容).
    SyncContent(content);
    return;
  }

  if (g_window.pulse_source != 0) {
    g_source_remove(g_window.pulse_source);
    g_window.pulse_source = 0;
  }
  ForgetWidgets();

  if (same_window) {
    gtk_window_set_default_widget(GTK_WINDOW(g_window.window), nullptr);
    GtkWidget* body = BuildBody(content);
    gtk_window_set_child(GTK_WINDOW(g_window.window), body);
    gtk_window_set_title(GTK_WINDOW(g_window.window), content.window_title.c_str());
    g_window.state = content.state;
    gtk_window_present(GTK_WINDOW(g_window.window));
    FocusDefaultButton();
    ResizeAgainSoon();
    return;
  }

  // A different window: the old one stays until the new one is up, so the flow moves
  // without a moment with nothing on screen.
  GtkWidget* window = gtk_window_new();
  // The class the module's own CSS is scoped to, so it never reaches the host's windows.
  gtk_widget_add_css_class(window, "dazzle_updater");
  gtk_window_set_title(GTK_WINDOW(window), content.window_title.c_str());
  if (OmarchyActive()) {
    // Omarchy's windows have no title bars: Hyprland draws a border and nothing else, and
    // GTK gives its header bar up when the compositor asks for server-side decorations —
    // unless a title bar widget is forced onto the window. So none is: the window is a
    // bare panel like every other window on that desktop, closed with the compositor's
    // own key, and the title stays with the window for the window list.
  } else {
    GtkWidget* header = gtk_header_bar_new();
    gtk_window_set_titlebar(GTK_WINDOW(window), header);
    if (content.window != UiWindow::kUpdateStatus) {
      // The update and permission windows keep their title for the window list, but do
      // not draw it — the prototype hides it for exactly these two.
      gtk_header_bar_set_title_widget(GTK_HEADER_BAR(header), gtk_label_new(""));
    }
    if (content.window == UiWindow::kUpdatePermission) {
      // The question must be answered: no close button.
      gtk_header_bar_set_show_title_buttons(GTK_HEADER_BAR(header), FALSE);
    }
  }
  if (content.window == UiWindow::kUpdatePermission) {
    gtk_window_set_deletable(GTK_WINDOW(window), FALSE);  // and no way to dismiss it
  }
  gtk_window_set_resizable(GTK_WINDOW(window), content.resizable);
  if (content.window == UiWindow::kUpdateAvailable && content.resizable) {
    // The spec's own opening size: 532 × 370 of content, and never narrower than 462.
    // Only with notes: without them the window is fixed and as tall as what it shows (a
    // default size would hold it open at 370 around a link and a check box).
    gtk_window_set_default_size(GTK_WINDOW(window), kAvailableWidth, kAvailableHeight);
    gtk_widget_set_size_request(window, kAvailableMinWidth, kAvailableMinHeight);
  }
  g_signal_connect(window, "close-request", G_CALLBACK(OnCloseRequest), nullptr);
  GtkEventController* keys = gtk_event_controller_key_new();
  g_signal_connect(keys, "key-pressed", G_CALLBACK(OnKeyPressed), nullptr);
  gtk_widget_add_controller(window, keys);

  // A dialog of the application's own window where there is one: it stays above that
  // window, and a tiling compositor (Hyprland, which Omarchy runs) floats it instead of
  // tiling it into a screen's worth of release notes. The window being replaced is not a
  // parent: the new one inherits its parent instead.
  GtkWidget* previous = g_window.window;
  if (GApplication* application = g_application_get_default();
      application != nullptr && GTK_IS_APPLICATION(application)) {
    GtkWindow* parent = gtk_application_get_active_window(GTK_APPLICATION(application));
    if (previous != nullptr && parent == GTK_WINDOW(previous)) {
      parent = gtk_window_get_transient_for(GTK_WINDOW(previous));
    }
    if (parent != nullptr) {
      gtk_window_set_transient_for(GTK_WINDOW(window), parent);
    }
  }

  // The window is set before its content is built: the default button is registered on
  // it as it is created.
  g_window.window = window;
  g_window.kind = content.window;
  g_window.state = content.state;
  gtk_window_set_child(GTK_WINDOW(window), BuildBody(content));
  gtk_window_present(GTK_WINDOW(window));
  FocusDefaultButton();
  if (previous != nullptr) {
    gtk_window_set_default_widget(GTK_WINDOW(previous), nullptr);
    gtk_window_destroy(GTK_WINDOW(previous));
  }
}

void HideOnMain() {
  if (g_window.pulse_source != 0) {
    g_source_remove(g_window.pulse_source);
    g_window.pulse_source = 0;
  }
  GtkWidget* window = g_window.window;
  g_window.window = nullptr;
  g_window.kind = UiWindow::kNone;
  ForgetWidgets();
  if (window != nullptr) {
    gtk_window_set_default_widget(GTK_WINDOW(window), nullptr);
    gtk_window_destroy(GTK_WINDOW(window));
  }
}

void PresentOnMain() {
  if (g_window.window != nullptr) {
    gtk_window_present(GTK_WINDOW(g_window.window));
  }
}

void QuitOnMain() {
  GtkWidget* ours = g_window.window;
  HideOnMain();

  // The application's own way out, so it can save what it has to save before the
  // installer replaces the directory (spec: 退出应用). An application object quits
  // through GApplication, which is what a GTK application's shutdown handlers hang off.
  GApplication* application = g_application_get_default();
  if (application != nullptr) {
    g_application_quit(application);
    return;
  }
  // Without one, closing the application's windows is the normal way out: gtk_main()
  // returns once the last toplevel is gone.
  GListModel* toplevels = gtk_window_get_toplevels();
  GPtrArray* windows = g_ptr_array_new_with_free_func(g_object_unref);
  const guint count = g_list_model_get_n_items(toplevels);
  for (guint index = 0; index < count; ++index) {
    GtkWindow* window = GTK_WINDOW(g_list_model_get_item(toplevels, index));
    if (window != nullptr && GTK_WIDGET(window) != ours) {
      g_ptr_array_add(windows, window);
    } else if (window != nullptr) {
      g_object_unref(window);
    }
  }
  for (guint index = 0; index < windows->len; ++index) {
    gtk_window_close(GTK_WINDOW(g_ptr_array_index(windows, index)));
  }
  g_ptr_array_unref(windows);
}

}  // namespace

// --- the backend ----------------------------------------------------------------------

namespace dazzle_updater {
namespace internal {
namespace {

class LinuxUiBackend final : public UiBackend {
 public:
  bool IsAvailable() const override {
    // The windows are drawn on the application's own GTK main loop, so there is nothing
    // to show until that application has initialized GTK and opened a display.
    return gtk_is_initialized() != FALSE && gdk_display_get_default() != nullptr;
  }

  void Draw(const UiContent& content) override {
    const UiContent copy = content;
    OnMainThread([copy] { DrawOnMain(copy); });
  }

  void Hide() override { OnMainThread([] { HideOnMain(); }); }

  void BringToFront() override { OnMainThread([] { PresentOnMain(); }); }

  void QuitApplication() override { OnMainThread([] { QuitOnMain(); }); }

  void SetEventSink(std::function<void(const UiEvent&)> sink) override {
    g_sink = std::move(sink);
  }
};

}  // namespace

UiBackend& GetUiBackend() {
  static LinuxUiBackend backend;
  return backend;
}

}  // namespace internal
}  // namespace dazzle_updater
