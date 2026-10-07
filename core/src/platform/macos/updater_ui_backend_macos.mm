#include "../../foundation/updater_ui_backend.h"
#include "../../markdown/markdown.h"
#include "../../updater_ui_state.h"
#include "updater_ui_controls_macos.h"

#import <AppKit/AppKit.h>

#include <cmath>
#include <string>

// UpdaterUI's windows on macOS: two NSWindows and the one-time question, drawn with
// AppKit's own controls (../../specs/updater-ui.md, "外观"), with the application's own
// icon and the wording of its language.
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

// --- metrics (../../specs/updater-ui.md, "外观") --------------------------------------
//
// The spec's numbers, which are Sparkle's; prototype/src/styles/native.css carries the
// same ones as the macOS tokens of the prototype.

constexpr CGFloat kMargin = 20;             // 四周 20 pt 边距
constexpr CGFloat kIconSize = 64;           // 64 pt 应用图标
constexpr CGFloat kIconGap = 8;             // the icon and the text beside it
constexpr CGFloat kGap = 12;                // between groups of controls
constexpr CGFloat kTextGap = 8;             // between a heading and its detail
constexpr CGFloat kNotesMinHeight = 200;    // 更新说明区域最小高 200 pt
constexpr CGFloat kNotesRadius = 6;         // 更新说明区域 6 pt 圆角
constexpr CGFloat kAvailableWidth = 532;    // 内容区默认 532 × 370 pt
constexpr CGFloat kAvailableHeight = 370;
constexpr CGFloat kAvailableMinWidth = 462;
constexpr CGFloat kAvailableMinHeight = 342;
constexpr CGFloat kStatusWidth = 400;       // 内容区宽 400 pt
constexpr CGFloat kStatusMinHeight = 107;   // 高约 107 pt
constexpr CGFloat kStatusTop = 4;           // 图标离窗口顶部 4 pt
constexpr CGFloat kPermissionWidth = 440;   // 内容区宽 440 pt
constexpr CGFloat kButtonHeight = 22;       // NSButton, regular size
constexpr CGFloat kCheckboxRowHeight = 14;  // AppKit's small check box, as Sparkle asks for
constexpr CGFloat kButtonMinWidth = 100;
constexpr CGFloat kProgressFrame = 20;      // the bar sits in a 20 pt frame
constexpr CGFloat kProgressHeight = 12;
constexpr CGFloat kBodyFontSize = 13;
constexpr CGFloat kSmallFontSize = 11;
constexpr CGFloat kNoteFontSize = 13;        // .markdown-view: --font-size
constexpr CGFloat kNoteHeadingFontSize = 16;  // its h2: 1.23em
constexpr CGFloat kNoteSubheadingFontSize = 14.3;  // its h3: 1.1em
constexpr CGFloat kNoteCodeFontSize = 12;     // its code: .92em
constexpr CGFloat kNotesInsetX = 10;          // .markdown-view: padding 8px 10px
constexpr CGFloat kNotesInsetY = 8;
constexpr CGFloat kNotesListIndent = 23.4;    // its list: padding-inline-start 1.8em
constexpr CGFloat kMessageFontSize = 10;
constexpr CGFloat kPairedButtonWidth = 127;   // .update-available-window__paired
constexpr CGFloat kPairedButtonMinWidth = 120;  // ...__primary
constexpr CGFloat kPermissionIconGap = 16;    // .update-permission-window: gap 16px
constexpr CGFloat kPermissionBodyGap = 14;    // ...__body: gap 14px
constexpr CGFloat kPermissionTop = 4;         // ...: padding 4px 20px 20px
constexpr NSInteger kNotesLinkTag = 1000;   // a tag no UiEventKind uses

// --- state shared with the delegate, main thread only --------------------------------

NSWindow* g_window = nil;                  // the window on screen, if any
UiWindow g_window_kind = UiWindow::kNone;  // which of the three it is
UiState g_state = UiState::kChecking;      // the state it draws
NSTextField* g_heading = nil;              // what an update in place touches
NSTextField* g_detail = nil;
NSTextField* g_status = nil;
DazzleProgressBar* g_progress = nil;
DazzleCheckbox* g_checkbox = nil;
NSString* g_notes_url = nil;               // the "View Release Notes" link's target
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

