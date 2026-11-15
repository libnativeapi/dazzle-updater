#include "../../foundation/updater_ui_backend.h"
#include "../../markdown/markdown.h"
#include "../../updater_ui_state.h"

#import <AppKit/AppKit.h>

#include <cmath>
#include <string>

// UpdaterUI's windows on macOS: two NSWindows and the one-time question, built from
// AppKit's own controls and laid out exactly as Sparkle 2.x lays out its windows
// (../../specs/updater-ui.md, "外观"): SUUpdateAlert.xib for the update window,
// SUStatusController.m for the status window, SUUpdatePermissionPrompt.xib for the
// question. Nothing in here is drawn by the module — the push buttons, the check box,
// the progress bar, the box around the release notes and the text in it are the system's,
// which is what makes the windows look like Sparkle's on every macOS release: the system
// draws them the way it draws Sparkle's.
//
// Sparkle lays its windows out with Auto Layout. The same metrics are applied here by
// hand, on alignment rectangles (what Auto Layout constrains), so a constant from one of
// Sparkle's constraints — "the buttons' bottom is 20 from the window's" — means the same
// thing in this file.
//
// Every method of the backend is called from any thread — the state machine runs on its
// worker — and does its work on the main thread, which is the only thread AppKit allows:
// each entry point copies what it needs and posts a block. Blocks run in the order they
// were posted, so the states arrive in the order the state machine drew them.
//
// The backend never reports an event from inside Draw/Hide/BringToFront/QuitApplication
// (the state machine holds its lock there): a close we asked for is made silent, and the
// user's own gestures arrive later, on the main thread.

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

// --- metrics: Sparkle's constraints, by file ---------------------------------------------

// Shared by the three windows.
constexpr CGFloat kMargin = 20;        // the standard 20 pt to the window's edges
constexpr CGFloat kIconSize = 64;      // the application icon
constexpr CGFloat kIconGap = 8;        // the icon and the text column beside it
constexpr CGFloat kTextGap = 8;        // a heading and the line under it
constexpr CGFloat kButtonGap = 12;     // between two buttons

// SUUpdateAlert.xib: a vertical NSStackView (8 pt apart, 12 after the title row and after
// the notes box) holding the title row, the notes box, the check box row and the buttons.
constexpr CGFloat kAvailableWidth = 532;
constexpr CGFloat kAvailableHeight = 370;
constexpr CGFloat kAvailableMinWidth = 462;
constexpr CGFloat kAvailableMinHeight = 150;
constexpr CGFloat kStackGap = 8;
constexpr CGFloat kGroupGap = 12;      // SUUpdateAlertGroupElementSpacing
constexpr CGFloat kNotesMinHeight = 200;
constexpr CGFloat kNotesRadius = 6;
constexpr CGFloat kNotesBorder = 1;
constexpr CGFloat kDefaultButtonMinWidth = 120;  // Install Update: width >= 120

// SUStatusController.m.
constexpr CGFloat kStatusWidth = 400;
constexpr CGFloat kStatusMinHeight = 107;
constexpr CGFloat kStatusIconTop = 4;
constexpr CGFloat kStatusButtonGap = 8;          // the button under the title block
constexpr CGFloat kStatusTextGap = 25;           // the status text and the button
constexpr CGFloat kStatusButtonMinWidth = 100;

// SUUpdatePermissionPrompt.xib: a stack with no spacing; each row carries its own.
constexpr CGFloat kPermissionWidth = 437;
constexpr CGFloat kPermissionIconLeft = 23;
constexpr CGFloat kPermissionIconTop = 18;
constexpr CGFloat kPermissionTextLeft = 106;     // icon trailing + 19
constexpr CGFloat kPermissionTextTop = 20;
constexpr CGFloat kPermissionDetailMinHeight = 34;
constexpr CGFloat kPermissionCheckboxTop = 6;
constexpr CGFloat kPermissionButtonsTop = 15;
constexpr CGFloat kPermissionButtonGap = 14;
constexpr CGFloat kPermissionDefaultMinWidth = 150;
constexpr CGFloat kPermissionOtherMinWidth = 101;

// The release notes, as SUTextViewReleaseNotesView draws Markdown in an NSTextView.
constexpr CGFloat kNotesInsetX = 4;
constexpr CGFloat kNotesInsetY = 8;

constexpr NSInteger kNotesLinkTag = 1000;   // a tag no UiEventKind uses

// --- state shared with the delegate, main thread only --------------------------------

NSWindow* g_window = nil;                  // the window on screen, if any
UiWindow g_window_kind = UiWindow::kNone;  // which of the three it is
UiState g_state = UiState::kChecking;      // the state it draws
NSTextField* g_heading = nil;              // what an update in place touches
NSTextField* g_detail = nil;
NSTextField* g_status = nil;
NSProgressIndicator* g_progress = nil;
NSButton* g_checkbox = nil;
NSString* g_notes_url = nil;               // the "View Release Notes" link's target
CGFloat g_available_min_height = kAvailableMinHeight;  // the rows around the notes, plus 200
BOOL g_ignoring_close = NO;                // a close we asked for is not the user's
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

NSString* ToNSString(const std::string& value) {
  return [NSString stringWithUTF8String:value.c_str()];
}

void ForgetViews() {
  g_heading = nil;
  g_detail = nil;
  g_status = nil;
  g_progress = nil;
  g_checkbox = nil;
  g_notes_url = nil;
}

}  // namespace

// --- the window and its events --------------------------------------------------------

@interface DazzleUpdaterWindowDelegate : NSObject <NSWindowDelegate, NSTextViewDelegate>
@end

@implementation DazzleUpdaterWindowDelegate

- (void)windowWillClose:(NSNotification*)notification {
  if (g_ignoring_close) {
    return;  // the module closed the window; the state machine already knows
  }
  ReportEvent(UiEventKind::kClose);
}

- (void)controlPressed:(NSButton*)sender {
  if (sender.tag == kNotesLinkTag) {
    if (g_notes_url != nil) {
      [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:g_notes_url]];
    }
    return;
  }
  ReportEvent(static_cast<UiEventKind>(sender.tag));
}

- (void)checkboxToggled:(NSButton*)sender {
  ReportEvent(static_cast<UiEventKind>(sender.tag), sender.state == NSControlStateValueOn);
}

- (BOOL)textView:(NSTextView*)textView clickedOnLink:(id)link atIndex:(NSUInteger)charIndex {
  NSURL* url = [link isKindOfClass:[NSURL class]] ? (NSURL*)link
                                                  : [NSURL URLWithString:[link description]];
  if (url != nil) {
    [[NSWorkspace sharedWorkspace] openURL:url];
  }
  return YES;
}

