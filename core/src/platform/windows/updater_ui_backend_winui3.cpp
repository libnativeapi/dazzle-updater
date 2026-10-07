// The Windows windows: WinUI 3 (Windows App SDK), driven by the cross-platform state
// machine. The state machine decides the state and builds the content
// (updater_ui_content.cpp, updater_ui_strings.cpp — the localized wording included); this
// file owns three windows and draws that content in them, exactly as
// platform/macos/updater_ui_backend_macos.mm does with AppKit and
// platform/linux/updater_ui_backend_linux.cpp does with GTK 4.
//
// The thread the windows belong to is the host's: XAML in a process belongs to the thread
// that started the Application, and building a window anywhere else answers
// RPC_E_WRONG_THREAD — or, for an object reached across the C ABI, an access violation.
// So the backend draws on the calling thread when that thread can hold windows, keeps what
// it cannot draw until the host pumps (UpdaterUI::Pump, from its own loop), and says so in
// its log rather than pretending a window exists.
//
// The geometry is the one specs/updater-ui.md tabulates for Windows: the prototype's
// Windows tokens — 8 pt gaps, 32 pt Fluent buttons, a 20 pt check box, 4 pt corners, the
// system accent colour — with Sparkle's content sizes (532 × 370 for the update window,
// 400 for the status window, 437 for the permission prompt).

#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Graphics.h>
#include <winrt/Windows.UI.h>
#include <winrt/Windows.UI.Text.h>
#include <winrt/Windows.UI.ViewManagement.h>
#include <winrt/Microsoft.UI.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.Documents.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>

#include <windows.h>