std::string ToUtf8(NSString* value) {
  if (value == nil) {
    return "";
  }
  const char* utf8 = [value UTF8String];
  return utf8 != nullptr ? std::string(utf8) : std::string();
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

@interface DazzleWindowDelegate : NSObject <NSWindowDelegate, NSTextViewDelegate>
@end

@implementation DazzleWindowDelegate

- (void)windowWillClose:(NSNotification*)notification {
  if (g_ignoring_close) {
    return;  // the module closed the window; the state machine already knows
  }
  ReportEvent(UiEventKind::kClose);
}

- (void)controlPressed:(id)sender {
  if ([sender isKindOfClass:[NSButton class]] && [sender tag] == kNotesLinkTag) {
    if (g_notes_url != nil) {
      [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:g_notes_url]];
    }
    return;
  }
  if ([sender isKindOfClass:[DazzleButton class]]) {
    ReportEvent(static_cast<UiEventKind>([(DazzleButton*)sender eventTag]));
  }
}

- (void)checkboxToggled:(DazzleCheckbox*)sender {
  ReportEvent(static_cast<UiEventKind>(sender.eventTag), sender.isChecked);
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

/// A view whose origin is its top-left corner, so the layout below reads the way the
/// spec's metrics do.
@interface DazzleFlippedView : NSView
@property(nonatomic, weak) DazzleButton* defaultButton;
@property(nonatomic, weak) DazzleButton* escapeButton;
@end

@implementation DazzleFlippedView
- (BOOL)isFlipped {
  return YES;
}

- (BOOL)acceptsFirstResponder {
  return YES;
}

- (void)viewDidChangeEffectiveAppearance {
  [super viewDidChangeEffectiveAppearance];
  [self setNeedsDisplay:YES];
}

- (void)drawRect:(NSRect)dirtyRect {
  [dazzle_updater::internal::MacCurrentTokens().window_background setFill];
  NSRectFill(dirtyRect);
}

/// The state's default button and its way out answer the return and escape keys, the way
/// AppKit's own default button cell would.
- (BOOL)performKeyEquivalent:(NSEvent*)event {
  NSString* characters = event.charactersIgnoringModifiers;
  if ([characters isEqualToString:@"\r"] || [characters isEqualToString:@"\003"]) {
    if (self.defaultButton != nil) {
      [self.defaultButton performClick];
      return YES;
    }
  }
  if ([characters isEqualToString:@"\033"]) {
    if (self.escapeButton != nil) {
      [self.escapeButton performClick];
      return YES;
    }
  }
  return [super performKeyEquivalent:event];
}

/// The same two keys reach here when this view is the first responder.
- (void)keyDown:(NSEvent*)event {
  NSString* characters = event.charactersIgnoringModifiers;
  if ([characters isEqualToString:@"\r"] || [characters isEqualToString:@"\003"]) {
    if (self.defaultButton != nil) {
      [self.defaultButton performClick];
      return;
    }
  }
  if ([characters isEqualToString:@"\033"]) {
    if (self.escapeButton != nil) {
      [self.escapeButton performClick];
      return;
    }
  }
  [super keyDown:event];
}

- (void)cancelOperation:(id)sender {
  if (self.escapeButton != nil) {
    [self.escapeButton performClick];
  }
}
@end

namespace {

DazzleWindowDelegate* Delegate() {
  static DazzleWindowDelegate* delegate = [[DazzleWindowDelegate alloc] init];
  return delegate;
}

// --- small builders -------------------------------------------------------------------

/// A label that wraps inside `width` and measures itself, so the layout below can stack
/// its rows without a constraint solver.
NSTextField* MakeLabel(NSString* text, NSFont* font, NSColor* color, bool wraps) {
  NSTextField* field = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 0, 0)];
  field.stringValue = text;
  field.font = font;
  field.textColor = color;
  field.editable = NO;
  field.selectable = NO;
  field.bezeled = NO;
  field.drawsBackground = NO;
  field.lineBreakMode = wraps ? NSLineBreakByWordWrapping : NSLineBreakByTruncatingTail;
  field.usesSingleLineMode = !wraps;
  return field;
}

CGFloat TextHeight(NSTextField* field, CGFloat width) {
  return [field sizeThatFits:NSMakeSize(width, CGFLOAT_MAX)].height;
}

const dazzle_updater::internal::MacTokens& Tokens() { return dazzle_updater::internal::MacCurrentTokens(); }

NSFont* BodyFont() { return [NSFont systemFontOfSize:kBodyFontSize]; }
NSFont* HeadingFont() { return [NSFont boldSystemFontOfSize:kBodyFontSize]; }
NSFont* SmallFont() { return [NSFont systemFontOfSize:kSmallFontSize]; }
NSFont* MessageFont() {
  return [NSFont monospacedSystemFontOfSize:kMessageFontSize weight:NSFontWeightRegular];
}

/// The application's icon, or AppKit's generic one when the running executable is not a
/// bundle that ships one.
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