@end

/// A view whose origin is its top-left corner, so the layout below reads the way
/// Sparkle's constraints do (top to bottom). The return and escape keys are AppKit's own
/// business: the default button carries "\r" and the state's way out "\033" as their key
/// equivalents, and the window presses them.
@interface DazzleUpdaterFlippedView : NSView
@end

@implementation DazzleUpdaterFlippedView
- (BOOL)isFlipped {
  return YES;
}
@end

namespace {

DazzleUpdaterWindowDelegate* Delegate() {
  static DazzleUpdaterWindowDelegate* delegate = [[DazzleUpdaterWindowDelegate alloc] init];
  return delegate;
}

// --- alignment rectangles -------------------------------------------------------------
//
// Auto Layout constrains a view's alignment rectangle — the frame minus the padding a
// control keeps around its bezel for shadows and focus rings — and so does this layout:
// every number from Sparkle's constraints is applied to alignment rectangles, and only the
// frame that results is set.

NSSize AlignmentSize(NSView* view) {
  return [view alignmentRectForFrame:view.frame].size;
}

/// Places `view` so that its alignment rectangle is `rect` (in the flipped superview).
void PlaceAligned(NSView* view, NSRect rect, NSAutoresizingMaskOptions mask) {
  view.frame = [view frameForAlignmentRect:rect];
  view.autoresizingMask = mask;
}

/// Sizes a control to its content, then widens it to `min_width` if it is narrower; both
/// on the alignment rectangle.
void FitControl(NSControl* control, CGFloat min_width) {
  [control sizeToFit];
  NSSize size = AlignmentSize(control);
  if (size.width < min_width) {
    size.width = min_width;
    control.frame = [control frameForAlignmentRect:NSMakeRect(0, 0, size.width, size.height)];
  }
}

void SetAlignedWidth(NSControl* control, CGFloat width) {
  NSSize size = AlignmentSize(control);
  control.frame = [control frameForAlignmentRect:NSMakeRect(0, 0, width, size.height)];
}

/// Gives the root its final size without letting autoresizing move the rows that were
/// just placed at their final positions; from here on the masks apply to the user's
/// resizing only.
void FinishRoot(NSView* root, NSSize size) {
  root.autoresizesSubviews = NO;
  [root setFrameSize:size];
  root.autoresizesSubviews = YES;
}

// --- small builders -------------------------------------------------------------------

/// A label that wraps inside `width` and measures itself, so the layout below can stack
/// its rows without a constraint solver.
NSTextField* MakeLabel(NSString* text, NSFont* font, NSColor* color, bool wraps) {
  NSTextField* field = [NSTextField labelWithString:text];
  field.font = font;
  field.textColor = color;
  field.lineBreakMode = wraps ? NSLineBreakByWordWrapping : NSLineBreakByTruncatingTail;
  field.usesSingleLineMode = !wraps;
  if (wraps) {
    field.preferredMaxLayoutWidth = 0;
  }
  return field;
}

CGFloat TextHeight(NSTextField* field, CGFloat width) {
  return ceil([field sizeThatFits:NSMakeSize(width, CGFLOAT_MAX)].height);
}

// Sparkle's fonts: the system font, bold for a window's title line ("systemBold" in the
// xibs), the small system font for the line under it ("smallSystem"), and the system font
// with monospaced digits for the status text — all in controlTextColor.
NSFont* HeadingFont() { return [NSFont boldSystemFontOfSize:[NSFont systemFontSize]]; }
NSFont* DetailFont() { return [NSFont systemFontOfSize:[NSFont smallSystemFontSize]]; }
NSFont* StatusFont() {
  return [NSFont monospacedDigitSystemFontOfSize:[NSFont systemFontSize]
                                          weight:NSFontWeightRegular];
}
NSColor* TextColor() { return [NSColor controlTextColor]; }

/// The application's icon, or AppKit's generic one when the running executable is not a
/// bundle that ships one (NSApplicationIcon in the xibs).
NSImageView* MakeAppIcon() {
  NSImage* image = [NSApp applicationIconImage];
  if (image == nil) {
    image = [NSImage imageNamed:NSImageNameApplicationIcon];
  }
  NSImageView* view = [[NSImageView alloc] initWithFrame:NSMakeRect(0, 0, kIconSize, kIconSize)];
  view.image = image;
  view.imageScaling = NSImageScaleProportionallyUpOrDown;
  return view;
}

/// A push button as Sparkle's xibs declare them: bezel "rounded", the system font, and
/// the return key on the default one — which is also what makes AppKit draw it in the
/// accent colour. `large` is what Sparkle gives the update and status windows' buttons
/// on macOS 26 (SUUpdateAlert.m, SUStatusController.m); the permission prompt keeps
/// the regular size.
NSButton* MakePushButton(const UiButton& button, id target, bool large) {
  NSButton* view = [NSButton buttonWithTitle:ToNSString(button.label)
                                      target:target
                                      action:@selector(controlPressed:)];
  view.bezelStyle = NSBezelStyleRounded;
  view.tag = static_cast<NSInteger>(button.event);
  if (large) {
    if (@available(macOS 26.0, *)) {
      view.controlSize = NSControlSizeLarge;
    }
  }
  if (button.is_default) {
    view.keyEquivalent = @"\r";
  }
  [view sizeToFit];
  return view;
}

/// The check box of both windows: AppKit's small one, in the small system font.
NSButton* MakeCheckbox(const UiCheckbox& checkbox, id target) {
  NSButton* view = [NSButton checkboxWithTitle:ToNSString(checkbox.label)
                                        target:target
                                        action:@selector(checkboxToggled:)];
  view.controlSize = NSControlSizeSmall;
  view.font = DetailFont();
  view.tag = static_cast<NSInteger>(UiEventKind::kCheckboxChanged);
  view.state = checkbox.checked ? NSControlStateValueOn : NSControlStateValueOff;
  [view sizeToFit];
  return view;
}

/// The bar-style NSProgressIndicator of SUStatusController, animated on its own thread.
NSProgressIndicator* MakeProgress(const UiProgress& progress) {
  NSProgressIndicator* view = [[NSProgressIndicator alloc] initWithFrame:NSZeroRect];
  view.style = NSProgressIndicatorStyleBar;
  view.wantsLayer = YES;
  view.usesThreadedAnimation = YES;
  view.minValue = 0;
  view.maxValue = 1;
  view.indeterminate = !progress.determinate;
  view.doubleValue = progress.fraction;
  [view sizeToFit];
  [view startAnimation:nil];
  return view;
}

void UpdateProgress(NSProgressIndicator* view, const UiProgress& progress) {
  view.indeterminate = !progress.determinate;
  view.doubleValue = progress.fraction;
  [view startAnimation:nil];
}

// --- the release notes ----------------------------------------------------------------
//
// Sparkle's SUTextViewReleaseNotesView: Foundation parses the Markdown
// (-initWithMarkdownString:), and the paragraphs are then styled from the presentation
// intents it leaves behind — a header is the bold system font at 1.5/1.3/1.2/1.1 of the
// body size with 0.8 em above and below, a paragraph has 0.5 em above and below (0.3 em
// after an item's paragraph), a list item is indented 1.5 em per level with a Menlo
// bullet or its ordinal before a tab, a code block is the monospaced system font and
// 0.25 em after. Bold, italic, inline code and links are carried by the attributed string
// and drawn by TextKit. Below macOS 12 the Markdown goes through HTML and the HTML
// importer instead, restyled to the same sizes.

#if defined(__MAC_OS_X_VERSION_MAX_ALLOWED) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 120000
#define DAZZLE_HAS_FOUNDATION_MARKDOWN 1
#else
#define DAZZLE_HAS_FOUNDATION_MARKDOWN 0
#endif

#if DAZZLE_HAS_FOUNDATION_MARKDOWN

API_AVAILABLE(macos(12.0))
void FormatMarkdownFragment(NSAttributedString* fragment, NSMutableAttributedString* output,
                            NSMutableParagraphStyle* style, bool can_process_list_item,
                            NSMutableSet<NSNumber*>* visited_list_items,
                            NSPresentationIntent* intent, NSFont* paragraph_font,
                            NSFont* monospaced_font, NSAttributedString* tab,
                            NSAttributedString* bullet) {
  bool is_list_item = false;
  NSFont* font = paragraph_font;
  switch (intent.intentKind) {
    case NSPresentationIntentKindHeader:
      switch (intent.headerLevel) {
        case 1:
          font = [NSFont boldSystemFontOfSize:paragraph_font.pointSize * 1.5];
          break;
        case 2:
          font = [NSFont boldSystemFontOfSize:paragraph_font.pointSize * 1.3];
          break;
        case 3:
          font = [NSFont boldSystemFontOfSize:paragraph_font.pointSize * 1.2];
          break;
        default:
          font = [NSFont boldSystemFontOfSize:paragraph_font.pointSize * 1.1];
          break;
      }
      break;
    case NSPresentationIntentKindListItem:
      is_list_item = true;
      break;
    default:
      break;
  }

  // The parent first: a paragraph's intent may be a list item or a block quote.
  NSPresentationIntent* parent = intent.parentIntent;
  if (parent != nil) {
    FormatMarkdownFragment(fragment, output, style, can_process_list_item && !is_list_item,
                           visited_list_items, parent, font, monospaced_font, tab, bullet);
  }

  switch (intent.intentKind) {
    case NSPresentationIntentKindHeader: {
      const CGFloat spacing = font.pointSize * 0.8;
      style.paragraphSpacingBefore += spacing;
      style.paragraphSpacing += spacing;
      NSMutableAttributedString* text = [fragment mutableCopy];
      [text addAttributes:@{NSFontAttributeName : font} range:NSMakeRange(0, text.length)];
      [output appendAttributedString:text];
      break;
    }
    case NSPresentationIntentKindParagraph: {
      if (parent != nil && parent.intentKind == NSPresentationIntentKindListItem) {
        style.paragraphSpacing += font.pointSize * 0.3;
      } else {
        const CGFloat spacing = font.pointSize * 0.5;
        style.paragraphSpacing += spacing;
        style.paragraphSpacingBefore += spacing;
      }
      NSMutableAttributedString* text = [fragment mutableCopy];
      [text addAttributes:@{NSFontAttributeName : font} range:NSMakeRange(0, text.length)];
      [output appendAttributedString:text];
      break;
    }
    case NSPresentationIntentKindListItem: {
      if (can_process_list_item) {
        const CGFloat first_line_indent = intent.indentationLevel * (font.pointSize * 1.5);
        style.firstLineHeadIndent += first_line_indent;
        const CGFloat tab_interval = style.defaultTabInterval;
        style.headIndent += ceil(first_line_indent / tab_interval) * tab_interval;

        NSNumber* identity = @(intent.identity);
        const bool visited = [visited_list_items containsObject:identity];
        const bool unordered =
            parent == nil || parent.intentKind == NSPresentationIntentKindUnorderedList;
        if (!visited) {
          if (unordered) {
            [output appendAttributedString:bullet];
          } else {
            NSString* ordinal = [NSString stringWithFormat:@"%ld", (long)intent.ordinal];
            [output appendAttributedString:[[NSAttributedString alloc]
                                               initWithString:[ordinal stringByAppendingString:@"."]
                                                   attributes:@{NSFontAttributeName : font}]];
          }
          [visited_list_items addObject:identity];
        }
        [output appendAttributedString:tab];
      }
      break;
    }
    case NSPresentationIntentKindBlockQuote:
      style.firstLineHeadIndent += style.defaultTabInterval;
      style.headIndent += style.defaultTabInterval;
      break;
    case NSPresentationIntentKindCodeBlock: {
      style.paragraphSpacing += font.pointSize * 0.25;
      [output appendAttributedString:tab];
      style.headIndent += style.defaultTabInterval;
      NSMutableAttributedString* text = [fragment mutableCopy];
      [text addAttributes:@{
        NSFontAttributeName : monospaced_font,
        NSForegroundColorAttributeName : [NSColor labelColor]
      }
                    range:NSMakeRange(0, text.length)];
      [output appendAttributedString:text];
      break;
    }
    default:
      break;
  }
}

API_AVAILABLE(macos(12.0))
NSAttributedString* FormatMarkdownAttributedString(NSAttributedString* original,
                                                   CGFloat font_size) {
  NSFont* paragraph_font = [NSFont systemFontOfSize:font_size];
  NSFont* monospaced_font = [NSFont monospacedSystemFontOfSize:font_size
                                                        weight:NSFontWeightRegular];
  NSMutableAttributedString* output = [[NSMutableAttributedString alloc] init];
  NSAttributedString* newline = [[NSAttributedString alloc] initWithString:@"\n"];
  // The bullet is too small in the system font; Menlo's is bigger at the same size.
  NSFont* bullet_font = [NSFont fontWithName:@"Menlo Regular" size:font_size];
  NSAttributedString* bullet = [[NSAttributedString alloc]
      initWithString:@"•"
          attributes:@{NSFontAttributeName : bullet_font != nil ? bullet_font : paragraph_font}];
  NSAttributedString* tab =
      [[NSAttributedString alloc] initWithString:@"\t"
                                      attributes:@{NSFontAttributeName : paragraph_font}];
  NSMutableSet<NSNumber*>* visited = [[NSMutableSet alloc] init];

  [original enumerateAttribute:NSPresentationIntentAttributeName
                       inRange:NSMakeRange(0, original.length)
                       options:0
                    usingBlock:^(NSPresentationIntent* intent, NSRange intent_range, BOOL* stop) {
                      // Every line is its own paragraph, so a code block's lines each get
                      // their indentation.
                      [original.string
                          enumerateSubstringsInRange:intent_range
                                             options:NSStringEnumerationByLines
                                          usingBlock:^(NSString* substring, NSRange range,
                                                       NSRange enclosing, BOOL* stop_lines) {
                                            if (output.length > 0) {
                                              [output appendAttributedString:newline];
                                            }
                                            NSAttributedString* fragment =
                                                [original attributedSubstringFromRange:range];
                                            NSMutableParagraphStyle* style =
                                                [[NSMutableParagraphStyle alloc] init];
                                            style.tabStops = @[];
                                            style.defaultTabInterval = paragraph_font.pointSize * 1.38;
                                            const NSUInteger before = output.length;
                                            FormatMarkdownFragment(fragment, output, style, true,
                                                                   visited, intent, paragraph_font,
                                                                   monospaced_font, tab, bullet);
                                            [output addAttributes:@{NSParagraphStyleAttributeName : style}
                                                            range:NSMakeRange(before, output.length - before)];
                                          }];
                    }];
  return output;
}

#endif  // DAZZLE_HAS_FOUNDATION_MARKDOWN

/// Below macOS 12: Markdown → HTML → attributed string, restyled to Sparkle's sizes. The
/// HTML importer brings fonts, colours and list markers of its own, so they are replaced.
NSAttributedString* NotesFromHtml(const std::string& markdown, CGFloat font_size) {
  NSString* html = ToNSString(dazzle_updater::MarkdownToHtml(markdown));
  NSData* data = [html dataUsingEncoding:NSUTF8StringEncoding];
  NSMutableAttributedString* text = [[NSMutableAttributedString alloc] initWithHTML:data
                                                                documentAttributes:nil];
  if (text == nil) {
    return [[NSAttributedString alloc] initWithString:@""];
  }
  const NSRange all = NSMakeRange(0, text.length);
  [text beginEditing];
  [text removeAttribute:NSForegroundColorAttributeName range:all];
  [text enumerateAttribute:NSFontAttributeName
                   inRange:all
                   options:0
                usingBlock:^(id value, NSRange range, BOOL* stop) {
                  NSFont* font = (NSFont*)value;
                  NSFontManager* manager = [NSFontManager sharedFontManager];
                  NSFont* replacement = nil;
                  if (font != nil && ([manager traitsOfFont:font] & NSFixedPitchFontMask)) {
                    replacement = [NSFont monospacedSystemFontOfSize:font_size
                                                               weight:NSFontWeightRegular];
                  } else {
                    // The importer's sizes (24/18/14/12 for h1/h2/h3/body) tell a heading
                    // from body text; Sparkle's ratios are 1.5/1.3/1.2/1.1.
                    const CGFloat size = font != nil ? font.pointSize : 12;
                    CGFloat scale = 1;
                    if (size >= 23) {
                      scale = 1.5;
                    } else if (size >= 17) {
                      scale = 1.3;
                    } else if (size >= 13.5) {
                      scale = 1.2;
                    }
                    const NSFontTraitMask traits =
                        font != nil ? ([manager traitsOfFont:font] & (NSBoldFontMask | NSItalicFontMask))
                                    : 0;
                    replacement = scale > 1 ? [NSFont boldSystemFontOfSize:font_size * scale]
                                            : [manager convertFont:[NSFont systemFontOfSize:font_size]
                                                       toHaveTrait:traits];
                  }
                  if (replacement != nil) {
                    [text addAttribute:NSFontAttributeName value:replacement range:range];
                  }
                }];
  // Paragraph spacing as above: a heading 0.8 em around, a paragraph 0.5 em.
  NSString* plain = text.string;
  NSUInteger start = 0;
  while (start < plain.length) {
    const NSRange line = [plain lineRangeForRange:NSMakeRange(start, 0)];
    if (line.length > 0) {
      NSMutableParagraphStyle* style =
          [[text attribute:NSParagraphStyleAttributeName atIndex:line.location effectiveRange:NULL]
              mutableCopy];
      if (style == nil) {
        style = [[NSMutableParagraphStyle alloc] init];
      }
      NSFont* font = [text attribute:NSFontAttributeName atIndex:line.location effectiveRange:NULL];
      const CGFloat size = font != nil ? font.pointSize : font_size;
      const CGFloat spacing = size > font_size + 0.5 ? size * 0.8 : size * 0.5;
      style.paragraphSpacingBefore = spacing;
      style.paragraphSpacing = spacing;
      if (style.textLists.count > 0) {
        // The importer leaves the marker in the text and indents at its own tab stops:
        // Sparkle's 1.5 em per level, marker then tab then text.
        const NSInteger level = MAX(1, (NSInteger)lround(style.headIndent / 36.0));
        const CGFloat indent = level * font_size * 1.5;
        const CGFloat interval = font_size * 1.38;
        [style setTextLists:@[]];
        style.firstLineHeadIndent = indent;
        style.headIndent = ceil(indent / interval) * interval + interval;
        style.tabStops = @[];
        style.defaultTabInterval = interval;
        style.paragraphSpacingBefore = 0;
        style.paragraphSpacing = size * 0.3;
      }
      [text addAttribute:NSParagraphStyleAttributeName value:style range:line];
    }
    start = NSMaxRange(line);
  }
  [text endEditing];
  return text;
}

NSAttributedString* NotesAttributedString(const UiContent& content) {
  const CGFloat font_size = [NSFont systemFontSize];
#if DAZZLE_HAS_FOUNDATION_MARKDOWN
  if (@available(macOS 12.0, *)) {
    NSError* error = nil;
    NSAttributedString* parsed =
        [[NSAttributedString alloc] initWithMarkdownString:ToNSString(content.notes_markdown)
                                                   options:nil
                                                   baseURL:nil
                                                     error:&error];
    if (parsed != nil) {
      return FormatMarkdownAttributedString(parsed, font_size);
    }
  }
#endif
  return NotesFromHtml(content.notes_markdown, font_size);
}

/// The notes box: Sparkle's NSBox — custom, 6 pt corners, a 1 pt separator border, the
/// text background, the content clipped to the inner radius — around the read-only text
/// view of SUTextViewReleaseNotesView (inset 4 × 8, a vertical scroller only).
NSView* MakeNotesBox(const UiContent& content, id target) {
  NSBox* box = [[NSBox alloc] initWithFrame:NSMakeRect(0, 0, 0, kNotesMinHeight)];
  box.boxType = NSBoxCustom;
  box.titlePosition = NSNoTitle;
  box.cornerRadius = kNotesRadius;
  box.borderWidth = kNotesBorder;
  box.borderColor = [NSColor separatorColor];
  box.fillColor = [NSColor textBackgroundColor];
  box.contentViewMargins = NSZeroSize;
  box.contentView.wantsLayer = YES;
  box.contentView.layer.masksToBounds = YES;
  box.contentView.layer.cornerRadius = kNotesRadius - kNotesBorder;

  NSScrollView* scroll = [[NSScrollView alloc] initWithFrame:box.contentView.bounds];
  scroll.hasVerticalScroller = YES;
  scroll.hasHorizontalScroller = NO;
  scroll.autohidesScrollers = YES;
  scroll.drawsBackground = NO;
  scroll.borderType = NSNoBorder;
  scroll.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

  const NSSize content_size = scroll.contentSize;
  NSTextView* text_view =
      [[NSTextView alloc] initWithFrame:NSMakeRect(0, 0, content_size.width, content_size.height)];
  text_view.minSize = NSMakeSize(0, content_size.height);
  text_view.maxSize = NSMakeSize(CGFLOAT_MAX, CGFLOAT_MAX);
  text_view.verticallyResizable = YES;
  text_view.horizontallyResizable = NO;
  text_view.autoresizingMask = NSViewWidthSizable;
  text_view.textContainer.widthTracksTextView = YES;
  text_view.textContainerInset = NSMakeSize(kNotesInsetX, kNotesInsetY);
  text_view.continuousSpellCheckingEnabled = NO;
  text_view.usesFontPanel = NO;
  text_view.editable = NO;
  text_view.selectable = YES;  // a link can be clicked and a note copied
  text_view.drawsBackground = NO;
  text_view.usesAdaptiveColorMappingForDarkAppearance = YES;
  text_view.delegate = target;  // clickedOnLink: opens the system browser
  [text_view.textStorage setAttributedString:NotesAttributedString(content)];

  scroll.documentView = text_view;
  [box.contentView addSubview:scroll];
  return box;
}

// --- layout ---------------------------------------------------------------------------

/// The heading, the small line under it and (ours) the error message, stacked at `x`,
/// `y` inside `width`; returns their total height and hands back the labels an update in
/// place would touch. `measure_only` lays nothing out.
CGFloat PlaceHeading(NSView* root, const UiContent& content, CGFloat x, CGFloat y, CGFloat width,
                     NSAutoresizingMaskOptions mask, NSTextField** heading_out,
                     NSTextField** detail_out) {
  NSTextField* heading = MakeLabel(ToNSString(content.heading), HeadingFont(), TextColor(), true);
  const CGFloat heading_height = TextHeight(heading, width);
  CGFloat height = heading_height;
  if (root != nil) {
    PlaceAligned(heading, NSMakeRect(x, y, width, heading_height), mask);
    [root addSubview:heading];
  }
  if (heading_out != nullptr) {
    *heading_out = heading;
  }
  if (detail_out != nullptr) {
    *detail_out = nil;
  }
  if (!content.detail.empty()) {
    NSTextField* detail = MakeLabel(ToNSString(content.detail), DetailFont(), TextColor(), true);
    const CGFloat detail_height = TextHeight(detail, width);
    if (root != nil) {
      PlaceAligned(detail, NSMakeRect(x, y + height + kTextGap, width, detail_height), mask);
      [root addSubview:detail];
    }
    if (detail_out != nullptr) {
      *detail_out = detail;
    }
    height += kTextGap + detail_height;
  }
  if (!content.error_message.empty()) {
    // GetErrorMessage(), selectable so it can be copied into a report; Sparkle shows it
    // as an alert's informative text, in the small system font.
    NSTextField* message = MakeLabel(ToNSString(content.error_message), DetailFont(),
                                     [NSColor secondaryLabelColor], true);
    message.selectable = YES;
    const CGFloat message_height = TextHeight(message, width);
    if (root != nil) {
      PlaceAligned(message, NSMakeRect(x, y + height + kTextGap, width, message_height), mask);
      [root addSubview:message];
    }
    height += kTextGap + message_height;
  }
  return height;
}

CGFloat MeasureHeading(const UiContent& content, CGFloat width) {
  return PlaceHeading(nil, content, 0, 0, width, 0, nullptr, nullptr);
}

NSArray<NSButton*>* MakeButtons(const UiContent& content, bool large) {
  NSMutableArray<NSButton*>* buttons = [NSMutableArray array];
  for (const UiButton& button : content.buttons) {
    [buttons addObject:MakePushButton(button, Delegate(), large)];
  }
  return buttons;
}

/// Gives the escape key to this state's way out: "以后再说", "跳过此版本" or "取消", which
/// the spec makes the same as Esc and the close button.
void GiveEscapeKey(NSArray<NSButton*>* buttons) {
  for (NSButton* button in buttons) {
    const UiEventKind kind = static_cast<UiEventKind>(button.tag);
    if (kind == UiEventKind::kSecondary || kind == UiEventKind::kCancel ||
        kind == UiEventKind::kSkip) {
      button.keyEquivalent = @"\033";
      return;
    }
  }
}

/// Lays the action row out from the trailing edge, `gap` apart, with the buttons'
/// alignment rectangles' tops at `y`; returns where the leftmost one starts.
CGFloat PlaceButtons(NSArray<NSButton*>* buttons, NSView* root, CGFloat right, CGFloat y,
                     CGFloat gap) {
  CGFloat x = right;
  for (NSButton* button in [buttons reverseObjectEnumerator]) {
    const NSSize size = AlignmentSize(button);
    x -= size.width;
    PlaceAligned(button, NSMakeRect(x, y, size.width, size.height),
                 NSViewMinXMargin | NSViewMinYMargin);
    [root addSubview:button];
    x -= gap;
  }
  return x + gap;
}

CGFloat ButtonsHeight(NSArray<NSButton*>* buttons) {
  CGFloat height = 0;
  for (NSButton* button in buttons) {
    height = MAX(height, AlignmentSize(button).height);
  }
  return height;
}

/// The UpdateAvailableWindow, SUUpdateAlert.xib's stack: the title row (icon and the
/// two lines centred on it), 12, the notes box, 12, the check box row, 8, the buttons —
/// Skip This Version on the left, the two others on the right, Remind Me Later as wide
/// as Install Update and Install Update at least 120. With notes the window is 532 × 370
/// and resizes, the box taking what the rows around it leave (never under 200); without
/// them the window is as tall as its rows.
NSView* MakeAvailableView(const UiContent& content, NSSize* size_out) {
  const CGFloat width = kAvailableWidth;
  const CGFloat text_x = kMargin + kIconSize + kIconGap;
  const CGFloat text_width = width - text_x - kMargin;

  DazzleUpdaterFlippedView* root =
      [[DazzleUpdaterFlippedView alloc] initWithFrame:NSMakeRect(0, 0, width, kAvailableHeight)];

  // The title row: 64 tall, the text block centred on the icon.
  const CGFloat text_height = MeasureHeading(content, text_width);
  const CGFloat row_height = MAX(kIconSize, text_height);
  NSImageView* icon = MakeAppIcon();
  PlaceAligned(icon, NSMakeRect(kMargin, (row_height - kIconSize) / 2, kIconSize, kIconSize),
               NSViewMaxXMargin | NSViewMaxYMargin);
  [root addSubview:icon];
  PlaceHeading(root, content, text_x, (row_height - text_height) / 2, text_width,
               NSViewWidthSizable | NSViewMaxYMargin, nullptr, nullptr);
  CGFloat y = row_height + kGroupGap;

  // The rows under the box are measured first: the box gets what they leave. The
  // buttons are large on macOS 26, as SUUpdateAlert.m makes them there.
  NSArray<NSButton*>* buttons = MakeButtons(content, true);
  GiveEscapeKey(buttons);
  if ([buttons count] >= 2) {
    NSButton* secondary = [buttons objectAtIndex:[buttons count] - 2];
    NSButton* primary = [buttons lastObject];
    const CGFloat paired = MAX(kDefaultButtonMinWidth,
                               MAX(AlignmentSize(secondary).width, AlignmentSize(primary).width));
    SetAlignedWidth(secondary, paired);
    SetAlignedWidth(primary, paired);
    if ([buttons count] >= 3) {
      NSButton* skip = [buttons firstObject];  // never narrower than Install Update
      SetAlignedWidth(skip, MAX(AlignmentSize(skip).width, paired));
    }
  }
  const CGFloat buttons_height = ButtonsHeight(buttons);
  NSButton* checkbox = content.checkbox.visible ? MakeCheckbox(content.checkbox, Delegate()) : nil;
  const CGFloat checkbox_height = checkbox != nil ? AlignmentSize(checkbox).height : 0;
  CGFloat below = 0;  // from the bottom of the notes box to the window's bottom
  if (checkbox != nil) {
    below += kGroupGap + checkbox_height + kStackGap;
  } else {
    below += kGroupGap;
  }
  below += buttons_height + kMargin;

  if (!content.notes_markdown.empty()) {
    const CGFloat notes_height = MAX(kNotesMinHeight, kAvailableHeight - y - below);
    // SUUpdateAlert.xib: the box may shrink to 200 (priority 999), nothing else gives.
    g_available_min_height = MAX(kAvailableMinHeight, y + kNotesMinHeight + below);
    NSView* notes = MakeNotesBox(content, Delegate());
    PlaceAligned(notes, NSMakeRect(kMargin, y, width - 2 * kMargin, notes_height),
                 NSViewWidthSizable | NSViewHeightSizable);
    [root addSubview:notes];
    y += notes_height;
  } else if (!content.notes_link_label.empty()) {
    // dazzle updater's own: a link where Sparkle would show nothing, in the text column.
    NSButton* link = [[NSButton alloc] initWithFrame:NSZeroRect];
    link.bordered = NO;
    link.attributedTitle = [[NSAttributedString alloc]
        initWithString:ToNSString(content.notes_link_label)
            attributes:@{NSForegroundColorAttributeName : [NSColor linkColor],
                         NSFontAttributeName : DetailFont()}];
    link.target = Delegate();
    link.action = @selector(controlPressed:);
    link.tag = kNotesLinkTag;
    [link sizeToFit];
    y -= kGroupGap - kStackGap;  // a plain row of the stack, 8 under the title
    const NSSize size = AlignmentSize(link);
    PlaceAligned(link, NSMakeRect(text_x, y, size.width, size.height),
                 NSViewMaxXMargin | NSViewMaxYMargin);
    [root addSubview:link];
    y += size.height;
  } else {
    y -= kGroupGap;  // nothing between the title row and what follows
  }

  if (checkbox != nil) {
    y += kGroupGap;
    PlaceAligned(checkbox, NSMakeRect(kMargin, y, width - 2 * kMargin, checkbox_height),
                 NSViewMaxXMargin | NSViewMinYMargin);
    [root addSubview:checkbox];
    g_checkbox = checkbox;
    y += checkbox_height + kStackGap;
  } else {
    y += kGroupGap;
  }

  // Skip This Version sits at the leading edge; the others at the trailing one.
  if ([buttons count] >= 3) {
    NSButton* skip = [buttons firstObject];
    const NSSize size = AlignmentSize(skip);
    PlaceAligned(skip, NSMakeRect(kMargin, y, size.width, size.height),
                 NSViewMaxXMargin | NSViewMinYMargin);
    [root addSubview:skip];
    PlaceButtons([buttons subarrayWithRange:NSMakeRange(1, [buttons count] - 1)], root,
                 width - kMargin, y, kButtonGap);
  } else {
    PlaceButtons(buttons, root, width - kMargin, y, kButtonGap);
  }
  y += buttons_height + kMargin;

  size_out->width = width;
  size_out->height = content.notes_markdown.empty() ? y : MAX(kAvailableHeight, y);
  FinishRoot(root, *size_out);
  return root;
}

/// The UpdateStatusWindow, SUStatusController's layout: the icon at (20, 4), the title
/// and the bar 8 under it as one block centred on the icon, the button 8 under that
/// block and 20 from the bottom and right edges, the status text on the button's
/// baseline from the text column, at least 25 before the button. 400 wide, 107 tall.
NSView* MakeStatusView(const UiContent& content, NSSize* size_out) {
  const CGFloat width = kStatusWidth;
  const CGFloat text_x = kMargin + kIconSize + kIconGap;
  const CGFloat text_width = width - text_x - kMargin;

  DazzleUpdaterFlippedView* root =
      [[DazzleUpdaterFlippedView alloc] initWithFrame:NSMakeRect(0, 0, width, kStatusMinHeight)];

  NSImageView* icon = MakeAppIcon();
  PlaceAligned(icon, NSMakeRect(kMargin, kStatusIconTop, kIconSize, kIconSize),
               NSViewMaxXMargin | NSViewMaxYMargin);
  [root addSubview:icon];
  const CGFloat icon_center = kStatusIconTop + kIconSize / 2;

  CGFloat block_bottom = 0;
  if (content.progress.visible) {
    NSTextField* heading = MakeLabel(ToNSString(content.heading), HeadingFont(), TextColor(), true);
    const CGFloat heading_height = TextHeight(heading, text_width);
    NSProgressIndicator* progress = MakeProgress(content.progress);
    const CGFloat progress_height = AlignmentSize(progress).height;
    const CGFloat block_height = heading_height + kTextGap + progress_height;
    const CGFloat block_top = icon_center - block_height / 2;
    PlaceAligned(heading, NSMakeRect(text_x, block_top, text_width, heading_height),
                 NSViewWidthSizable | NSViewMaxYMargin);
    [root addSubview:heading];
    PlaceAligned(progress,
                 NSMakeRect(text_x, block_top + heading_height + kTextGap, text_width, progress_height),
                 NSViewWidthSizable | NSViewMaxYMargin);
    [root addSubview:progress];
    g_heading = heading;
    g_progress = progress;
    block_bottom = block_top + block_height;
  } else {
    // A message instead of the bar: the same block, centred on the icon the same way.
    const CGFloat block_height = MeasureHeading(content, text_width);
    const CGFloat block_top = MAX(kStatusIconTop, icon_center - block_height / 2);
    NSTextField* heading = nil;
    NSTextField* detail = nil;
    PlaceHeading(root, content, text_x, block_top, text_width,
                 NSViewWidthSizable | NSViewMaxYMargin, &heading, &detail);
    g_heading = heading;
    g_detail = detail;
    block_bottom = block_top + block_height;
  }

  NSArray<NSButton*>* buttons = MakeButtons(content, true);
  GiveEscapeKey(buttons);
  for (NSButton* button in buttons) {
    FitControl(button, kStatusButtonMinWidth);
  }
  const CGFloat buttons_top = block_bottom + kStatusButtonGap;
  const CGFloat buttons_left = PlaceButtons(buttons, root, width - kMargin, buttons_top, kButtonGap);
  const CGFloat buttons_height = ButtonsHeight(buttons);

  // The status text shares the button's baseline.
  g_status = MakeLabel(ToNSString(content.progress.status_text), StatusFont(), TextColor(), false);
  if (!content.progress.status_text.empty()) {
    const CGFloat status_height = AlignmentSize(g_status).height;
    CGFloat status_top = buttons_top;
    if ([buttons count] > 0) {
      NSButton* first = [buttons firstObject];
      status_top += first.firstBaselineOffsetFromTop - g_status.firstBaselineOffsetFromTop;
    }
    const CGFloat status_right = [buttons count] > 0 ? buttons_left - kStatusTextGap : width - kMargin;
    PlaceAligned(g_status, NSMakeRect(text_x, status_top, MAX(0, status_right - text_x), status_height),
                 NSViewWidthSizable | NSViewMinYMargin);
    [root addSubview:g_status];
  }

  size_out->width = width;
  size_out->height = MAX(kStatusMinHeight, buttons_top + buttons_height + kMargin);
  FinishRoot(root, *size_out);
  return root;
}

/// The UpdatePermissionWindow, SUUpdatePermissionPrompt.xib's stack with no spacing:
/// the prompt row (icon at (23, 18); the question 20 from the top and the text under it
/// 8 lower, both from x = 106 to the right margin, the text at least 34 tall), the
/// check box row (6 above the box), the answer row (15 above the buttons, 20 under
/// them; Check Automatically at the right margin, Don't Check 14 before it).
NSView* MakePermissionView(const UiContent& content, NSSize* size_out) {
  const CGFloat width = kPermissionWidth;
  const CGFloat text_x = kPermissionTextLeft;
  const CGFloat text_width = width - text_x - kMargin;

  DazzleUpdaterFlippedView* root =
      [[DazzleUpdaterFlippedView alloc] initWithFrame:NSMakeRect(0, 0, width, kStatusMinHeight)];

  NSImageView* icon = MakeAppIcon();
  PlaceAligned(icon, NSMakeRect(kPermissionIconLeft, kPermissionIconTop, kIconSize, kIconSize),
               NSViewMaxXMargin | NSViewMaxYMargin);
  [root addSubview:icon];

  NSTextField* heading = MakeLabel(ToNSString(content.heading), HeadingFont(), TextColor(), true);
  const CGFloat heading_height = TextHeight(heading, text_width);
  PlaceAligned(heading, NSMakeRect(text_x, kPermissionTextTop, text_width, heading_height),
               NSViewWidthSizable | NSViewMaxYMargin);
  [root addSubview:heading];
  CGFloat y = kPermissionTextTop + heading_height;
  if (!content.detail.empty()) {
    NSTextField* detail = MakeLabel(ToNSString(content.detail), DetailFont(), TextColor(), true);
    const CGFloat detail_height = MAX(kPermissionDetailMinHeight, TextHeight(detail, text_width));
    PlaceAligned(detail, NSMakeRect(text_x, y + kTextGap, text_width, detail_height),
                 NSViewWidthSizable | NSViewMaxYMargin);
    [root addSubview:detail];
    y += kTextGap + detail_height;
  }

  if (content.checkbox.visible) {
    NSButton* checkbox = MakeCheckbox(content.checkbox, Delegate());
    const CGFloat checkbox_height = AlignmentSize(checkbox).height;
    y += kPermissionCheckboxTop;
    PlaceAligned(checkbox, NSMakeRect(text_x - 1, y, text_width + 1, checkbox_height),
                 NSViewMaxXMargin | NSViewMaxYMargin);
    [root addSubview:checkbox];
    g_checkbox = checkbox;
    y += checkbox_height;
  }

  NSArray<NSButton*>* buttons = MakeButtons(content, false);
  GiveEscapeKey(buttons);
  for (NSButton* button in buttons) {
    FitControl(button, [button.keyEquivalent isEqualToString:@"\r"] ? kPermissionDefaultMinWidth
                                                                       : kPermissionOtherMinWidth);
  }
  y += kPermissionButtonsTop;
  PlaceButtons(buttons, root, width - kMargin, y, kPermissionButtonGap);
  y += ButtonsHeight(buttons) + kMargin;

  size_out->width = width;
  size_out->height = y;
  FinishRoot(root, *size_out);
  return root;
}

void ActivateApplication() {
  if (@available(macOS 14.0, *)) {
    [NSApp activate];
  } else {
    [NSApp activateIgnoringOtherApps:YES];
  }
}

/// Draws `content`; runs on the main thread.
void DrawOnMain(const UiContent& content) {
  // A command-line tool has no NSApplication until something asks for one; an application
  // has had one since it started.
  [NSApplication sharedApplication];

  const bool have_window = g_window != nil;
  const bool same_window =
      have_window && IsUpdateWindow(content.window) && g_window_kind == content.window;

  if (same_window && g_state == content.state) {
    // The same state again: a progress tick, or the check box. Only the values change, so
    // the window is not rebuilt (spec: 同一窗口内的状态转换只更新窗口内容).
    if (g_heading != nil) {
      g_heading.stringValue = ToNSString(content.heading);
    }
    if (g_detail != nil) {
      g_detail.stringValue = ToNSString(content.detail);
    }
    if (g_progress != nil) {
      UpdateProgress(g_progress, content.progress);
    }
    if (g_status != nil) {
      g_status.stringValue = ToNSString(content.progress.status_text);
    }
    if (g_checkbox != nil) {
      g_checkbox.state = content.checkbox.checked ? NSControlStateValueOn : NSControlStateValueOff;
    }
    return;
  }

  ForgetViews();
  NSSize size = NSMakeSize(kStatusWidth, kStatusMinHeight);
  NSView* root = nil;
  switch (content.window) {
    case UiWindow::kUpdateAvailable:
      root = MakeAvailableView(content, &size);
      break;
    case UiWindow::kUpdatePermission:
      root = MakePermissionView(content, &size);
      break;
    case UiWindow::kUpdateStatus:
    case UiWindow::kNone:
      root = MakeStatusView(content, &size);
      break;
  }

  g_window_kind = content.window;
  g_state = content.state;

  if (same_window) {
    [g_window setContentSize:size];
    g_window.contentView = root;
    [g_window makeKeyAndOrderFront:nil];
    ActivateApplication();
    return;
  }

  // A different window: the spec has the new one open where the old one was, so the
  // change reads as an in-place replacement.
  const NSRect previous = have_window ? g_window.frame : NSZeroRect;
  NSWindowStyleMask mask = NSWindowStyleMaskTitled;
  if (content.window != UiWindow::kUpdatePermission) {
    mask |= NSWindowStyleMaskClosable;  // SUUpdatePermissionPrompt.xib: titled only
  }
  if (content.window == UiWindow::kUpdateAvailable) {
    mask |= NSWindowStyleMaskMiniaturizable;  // SUUpdateAlert.xib
  }
  if (content.resizable) {
    mask |= NSWindowStyleMaskResizable;
  }
  NSWindow* window =
      [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, size.width, size.height)
                                  styleMask:mask
                                    backing:NSBackingStoreBuffered
                                      defer:NO];
  window.title = ToNSString(content.window_title);
  if (content.window == UiWindow::kUpdateAvailable) {
    // SUUpdateAlert: a transparent title bar with the title hidden (it stays in the
    // window list and for accessibility), and the window movable by its background.
    window.titlebarAppearsTransparent = YES;
    window.titleVisibility = NSWindowTitleHidden;
    window.movableByWindowBackground = YES;
  } else if (content.window == UiWindow::kUpdatePermission) {
    window.titleVisibility = NSWindowTitleHidden;  // the prompt has no title of its own
  }
  window.releasedWhenClosed = NO;  // this module holds the window until the process ends
  window.delegate = Delegate();
  if (content.window == UiWindow::kUpdateStatus) {
    // SUStatusController: the status window does not push a full-screen window aside.
    window.collectionBehavior = NSWindowCollectionBehaviorFullScreenAuxiliary;
  }
  if (!content.closable) {
    [[window standardWindowButton:NSWindowCloseButton] setEnabled:NO];
  }
  window.contentView = root;
  [window setContentSize:size];
  if (content.resizable) {
    [window setContentMinSize:NSMakeSize(kAvailableMinWidth, g_available_min_height)];
  }
  if (have_window) {
    const NSSize frame_size = window.frame.size;
    [window setFrameOrigin:NSMakePoint(NSMidX(previous) - frame_size.width / 2,
                                       NSMidY(previous) - frame_size.height / 2)];
  } else {
    [window center];
  }