#include "../../foundation/updater_ui_backend.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace dazzle {
namespace internal {
namespace {

namespace xaml = winrt::Microsoft::UI::Xaml;
namespace controls = winrt::Microsoft::UI::Xaml::Controls;
namespace documents = winrt::Microsoft::UI::Xaml::Documents;
namespace dispatching = winrt::Microsoft::UI::Dispatching;
namespace media = winrt::Microsoft::UI::Xaml::Media;
namespace viewmanagement = winrt::Windows::UI::ViewManagement;
namespace windowing = winrt::Microsoft::UI::Windowing;
using winrt::Windows::UI::Text::FontStyle;
using winrt::Windows::UI::Text::FontWeights;

/// The Windows metrics: the prototype's Windows tokens and the sizes the spec gives.
constexpr double kMargin = 20.0;
constexpr double kIconSize = 64.0;
constexpr double kGap = 8.0;
constexpr double kButtonGap = 8.0;   // Windows: 8 (macOS: 12)
constexpr double kMinButtonWidth = 120.0;
constexpr double kNotesRadius = 4.0;
constexpr double kNotesMinHeight = 200.0;
constexpr double kCheckboxRow = 20.0;
constexpr double kButtonHeight = 32.0;
constexpr double kButtonRadius = 4.0;
constexpr double kBodySize = 14.0;
constexpr double kCaptionSize = 12.0;
constexpr double kTitleBarHeight = 32.0;
constexpr double kUpdateWindowWidth = 532.0;
constexpr double kUpdateWindowHeight = 370.0;
constexpr double kStatusWindowWidth = 400.0;
constexpr double kStatusWindowHeight = 4.0 + kIconSize + kGap + kButtonHeight + kMargin;
constexpr double kPermissionWindowWidth = 437.0;
constexpr double kPermissionWindowHeight = 220.0;

/// Writes one line about what the windows are doing, when DAZZLE_WINDOWS_UI_LOG is set.
///
/// The module's windows are traced rather than silently absent: a backend that cannot draw
/// has to say so, and this is where it does. Off by default: a library does not write files
/// in %TEMP% unless it was asked to.
void Diagnose(std::string_view line) {
  wchar_t enabled[8] = {};
  if (GetEnvironmentVariableW(L"DAZZLE_WINDOWS_UI_LOG", enabled, 8) == 0 || enabled[0] != L'1') {
    return;
  }
  wchar_t path[MAX_PATH] = {};
  if (GetEnvironmentVariableW(L"TEMP", path, MAX_PATH) == 0) {
    return;
  }
  std::wstring file(path);
  file += L"\\dazzle-winui3-backend.log";
  HANDLE handle = CreateFileW(file.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
                              OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (handle == INVALID_HANDLE_VALUE) {
    return;
  }
  std::string text(line);
  text += "\r\n";
  DWORD written = 0;
  WriteFile(handle, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
  CloseHandle(handle);
}

bool DarkTheme() {
  if (auto application = xaml::Application::Current()) {
    return application.RequestedTheme() == xaml::ApplicationTheme::Dark;
  }
  return false;
}

winrt::Windows::UI::Color ColorOf(uint32_t rgb) {
  return winrt::Windows::UI::ColorHelper::FromArgb(
      255, static_cast<uint8_t>((rgb >> 16) & 0xFF), static_cast<uint8_t>((rgb >> 8) & 0xFF),
      static_cast<uint8_t>(rgb & 0xFF));
}

media::SolidColorBrush Brush(uint32_t rgb) { return media::SolidColorBrush(ColorOf(rgb)); }

/// The prototype's Windows colours, light and dark.
media::Brush SecondaryText() { return Brush(DarkTheme() ? 0xC5C5C5 : 0x5D5D5D); }
media::Brush CardBackground() { return Brush(DarkTheme() ? 0x2B2B2B : 0xFFFFFF); }
media::Brush CardStroke() { return Brush(DarkTheme() ? 0x3D3D3D : 0xE5E5E5); }
media::Brush ControlFill() { return Brush(DarkTheme() ? 0x2D2D2D : 0xFBFBFB); }
media::Brush ControlStroke() { return Brush(DarkTheme() ? 0x3D3D3D : 0xE5E5E5); }
media::Brush OnAccent() { return Brush(DarkTheme() ? 0x000000 : 0xFFFFFF); }

/// The user's accent colour, as the system reports it.
media::Brush Accent() {
  try {
    return media::SolidColorBrush(
        viewmanagement::UISettings().GetColorValue(viewmanagement::UIColorType::Accent));
  } catch (...) {
    return Brush(0x0F6CBD);
  }
}

/// The host application's icon. The GDI-to-WriteableBitmap conversion this wants is the one
/// piece of these windows that would be our own interop rather than XAML, so it is left for
/// later: a window without an icon is still a window.
media::ImageSource AppIcon(HINSTANCE instance) {
  (void)instance;
  return nullptr;
}

controls::TextBlock Text(std::string_view value, double size) {
  controls::TextBlock block;
  block.Text(winrt::to_hstring(value));
  block.FontSize(size);
  block.TextWrapping(xaml::TextWrapping::Wrap);
  return block;
}

controls::Button MakeButton(std::string_view label, bool is_default) {
  controls::Button button;
  button.Content(winrt::box_value(winrt::to_hstring(label)));
  button.MinWidth(kMinButtonWidth);
  button.MinHeight(kButtonHeight);
  button.CornerRadius(xaml::CornerRadius{static_cast<float>(kButtonRadius)});
  if (is_default) {
    button.Background(Accent());
    button.Foreground(OnAccent());
  } else {
    button.Background(ControlFill());
    button.BorderBrush(ControlStroke());
  }
  return button;
}

/// The release notes as a RichTextBlock: the Markdown subset every platform renders here
/// (headings, bullets, bold, italic, code, links).
void FillNotes(controls::RichTextBlock const& target, std::string_view markdown) {
  auto blocks = target.Blocks();
  blocks.Clear();
  size_t start = 0;
  while (start <= markdown.size()) {
    const size_t end = markdown.find('\n', start);
    std::string line(markdown.substr(
        start, end == std::string_view::npos ? std::string_view::npos : end - start));
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
      line.pop_back();
    }
    if (!line.empty() && line.rfind("```", 0) != 0) {
      documents::Paragraph paragraph;
      paragraph.FontSize(kBodySize);
      paragraph.Margin(xaml::Thickness{0, 0, 0, 8});
      size_t offset = 0;
      if (line[0] == '#') {
        const size_t hashes = line.find_first_not_of('#');
        paragraph.FontWeight(FontWeights::Bold());
        const size_t text_start = line.find_first_not_of("# ", hashes);
        offset = text_start == std::string::npos ? line.size() : text_start;
      } else if (line.rfind("- ", 0) == 0) {
        paragraph.TextIndent(18);
        documents::Run bullet;
        bullet.Text(L"\u2022  ");
        paragraph.Inlines().Append(bullet);
        offset = 2;
      }
      std::string text = line.substr(offset);
      size_t position = 0;
      while (position < text.size()) {
        size_t marker = text.find_first_of("*`[", position);
        if (marker == std::string::npos) {
          documents::Run plain;
          plain.Text(winrt::to_hstring(text.substr(position)));
          paragraph.Inlines().Append(plain);
          break;
        }
        if (marker > position) {
          documents::Run plain;
          plain.Text(winrt::to_hstring(text.substr(position, marker - position)));
          paragraph.Inlines().Append(plain);
        }
        const char kind = text[marker];
        if (kind == '[') {
          const size_t label_end = text.find("](", marker + 1);
          const size_t close =
              label_end == std::string::npos ? std::string::npos : text.find(')', label_end + 2);
          if (label_end == std::string::npos || close == std::string::npos) {
            documents::Run plain;
            plain.Text(winrt::to_hstring(text.substr(marker)));
            paragraph.Inlines().Append(plain);
            break;
          }
          documents::Hyperlink link;
          documents::Run label;
          label.Text(winrt::to_hstring(text.substr(marker + 1, label_end - marker - 1)));
          link.Inlines().Append(label);
          try {
            link.NavigateUri(winrt::Windows::Foundation::Uri(
                winrt::to_hstring(text.substr(label_end + 2, close - label_end - 2))));
          } catch (...) {
          }
          paragraph.Inlines().Append(link);
          position = close + 1;
          continue;
        }
        const bool bold = kind == '*' && text.compare(marker, 2, "**") == 0;
        const size_t marker_length = kind == '*' ? (bold ? 2 : 1) : 1;
        const size_t close =
            text.find(kind == '*' ? (bold ? "**" : "*") : std::string(1, kind),
                      marker + marker_length);
        if (close == std::string::npos) {
          documents::Run plain;
          plain.Text(winrt::to_hstring(text.substr(marker)));
          paragraph.Inlines().Append(plain);
          break;
        }
        documents::Run run;
        run.Text(winrt::to_hstring(
            text.substr(marker + marker_length, close - marker - marker_length)));
        if (kind == '*') {
          if (bold) {
            run.FontWeight(FontWeights::Bold());
          } else {
            run.FontStyle(FontStyle::Italic);
          }
        } else {
          run.FontFamily(media::FontFamily(L"Consolas"));
        }
        paragraph.Inlines().Append(run);
        position = close + marker_length;
      }
      blocks.Append(paragraph);
    }
    if (end == std::string_view::npos) {
      break;
    }
    start = end + 1;
  }
}

/// Which window a state belongs to.
enum class WindowKind { kNone, kUpdate, kStatus, kPermission };

/// One window, and the controls whose content changes in place.
struct WindowState {
  WindowKind kind = WindowKind::kNone;
  xaml::Window window{nullptr};
  controls::Grid root{nullptr};
  controls::TextBlock title{nullptr};
  controls::TextBlock heading{nullptr};
  controls::TextBlock detail{nullptr};
  controls::TextBlock error{nullptr};
  controls::Border notes{nullptr};
  controls::RichTextBlock notes_text{nullptr};
  controls::CheckBox checkbox{nullptr};
  controls::ProgressBar progress{nullptr};
  controls::TextBlock status{nullptr};
  controls::StackPanel actions{nullptr};
  bool shown = false;
};

}  // namespace

/// This process's windows, on the host's XAML thread.
class WinUi3Backend final : public UiBackend {
 public:
  WinUi3Backend() = default;