DazzleButton* MakePushButton(const UiButton& button, id target) {
  DazzleButton* view = [DazzleButton buttonWithTitle:ToNSString(button.label)
                                           isDefault:button.is_default
                                              target:target
                                              action:@selector(controlPressed:)];
  view.eventTag = static_cast<NSInteger>(button.event);
  if (button.is_default) {
    view.keyEquivalent = @"\r";  // the return key presses the default button
  }
  [view sizeToFit];
  view.frame = NSMakeRect(0, 0, MAX(view.frame.size.width, kButtonMinWidth),
                          view.frame.size.height);
  return view;
}

DazzleCheckbox* MakeCheckbox(const UiCheckbox& checkbox, id target) {
  DazzleCheckbox* view = [DazzleCheckbox checkboxWithTitle:ToNSString(checkbox.label)
                                                    target:target
                                                    action:@selector(checkboxToggled:)];
  view.eventTag = static_cast<NSInteger>(UiEventKind::kCheckboxChanged);
  view.checked = checkbox.checked;
  [view sizeToFit];
  return view;
}

DazzleProgressBar* MakeProgress(const UiProgress& progress) {
  DazzleProgressBar* view =
      [[DazzleProgressBar alloc] initWithFrame:NSMakeRect(0, 0, 0, Tokens().progress_height)];
  view.fraction = progress.fraction;
  view.indeterminate = !progress.determinate;
  return view;
}

/// The HTML importer turns `<li>` into a text-list marker *and* leaves the marker in the
/// text, which draws each bullet twice and indents its text at the importer's own tab
/// stops. The paragraph is given plain tab stops instead: the bullet on the first, the
/// item's text — and its wrapped lines — on the second.
void NormalizeListParagraphs(NSMutableAttributedString* text) {
  NSString* plain = text.string;
  NSUInteger start = 0;
  while (start < plain.length) {
    const NSRange line = [plain lineRangeForRange:NSMakeRange(start, 0)];
    const NSParagraphStyle* style =
        [text attribute:NSParagraphStyleAttributeName atIndex:line.location effectiveRange:NULL];
    if (style != nil && style.textLists.count > 0) {
      NSMutableParagraphStyle* normalized = [style mutableCopy];
      // The importer indents a nested list 36 pt per level.
      const NSInteger level = MAX(1, (NSInteger)lround(style.headIndent / 36.0));
      const CGFloat base = (CGFloat)(level - 1) * 18.0;
      [normalized setTextLists:@[]];
      normalized.firstLineHeadIndent = 0;
      normalized.headIndent = base + 28;
      normalized.tabStops = @[
        [[NSTextTab alloc] initWithTextAlignment:NSTextAlignmentLeft location:base + 14 options:@{}],
        [[NSTextTab alloc] initWithTextAlignment:NSTextAlignmentLeft location:base + 28 options:@{}]
      ];
      normalized.defaultTabInterval = base + 28;
      [text addAttribute:NSParagraphStyleAttributeName value:normalized range:line];
    }
    start = NSMaxRange(line);
  }
}

/// Whether every run of a paragraph is monospaced, which is how a code block is told
/// from a line of text with ``code`` in it: the importer gives both a monospaced font.
bool IsCodeParagraph(NSAttributedString* text, NSRange paragraph) {
  __block bool monospaced = true;
  __block bool any = false;
  [text enumerateAttribute:NSFontAttributeName
                   inRange:paragraph
                   options:0
                usingBlock:^(id value, NSRange range, BOOL* stop) {
                  NSFont* font = (NSFont*)value;
                  if (font == nil) {
                    return;
                  }
                  any = true;
                  if (!([[NSFontManager sharedFontManager] traitsOfFont:font] &
                        NSFixedPitchFontMask)) {
                    monospaced = false;
                    *stop = YES;
                  }
                }];
  return any && monospaced;
}

