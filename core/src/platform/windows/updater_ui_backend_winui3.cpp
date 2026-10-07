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
// The backend discovers that thread on the first check and creates a message-only Win32
// window there. Worker results, progress, closing and quitting are posted to that window;
// the application's normal message loop delivers them without a timer or a managed COM
// handle crossing the C ABI. All of this scheduling belongs to core.
//
// The geometry is the one specs/updater-ui.md tabulates for Windows: the prototype's
// Windows tokens — 8 pt gaps, 32 pt Fluent buttons, a 20 pt check box, 4 pt corners, the
// system accent colour — with Sparkle's content sizes (532 × 370 for the update window,
// 400 for the status window, 437 for the permission prompt). The controls are WinUI's own,
// styled from the theme resources the host merged (XamlControlsResources), so hover,
// pressed, focus and the dark theme are the framework's.

#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Graphics.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.h>
#include <winrt/Windows.UI.Text.h>
#include <winrt/Microsoft.UI.h>
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
#include "md4c/md4c.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <exception>
#include <functional>
#include <limits>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace dazzle_updater {
namespace internal {
namespace {

namespace xaml = winrt::Microsoft::UI::Xaml;
namespace controls = winrt::Microsoft::UI::Xaml::Controls;
namespace documents = winrt::Microsoft::UI::Xaml::Documents;
namespace input = winrt::Microsoft::UI::Xaml::Input;
namespace media = winrt::Microsoft::UI::Xaml::Media;
namespace imaging = winrt::Microsoft::UI::Xaml::Media::Imaging;
namespace windowing = winrt::Microsoft::UI::Windowing;
using winrt::Windows::System::VirtualKey;
using winrt::Windows::UI::Text::FontStyle;
using winrt::Windows::UI::Text::FontWeights;

/// The Windows metrics: the prototype's Windows tokens and the sizes the spec gives.
constexpr double kMargin = 20.0;
constexpr double kIconSize = 64.0;
constexpr double kIconGap = 8.0;
constexpr double kGap = 8.0;
constexpr double kButtonGap = 8.0;   // Windows: 8 (macOS: 12)
constexpr double kMinButtonWidth = 120.0;
constexpr double kStatusButtonWidth = 100.0;
constexpr double kNotesRadius = 4.0;
constexpr double kNotesMinHeight = 200.0;
constexpr double kCheckboxRow = 20.0;
constexpr double kButtonHeight = 32.0;
constexpr double kBodySize = 14.0;
constexpr double kCaptionSize = 12.0;
constexpr double kTitleBarHeight = 32.0;
constexpr double kUpdateWindowWidth = 532.0;
constexpr double kUpdateWindowHeight = 370.0;
constexpr double kStatusWindowWidth = 400.0;
constexpr double kPermissionWindowWidth = 437.0;
constexpr wchar_t kMonospace[] = L"Cascadia Mono, Consolas";

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

const char* StateName(UiState state) {
  switch (state) {
    case UiState::kChecking: return "Checking";
    case UiState::kAvailable: return "Available";
    case UiState::kDownloaded: return "Downloaded";
    case UiState::kDownloading: return "Downloading";
    case UiState::kReady: return "Ready";
    case UiState::kInstalling: return "Installing";
    case UiState::kUpToDate: return "UpToDate";
    case UiState::kError: return "Error";
    case UiState::kPermission: return "Permission";
  }
  return "?";
}

const char* EventName(UiEventKind kind) {
  switch (kind) {
    case UiEventKind::kPrimary: return "primary";
    case UiEventKind::kSecondary: return "secondary";
    case UiEventKind::kCancel: return "cancel";
    case UiEventKind::kSkip: return "skip";
    case UiEventKind::kClose: return "close";
    case UiEventKind::kCheckboxChanged: return "checkbox";
  }
  return "?";
}

bool DarkTheme() {
  if (auto application = xaml::Application::Current()) {
    return application.RequestedTheme() == xaml::ApplicationTheme::Dark;
  }
  return false;
}

/// A resource from the host's merged theme (XamlControlsResources), or null when the host
/// merged none: the windows still draw, with the fallbacks below.
winrt::Windows::Foundation::IInspectable ThemeResource(const wchar_t* key) {
  try {
    if (auto application = xaml::Application::Current()) {
      // Lookup, not HasKey: only Lookup searches the merged and theme dictionaries.
      return application.Resources().Lookup(winrt::box_value(winrt::hstring(key)));
    }
  } catch (...) {
  }
  return nullptr;
}

media::Brush ThemeBrush(const wchar_t* key, uint32_t light, uint32_t dark) {
  if (auto brush = ThemeResource(key).try_as<media::Brush>()) {
    return brush;
  }
  const uint32_t rgb = DarkTheme() ? dark : light;
  return media::SolidColorBrush(winrt::Windows::UI::ColorHelper::FromArgb(
      255, static_cast<uint8_t>((rgb >> 16) & 0xFF), static_cast<uint8_t>((rgb >> 8) & 0xFF),
      static_cast<uint8_t>(rgb & 0xFF)));
}

/// The prototype's Windows colours, from the theme when the host merged it.
media::Brush SecondaryText() {
  return ThemeBrush(L"TextFillColorSecondaryBrush", 0x5D5D5D, 0xC5C5C5);
}
media::Brush CardBackground() {
  return ThemeBrush(L"CardBackgroundFillColorDefaultBrush", 0xFFFFFF, 0x2B2B2B);
}
media::Brush CardStroke() { return ThemeBrush(L"CardStrokeColorDefaultBrush", 0xE5E5E5, 0x3D3D3D); }

/// The IBufferByteAccess every WinRT buffer implements: the one way to write a
/// WriteableBitmap's pixels from C++.
struct __declspec(uuid("905a0fef-bc53-11df-8c49-001e4fc686da")) IBufferByteAccess : ::IUnknown {
  virtual HRESULT __stdcall Buffer(uint8_t** value) = 0;
};

/// Draws `icon` into a top-down 32-bit DIB of `size` pixels and returns its pixels,
/// premultiplied BGRA — the layout a WriteableBitmap takes as it is.
std::vector<uint8_t> IconPixels(HICON icon, int size) {
  std::vector<uint8_t> pixels;
  HDC screen = GetDC(nullptr);
  HDC dc = CreateCompatibleDC(screen);
  ReleaseDC(nullptr, screen);
  if (dc == nullptr) {
    return pixels;
  }
  BITMAPINFO info = {};
  info.bmiHeader.biSize = sizeof(info.bmiHeader);
  info.bmiHeader.biWidth = size;
  info.bmiHeader.biHeight = -size;  // top-down
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;
  auto draw = [&](UINT flags) {
    void* bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    std::vector<uint8_t> out;
    if (bitmap == nullptr || bits == nullptr) {
      return out;
    }
    HGDIOBJ previous = SelectObject(dc, bitmap);
    std::memset(bits, 0, static_cast<size_t>(size) * size * 4);
    DrawIconEx(dc, 0, 0, icon, size, size, 0, nullptr, flags);
    GdiFlush();
    out.assign(static_cast<uint8_t*>(bits), static_cast<uint8_t*>(bits) + size * size * 4);
    SelectObject(dc, previous);
    DeleteObject(bitmap);
    return out;
  };
  pixels = draw(DI_NORMAL);
  bool has_alpha = false;
  for (size_t i = 3; i < pixels.size(); i += 4) {
    if (pixels[i] != 0) {
      has_alpha = true;
      break;
    }
  }
  if (!has_alpha && !pixels.empty()) {
    // An icon without an alpha channel: its mask says which pixels are the icon.
    const std::vector<uint8_t> mask = draw(DI_MASK);
    for (size_t i = 0; i + 3 < pixels.size() && i + 3 < mask.size(); i += 4) {
      const bool transparent = mask[i] != 0;
      if (transparent) {
        pixels[i] = pixels[i + 1] = pixels[i + 2] = pixels[i + 3] = 0;
      } else {
        pixels[i + 3] = 255;
      }
    }
  }
  DeleteDC(dc);
  return pixels;
}

/// The host application's icon — the first icon resource of its executable, which is the
/// one Explorer and the taskbar show — at 64 pt for the system's scale. Without one, the
/// system's generic application icon, as Windows itself shows for such a program.
media::ImageSource AppIcon() {
  static media::ImageSource cached{nullptr};
  if (cached) {
    return cached;
  }
  const int size =
      static_cast<int>(std::lround(kIconSize * static_cast<double>(GetDpiForSystem()) / 96.0));
  wchar_t path[MAX_PATH] = {};
  GetModuleFileNameW(nullptr, path, MAX_PATH);
  HICON icon = nullptr;
  bool owned = false;
  if (PrivateExtractIconsW(path, 0, size, size, &icon, nullptr, 1, LR_DEFAULTCOLOR) == 1 &&
      icon != nullptr) {
    owned = true;
  } else {
    icon = static_cast<HICON>(
        LoadImageW(nullptr, MAKEINTRESOURCEW(32512) /* IDI_APPLICATION */, IMAGE_ICON, size, size, LR_SHARED));
  }
  if (icon == nullptr) {
    return nullptr;
  }
  const std::vector<uint8_t> pixels = IconPixels(icon, size);
  if (owned) {
    DestroyIcon(icon);
  }
  if (pixels.empty()) {
    return nullptr;
  }
  try {
    imaging::WriteableBitmap bitmap(size, size);
    auto buffer = bitmap.PixelBuffer();
    uint8_t* target = nullptr;
    buffer.as<IBufferByteAccess>()->Buffer(&target);
    if (target == nullptr || buffer.Capacity() < pixels.size()) {
      return nullptr;
    }
    std::memcpy(target, pixels.data(), pixels.size());
    bitmap.Invalidate();
    cached = bitmap;
  } catch (...) {
    return nullptr;
  }
  return cached;
}

controls::Image IconImage() {
  controls::Image image;
  image.Width(kIconSize);
  image.Height(kIconSize);
  image.VerticalAlignment(xaml::VerticalAlignment::Top);
  if (auto icon = AppIcon()) {
    image.Source(icon);
  }
  return image;
}

controls::TextBlock Text(std::string_view value, double size) {
  controls::TextBlock block;
  block.Text(winrt::to_hstring(value));
  block.FontSize(size);
  block.TextWrapping(xaml::TextWrapping::Wrap);
  return block;
}

/// The scheme test the spec asks for: only a link the system browser can open safely is a
/// link. Anything else — `file:`, `javascript:`, an application's own scheme — is drawn as
/// plain text.
bool IsLinkableScheme(const std::string& url) {
  return url.compare(0, 7, "http://") == 0 || url.compare(0, 8, "https://") == 0 ||
         url.compare(0, 7, "mailto:") == 0;
}

/// Markdown → the blocks of a RichTextBlock, over md4c's parser (core/third_party/md4c),
/// exactly the subset the other platforms render: headings, paragraphs, ordered and
/// unordered lists, bold, italic, inline code, code blocks and links. Images and raw HTML
/// are dropped.
///
/// md4c reports blocks and spans with the text in between; the writer keeps the paragraph
/// being filled and a stack of the spans the text is currently inside, so bold inside a
/// link, or a link inside a list item, comes out right.
class NotesWriter {
 public:
  explicit NotesWriter(controls::RichTextBlock const& target) : blocks_(target.Blocks()) {}