  ~WinUi3Backend() override {
    for (auto* window : {&update_, &status_, &permission_}) {
      if (window->window) {
        window->window.Close();
      }
    }
  }

  bool IsAvailable() const override { return CanDrawOnCallingThread(); }

  void SetHostUiQueue(int64_t queue) override {
    // Deliberately unused. A handle that crossed the C ABI is a projected object's IUnknown,
    // and calling into it from here is what access-violated inside TryEnqueue: the queue is
    // taken from the thread that draws instead, which is the host's UI thread.
    (void)queue;
  }

  void Draw(const UiContent& content) override {
    if (CanDrawOnCallingThread() && GetCurrentThreadId() == ui_thread_id_) {
      RunGuarded([this, content] { DrawOnThread(content); });
      return;
    }
    {
      std::lock_guard<std::mutex> lock(pending_mutex_);
      pending_ = content;
    }
    Diagnose("draw: kept for the host's next Pump()");
  }

  void Pump() override {
    if (!CanDrawOnCallingThread() || GetCurrentThreadId() != ui_thread_id_) {
      return;
    }
    std::optional<UiContent> content;
    {
      std::lock_guard<std::mutex> lock(pending_mutex_);
      content.swap(pending_);
    }
    if (content) {
      Diagnose("pump: drawing what was kept");
      RunGuarded([this, kept = *content] { DrawOnThread(kept); });
    }
  }