/// The spacing and leading of the prototype's stylesheet: line-height 1.35, a paragraph
/// 0.6em apart, a heading 0.9em above and 0.4em below, a code block inset and filled with
/// the window's own background.
void SpaceNotesParagraphs(NSMutableAttributedString* text) {
  NSString* plain = text.string;
  NSUInteger start = 0;
  while (start < plain.length) {
    const NSRange line = [plain lineRangeForRange:NSMakeRange(start, 0)];
    if (line.length > 0) {
      NSMutableParagraphStyle* style =
          [[text attribute:NSParagraphStyleAttributeName
                   atIndex:line.location
            effectiveRange:NULL] mutableCopy];
      if (style == nil) {
        style = [[NSMutableParagraphStyle alloc] init];
      }
      NSFont* font = [text attribute:NSFontAttributeName atIndex:line.location effectiveRange:NULL];
      const CGFloat size = font != nil ? font.pointSize : kNoteFontSize;
      const bool code = IsCodeParagraph(text, line);
      const bool heading = !code && size >= kNoteSubheadingFontSize - 0.5;
      style.minimumLineHeight = style.maximumLineHeight = round(size * 1.35);

      if (style.textLists.count > 0) {
        style.paragraphSpacing = 0;   // the prototype's li has no margin of its own
      } else if (code) {
        // The margins of two blocks collapse in the stylesheet, so only the lower one
        // is added here: the paragraph above already brought its own.
        style.paragraphSpacingBefore = 0;
        style.paragraphSpacing = 8;
        style.firstLineHeadIndent = 8;  // its pre: padding 6px 8px
        style.headIndent = 8;
        [text addAttribute:NSBackgroundColorAttributeName
                     value:Tokens().window_background
                     range:line];
      } else if (heading) {
        style.paragraphSpacingBefore = line.location > 0 ? 12 : 0;
        style.paragraphSpacing = 5;
      } else {
        style.paragraphSpacingBefore = 0;
        style.paragraphSpacing = 8;
      }
      [text addAttribute:NSParagraphStyleAttributeName value:style range:line];
    }
    start = NSMaxRange(line);
  }
}

/// Markdown → HTML → attributed string, then the window's own typography: the system
/// font at the notes' size, headings a step larger, code in the monospaced system font.
/// The HTML importer brings fonts, colors and list styles of its own, so they are
/// replaced rather than trusted.
NSAttributedString* NotesAttributedString(const UiContent& content) {
  NSString* html = ToNSString(dazzle_updater::MarkdownToHtml(content.notes_markdown));
  NSData* data = [html dataUsingEncoding:NSUTF8StringEncoding];
  NSMutableAttributedString* text = [[NSMutableAttributedString alloc] initWithHTML:data
                                                                documentAttributes:nil];
  if (text == nil) {
    return [[NSAttributedString alloc] initWithString:@""];
  }

  const NSRange all = NSMakeRange(0, text.length);
  [text beginEditing];
  [text addAttribute:NSForegroundColorAttributeName value:Tokens().text range:all];
  [text enumerateAttribute:NSFontAttributeName
                   inRange:all
                   options:0
                usingBlock:^(id value, NSRange range, BOOL* stop) {
                  NSFont* font = (NSFont*)value;
                  NSFontManager* manager = [NSFontManager sharedFontManager];
                  NSFont* replacement = nil;
                  if (font != nil && ([manager traitsOfFont:font] & NSFixedPitchFontMask)) {
                    replacement = [NSFont monospacedSystemFontOfSize:kNoteCodeFontSize
                                                               weight:NSFontWeightRegular];
                  } else {
                    // The importer's sizes tell a heading from body text; the prototype
                    // steps headings up from the body size (1.23em and 1.1em).
                    const CGFloat size = font != nil ? font.pointSize : kNoteFontSize;
                    const CGFloat base = size >= 17.0
                                             ? kNoteHeadingFontSize
                                             : (size >= 13.5 ? kNoteSubheadingFontSize
                                                             : kNoteFontSize);
                    const NSFontTraitMask traits =
                        font != nil ? ([manager traitsOfFont:font] &
                                       (NSBoldFontMask | NSItalicFontMask))
                                    : 0;
                    replacement = [manager convertFont:[NSFont systemFontOfSize:base]
                                           toHaveTrait:traits];
                  }
                  if (replacement != nil) {
                    [text addAttribute:NSFontAttributeName value:replacement range:range];
                  }
                }];
  NormalizeListParagraphs(text);
  SpaceNotesParagraphs(text);
  [text endEditing];
  return text;
}