  void Render(const std::string& markdown) {
    blocks_.Clear();
    MD_PARSER parser;
    std::memset(&parser, 0, sizeof(parser));
    parser.flags = MD_DIALECT_GITHUB;
    parser.enter_block = [](MD_BLOCKTYPE type, void* detail, void* self) {
      static_cast<NotesWriter*>(self)->EnterBlock(type, detail);
      return 0;
    };
    parser.leave_block = [](MD_BLOCKTYPE type, void*, void* self) {
      static_cast<NotesWriter*>(self)->LeaveBlock(type);
      return 0;
    };
    parser.enter_span = [](MD_SPANTYPE type, void* detail, void* self) {
      static_cast<NotesWriter*>(self)->EnterSpan(type, detail);
      return 0;
    };
    parser.leave_span = [](MD_SPANTYPE type, void*, void* self) {
      static_cast<NotesWriter*>(self)->LeaveSpan(type);
      return 0;
    };
    parser.text = [](MD_TEXTTYPE type, const MD_CHAR* text, MD_SIZE size, void* self) {
      static_cast<NotesWriter*>(self)->Text(type, std::string(text, size));
      return 0;
    };
    md_parse(markdown.c_str(), static_cast<MD_SIZE>(markdown.size()), &parser, this);
  }

 private:
  enum class Block { kParagraph, kHeading, kSubheading, kCode, kListItem };