  void Hide() override {
    Run([this] {
      for (auto* window : {&update_, &status_, &permission_}) {
        if (window->window) {
          window->window.Close();
          *window = WindowState{};
        }
      }
    });
  }

  void BringToFront() override {
    Run([this] {
      if (auto* window = WindowOnScreen()) {
        window->window.Activate();
      }
    });
  }

  void QuitApplication() override { Run([] { PostQuitMessage(0); }); }

  void SetEventSink(std::function<void(const UiEvent&)> sink) override {
    std::lock_guard<std::mutex> lock(sink_mutex_);
    sink_ = std::move(sink);
  }

 private:
  /// Whether this thread can hold the module's windows, decided once by making one. Not by
  /// asking for a DispatcherQueue: Microsoft.UI.Dispatching's GetForCurrentThread() answers
  /// null to a native module in a self-contained app, and a handle taken across the C ABI is
  /// a CCW that crashes when called through.
  bool CanDrawOnCallingThread() const {
    if (probed_) {
      return available_.load();
    }
    probed_ = true;
    try {
      xaml::Window probe;
      probe.Close();
      ui_thread_id_ = GetCurrentThreadId();
      available_.store(true);
      Diagnose("the calling thread can hold XAML windows");
    } catch (winrt::hresult_error const& error) {
      Diagnose(std::string("the calling thread cannot hold XAML windows: ") +
               winrt::to_string(error.message()));
      available_.store(false);
    }
    return available_.load();
  }

  /// Runs work where the windows live: directly when the caller is that thread, through the
  /// host's queue when one was handed over, and otherwise not at all — with a line in the log,
  /// because a window that cannot be drawn must not look like one that was.
  void Run(std::function<void()> work) {
    if (CanDrawOnCallingThread() && GetCurrentThreadId() == ui_thread_id_) {
      RunGuarded(std::move(work));
      return;
    }
    if (auto queue = host_queue_) {
      queue.TryEnqueue([this, work = std::move(work)]() mutable { RunGuarded(std::move(work)); });
      return;
    }
    Diagnose("work from a thread that is not the host's UI thread: nothing done");
  }

  /// Work on the XAML thread, with the exception handling that keeps a failure in the log
  /// rather than in the process: an escaping exception there ends the application.
  void RunGuarded(std::function<void()> work) {
    try {
      work();
    } catch (winrt::hresult_error const& error) {
      Diagnose(std::string("window work failed: ") + winrt::to_string(error.message()));
      available_.store(false);
    } catch (std::exception const& error) {
      Diagnose(std::string("window work failed: ") + error.what());
      available_.store(false);
    } catch (...) {
      Diagnose("window work failed: unknown exception");
      available_.store(false);
    }
  }

  /// Reports a button press to the state machine, from whichever thread the control ran on.
  void Report(UiEventKind kind, bool flag = false) {
    std::function<void(const UiEvent&)> sink;
    {
      std::lock_guard<std::mutex> lock(sink_mutex_);
      sink = sink_;
    }
    if (sink) {
      UiEvent event;
      event.kind = kind;
      event.flag = flag;
      sink(event);
    }
  }

  WindowState* WindowFor(WindowKind kind) {
    switch (kind) {
      case WindowKind::kUpdate: return &update_;
      case WindowKind::kStatus: return &status_;
      case WindowKind::kPermission: return &permission_;
      default: return nullptr;
    }
  }

  WindowState* WindowOnScreen() {
    for (auto* window : {&update_, &status_, &permission_}) {
      if (window->shown) {
        return window;
      }
    }
    return nullptr;
  }

