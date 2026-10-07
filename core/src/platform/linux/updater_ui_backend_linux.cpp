#include "../../foundation/updater_ui_backend.h"
#include "../../updater_ui_state.h"
#include "updater_ui_notes_linux.h"

#include <gtk/gtk.h>

#include <cstdlib>
#include <functional>
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

using dazzle::internal::IsUpdateWindow;
using dazzle::internal::UiButton;
using dazzle::internal::UiCheckbox;
using dazzle::internal::UiContent;
using dazzle::internal::UiEvent;
using dazzle::internal::UiEventKind;
using dazzle::internal::UiProgress;
using dazzle::internal::UiState;
using dazzle::internal::UiWindow;

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
    ".dazzle-heading { font-weight: bold; }\n"
    ".dazzle-detail { }\n"
    ".dazzle-status { font-size: 11px; opacity: 0.65; }\n"
    ".dazzle-notes { border: 1px solid alpha(currentColor, 0.15); border-radius: 6px; }\n"
    ".dazzle-notes text { background-color: transparent; }\n"
    ".dazzle-buttons button { min-width: 100px; }\n";

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
        g_object_get_data(G_OBJECT(item->data), "dazzle-link-index");
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
  gtk_widget_add_css_class(scroller, "dazzle-notes");

  GtkWidget* view = gtk_text_view_new();
  gtk_text_view_set_editable(GTK_TEXT_VIEW(view), FALSE);
  gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(view), FALSE);
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view), GTK_WRAP_WORD_CHAR);
  gtk_text_view_set_left_margin(GTK_TEXT_VIEW(view), 10);
  gtk_text_view_set_right_margin(GTK_TEXT_VIEW(view), 10);
  gtk_text_view_set_top_margin(GTK_TEXT_VIEW(view), 8);
  gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(view), 8);

  g_window.notes_urls.clear();
  dazzle::internal::RenderNotesInto(gtk_text_view_get_buffer(GTK_TEXT_VIEW(view)),
                                    content.notes_markdown, &g_window.notes_urls);

  GtkGesture* click = gtk_gesture_click_new();
  gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click), GDK_BUTTON_PRIMARY);
  g_signal_connect(click, "released", G_CALLBACK(OnNotesClicked), view);
  gtk_widget_add_controller(view, GTK_EVENT_CONTROLLER(click));

  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), view);
  return scroller;
}

void OnButtonClicked(GtkButton* button, gpointer user_data) {
  (void)user_data;
  const int tag = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "dazzle-event"));
  ReportEvent(static_cast<UiEventKind>(tag - 1));
}

void OnCheckboxToggled(GtkCheckButton* checkbox, gpointer user_data) {
  (void)user_data;
  if (g_window.updating_checkbox) {
    return;
  }
  ReportEvent(UiEventKind::kCheckboxChanged, gtk_check_button_get_active(checkbox) == TRUE);
}

GtkWidget* MakeCheckbox(const UiCheckbox& checkbox) {
  GtkWidget* widget = gtk_check_button_new_with_label(checkbox.label.c_str());
  g_window.updating_checkbox = true;
  gtk_check_button_set_active(GTK_CHECK_BUTTON(widget), checkbox.checked);
  g_window.updating_checkbox = false;
  g_signal_connect(widget, "toggled", G_CALLBACK(OnCheckboxToggled), nullptr);
  gtk_widget_set_halign(widget, GTK_ALIGN_START);
  return widget;
}