  /// The paragraph text goes into, made when the first text of a block arrives.
  documents::Paragraph& Current() {
    if (!paragraph_) {
      paragraph_ = documents::Paragraph();
      paragraph_.FontSize(kBodySize);
      const double indent = 18.0 * list_depth_;
      double top = blocks_.Size() == 0 ? 0.0 : 8.0;
      switch (block_) {
        case Block::kHeading:
          paragraph_.FontSize(17.0);
          paragraph_.FontWeight(FontWeights::SemiBold());
          top = blocks_.Size() == 0 ? 0.0 : 14.0;
          break;
        case Block::kSubheading:
          paragraph_.FontSize(kBodySize);
          paragraph_.FontWeight(FontWeights::SemiBold());
          top = blocks_.Size() == 0 ? 0.0 : 12.0;
          break;
        case Block::kCode:
          paragraph_.FontFamily(media::FontFamily(kMonospace));
          paragraph_.FontSize(13.0);
          break;
        case Block::kListItem:
          top = blocks_.Size() == 0 ? 0.0 : 4.0;
          break;
        default:
          break;
      }
      paragraph_.Margin(xaml::Thickness{block_ == Block::kCode ? 12.0 : indent, top, 0, 0});
      blocks_.Append(paragraph_);
      spans_.clear();
      if (!pending_marker_.empty()) {
        documents::Run marker;
        marker.Text(winrt::to_hstring(pending_marker_));
        paragraph_.Inlines().Append(marker);
        pending_marker_.clear();
      }
    }
    return paragraph_;
  }

  /// Appends an inline to the innermost open span, or to the paragraph.
  void Append(documents::Inline const& inline_element) {
    Current();
    if (!spans_.empty() && spans_.back()) {
      spans_.back().Inlines().Append(inline_element);
    } else {
      paragraph_.Inlines().Append(inline_element);
    }
  }

  void EnterBlock(MD_BLOCKTYPE type, void* detail) {
    switch (type) {
      case MD_BLOCK_H:
        paragraph_ = nullptr;
        block_ = static_cast<const MD_BLOCK_H_DETAIL*>(detail)->level <= 2 ? Block::kHeading
                                                                            : Block::kSubheading;
        break;
      case MD_BLOCK_CODE:
        paragraph_ = nullptr;
        block_ = Block::kCode;
        break;
      case MD_BLOCK_P:
        // A paragraph inside a list item (a loose list) continues the item's own paragraph.
        if (block_ != Block::kListItem) {
          paragraph_ = nullptr;
          block_ = Block::kParagraph;
        }
        break;
      case MD_BLOCK_UL:
        paragraph_ = nullptr;
        ++list_depth_;
        counters_.push_back(-1);
        break;
      case MD_BLOCK_OL:
        paragraph_ = nullptr;
        ++list_depth_;
        counters_.push_back(static_cast<int>(static_cast<const MD_BLOCK_OL_DETAIL*>(detail)->start));
        break;
      case MD_BLOCK_LI:
        paragraph_ = nullptr;
        block_ = Block::kListItem;
        if (!counters_.empty() && counters_.back() >= 0) {
          pending_marker_ = std::to_string(counters_.back()++) + ".  ";
        } else {
          pending_marker_ = "•  ";
        }
        break;
      default:
        break;
    }
  }

  void LeaveBlock(MD_BLOCKTYPE type) {
    switch (type) {
      case MD_BLOCK_H:
      case MD_BLOCK_CODE:
        paragraph_ = nullptr;
        block_ = list_depth_ > 0 ? Block::kListItem : Block::kParagraph;
        break;
      case MD_BLOCK_P:
        if (block_ != Block::kListItem) {
          paragraph_ = nullptr;
        }
        break;
      case MD_BLOCK_LI:
        paragraph_ = nullptr;
        pending_marker_.clear();
        break;
      case MD_BLOCK_UL:
      case MD_BLOCK_OL:
        paragraph_ = nullptr;
        if (list_depth_ > 0) {
          --list_depth_;
        }
        if (!counters_.empty()) {
          counters_.pop_back();
        }
        block_ = list_depth_ > 0 ? Block::kListItem : Block::kParagraph;
        break;
      default:
        break;
    }
  }

  void EnterSpan(MD_SPANTYPE type, void* detail) {
    switch (type) {
      case MD_SPAN_EM: {
        documents::Italic span;
        Append(span);
        spans_.push_back(span);
        break;
      }
      case MD_SPAN_STRONG: {
        documents::Bold span;
        Append(span);
        spans_.push_back(span);
        break;
      }
      case MD_SPAN_IMG:
        ++swallowing_image_;  // images are dropped, alt text included
        break;
      case MD_SPAN_A: {
        const auto* link = static_cast<const MD_SPAN_A_DETAIL*>(detail);
        std::string url;
        for (int index = 0; link->href.substr_offsets[index] < link->href.size; ++index) {
          const MD_OFFSET offset = link->href.substr_offsets[index];
          url.append(link->href.text + offset, link->href.substr_offsets[index + 1] - offset);
        }
        documents::Span span{nullptr};
        if (IsLinkableScheme(url) && !in_link_) {
          try {
            documents::Hyperlink hyperlink;
            hyperlink.NavigateUri(winrt::Windows::Foundation::Uri(winrt::to_hstring(url)));
            span = hyperlink;
            in_link_ = true;
          } catch (...) {
            span = nullptr;  // an address Uri cannot parse is text, like any other scheme
          }
        }
        if (span) {
          Append(span);
        }
        spans_.push_back(span);  // a null entry: the text goes to the enclosing span
        links_.push_back(static_cast<bool>(span));
        break;
      }
      default:
        break;
    }
  }

  void LeaveSpan(MD_SPANTYPE type) {
    switch (type) {
      case MD_SPAN_EM:
      case MD_SPAN_STRONG:
        if (!spans_.empty()) {
          spans_.pop_back();
        }
        break;
      case MD_SPAN_A:
        if (!spans_.empty()) {
          spans_.pop_back();
        }
        if (!links_.empty()) {
          if (links_.back()) {
            in_link_ = false;
          }
          links_.pop_back();
        }
        break;
      case MD_SPAN_IMG:
        --swallowing_image_;
        break;
      default:
        break;
    }
  }

  void Text(MD_TEXTTYPE type, const std::string& text) {
    if (swallowing_image_ > 0 || type == MD_TEXT_HTML || type == MD_TEXT_NULLCHAR) {
      return;  // raw HTML is dropped, the way images are
    }
    if (type == MD_TEXT_BR || (type == MD_TEXT_SOFTBR && block_ == Block::kCode)) {
      Append(documents::LineBreak());
      return;
    }
    if (type == MD_TEXT_SOFTBR) {
      AppendRun(" ", false);
      return;
    }
    if (block_ == Block::kCode) {
      // A code block's text arrives line by line, each with its newline.
      std::string line = text;
      const bool ends_line = !line.empty() && line.back() == '\n';
      if (ends_line) {
        line.pop_back();
      }
      if (!line.empty()) {
        AppendRun(line, false);
      }
      if (ends_line) {
        Append(documents::LineBreak());
      }
      return;
    }
    AppendRun(text, type == MD_TEXT_CODE);
  }