/// The notes box: Sparkle's NSBox — 6 pt corners, a 1 pt separator border, the text
/// background — around a read-only text view. No WebView: the Markdown becomes HTML and
/// the HTML becomes an attributed string, which AppKit does without loading anything.
NSView* MakeNotesBox(const UiContent& content, id target) {
  NSBox* box = [[NSBox alloc] initWithFrame:NSMakeRect(0, 0, 0, kNotesMinHeight)];
  box.boxType = NSBoxCustom;
  box.cornerRadius = kNotesRadius;
  box.borderWidth = 1;
  box.borderColor = Tokens().separator;
  box.fillColor = Tokens().text_background;
  box.titlePosition = NSNoTitle;

  NSTextView* text_view = [[NSTextView alloc] initWithFrame:NSZeroRect];
  text_view.editable = NO;
  text_view.selectable = YES;  // a link can be clicked and a note copied
  text_view.drawsBackground = NO;
  text_view.textContainerInset = NSMakeSize(kNotesInsetX, kNotesInsetY);
  text_view.delegate = target;  // clickedOnLink: opens the system browser
  // A link is coloured and not underlined, as the prototype draws it; AppKit would
  // underline it by default.
  text_view.linkTextAttributes = @{
    NSForegroundColorAttributeName : Tokens().link,
    NSUnderlineStyleAttributeName : @(NSUnderlineStyleNone)
  };
  text_view.textStorage.attributedString = NotesAttributedString(content);
  text_view.minSize = NSMakeSize(0, 0);
  text_view.maxSize = NSMakeSize(CGFLOAT_MAX, CGFLOAT_MAX);
  text_view.verticallyResizable = YES;
  text_view.horizontallyResizable = NO;
  text_view.autoresizingMask = NSViewWidthSizable;
  text_view.textContainer.widthTracksTextView = YES;

  NSScrollView* scroll = [[NSScrollView alloc] initWithFrame:box.bounds];
  scroll.hasVerticalScroller = YES;
  scroll.autohidesScrollers = YES;
  scroll.drawsBackground = NO;
  scroll.borderType = NSNoBorder;
  scroll.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
  scroll.documentView = text_view;
  box.contentView = scroll;
  return box;
}

// --- layout ---------------------------------------------------------------------------

/// The height the heading, its detail and the error message take at `width`. The pieces
/// of the layout are measured first so a window can be sized from the outside in.
CGFloat MeasureHeading(const UiContent& content, CGFloat width, CGFloat* heading_height,
                       CGFloat* detail_height, CGFloat* message_height) {
  *heading_height = TextHeight(MakeLabel(ToNSString(content.heading), HeadingFont(),
                                         Tokens().text, true),
                               width);
  *detail_height = content.detail.empty()
                       ? 0
                       : TextHeight(MakeLabel(ToNSString(content.detail), SmallFont(),
                                              Tokens().text_secondary, true),
                                    width);
  *message_height = content.error_message.empty()
                        ? 0
                        : TextHeight(MakeLabel(ToNSString(content.error_message), MessageFont(),
                                               Tokens().text_secondary, true),
                                     width);
  CGFloat height = *heading_height;
  if (*detail_height > 0) {
    height += kTextGap + *detail_height;
  }
  if (*message_height > 0) {
    height += 6 + *message_height;
  }
  return height;
}

/// Draws the heading, the small line under it and the error message; returns their total
/// height and hands back the labels an update in place would touch.
CGFloat PlaceHeading(NSView* root, const UiContent& content, CGFloat x, CGFloat y, CGFloat width,
                     NSTextField** heading_out, NSTextField** detail_out,
                     NSTextField** message_out) {
  CGFloat heading_height = 0;
  CGFloat detail_height = 0;
  CGFloat message_height = 0;
  const CGFloat height =
      MeasureHeading(content, width, &heading_height, &detail_height, &message_height);

  NSTextField* heading =
      MakeLabel(ToNSString(content.heading), HeadingFont(), Tokens().text, true);
  heading.frame = NSMakeRect(x, y, width, heading_height);
  [root addSubview:heading];
  if (heading_out != nullptr) {
    *heading_out = heading;
  }

  if (detail_out != nullptr) {
    *detail_out = nil;
  }
  if (detail_height > 0) {
    NSTextField* detail =
        MakeLabel(ToNSString(content.detail), SmallFont(), Tokens().text_secondary, true);
    detail.frame = NSMakeRect(x, y + heading_height + kTextGap, width, detail_height);
    [root addSubview:detail];
    if (detail_out != nullptr) {
      *detail_out = detail;
    }
  }

  if (message_out != nullptr) {
    *message_out = nil;
  }
  if (message_height > 0) {
    NSTextField* message =
        MakeLabel(ToNSString(content.error_message), MessageFont(),
                  Tokens().text_secondary, true);
    message.selectable = YES;  // GetErrorMessage(), so it can be copied into a report
    message.frame = NSMakeRect(x, y + heading_height +
                                      (detail_height > 0 ? kTextGap + detail_height : 0) + 6,
                               width, message_height);
    [root addSubview:message];
    if (message_out != nullptr) {
      *message_out = message;
    }
  }
  return height;
}