  static WindowKind KindOf(UiWindow window) {
    switch (window) {
      case UiWindow::kUpdateAvailable: return WindowKind::kUpdate;
      case UiWindow::kUpdateStatus: return WindowKind::kStatus;
      case UiWindow::kUpdatePermission: return WindowKind::kPermission;
      default: return WindowKind::kNone;
    }
  }

  /// The drawing itself, always on the thread the windows belong to.
  void DrawOnThread(const UiContent& content) {
    const WindowKind kind = KindOf(content.window);
    if (kind == WindowKind::kNone) {
      return;
    }

    WindowState* target = WindowFor(kind);
    WindowState* previous = WindowOnScreen();
    const bool replacing = previous != nullptr && previous != target;
    if (!target->window) {
      BuildWindow(*target, kind, content);
    }
    UpdateWindow(*target, content);
    if (replacing && previous->window) {
      // Centred on the window it replaced, then that one goes away.
      CentreOn(*target, *previous);
      previous->window.Close();
      *previous = WindowState{};
    }
    target->shown = true;
    target->window.Activate();
  }

  void BuildWindow(WindowState& state, WindowKind kind, const UiContent& content) {
    state.kind = kind;
    state.window = xaml::Window();
    state.window.Title(winrt::to_hstring(content.window_title));
    try {
      state.window.SystemBackdrop(media::MicaBackdrop());
    } catch (...) {
    }

    // The update and status windows draw their own 32 pt title bar so Mica runs behind it,
    // which is also how Sparkle's transparent title bars work. The permission prompt has no
    // title at all: its caption is hidden, and Sparkle's prompt is untitled.
    const bool draws_title_bar = kind != WindowKind::kPermission;
    if (!draws_title_bar) {
      state.title = nullptr;
    } else {
      state.title = Text(content.window_title, kCaptionSize);
      state.title.Foreground(SecondaryText());
      state.title.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
      state.title.VerticalAlignment(xaml::VerticalAlignment::Center);
    }
    auto bar = controls::Grid();
    bar.Height(draws_title_bar ? kTitleBarHeight : 0.0);
    bar.Padding(xaml::Thickness{12, 0, 12, 0});
    if (state.title) {
      bar.Children().Append(state.title);
    }
    state.window.ExtendsContentIntoTitleBar(draws_title_bar);
    if (draws_title_bar) {
      state.window.SetTitleBar(bar);
    }

    auto root = controls::Grid();
    root.RowDefinitions().Append(controls::RowDefinition());
    root.RowDefinitions().Append(controls::RowDefinition());
    root.RowDefinitions().GetAt(0).Height(
        xaml::GridLengthHelper::FromPixels(draws_title_bar ? kTitleBarHeight : 0.0));
    root.RowDefinitions().GetAt(1).Height(
        xaml::GridLengthHelper::FromValueAndType(1, xaml::GridUnitType::Star));
    controls::Grid::SetRow(bar, 0);
    root.Children().Append(bar);
    state.root = root;

    if (kind == WindowKind::kUpdate) {
      auto layout = controls::Grid();
      layout.Padding(xaml::Thickness{kMargin, 0, kMargin, kMargin});
      layout.RowSpacing(kGap);
      for (int i = 0; i < 4; ++i) {
        layout.RowDefinitions().Append(controls::RowDefinition());
      }
      layout.RowDefinitions().GetAt(0).Height(xaml::GridLengthHelper::Auto());
      layout.RowDefinitions().GetAt(1).Height(
          xaml::GridLengthHelper::FromValueAndType(1, xaml::GridUnitType::Star));
      layout.RowDefinitions().GetAt(2).Height(xaml::GridLengthHelper::Auto());
      layout.RowDefinitions().GetAt(3).Height(xaml::GridLengthHelper::Auto());

      auto title_row = controls::Grid();
      title_row.ColumnSpacing(kGap);
      title_row.MinHeight(kIconSize);
      title_row.ColumnDefinitions().Append(controls::ColumnDefinition());
      title_row.ColumnDefinitions().Append(controls::ColumnDefinition());
      title_row.ColumnDefinitions().GetAt(0).Width(xaml::GridLengthHelper::Auto());
      title_row.ColumnDefinitions().GetAt(1).Width(
          xaml::GridLengthHelper::FromValueAndType(1, xaml::GridUnitType::Star));
      if (auto icon = AppIcon(GetModuleHandleW(nullptr))) {
        controls::Image image;
        image.Source(icon);
        image.Width(kIconSize);
        image.Height(kIconSize);
        controls::Grid::SetColumn(image, 0);
        title_row.Children().Append(image);
      }
      auto text_column = controls::StackPanel();
      text_column.Spacing(kGap);
      text_column.VerticalAlignment(xaml::VerticalAlignment::Center);
      state.heading = Text("", kBodySize);
      state.heading.FontWeight(FontWeights::SemiBold());
      state.detail = Text("", kCaptionSize);
      state.detail.Foreground(SecondaryText());
      text_column.Children().Append(state.heading);
      text_column.Children().Append(state.detail);
      controls::Grid::SetColumn(text_column, 1);
      title_row.Children().Append(text_column);
      controls::Grid::SetRow(title_row, 0);
      layout.Children().Append(title_row);

      state.notes = controls::Border();
      state.notes.Background(CardBackground());
      state.notes.BorderBrush(CardStroke());
      state.notes.BorderThickness(xaml::Thickness{1, 1, 1, 1});
      state.notes.CornerRadius(xaml::CornerRadius{static_cast<float>(kNotesRadius)});
      state.notes.MinHeight(kNotesMinHeight);
      state.notes_text = controls::RichTextBlock();
      state.notes_text.Padding(xaml::Thickness{10, 8, 10, 8});
      state.notes_text.TextWrapping(xaml::TextWrapping::Wrap);
      auto scroller = controls::ScrollViewer();
      scroller.Content(state.notes_text);
      state.notes.Child(scroller);
      controls::Grid::SetRow(state.notes, 1);
      layout.Children().Append(state.notes);

      state.checkbox = controls::CheckBox();
      state.checkbox.MinHeight(kCheckboxRow);
      state.checkbox.Click([this](auto&&, auto&&) { Report(UiEventKind::kCheckboxChanged, true); });
      controls::Grid::SetRow(state.checkbox, 2);
      layout.Children().Append(state.checkbox);

      state.actions = controls::StackPanel();
      state.actions.Orientation(controls::Orientation::Horizontal);
      state.actions.Spacing(kButtonGap);
      state.actions.HorizontalAlignment(xaml::HorizontalAlignment::Right);
      controls::Grid::SetRow(state.actions, 3);
      layout.Children().Append(state.actions);

      controls::Grid::SetRow(layout, 1);
      state.root.Children().Append(layout);
    } else if (kind == WindowKind::kStatus) {
      auto layout = controls::Grid();
      layout.Padding(xaml::Thickness{kMargin, 4, kMargin, kMargin});
      layout.ColumnSpacing(kGap);
      layout.RowSpacing(kButtonGap);
      layout.ColumnDefinitions().Append(controls::ColumnDefinition());
      layout.ColumnDefinitions().Append(controls::ColumnDefinition());
      layout.ColumnDefinitions().GetAt(0).Width(xaml::GridLengthHelper::Auto());
      layout.ColumnDefinitions().GetAt(1).Width(
          xaml::GridLengthHelper::FromValueAndType(1, xaml::GridUnitType::Star));
      layout.RowDefinitions().Append(controls::RowDefinition());
      layout.RowDefinitions().Append(controls::RowDefinition());

      if (auto icon = AppIcon(GetModuleHandleW(nullptr))) {
        controls::Image image;
        image.Source(icon);
        image.Width(kIconSize);
        image.Height(kIconSize);
        image.VerticalAlignment(xaml::VerticalAlignment::Top);
        controls::Grid::SetColumn(image, 0);
        layout.Children().Append(image);
      }
      auto body = controls::StackPanel();
      body.Spacing(kGap);
      body.VerticalAlignment(xaml::VerticalAlignment::Center);
      state.heading = Text("", kBodySize);
      state.heading.FontWeight(FontWeights::SemiBold());
      state.progress = controls::ProgressBar();
      state.progress.Height(3);
      body.Children().Append(state.heading);
      body.Children().Append(state.progress);
      controls::Grid::SetColumn(body, 1);
      layout.Children().Append(body);

      auto footer = controls::Grid();
      footer.ColumnSpacing(kButtonGap);
      footer.ColumnDefinitions().Append(controls::ColumnDefinition());
      footer.ColumnDefinitions().Append(controls::ColumnDefinition());
      footer.ColumnDefinitions().GetAt(0).Width(
          xaml::GridLengthHelper::FromValueAndType(1, xaml::GridUnitType::Star));
      footer.ColumnDefinitions().GetAt(1).Width(xaml::GridLengthHelper::Auto());
      auto status_column = controls::StackPanel();
      state.status = Text("", kBodySize);
      state.status.Foreground(SecondaryText());
      state.error = Text("", kCaptionSize);
      state.error.Foreground(SecondaryText());
      status_column.Children().Append(state.status);
      status_column.Children().Append(state.error);
      controls::Grid::SetColumn(status_column, 0);
      footer.Children().Append(status_column);
      state.actions = controls::StackPanel();
      state.actions.Orientation(controls::Orientation::Horizontal);
      state.actions.Spacing(kButtonGap);
      controls::Grid::SetColumn(state.actions, 1);
      footer.Children().Append(state.actions);
      controls::Grid::SetRow(footer, 1);
      controls::Grid::SetColumnSpan(footer, 2);
      layout.Children().Append(footer);

      controls::Grid::SetRow(layout, 1);
      state.root.Children().Append(layout);
    } else {
      // The permission prompt: Sparkle's geometry, with no caption bar — the question has to
      // be answered, and Escape is the keyboard's "Don't Check".
      HideCaption(state.window);
      auto layout = controls::Grid();
      layout.Padding(xaml::Thickness{23, 18, 20, 20});
      layout.ColumnSpacing(19);
      layout.ColumnDefinitions().Append(controls::ColumnDefinition());
      layout.ColumnDefinitions().Append(controls::ColumnDefinition());
      layout.ColumnDefinitions().GetAt(0).Width(xaml::GridLengthHelper::Auto());
      layout.ColumnDefinitions().GetAt(1).Width(
          xaml::GridLengthHelper::FromValueAndType(1, xaml::GridUnitType::Star));
      if (auto icon = AppIcon(GetModuleHandleW(nullptr))) {
        controls::Image image;
        image.Source(icon);
        image.Width(kIconSize);
        image.Height(kIconSize);
        image.VerticalAlignment(xaml::VerticalAlignment::Top);
        controls::Grid::SetColumn(image, 0);
        layout.Children().Append(image);
      }
      auto body = controls::StackPanel();
      body.Spacing(kGap);
      state.heading = Text("", kBodySize);
      state.heading.FontWeight(FontWeights::SemiBold());
      state.detail = Text("", kCaptionSize);
      state.detail.Foreground(SecondaryText());
      state.checkbox = controls::CheckBox();
      state.checkbox.MinHeight(kCheckboxRow);
      state.checkbox.HorizontalAlignment(xaml::HorizontalAlignment::Left);
      state.checkbox.Click([this](auto&&, auto&&) { Report(UiEventKind::kCheckboxChanged, true); });
      state.actions = controls::StackPanel();
      state.actions.Orientation(controls::Orientation::Horizontal);
      state.actions.Spacing(14);
      state.actions.HorizontalAlignment(xaml::HorizontalAlignment::Right);
      state.actions.Margin(xaml::Thickness{0, 16, 0, 0});
      body.Children().Append(state.heading);
      body.Children().Append(state.detail);
      body.Children().Append(state.checkbox);
      body.Children().Append(state.actions);
      controls::Grid::SetColumn(body, 1);
      layout.Children().Append(body);
      controls::Grid::SetRow(layout, 1);
      state.root.Children().Append(layout);
    }

    state.window.Content(state.root);
  }