/// The state's action row, at the trailing edge. The default button is the window's
/// default widget, so Return presses it; the window's own Escape handling reports the
/// same event a close does, which is what the spec asks of both.
GtkWidget* MakeButtons(const UiContent& content) {
  GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, kGap);
  gtk_widget_set_halign(row, GTK_ALIGN_END);
  gtk_widget_add_css_class(row, "dazzle-buttons");
  for (const UiButton& button : content.buttons) {
    GtkWidget* widget = gtk_button_new_with_label(button.label.c_str());
    gtk_widget_set_size_request(widget, kButtonMinWidth, -1);
    g_object_set_data(G_OBJECT(widget), "dazzle-event",
                      GINT_TO_POINTER(static_cast<int>(button.event) + 1));
    g_signal_connect(widget, "clicked", G_CALLBACK(OnButtonClicked), nullptr);
    if (button.is_default) {
      gtk_widget_add_css_class(widget, "suggested-action");
      gtk_window_set_default_widget(GTK_WINDOW(g_window.window), widget);
      g_window.default_button = widget;
    }
    gtk_box_append(GTK_BOX(row), widget);
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
  g_window.heading = MakeLabel(content.heading, "dazzle-heading");
  g_window.detail = MakeLabel(content.detail, "dazzle-detail");
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
    g_window.checkbox = MakeCheckbox(content.checkbox);
    gtk_widget_set_margin_top(g_window.checkbox, kGap);
    gtk_box_append(GTK_BOX(root), g_window.checkbox);
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
  g_window.heading = MakeLabel(content.heading, "dazzle-heading");
  CapLabelWidth(g_window.heading, kStatusTextWidth);
  gtk_box_append(GTK_BOX(text_column), g_window.heading);
  if (content.progress.visible) {
    g_window.progress = gtk_progress_bar_new();
    gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(g_window.progress), FALSE);
    SyncProgress(content.progress);
    gtk_box_append(GTK_BOX(text_column), g_window.progress);
  } else {
    g_window.detail = MakeLabel(content.detail, "dazzle-detail");
    CapLabelWidth(g_window.detail, kStatusTextWidth);
    gtk_box_append(GTK_BOX(text_column), g_window.detail);
  }
  gtk_box_append(GTK_BOX(body_row), text_column);
  gtk_box_append(GTK_BOX(root), body_row);

  GtkWidget* footer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, kGap);
  g_window.status = MakeLabel(content.progress.status_text, "dazzle-status");
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
  GtkWidget* message = MakeLabel(content.error_message, "dazzle-status");
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
  g_window.heading = MakeLabel(content.heading, "dazzle-heading");
  g_window.detail = MakeLabel(content.detail, "dazzle-detail");
  CapLabelWidth(g_window.heading, kPermissionTextWidth);
  CapLabelWidth(g_window.detail, kPermissionTextWidth);
  gtk_box_append(GTK_BOX(text_column), g_window.heading);
  gtk_box_append(GTK_BOX(text_column), g_window.detail);
  gtk_box_append(GTK_BOX(title_row), text_column);
  gtk_box_append(GTK_BOX(root), title_row);

  if (content.checkbox.visible) {
    g_window.checkbox = MakeCheckbox(content.checkbox);
    gtk_box_append(GTK_BOX(root), g_window.checkbox);
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
  if (g_window.checkbox != nullptr) {
    g_window.updating_checkbox = true;
    gtk_check_button_set_active(GTK_CHECK_BUTTON(g_window.checkbox), content.checkbox.checked);
    g_window.updating_checkbox = false;
  }
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
    return;
  }

  // A different window: the old one stays until the new one is up, so the flow moves
  // without a moment with nothing on screen.
  GtkWidget* window = gtk_window_new();
  gtk_window_set_title(GTK_WINDOW(window), content.window_title.c_str());
  GtkWidget* header = gtk_header_bar_new();
  gtk_window_set_titlebar(GTK_WINDOW(window), header);
  if (content.window != UiWindow::kUpdateStatus) {
    // The update and permission windows keep their title for the window list, but do not
    // draw it — the prototype hides it for exactly these two.
    gtk_header_bar_set_title_widget(GTK_HEADER_BAR(header), gtk_label_new(""));
  }
  if (content.window == UiWindow::kUpdatePermission) {
    // The question must be answered: no close button, and no way to dismiss it.
    gtk_header_bar_set_show_title_buttons(GTK_HEADER_BAR(header), FALSE);
    gtk_window_set_deletable(GTK_WINDOW(window), FALSE);
  }
  gtk_window_set_resizable(GTK_WINDOW(window), content.resizable);
  if (content.window == UiWindow::kUpdateAvailable) {
    // The spec's own opening size: 532 × 370 of content, and never narrower than 462.
    gtk_window_set_default_size(GTK_WINDOW(window), kAvailableWidth, kAvailableHeight);
    gtk_widget_set_size_request(window, kAvailableMinWidth, kAvailableMinHeight);
  }
  g_signal_connect(window, "close-request", G_CALLBACK(OnCloseRequest), nullptr);
  GtkEventController* keys = gtk_event_controller_key_new();
  g_signal_connect(keys, "key-pressed", G_CALLBACK(OnKeyPressed), nullptr);
  gtk_widget_add_controller(window, keys);

  // The window is set before its content is built: the default button is registered on
  // it as it is created.
  GtkWidget* previous = g_window.window;
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

namespace dazzle {
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
}  // namespace dazzle