/// Lays the action row out from the trailing edge, 12 pt apart, and returns where its
/// leftmost button starts.
CGFloat PlaceButtons(NSArray<DazzleButton*>* buttons, NSView* root, CGFloat right, CGFloat y) {
  CGFloat x = right;
  for (DazzleButton* button in [buttons reverseObjectEnumerator]) {
    x -= button.frame.size.width;
    button.frame = NSMakeRect(x, y, button.frame.size.width, kButtonHeight);
    [root addSubview:button];
    x -= kGap;
  }
  return x + kGap;
}

void CollectButtons(NSView* view, NSMutableArray<DazzleButton*>* found) {
  if ([view isKindOfClass:[DazzleButton class]]) {
    [found addObject:(DazzleButton*)view];
  }
  for (NSView* child in view.subviews) {
    CollectButtons(child, found);
  }
}

/// Hands the state's two keys to the buttons that carry them: the return key to the
/// default button, escape to the one the state leaves by.
void WireKeys(NSView* root) {
  if (![root isKindOfClass:[DazzleFlippedView class]]) {
    return;
  }
  DazzleFlippedView* flipped = (DazzleFlippedView*)root;
  NSMutableArray<DazzleButton*>* buttons = [NSMutableArray array];
  CollectButtons(root, buttons);
  for (DazzleButton* button in buttons) {
    if ([button.keyEquivalent isEqualToString:@"\r"]) {
      flipped.defaultButton = button;
    } else if ([button.keyEquivalent isEqualToString:@"\033"]) {
      flipped.escapeButton = button;
    }
  }
}

/// The Available window pairs its trailing two buttons: both as wide as the wider of
/// them and at least the prototype's 127 pt, with the default one never below 120 pt.
void PairTrailingButtons(NSArray<DazzleButton*>* buttons) {
  if ([buttons count] < 2) {
    return;
  }
  DazzleButton* secondary = [buttons objectAtIndex:[buttons count] - 2];
  DazzleButton* primary = [buttons lastObject];
  const CGFloat width =
      MAX(kPairedButtonWidth, MAX(secondary.frame.size.width, primary.frame.size.width));
  secondary.frame = NSMakeRect(0, 0, width, kButtonHeight);
  primary.frame = NSMakeRect(0, 0, MAX(width, kPairedButtonMinWidth), kButtonHeight);
}

NSArray<DazzleButton*>* MakeButtons(const UiContent& content) {
  NSMutableArray<DazzleButton*>* buttons = [NSMutableArray array];
  for (const UiButton& button : content.buttons) {
    [buttons addObject:MakePushButton(button, Delegate())];
  }
  return buttons;
}

/// Gives the escape key to this state's way out: "以后再说", "跳过此版本" or "取消", which
/// the spec makes the same as Esc and the close button.
void GiveEscapeKey(NSArray<DazzleButton*>* buttons) {
  for (DazzleButton* button in buttons) {
    const UiEventKind kind = static_cast<UiEventKind>(button.eventTag);
    if (kind == UiEventKind::kSecondary || kind == UiEventKind::kCancel ||
        kind == UiEventKind::kSkip) {
      button.keyEquivalent = @"\033";
      return;
    }
  }
}