  /// The parts that change with a state, updated in place.
  void UpdateWindow(WindowState& state, const UiContent& content) {
    state.window.Title(winrt::to_hstring(content.window_title));
    if (state.title) {
      state.title.Text(winrt::to_hstring(content.window_title));
    }
    if (state.heading) {
      state.heading.Text(winrt::to_hstring(content.heading));
    }
    if (state.detail) {
      state.detail.Text(winrt::to_hstring(content.detail));
    }
    if (state.error) {
      state.error.Text(winrt::to_hstring(content.error_message));
    }
    if (state.notes_text) {
      FillNotes(state.notes_text, content.notes_markdown);
    }
    if (state.notes) {
      state.notes.Visibility(content.notes_markdown.empty() ? xaml::Visibility::Collapsed
                                                            : xaml::Visibility::Visible);
    }
    if (state.checkbox) {
      state.checkbox.Visibility(content.checkbox.visible ? xaml::Visibility::Visible
                                                         : xaml::Visibility::Collapsed);
      if (content.checkbox.visible) {
        state.checkbox.Content(winrt::box_value(winrt::to_hstring(content.checkbox.label)));
        state.checkbox.IsChecked(content.checkbox.checked);
      }
    }
    if (state.progress) {
      state.progress.Visibility(content.progress.visible ? xaml::Visibility::Visible
                                                         : xaml::Visibility::Collapsed);
      state.progress.IsIndeterminate(!content.progress.determinate);
      state.progress.Value(content.progress.fraction * 100.0);
    }
    if (state.status) {
      state.status.Text(winrt::to_hstring(content.progress.status_text));
    }
    if (state.actions) {
      auto children = state.actions.Children();
      children.Clear();
      for (const auto& button : content.buttons) {
        auto control = MakeButton(button.label, button.is_default);
        const UiEventKind kind = button.event;
        control.Click([this, kind](auto&&, auto&&) { Report(kind); });
        children.Append(control);
      }
    }

    ResizeContent(state);
  }

