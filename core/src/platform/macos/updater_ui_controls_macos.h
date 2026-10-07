#pragma once

#import <AppKit/AppKit.h>

#include <string>

// The macOS control faces UpdaterUI draws, as the values the prototype defines them
// with (prototype/src/styles/native.css, the `--button-*`, `--checkbox`/`.checkbox` and
// `--progress-*` tokens of each desktop generation and appearance).
//
// The macOS windows are not system controls: the prototype draws its own buttons,
// check boxes and progress bars, and the module draws the same ones so the two match
// (specs/updater-ui.md, "外观"). What the system still draws is the window itself — its
// title bar, its traffic lights, its shadow and its resizing — because those carry
// behaviour the module must not re-invent.
//
// Two generations, as the prototype has them: macOS 26 and later draw the Liquid Glass
// tokens (the prototype's "macOS 27" desktop: 28 pt capsules, 16 pt window corners,
// specular rims), everything older the flat ones (22 pt buttons, 6 pt corners).

namespace dazzle_updater {
namespace internal {

/// One generation of the tokens, in one appearance.
struct MacTokens {
  // The window and its text.
  NSColor* window_background;
  NSColor* text;
  NSColor* text_secondary;
  NSColor* text_disabled;
  NSColor* text_background;  // the release notes box
  NSColor* separator;
  NSColor* link;

  // The accent, and the two faces the default button uses.
  NSColor* accent;
  NSColor* accent_pressed;
  NSColor* on_accent;

  // Buttons.
  NSColor* button_background;
  NSColor* button_background_pressed;
  NSColor* button_default_background_top;
  NSColor* button_default_background_bottom;
  NSColor* button_border;    // the 0.5–1 pt rim
  NSColor* button_shadow;    // the soft drop under the face, nil for none
  CGFloat button_height;
  CGFloat button_radius;     // >= 999 means a capsule
  CGFloat button_padding_x;
  bool button_glass_rims;    // the Liquid Glass specular rims, top and bottom

  // Progress bars.
  CGFloat progress_height;
  CGFloat progress_radius;
  NSColor* progress_track;
  bool progress_track_border;

  // The small check box of both windows and the permission prompt.
  CGFloat checkbox_size;
  CGFloat checkbox_radius;
  CGFloat checkbox_gap;

  // The window itself: what the module sets where AppKit allows it.
  CGFloat window_radius;
  CGFloat content_top;  // the prototype's title bar height
};

/// The tokens the running system gets: Liquid Glass on macOS 26 and later, flat below.
const MacTokens& MacCurrentTokens();

/// The tokens of one appearance of that generation; used by the controls when the
/// effective appearance changes.
const MacTokens& MacTokensForAppearance(NSAppearance* appearance);

}  // namespace internal
}  // namespace dazzle_updater

/// A push button drawn from the tokens: a capsule on Liquid Glass, a rounded bezel
/// below, with the generation's fill, rims, drop shadow, focus ring and pressed state.
@interface DazzleButton : NSView
+ (instancetype)buttonWithTitle:(NSString*)title
                      isDefault:(BOOL)isDefault
                         target:(id)target
                         action:(SEL)action;
/// The UiEventKind the button reports, handed back as the action's sender.
@property(nonatomic) NSInteger eventTag;
@property(nonatomic, getter=isEnabled) BOOL enabled;
/// Sizes the face to its label: the token height, and the label plus the token's padding.
- (void)sizeToFit;
@property(nonatomic, copy) NSString* title;
@property(nonatomic, readonly, getter=isDefaultButton) BOOL defaultButton;
/// "\r" for the state's default button, "\033" for its way out; the window presses them.
@property(nonatomic, copy) NSString* keyEquivalent;
- (void)performClick;
@end

/// The small check box of the Available window and the permission prompt.
@interface DazzleCheckbox : NSView
+ (instancetype)checkboxWithTitle:(NSString*)title target:(id)target action:(SEL)action;
@property(nonatomic) NSInteger eventTag;
@property(nonatomic, getter=isChecked) BOOL checked;
- (void)sizeToFit;
@property(nonatomic, copy) NSString* title;
@end

/// The 6 pt bar both windows show while something is in progress.
@interface DazzleProgressBar : NSView
@property(nonatomic, getter=isIndeterminate) BOOL indeterminate;
@property(nonatomic) double fraction;
- (void)startAnimating;
- (void)stopAnimating;
@end