/// The UpdateAvailableWindow: the icon and the version beside it, the release notes, the
/// automatic-downloads check box and the action row. With notes it is the spec's
/// 532 × 370 and resizes; without them it fits what it shows.
NSView* MakeAvailableView(const UiContent& content, NSSize* size_out) {
  const CGFloat width = kAvailableWidth;
  const CGFloat text_x = kMargin + kIconSize + kIconGap;
  const CGFloat text_width = width - text_x - kMargin;

  DazzleFlippedView* root =
      [[DazzleFlippedView alloc] initWithFrame:NSMakeRect(0, 0, width, kAvailableHeight)];

  // Measured first: the icon is centred against the text beside it.
  CGFloat heading_height = 0;
  CGFloat detail_height = 0;
  CGFloat message_height = 0;
  const CGFloat text_height =
      MeasureHeading(content, text_width, &heading_height, &detail_height, &message_height);
  const CGFloat title_height = MAX(kIconSize, text_height);

  NSImageView* icon = MakeAppIcon();
  icon.frame = NSMakeRect(kMargin, kMargin + (title_height - kIconSize) / 2, kIconSize, kIconSize);
  [root addSubview:icon];
  PlaceHeading(root, content, text_x, kMargin + (title_height - text_height) / 2, text_width,
               nullptr, nullptr, nullptr);

  CGFloat y = kMargin + title_height + kGap;
  if (!content.notes_markdown.empty()) {
    // The notes take what is left of the window's height, and at least the spec's 200 pt.
    // The check box row is 14 pt (AppKit's small control, which is what Sparkle asks for).
    const CGFloat below =
        (content.checkbox.visible ? kGap + kCheckboxRowHeight : 0) + kGap + kButtonHeight + kMargin;
    const CGFloat notes_height = MAX(kNotesMinHeight, kAvailableHeight - y - below);
    NSView* notes = MakeNotesBox(content, Delegate());
    notes.frame = NSMakeRect(kMargin, y, width - 2 * kMargin, notes_height);
    [root addSubview:notes];
    y += notes_height;
  } else if (!content.notes_link_label.empty()) {
    NSButton* link = [[NSButton alloc] initWithFrame:NSZeroRect];
    link.bordered = NO;
    link.attributedTitle = [[NSAttributedString alloc]
        initWithString:ToNSString(content.notes_link_label)
            attributes:@{NSForegroundColorAttributeName : Tokens().link,
                         NSFontAttributeName : SmallFont()}];
    link.target = Delegate();
    link.action = @selector(controlPressed:);
    link.tag = kNotesLinkTag;
    [link sizeToFit];
    link.frame = NSMakeRect(text_x, y, link.frame.size.width, link.frame.size.height);
    [root addSubview:link];
    y += link.frame.size.height;
  }

  if (content.checkbox.visible) {
    // Under the notes box, against the window's own margin, not under the text column.
    y += kGap;
    DazzleCheckbox* checkbox = MakeCheckbox(content.checkbox, Delegate());
    checkbox.frame = NSMakeRect(kMargin, y, width - 2 * kMargin, checkbox.frame.size.height);
    [root addSubview:checkbox];
    g_checkbox = checkbox;
    y += checkbox.frame.size.height;
    y += kGap - 4;  // the check box sits 8 pt above the buttons, not 12
  } else {
    y += kGap;
  }

  NSArray<DazzleButton*>* buttons = MakeButtons(content);
  PairTrailingButtons(buttons);
  GiveEscapeKey(buttons);
  PlaceButtons(buttons, root, width - kMargin, y);
  y += kButtonHeight + kMargin;

  size_out->width = width;
  size_out->height = content.notes_markdown.empty() ? y : MAX(kAvailableHeight, y);
  return root;
}

/// The UpdateStatusWindow: the icon, one line of text or a progress bar, and the state's
/// buttons, in the fixed 400 pt layout.
NSView* MakeStatusView(const UiContent& content, NSSize* size_out) {
  const CGFloat width = kStatusWidth;
  const CGFloat text_x = kMargin + kIconSize + kIconGap;
  const CGFloat text_width = width - text_x - kMargin;

  DazzleFlippedView* root =
      [[DazzleFlippedView alloc] initWithFrame:NSMakeRect(0, 0, width, kStatusMinHeight)];

  CGFloat body_height = 0;
  if (content.progress.visible) {
    NSTextField* heading =
        MakeLabel(ToNSString(content.heading), HeadingFont(), Tokens().text, true);
    const CGFloat heading_height = TextHeight(heading, text_width);
    // The message or the bar is centred on the icon (the prototype's grid row 1 with
    // justify-content: center).
    const CGFloat block_height = heading_height + kIconGap + kProgressFrame;
    const CGFloat body_top = kStatusTop + MAX(0, (kIconSize - block_height) / 2);
    heading.frame = NSMakeRect(text_x, body_top, text_width, heading_height);
    [root addSubview:heading];
    g_heading = heading;

    DazzleProgressBar* progress = MakeProgress(content.progress);
    progress.frame =
        NSMakeRect(text_x, body_top + heading_height + kIconGap, text_width, kProgressHeight);
    [root addSubview:progress];
    g_progress = progress;
    body_height = MAX(kIconSize, block_height);
  } else {
    // A message starts 12 pt under the icon's top edge, whatever its length.
    NSTextField* heading = nil;
    NSTextField* detail = nil;
    body_height = PlaceHeading(root, content, text_x, kStatusTop + 12, text_width, &heading,
                               &detail, nullptr);
    g_heading = heading;
    g_detail = detail;
  }

  NSImageView* icon = MakeAppIcon();
  icon.frame = NSMakeRect(kMargin, kStatusTop, kIconSize, kIconSize);
  [root addSubview:icon];

  const CGFloat footer_y = kStatusTop + MAX(kIconSize, body_height) - 2;
  NSArray<DazzleButton*>* buttons = MakeButtons(content);
  GiveEscapeKey(buttons);
  const CGFloat buttons_left = PlaceButtons(buttons, root, width - kMargin, footer_y);

  // The status text sits on the buttons' line, starting where the body does.
  g_status = MakeLabel(ToNSString(content.progress.status_text),
                       [NSFont monospacedDigitSystemFontOfSize:kSmallFontSize
                                                        weight:NSFontWeightRegular],
                       Tokens().text_secondary, false);
  if (!content.progress.status_text.empty()) {
    CGFloat status_right = buttons_left;
    if ([buttons count] > 0) {
      status_right = ((DazzleButton*)[buttons objectAtIndex:0]).frame.origin.x - kGap;
    }
    g_status.frame = NSMakeRect(text_x, footer_y + (kButtonHeight - 14) / 2,
                                MAX(0, status_right - text_x), 14);
    [root addSubview:g_status];
  }

  size_out->width = width;
  size_out->height = MAX(kStatusMinHeight, footer_y + kButtonHeight + kMargin);
  return root;
}