  void AppendRun(const std::string& text, bool code) {
    // Visible text of a skipped image never reaches here; an empty run is not worth a node.
    if (text.empty()) {
      return;
    }
    documents::Run run;
    run.Text(winrt::to_hstring(text));
    if (code) {
      run.FontFamily(media::FontFamily(kMonospace));
    }
    Append(run);
  }

  winrt::Microsoft::UI::Xaml::Documents::BlockCollection blocks_;
  documents::Paragraph paragraph_{nullptr};
  std::vector<documents::Span> spans_;
  std::vector<bool> links_;
  std::vector<int> counters_;  // -1 for an unordered list
  std::string pending_marker_;
  Block block_ = Block::kParagraph;
  int list_depth_ = 0;
  int swallowing_image_ = 0;
  bool in_link_ = false;
};

/// Which window a state belongs to.
enum class WindowKind { kNone, kUpdate, kStatus, kPermission };

/// One window, and the controls whose content changes in place.
struct WindowState {
  WindowKind kind = WindowKind::kNone;
  xaml::Window window{nullptr};
  controls::Grid root{nullptr};
  controls::Grid layout{nullptr};
  controls::TextBlock title{nullptr};
  controls::TextBlock heading{nullptr};
  controls::TextBlock detail{nullptr};
  controls::TextBlock error{nullptr};
  controls::Border notes{nullptr};
  controls::RichTextBlock notes_text{nullptr};
  controls::HyperlinkButton notes_link{nullptr};
  controls::CheckBox checkbox{nullptr};
  controls::ProgressBar progress{nullptr};
  controls::TextBlock status{nullptr};
  controls::Panel actions{nullptr};
  controls::Button default_button{nullptr};
  UiEventKind default_event = UiEventKind::kPrimary;
  std::vector<std::string> button_labels;
  std::string notes_markdown;
  bool has_notes = false;
  bool closable = true;
  bool updating_checkbox = false;
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
        try {
          window->window.Close();
        } catch (...) {
        }
      }
    }
  }

  bool IsAvailable() const override { return CanDrawOnCallingThread(); }

  void Draw(const UiContent& content) override {
    Run([this, content] { DrawOnThread(content); }, Work::kDraw);
  }

  void Hide() override {
    Run([this] { CloseAll(); });
  }

  void BringToFront() override {
    Run([this] {
      if (auto* window = WindowOnScreen()) {
        window->window.Activate();
      }
    });
  }

  void QuitApplication() override {
    // The spec's Windows way (specs/updater-ui.md, "退出应用"): WM_CLOSE to every visible
    // top-level window of this process but the module's own, so the host's own close
    // handling — saving, asking — runs as it would for the user.
    Run([this] {
      CloseAll();
      EnumWindows(
          [](HWND window, LPARAM) -> BOOL {
            DWORD process = 0;
            GetWindowThreadProcessId(window, &process);
            if (process == GetCurrentProcessId() && IsWindowVisible(window) &&
                GetWindow(window, GW_OWNER) == nullptr) {
              PostMessageW(window, WM_CLOSE, 0, 0);
            }
            return TRUE;
          },
          0);
      Diagnose("quit: asked the host's windows to close");
    });
  }

  void SetEventSink(std::function<void(const UiEvent&)> sink) override {
    std::lock_guard<std::mutex> lock(sink_mutex_);
    sink_ = std::move(sink);
  }

 private:
  enum class Work { kOther, kDraw };

  bool OnUiThread() const {
    return available_.load() && GetCurrentThreadId() == ui_thread_id_;
  }

  // A native message window keeps dispatch inside the library. Register the class with
  // this module's instance so that statically and dynamically linked builds both work.
  static constexpr UINT kDispatchMessage = WM_APP + 1;

  static LRESULT CALLBACK DispatchWindowProc(HWND window, UINT message, WPARAM wparam,
                                         LPARAM lparam) {
    if (message == WM_NCCREATE) {
      const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
      SetWindowLongPtrW(window, GWLP_USERDATA,
                        reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    } else if (message == kDispatchMessage) {
      auto* backend = reinterpret_cast<WinUi3Backend*>(
          GetWindowLongPtrW(window, GWLP_USERDATA));
      if (backend != nullptr) {
        backend->DrainPending();
      }
      return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
  }

  HWND CreateDispatchWindow() const {
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&DispatchWindowProc), &module)) {
      return nullptr;
    }
    constexpr wchar_t kClassName[] = L"DazzleUpdater.UpdaterUI.Dispatcher";
    WNDCLASSW window_class{};
    window_class.hInstance = module;
    window_class.lpfnWndProc = &DispatchWindowProc;
    window_class.lpszClassName = kClassName;
    if (!RegisterClassW(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
      return nullptr;
    }
    return CreateWindowExW(0, kClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
                           module, const_cast<WinUi3Backend*>(this));
  }

  /// Only a successful probe is remembered: a first call from a worker thread must not
  /// rule out the host's XAML thread for good. Publish the dispatch handle and thread ID
  /// together before worker threads can submit work.
  bool CanDrawOnCallingThread() const {
    if (available_.load()) {
      return true;
    }
    std::lock_guard<std::mutex> lock(probe_mutex_);
    if (available_.load()) {
      return true;
    }
    try {
      xaml::Window probe;
      probe.Close();
      dispatch_window_ = CreateDispatchWindow();
      if (dispatch_window_ == nullptr) {
        Diagnose("could not create the UI dispatch window");
        return false;
      }
      ui_thread_id_ = GetCurrentThreadId();
      available_.store(true);
      Diagnose("the calling thread can hold XAML windows; core dispatch is ready");
    } catch (winrt::hresult_error const& error) {
      Diagnose(std::string("the calling thread cannot hold XAML windows: ") +
               winrt::to_string(error.message()));
    } catch (...) {
      Diagnose("the calling thread cannot hold XAML windows");
    }
    return available_.load();
  }

  void DrainPending() {
    if (!OnUiThread()) {
      return;
    }
    std::deque<std::function<void()>> work;
    {
      std::lock_guard<std::mutex> lock(pending_mutex_);
      work.swap(pending_);
      last_pending_is_draw_ = false;
    }
    if (!work.empty()) {
      Diagnose("dispatch: running queued work");
    }
    for (auto& item : work) {
      RunGuarded(std::move(item));
    }
  }

  /// Consecutive pending draws collapse into the newest content. Other actions retain
  /// their order, so closing or quitting cannot be replaced by a progress redraw.
  void Run(std::function<void()> work, Work kind = Work::kOther) {
    // Queue even calls from the UI thread: a close following a pending worker redraw
    // must run after that redraw, otherwise draining it could reopen the closed window.
    {
      std::lock_guard<std::mutex> lock(pending_mutex_);
      if (kind == Work::kDraw && last_pending_is_draw_ && !pending_.empty()) {
        pending_.back() = std::move(work);
      } else {
        pending_.push_back(std::move(work));
      }
      last_pending_is_draw_ = kind == Work::kDraw;
    }
    if (!PostMessageW(dispatch_window_, kDispatchMessage, 0, 0)) {
      Diagnose("could not post work to the UI thread");
    }
  }

  /// Work on the XAML thread, with the exception handling that keeps a failure in the log
  /// rather than in the process: an escaping exception there ends the application.
  void RunGuarded(std::function<void()> work) {
    try {
      work();
    } catch (winrt::hresult_error const& error) {
      Diagnose(std::string("window work failed: ") + winrt::to_string(error.message()));
    } catch (std::exception const& error) {
      Diagnose(std::string("window work failed: ") + error.what());
    } catch (...) {
      Diagnose("window work failed: unknown exception");
    }
  }

  /// Reports a user action to the state machine. Never from inside Draw/Hide: the state
  /// machine holds its lock then (updater_ui_state.h), so every report is a control's own
  /// event on the UI thread, or queued behind the current one.
  void Report(UiEventKind kind, bool flag = false) {
    std::function<void(const UiEvent&)> sink;
    {
      std::lock_guard<std::mutex> lock(sink_mutex_);
      sink = sink_;
    }
    Diagnose(std::string("event: ") + EventName(kind));
    if (sink) {
      UiEvent event;
      event.kind = kind;
      event.flag = flag;
      sink(event);
    }
  }

  /// The close button and Escape: the same event, and only while the state has a way out
  /// (Installing has none).
  void ReportClose(WindowState& state) {
    if (state.closable) {
      Report(UiEventKind::kClose);
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

  void CloseWindow(WindowState& state) {
    if (state.window) {
      auto window = state.window;
      state = WindowState{};
      window.Close();
    }
  }

  void CloseAll() {
    for (auto* window : {&update_, &status_, &permission_}) {
      CloseWindow(*window);
    }
  }

  /// The drawing itself, always on the thread the windows belong to.
  void DrawOnThread(const UiContent& content) {
    const WindowKind kind = KindOf(content.window);
    if (kind == WindowKind::kNone) {
      return;
    }
    Diagnose(std::string("draw: ") + StateName(content.state));

    WindowState* target = WindowFor(kind);
    WindowState* previous = WindowOnScreen();
    const bool replacing = previous != nullptr && previous != target;
    const bool created = !target->window;
    if (created) {
      BuildWindow(*target, kind, content);
    }
    UpdateWindow(*target, content);
    if (replacing && previous->window) {
      // Centred on the window it replaced, then that one goes away.
      CentreOn(*target, *previous);
      CloseWindow(*previous);
    } else if (created) {
      CentreOnScreen(*target);
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
    auto bar = controls::Grid();
    bar.Height(draws_title_bar ? kTitleBarHeight : 0.0);
    bar.Padding(xaml::Thickness{12, 0, 12, 0});
    if (draws_title_bar) {
      state.title = Text(content.window_title, kCaptionSize);
      state.title.Foreground(SecondaryText());
      state.title.TextWrapping(xaml::TextWrapping::NoWrap);
      state.title.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
      state.title.VerticalAlignment(xaml::VerticalAlignment::Center);
      bar.Children().Append(state.title);
    }
    state.window.ExtendsContentIntoTitleBar(draws_title_bar);
    if (draws_title_bar) {
      state.window.SetTitleBar(bar);
      ThemeCaptionButtons(state.window);
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

    // Escape is the close button; Return presses the default button when the focus is not
    // on a control that takes Return itself (a button, which then presses itself).
    WindowState* self = &state;
    root.KeyDown([this, self](auto&&, input::KeyRoutedEventArgs const& args) {
      if (args.Key() == VirtualKey::Escape) {
        args.Handled(true);
        ReportClose(*self);
      } else if (args.Key() == VirtualKey::Enter && self->default_button) {
        args.Handled(true);
        ReportDefault(*self);
      }
    });
    // The close button: kept open, and reported, so the state machine decides — it closes
    // the window itself (Window::Close, which does not come back here), or keeps it, as it
    // does while installing.
    if (auto app_window = AppWindowOf(state.window)) {
      app_window.Closing([this, self](auto&&, windowing::AppWindowClosingEventArgs const& args) {
        args.Cancel(true);
        // Not from inside the closing event: the report closes this very window.
        Run([this, self] { ReportClose(*self); });
      });
    }

    auto buttons = [&] {
      if (kind == WindowKind::kUpdate) {
        // Skip This Version ··· Later · Install: the skip button at the leading edge, the
        // pair at the trailing one (SUUpdateAlert.xib, and the prototype).
        auto row = controls::Grid();
        row.ColumnSpacing(kButtonGap);
        for (int i = 0; i < 3; ++i) {
          row.ColumnDefinitions().Append(controls::ColumnDefinition());
        }
        row.ColumnDefinitions().GetAt(0).Width(xaml::GridLengthHelper::Auto());
        row.ColumnDefinitions().GetAt(1).Width(
            xaml::GridLengthHelper::FromValueAndType(1, xaml::GridUnitType::Star));
        row.ColumnDefinitions().GetAt(2).Width(xaml::GridLengthHelper::Auto());
        return static_cast<controls::Panel>(row);
      }
      auto row = controls::StackPanel();
      row.Orientation(controls::Orientation::Horizontal);
      row.Spacing(kind == WindowKind::kPermission ? 14.0 : kButtonGap);
      row.HorizontalAlignment(xaml::HorizontalAlignment::Right);
      row.VerticalAlignment(xaml::VerticalAlignment::Bottom);
      return static_cast<controls::Panel>(row);
    };

    auto make_checkbox = [this, self] {
      controls::CheckBox checkbox;
      checkbox.MinHeight(kCheckboxRow);
      checkbox.HorizontalAlignment(xaml::HorizontalAlignment::Left);
      checkbox.Click([this, self](auto&& sender, auto&&) {
        if (self->updating_checkbox) {
          return;
        }
        auto value = sender.template as<controls::CheckBox>().IsChecked();
        Report(UiEventKind::kCheckboxChanged, value && value.Value());
      });
      return checkbox;
    };

    if (kind == WindowKind::kUpdate) {
      auto layout = controls::Grid();
      layout.Padding(xaml::Thickness{kMargin, 0, kMargin, kMargin});
      layout.RowSpacing(kGap);
      for (int i = 0; i < 5; ++i) {
        layout.RowDefinitions().Append(controls::RowDefinition());
        layout.RowDefinitions().GetAt(i).Height(xaml::GridLengthHelper::Auto());
      }
      layout.RowDefinitions().GetAt(1).Height(
          xaml::GridLengthHelper::FromValueAndType(1, xaml::GridUnitType::Star));
      state.layout = layout;

      auto title_row = controls::Grid();
      title_row.ColumnSpacing(kIconGap);
      title_row.ColumnDefinitions().Append(controls::ColumnDefinition());
      title_row.ColumnDefinitions().Append(controls::ColumnDefinition());
      title_row.ColumnDefinitions().GetAt(0).Width(xaml::GridLengthHelper::Auto());
      title_row.ColumnDefinitions().GetAt(1).Width(
          xaml::GridLengthHelper::FromValueAndType(1, xaml::GridUnitType::Star));
      auto image = IconImage();
      controls::Grid::SetColumn(image, 0);
      title_row.Children().Append(image);
      auto text_column = controls::StackPanel();
      text_column.Spacing(kGap);
      text_column.VerticalAlignment(xaml::VerticalAlignment::Center);
      state.heading = Text("", kBodySize);
      state.heading.FontWeight(FontWeights::SemiBold());
      state.detail = Text("", kCaptionSize);
      state.detail.Foreground(SecondaryText());
      state.notes_link = controls::HyperlinkButton();
      state.notes_link.Padding(xaml::Thickness{0, 0, 0, 0});
      state.notes_link.FontSize(kCaptionSize);
      text_column.Children().Append(state.heading);
      text_column.Children().Append(state.detail);
      text_column.Children().Append(state.notes_link);
      controls::Grid::SetColumn(text_column, 1);
      title_row.Children().Append(text_column);
      controls::Grid::SetRow(title_row, 0);
      layout.Children().Append(title_row);

      state.notes = controls::Border();
      state.notes.Background(CardBackground());
      state.notes.BorderBrush(CardStroke());
      state.notes.BorderThickness(xaml::Thickness{1, 1, 1, 1});
      state.notes.CornerRadius(xaml::CornerRadius{kNotesRadius, kNotesRadius, kNotesRadius,
                                                  kNotesRadius});
      state.notes.MinHeight(kNotesMinHeight);
      state.notes_text = controls::RichTextBlock();
      state.notes_text.Padding(xaml::Thickness{12, 10, 12, 10});
      state.notes_text.TextWrapping(xaml::TextWrapping::Wrap);
      state.notes_text.IsTextSelectionEnabled(true);
      auto scroller = controls::ScrollViewer();
      scroller.Content(state.notes_text);
      state.notes.Child(scroller);
      controls::Grid::SetRow(state.notes, 1);
      layout.Children().Append(state.notes);

      state.checkbox = make_checkbox();
      controls::Grid::SetRow(state.checkbox, 2);
      layout.Children().Append(state.checkbox);

      state.actions = buttons();
      state.actions.Margin(xaml::Thickness{0, 4, 0, 0});
      controls::Grid::SetRow(state.actions, 3);
      layout.Children().Append(state.actions);

      controls::Grid::SetRow(layout, 1);
      state.root.Children().Append(layout);
    } else if (kind == WindowKind::kStatus) {
      // SUStatusController: the icon at (20, 4); beside it the heading over the bar, or
      // over the detail, centred against the icon; the status text and the buttons share
      // the last row.
      auto layout = controls::Grid();
      layout.Padding(xaml::Thickness{kMargin, 4, kMargin, kMargin});
      layout.ColumnSpacing(kIconGap);
      layout.RowSpacing(kGap);
      layout.ColumnDefinitions().Append(controls::ColumnDefinition());
      layout.ColumnDefinitions().Append(controls::ColumnDefinition());
      layout.ColumnDefinitions().GetAt(0).Width(xaml::GridLengthHelper::Auto());
      layout.ColumnDefinitions().GetAt(1).Width(
          xaml::GridLengthHelper::FromValueAndType(1, xaml::GridUnitType::Star));
      layout.RowDefinitions().Append(controls::RowDefinition());
      layout.RowDefinitions().Append(controls::RowDefinition());
      layout.RowDefinitions().GetAt(0).Height(xaml::GridLengthHelper::Auto());
      layout.RowDefinitions().GetAt(1).Height(xaml::GridLengthHelper::Auto());

      auto image = IconImage();
      controls::Grid::SetColumn(image, 0);
      layout.Children().Append(image);
      state.heading = Text("", kBodySize);
      state.heading.FontWeight(FontWeights::SemiBold());
      state.progress = controls::ProgressBar();
      state.progress.Margin(xaml::Thickness{0, 4, 0, 0});
      state.detail = Text("", kCaptionSize);
      state.detail.Foreground(SecondaryText());
      state.detail.IsTextSelectionEnabled(true);
      state.error = Text("", kCaptionSize);
      state.error.Foreground(SecondaryText());
      state.error.FontFamily(media::FontFamily(kMonospace));
      state.error.IsTextSelectionEnabled(true);
      auto centred = controls::StackPanel();
      centred.Spacing(kGap);
      centred.VerticalAlignment(xaml::VerticalAlignment::Center);
      centred.Children().Append(state.heading);
      centred.Children().Append(state.progress);
      centred.Children().Append(state.detail);
      centred.Children().Append(state.error);
      auto frame = controls::Grid();
      frame.MinHeight(kIconSize);
      frame.Children().Append(centred);
      controls::Grid::SetColumn(frame, 1);
      layout.Children().Append(frame);

      auto footer = controls::Grid();
      footer.ColumnSpacing(kButtonGap);
      footer.ColumnDefinitions().Append(controls::ColumnDefinition());
      footer.ColumnDefinitions().Append(controls::ColumnDefinition());
      footer.ColumnDefinitions().GetAt(0).Width(
          xaml::GridLengthHelper::FromValueAndType(1, xaml::GridUnitType::Star));
      footer.ColumnDefinitions().GetAt(1).Width(xaml::GridLengthHelper::Auto());
      state.status = Text("", kBodySize);
      state.status.Foreground(SecondaryText());
      state.status.TextWrapping(xaml::TextWrapping::NoWrap);
      state.status.VerticalAlignment(xaml::VerticalAlignment::Center);
      // Tabular digits, so the byte counts do not jitter as they change.
      state.status.FontFamily(media::FontFamily(L"Segoe UI Variable Text, Segoe UI"));
      controls::Grid::SetColumn(state.status, 0);
      footer.Children().Append(state.status);
      state.actions = buttons();
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
      auto image = IconImage();
      controls::Grid::SetColumn(image, 0);
      layout.Children().Append(image);
      auto body = controls::StackPanel();
      body.Spacing(kGap);
      body.Margin(xaml::Thickness{0, 2, 0, 0});
      state.heading = Text("", kBodySize);
      state.heading.FontWeight(FontWeights::SemiBold());
      state.detail = Text("", kCaptionSize);
      state.detail.Foreground(SecondaryText());
      state.checkbox = make_checkbox();
      state.actions = buttons();
      state.actions.Margin(xaml::Thickness{0, 12, 0, 0});
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

  void ReportDefault(WindowState& state) {
    if (state.default_button) {
      Report(state.default_event);
    }
  }

  static xaml::Visibility VisibleIf(bool visible) {
    return visible ? xaml::Visibility::Visible : xaml::Visibility::Collapsed;
  }

  /// The parts that change with a state, updated in place.
  void UpdateWindow(WindowState& state, const UiContent& content) {
    state.closable = content.closable;
    state.window.Title(winrt::to_hstring(content.window_title));
    if (state.title) {
      state.title.Text(winrt::to_hstring(content.window_title));
    }
    if (state.heading) {
      state.heading.Text(winrt::to_hstring(content.heading));
    }
    if (state.detail) {
      state.detail.Text(winrt::to_hstring(content.detail));
      state.detail.Visibility(VisibleIf(!content.detail.empty()));
    }
    if (state.error) {
      state.error.Text(winrt::to_hstring(content.error_message));
      state.error.Visibility(VisibleIf(!content.error_message.empty()));
    }
    state.has_notes = !content.notes_markdown.empty();
    if (state.notes_text && content.notes_markdown != state.notes_markdown) {
      // Only when the notes change: a redraw for the check box must not scroll them back.
      NotesWriter(state.notes_text).Render(content.notes_markdown);
      state.notes_markdown = content.notes_markdown;
    }
    if (state.notes) {
      state.notes.Visibility(VisibleIf(state.has_notes));
    }
    if (state.layout) {
      // The notes row takes the window's spare height only when there are notes: without
      // them the window is as tall as its content, and a star row would open a gap.
      state.layout.RowDefinitions().GetAt(1).Height(
          state.has_notes ? xaml::GridLengthHelper::FromValueAndType(1, xaml::GridUnitType::Star)
                          : xaml::GridLengthHelper::Auto());
    }
    if (state.notes_link) {
      const bool linked = !state.has_notes && !content.notes_url.empty() &&
                          IsLinkableScheme(content.notes_url);
      state.notes_link.Visibility(VisibleIf(linked));
      if (linked) {
        state.notes_link.Content(winrt::box_value(winrt::to_hstring(content.notes_link_label)));
        try {
          state.notes_link.NavigateUri(
              winrt::Windows::Foundation::Uri(winrt::to_hstring(content.notes_url)));
        } catch (...) {
          state.notes_link.Visibility(xaml::Visibility::Collapsed);
        }
      }
    }
    if (state.checkbox) {
      state.checkbox.Visibility(VisibleIf(content.checkbox.visible));
      if (content.checkbox.visible) {
        state.updating_checkbox = true;
        state.checkbox.Content(winrt::box_value(winrt::to_hstring(content.checkbox.label)));
        state.checkbox.IsChecked(content.checkbox.checked);
        state.updating_checkbox = false;
      }
    }
    if (state.progress) {
      state.progress.Visibility(VisibleIf(content.progress.visible));
      state.progress.IsIndeterminate(!content.progress.determinate);
      state.progress.Value(content.progress.fraction * 100.0);
    }
    if (state.status) {
      state.status.Text(winrt::to_hstring(content.progress.status_text));
    }
    if (state.actions && ButtonsChanged(state, content)) {
      FillButtons(state, content);
    }

    ApplyPresenter(state, content);
    ResizeContent(state);
  }

  /// Whether the action row differs from the one on screen: a download's progress redraws
  /// every 100 ms, and rebuilding its Cancel button each time would drop the focus.
  bool ButtonsChanged(WindowState& state, const UiContent& content) {
    std::vector<std::string> labels;
    for (const auto& button : content.buttons) {
      labels.push_back(button.label + (button.is_default ? "*" : ""));
    }
    if (labels == state.button_labels && state.actions.Children().Size() > 0) {
      return false;
    }
    state.button_labels = labels;
    return true;
  }

  void FillButtons(WindowState& state, const UiContent& content) {
    auto children = state.actions.Children();
    children.Clear();
    state.default_button = nullptr;

    std::vector<controls::Button> made;
    controls::StackPanel pair{nullptr};
    if (state.kind == WindowKind::kUpdate) {
      pair = controls::StackPanel();
      pair.Orientation(controls::Orientation::Horizontal);
      pair.Spacing(kButtonGap);
      controls::Grid::SetColumn(pair, 2);
      children.Append(pair);
    }
    const double min_width =
        state.kind == WindowKind::kStatus ? kStatusButtonWidth : kMinButtonWidth;
    for (const auto& button : content.buttons) {
      controls::Button control;
      control.Content(winrt::box_value(winrt::to_hstring(button.label)));
      control.MinWidth(min_width);
      control.MinHeight(kButtonHeight);
      if (button.is_default) {
        // The accent button is WinUI's own style: its hover, pressed, disabled and dark
        // states come with it, which a Background set by hand would lose.
        if (auto style = ThemeResource(L"AccentButtonStyle").try_as<xaml::Style>()) {
          control.Style(style);
        }
        state.default_button = control;
        state.default_event = button.event;
      }
      const UiEventKind kind = button.event;
      control.Click([this, kind](auto&&, auto&&) { Report(kind); });
      if (pair && kind == UiEventKind::kSkip) {
        controls::Grid::SetColumn(control, 0);
        children.Append(control);
      } else if (pair) {
        pair.Children().Append(control);
      } else {
        children.Append(control);
      }
      made.push_back(control);
    }
    // Later and Install are equal in width (SUUpdateAlert.xib), and Skip is no narrower
    // than Install: the widest of them sets the minimum for all.
    if (pair) {
      double widest = kMinButtonWidth;
      for (auto& control : made) {
        control.Measure(winrt::Windows::Foundation::Size{
            std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity()});
        widest = (std::max)(widest, static_cast<double>(control.DesiredSize().Width));
      }
      for (auto& control : made) {
        control.MinWidth(widest);
      }
    }
    // The keyboard starts on the default button — or, in a state without one, on the way
    // out — so Return and Escape reach the window at once.
    controls::Button focus = state.default_button;
    if (!focus && !made.empty()) {
      focus = made.front();
    }
    if (focus) {
      focus.Loaded([](auto&& sender, auto&&) {
        sender.template as<controls::Control>().Focus(xaml::FocusState::Programmatic);
      });
    }
  }

  /// What the window lets the user do with its frame, per state: only the update window
  /// with notes resizes, nothing maximizes, and the status window does not minimize away
  /// from a running download.
  void ApplyPresenter(WindowState& state, const UiContent& content) {
    auto app_window = AppWindowOf(state.window);
    if (!app_window) {
      return;
    }
    if (auto presenter = app_window.Presenter().try_as<windowing::OverlappedPresenter>()) {
      presenter.IsMaximizable(false);
      presenter.IsResizable(state.kind == WindowKind::kUpdate && content.resizable);
      presenter.IsMinimizable(state.kind == WindowKind::kUpdate);
    }
  }

  void ResizeContent(WindowState& state) {
    double width = kUpdateWindowWidth;
    if (state.kind == WindowKind::kStatus) {
      width = kStatusWindowWidth;
    } else if (state.kind == WindowKind::kPermission) {
      width = kPermissionWindowWidth;
    }
    double height = kUpdateWindowHeight;
    if (state.kind != WindowKind::kUpdate || !state.has_notes) {
      // Everything but the update window with notes is as tall as what it shows.
      state.root.Measure(
          winrt::Windows::Foundation::Size{static_cast<float>(width),
                                           std::numeric_limits<float>::infinity()});
      height = std::ceil(state.root.DesiredSize().Height);
    } else if (state.shown) {
      return;  // the user may have resized it; a redraw keeps their size
    }
    auto app_window = AppWindowOf(state.window);
    if (!app_window) {
      return;
    }
    // ResizeClient adds the system caption to what it is asked for, even though the content
    // extends into the title bar and draws its own; the client area then comes out a caption
    // taller than the content. So the request is corrected by what the first one gave.
    const double scale = DpiScale(state.window);
    const winrt::Windows::Graphics::SizeInt32 size{static_cast<int32_t>(std::lround(width * scale)),
                                                   static_cast<int32_t>(std::lround(height * scale))};
    auto current = app_window.ClientSize();
    if (current.Width == size.Width && current.Height == size.Height) {
      return;
    }
    app_window.ResizeClient(size);
    current = app_window.ClientSize();
    if (current.Height != size.Height) {
      app_window.ResizeClient(winrt::Windows::Graphics::SizeInt32{
          size.Width, size.Height - (current.Height - size.Height)});
    }
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
    app_window.Move(winrt::Windows::Graphics::PointInt32{
        position.X + (other_size.Width - size.Width) / 2,
        position.Y + (other_size.Height - size.Height) / 2});
  }

  /// A window that replaces nothing opens in the middle of the work area of the display it
  /// is on, the way a dialog does, rather than wherever the system cascades new windows.
  void CentreOnScreen(WindowState& state) {
    auto app_window = AppWindowOf(state.window);
    if (!app_window) {
      return;
    }
    try {
      auto area = windowing::DisplayArea::GetFromWindowId(app_window.Id(),
                                                           windowing::DisplayAreaFallback::Nearest);
      const auto work = area.WorkArea();
      const auto size = app_window.Size();
      app_window.Move(winrt::Windows::Graphics::PointInt32{
          work.X + (work.Width - size.Width) / 2, work.Y + (work.Height - size.Height) / 3});
    } catch (...) {
    }
  }

  /// The caption buttons are the system's, drawn over content that extends into the title
  /// bar; they follow the application's theme only when told to, and in the dark theme
  /// their default glyphs are dark on dark.
  static void ThemeCaptionButtons(xaml::Window const& window) {
    auto app_window = AppWindowOf(window);
    if (!app_window) {
      return;
    }
    try {
      auto bar = app_window.TitleBar();
      const bool dark = DarkTheme();
      using winrt::Windows::UI::ColorHelper;
      const auto glyph = dark ? ColorHelper::FromArgb(255, 255, 255, 255)
                              : ColorHelper::FromArgb(255, 0, 0, 0);
      const auto inactive = dark ? ColorHelper::FromArgb(255, 150, 150, 150)
                                 : ColorHelper::FromArgb(255, 120, 120, 120);
      const auto hover = dark ? ColorHelper::FromArgb(0x0F, 255, 255, 255)
                              : ColorHelper::FromArgb(0x09, 0, 0, 0);
      const auto transparent = ColorHelper::FromArgb(0, 0, 0, 0);
      bar.ButtonForegroundColor(glyph);
      bar.ButtonHoverForegroundColor(glyph);
      bar.ButtonPressedForegroundColor(glyph);
      bar.ButtonInactiveForegroundColor(inactive);
      bar.ButtonBackgroundColor(transparent);
      bar.ButtonInactiveBackgroundColor(transparent);
      bar.ButtonHoverBackgroundColor(hover);
      bar.ButtonPressedBackgroundColor(hover);
    } catch (...) {
    }
  }

  static void HideCaption(xaml::Window const& window) {
    if (auto app_window = AppWindowOf(window)) {
      if (auto presenter = app_window.Presenter().try_as<windowing::OverlappedPresenter>()) {
        presenter.SetBorderAndTitleBar(true, false);
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
    return static_cast<double>(GetDpiForSystem()) / 96.0;
  }

  WindowState update_;
  WindowState status_;
  WindowState permission_;
  mutable HWND dispatch_window_ = nullptr;
  mutable std::atomic<bool> available_{false};
  mutable std::mutex probe_mutex_;
  mutable DWORD ui_thread_id_ = 0;
  std::mutex pending_mutex_;
  std::deque<std::function<void()>> pending_;
  bool last_pending_is_draw_ = false;
  std::mutex sink_mutex_;
  std::function<void(const UiEvent&)> sink_;
};

UiBackend& GetUiBackend() {
  // The runner and its worker live until process exit. Keep the backend alive too:
  // static destruction may happen on a thread that does not own the XAML windows.
  static WinUi3Backend* backend = new WinUi3Backend();
  return *backend;
}

}  // namespace internal
}  // namespace dazzle_updater