  void ResizeContent(WindowState& state) {
    double width = kUpdateWindowWidth;
    double height = kUpdateWindowHeight;
    if (state.kind == WindowKind::kStatus) {
      width = kStatusWindowWidth;
      height = kStatusWindowHeight;
    } else if (state.kind == WindowKind::kPermission) {
      width = kPermissionWindowWidth;
      height = kPermissionWindowHeight;
    }
    auto app_window = AppWindowOf(state.window);
    if (!app_window) {
      return;
    }
    // ResizeClient counts the caption itself once the content extends into it, so asking for
    // the content height gives the title bar plus that content — Sparkle's numbers.
    const double scale = DpiScale(state.window);
    app_window.ResizeClient(winrt::Windows::Graphics::SizeInt32{
        static_cast<int32_t>(width * scale), static_cast<int32_t>(height * scale)});
  }

  void CentreOn(WindowState& state, WindowState& other) {
    auto app_window = AppWindowOf(state.window);
    auto other_window = AppWindowOf(other.window);
    if (!app_window || !other_window) {
      return;
    }
    const auto position = other_window.Position();
    const auto size = app_window.Size();
    const auto other_size = other_window.Size();
    app_window.Move(winrt::Windows::Graphics::PointInt32{position.X + (other_size.Width - size.Width) / 2,
                                          position.Y + (other_size.Height - size.Height) / 2});
  }