/// The UpdatePermissionWindow: the one-time question, its check box and its two answers.
NSView* MakePermissionView(const UiContent& content, NSSize* size_out) {
  const CGFloat width = kPermissionWidth;
  const CGFloat text_x = kMargin + kIconSize + kPermissionIconGap;
  const CGFloat text_width = width - text_x - kMargin;
  const CGFloat top = kPermissionTop;

  DazzleFlippedView* root =
      [[DazzleFlippedView alloc] initWithFrame:NSMakeRect(0, 0, width, kStatusMinHeight)];

  CGFloat heading_height = 0;
  CGFloat detail_height = 0;
  CGFloat message_height = 0;
  const CGFloat text_height =
      MeasureHeading(content, text_width, &heading_height, &detail_height, &message_height);
  const CGFloat title_height = MAX(kIconSize, text_height);

  NSImageView* icon = MakeAppIcon();
  icon.frame = NSMakeRect(kMargin, top + (title_height - kIconSize) / 2, kIconSize, kIconSize);
  [root addSubview:icon];
  PlaceHeading(root, content, text_x, top + (title_height - text_height) / 2, text_width,
               nullptr, nullptr, nullptr);

  CGFloat y = top + title_height;
  if (content.checkbox.visible) {
    y += kPermissionBodyGap;
    DazzleCheckbox* checkbox = MakeCheckbox(content.checkbox, Delegate());
    checkbox.frame = NSMakeRect(text_x, y, text_width, checkbox.frame.size.height);
    [root addSubview:checkbox];
    y += checkbox.frame.size.height;
  }

  NSArray<DazzleButton*>* buttons = MakeButtons(content);
  GiveEscapeKey(buttons);
  y += kPermissionBodyGap + 4;  // the body's gap, plus the buttons' own 4 pt
  PlaceButtons(buttons, root, width - kMargin, y);
  y += kButtonHeight + kMargin;

  size_out->width = width;
  size_out->height = y;
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
      g_progress.fraction = content.progress.fraction;
      g_progress.indeterminate = !content.progress.determinate;
    }
    if (g_status != nil) {
      g_status.stringValue = ToNSString(content.progress.status_text);
    }
    if (g_checkbox != nil) {
      g_checkbox.checked = content.checkbox.checked;
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
    g_window.contentView = root;
    [g_window setContentSize:size];
    WireKeys(root);
    [g_window makeFirstResponder:root];
    [g_window makeKeyAndOrderFront:nil];
    ActivateApplication();
    return;
  }

  // A different window: the spec has the new one open where the old one was, so the
  // change reads as an in-place replacement.
  const NSRect previous = have_window ? g_window.frame : NSZeroRect;
  NSWindowStyleMask mask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable;
  if (content.resizable) {
    mask |= NSWindowStyleMaskResizable;
  }
  NSWindow* window =
      [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, size.width, size.height)
                                  styleMask:mask
                                    backing:NSBackingStoreBuffered
                                      defer:NO];
  window.title = ToNSString(content.window_title);
  if (content.window != UiWindow::kUpdateStatus) {
    // The Available and Permission windows keep their title for the window list and for
    // accessibility but do not draw it, as Sparkle's update alert does — the prototype
    // passes titleHidden for exactly these two.
    window.titleVisibility = NSWindowTitleHidden;
  }
  window.releasedWhenClosed = NO;  // this module holds the window until the process ends
  window.delegate = Delegate();
  WireKeys(root);
  if (!content.closable) {
    [[window standardWindowButton:NSWindowCloseButton] setEnabled:NO];
  }
  window.backgroundColor = Tokens().window_background;
  window.contentView = root;
  [window setContentSize:size];
  [window makeFirstResponder:root];
  if (content.resizable) {
    [window setContentMinSize:NSMakeSize(kAvailableMinWidth,
                                         MAX(kAvailableMinHeight, size.height))];
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