  NSWindow* replaced = g_window;
  g_window = window;
  [window makeKeyAndOrderFront:nil];
  ActivateApplication();
  if (replaced != nil) {
    g_ignoring_close = YES;
    [replaced close];
    g_ignoring_close = NO;
  }
}

void HideOnMain() {
  if (g_window == nil) {
    return;
  }
  NSWindow* window = g_window;
  g_window = nil;
  g_window_kind = UiWindow::kNone;
  ForgetViews();
  g_ignoring_close = YES;  // a window we were told to close is not the user's doing
  [window close];
  g_ignoring_close = NO;
}

}  // namespace

// --- the backend ----------------------------------------------------------------------

namespace dazzle_updater {
namespace internal {
namespace {

class MacUiBackend final : public UiBackend {
 public:
  bool IsAvailable() const override { return true; }

  void Draw(const UiContent& content) override {
    const UiContent copy = content;
    dispatch_async(dispatch_get_main_queue(), ^{
      DrawOnMain(copy);
    });
  }

  void Hide() override {
    dispatch_async(dispatch_get_main_queue(), ^{
      HideOnMain();
    });
  }

  void BringToFront() override {
    dispatch_async(dispatch_get_main_queue(), ^{
      if (g_window != nil) {
        [g_window makeKeyAndOrderFront:nil];
        ActivateApplication();
      }
    });
  }

  void QuitApplication() override {
    dispatch_async(dispatch_get_main_queue(), ^{
      // The application's own way out, so it can save what it has to save before the
      // installer replaces it (spec: 退出应用).
      [NSApp terminate:nil];
    });
  }

  void SetEventSink(std::function<void(const UiEvent&)> sink) override {
    g_sink = std::move(sink);
  }
};

}  // namespace

UiBackend& GetUiBackend() {
  static MacUiBackend backend;
  return backend;
}

}  // namespace internal
}  // namespace dazzle_updater