  static void HideCaption(xaml::Window const& window) {
    if (auto app_window = AppWindowOf(window)) {
      if (auto presenter = app_window.Presenter().try_as<windowing::OverlappedPresenter>()) {
        presenter.SetBorderAndTitleBar(true, false);
        presenter.IsResizable(false);
        presenter.IsMaximizable(false);
        presenter.IsMinimizable(false);
      }
    }
  }

  static windowing::AppWindow AppWindowOf(xaml::Window const& window) {
    if (!window) {
      return nullptr;
    }
    try {
      return window.AppWindow();
    } catch (...) {
      return nullptr;
    }
  }

  /// The monitor's scale, from the XAML root: no window handle is involved.
  static double DpiScale(xaml::Window const& window) {
    try {
      if (auto content = window.Content()) {
        if (auto root = content.XamlRoot()) {
          return root.RasterizationScale();
        }
      }
    } catch (...) {
    }
    return 1.0;
  }

  WindowState update_;
  WindowState status_;
  WindowState permission_;
  dispatching::DispatcherQueue host_queue_{nullptr};
  mutable std::atomic<bool> available_{false};
  mutable bool probed_ = false;
  mutable DWORD ui_thread_id_ = 0;
  std::mutex pending_mutex_;
  std::optional<UiContent> pending_;
  std::mutex sink_mutex_;
  std::function<void(const UiEvent&)> sink_;
};

UiBackend& GetUiBackend() {
  static WinUi3Backend backend;
  return backend;
}

}  // namespace internal
}  // namespace dazzle
