#include "updater_ui_controls_macos.h"

#import <AppKit/AppKit.h>

#include <cmath>

// The three control faces the windows are built from, drawn from the tokens in
// updater_ui_controls_macos.h — the same values the prototype draws in
// prototype/src/components/ui/{button,checkbox,progress-bar}.css. See that header for
// why they are drawn rather than taken from AppKit.
//
// Each control keeps AppKit's contract: it sends its action to its target on release,
// takes the first responder for the keyboard, draws a focus ring, and reports itself to
// accessibility as what it is.

namespace {

bool IsDarkAppearance(NSAppearance* appearance) {
  if (appearance == nil) {
    return false;
  }
  NSAppearanceName name = [appearance bestMatchFromAppearancesWithNames:@[
    NSAppearanceNameAqua, NSAppearanceNameDarkAqua
  ]];
  return [name isEqualToString:NSAppearanceNameDarkAqua];
}

NSColor* Rgba(CGFloat red, CGFloat green, CGFloat blue, CGFloat alpha) {
  return [NSColor colorWithSRGBRed:red / 255.0 green:green / 255.0 blue:blue / 255.0 alpha:alpha];
}

NSColor* Hex(CGFloat red, CGFloat green, CGFloat blue) { return Rgba(red, green, blue, 1.0); }

}  // namespace

namespace dazzle {
namespace internal {
namespace {

/// The flat generation, macOS 15 and earlier (native.css, the base macOS block).
MacTokens FlatTokens(bool dark) {
  MacTokens tokens{};
  tokens.window_background = dark ? Hex(50, 50, 50) : Hex(236, 236, 236);
  tokens.text = dark ? Rgba(255, 255, 255, 0.85) : Rgba(0, 0, 0, 0.85);
  tokens.text_secondary = dark ? Rgba(255, 255, 255, 0.55) : Rgba(0, 0, 0, 0.5);
  tokens.text_disabled = dark ? Rgba(255, 255, 255, 0.25) : Rgba(0, 0, 0, 0.25);
  tokens.text_background = dark ? Hex(30, 30, 30) : Hex(255, 255, 255);
  tokens.separator = dark ? Rgba(255, 255, 255, 0.1) : Rgba(0, 0, 0, 0.1);
  tokens.link = dark ? Hex(65, 156, 255) : Hex(0, 104, 218);
  // The user's own accent, as the system resolves it in this appearance.
  tokens.accent = [NSColor controlAccentColor];
  tokens.accent_pressed = dark ? Hex(59, 155, 255) : Hex(0, 98, 204);
  tokens.on_accent = Hex(255, 255, 255);
  tokens.button_background = dark ? Rgba(255, 255, 255, 0.25) : Hex(255, 255, 255);
  tokens.button_background_pressed = dark ? Rgba(255, 255, 255, 0.35) : Hex(229, 229, 229);
  tokens.button_default_background_top = dark ? Hex(27, 140, 255) : Hex(42, 147, 255);
  tokens.button_default_background_bottom = dark ? Hex(10, 122, 255) : Hex(0, 122, 255);
  tokens.button_border = Rgba(0, 0, 0, 0.12);
  tokens.button_shadow = Rgba(0, 0, 0, dark ? 0.3 : 0.2);
  tokens.button_height = 22;
  tokens.button_radius = 6;
  tokens.button_padding_x = 12;
  tokens.button_glass_rims = false;
  tokens.progress_height = 6;
  tokens.progress_radius = 3;
  tokens.progress_track = dark ? Rgba(255, 255, 255, 0.1) : Rgba(0, 0, 0, 0.07);
  tokens.progress_track_border = true;
  // AppKit's small check box (controlSize .small) measures 14 pt on macOS 26, and 14 pt is
  // the height of the check box row in SUUpdateAlert.xib — the size Sparkle's windows ask
  // for. mini 12, regular 16 and large 18 are the other control sizes.
  tokens.checkbox_size = 14;
  tokens.checkbox_radius = 3.5;
  tokens.checkbox_gap = 5;
  tokens.window_radius = 10;
  tokens.content_top = 28;
  return tokens;
}

/// The Liquid Glass generation, macOS 26 and later (native.css, the macOS 27 block).
MacTokens GlassTokens(bool dark) {
  MacTokens tokens{};
  tokens.window_background = dark ? Hex(35, 35, 38) : Hex(245, 245, 247);
  tokens.text = dark ? Rgba(255, 255, 255, 0.85) : Rgba(0, 0, 0, 0.85);
  tokens.text_secondary = dark ? Rgba(255, 255, 255, 0.55) : Rgba(0, 0, 0, 0.5);
  tokens.text_disabled = dark ? Rgba(255, 255, 255, 0.25) : Rgba(0, 0, 0, 0.25);
  tokens.text_background = dark ? Hex(26, 26, 28) : Hex(255, 255, 255);
  tokens.separator = dark ? Rgba(255, 255, 255, 0.08) : Rgba(0, 0, 0, 0.07);
  tokens.link = dark ? Hex(63, 166, 255) : Hex(0, 119, 230);
  tokens.accent = FlatTokens(dark).accent;
  tokens.accent_pressed = dark ? Hex(42, 160, 255) : Hex(0, 116, 224);
  tokens.on_accent = Hex(255, 255, 255);
  tokens.button_background = dark ? Rgba(255, 255, 255, 0.13) : Rgba(255, 255, 255, 0.92);
  tokens.button_background_pressed = dark ? Rgba(255, 255, 255, 0.2) : Hex(236, 236, 240);
  tokens.button_default_background_top = tokens.accent;
  tokens.button_default_background_bottom = tokens.accent;
  tokens.button_border = Rgba(0, 0, 0, dark ? 0.3 : 0.08);
  tokens.button_shadow = Rgba(0, 0, 0, dark ? 0.3 : 0.1);
  tokens.button_height = 28;
  tokens.button_radius = 999;  // a capsule
  tokens.button_padding_x = 18;
  tokens.button_glass_rims = true;
  tokens.progress_height = 6;
  tokens.progress_radius = 999;
  tokens.progress_track = dark ? Rgba(255, 255, 255, 0.1) : Rgba(0, 0, 0, 0.07);
  tokens.progress_track_border = false;
  tokens.checkbox_size = 14;
  tokens.checkbox_radius = 4;
  tokens.checkbox_gap = 5;
  tokens.window_radius = 16;
  tokens.content_top = 32;
  return tokens;
}

const MacTokens& CachedTokens(bool glass, bool dark) {
  static MacTokens flat_light = FlatTokens(false);
  static MacTokens flat_dark = FlatTokens(true);
  static MacTokens glass_light = GlassTokens(false);
  static MacTokens glass_dark = GlassTokens(true);
  if (glass) {
    return dark ? glass_dark : glass_light;
  }
  return dark ? flat_dark : flat_light;
}

}  // namespace

/// Whether this system draws the Liquid Glass generation.
bool LiquidGlassGeneration() {
  if (@available(macOS 26.0, *)) {
    return true;
  }
  return false;
}

const MacTokens& MacCurrentTokens() {
  return MacTokensForAppearance([NSApp effectiveAppearance]);
}

const MacTokens& MacTokensForAppearance(NSAppearance* appearance) {
  return CachedTokens(LiquidGlassGeneration(), IsDarkAppearance(appearance));
}

}  // namespace internal
}  // namespace dazzle

// --- drawing helpers ------------------------------------------------------------------

namespace {

using dazzle::internal::MacTokens;

NSBezierPath* RoundedPath(NSRect rect, CGFloat radius) {
  return [NSBezierPath bezierPathWithRoundedRect:rect xRadius:radius yRadius:radius];
}

void FillRounded(NSRect rect, CGFloat radius, NSColor* color) {
  [color setFill];
  [RoundedPath(rect, radius) fill];
}

/// A button's face and its drop shadow in one fill: `0 1px 2.5px rgba(0,0,0,.1)` behind
/// the face, so the fill is what the shadow is cast from.
void FillFaceWithShadow(NSRect rect, CGFloat radius, NSColor* color, NSColor* shadow_color) {
  NSShadow* shadow = nil;
  if (shadow_color != nil) {
    shadow = [[NSShadow alloc] init];
    shadow.shadowColor = shadow_color;
    shadow.shadowBlurRadius = 2.5;
    shadow.shadowOffset = NSMakeSize(0, -1);
  }
  [NSGraphicsContext saveGraphicsState];
  [shadow set];
  FillRounded(rect, radius, color);
  [NSGraphicsContext restoreGraphicsState];
}

void StrokeRounded(NSRect rect, CGFloat radius, NSColor* color, CGFloat width) {
  if (color == nil) {
    return;
  }
  [color setStroke];
  NSBezierPath* path = RoundedPath(NSInsetRect(rect, width / 2, width / 2), radius - width / 2);
  path.lineWidth = width;
  [path stroke];
}

/// A button's face, drawn the way its generation's box-shadow list describes it.
void DrawButtonFace(NSRect rect, const MacTokens& tokens, BOOL is_default, BOOL pressed,
                    BOOL enabled) {
  const CGFloat radius = tokens.button_radius >= 999 ? rect.size.height / 2 : tokens.button_radius;

  if (tokens.button_glass_rims) {
    // Liquid Glass: a flat fill, a specular rim top and bottom, a hairline outside, a
    // short drop shadow.
    NSColor* fill = tokens.button_background;
    if (is_default) {
      fill = pressed ? tokens.accent_pressed : tokens.accent;
    } else if (pressed) {
      fill = tokens.button_background_pressed;
    }
    if (!enabled && is_default) {
      fill = tokens.button_background;  // a disabled default wears the plain face
    }
    FillFaceWithShadow(rect, radius, fill, enabled ? tokens.button_shadow : nil);
    if (is_default) {
      StrokeRounded(rect, radius, Rgba(0, 70, 160, 0.2), 0.5);
    } else {
      StrokeRounded(rect, radius, tokens.button_border, 0.5);
    }
    // The rim: white along the top inside edge, a dark hairline along the bottom one.
    NSGraphicsContext* context = [NSGraphicsContext currentContext];
    [context saveGraphicsState];
    [RoundedPath(rect, radius) addClip];
    const CGFloat rim = 0.5;
    NSColor* top_rim = is_default ? Rgba(255, 255, 255, 0.4) : Rgba(255, 255, 255, 0.9);
    FillRounded(NSMakeRect(rect.origin.x, NSMaxY(rect) - rim, rect.size.width, rim), 0, top_rim);
    NSColor* bottom_rim = is_default ? Rgba(0, 0, 0, 0.08) : Rgba(0, 0, 0, 0.05);
    FillRounded(NSMakeRect(rect.origin.x, rect.origin.y, rect.size.width, rim), 0, bottom_rim);
    [context restoreGraphicsState];
    return;
  }

  // The flat generation: a bezel, a hairline and a soft drop; the default is a gradient.
  if (is_default && enabled) {
    // The gradient is the fill, so the shadow is set before it is drawn.
    NSShadow* shadow = [[NSShadow alloc] init];
    shadow.shadowColor = Rgba(0, 0, 0, 0.25);
    shadow.shadowBlurRadius = 2.5;
    shadow.shadowOffset = NSMakeSize(0, -1);
    [NSGraphicsContext saveGraphicsState];
    [shadow set];
    NSGradient* gradient =
        [[NSGradient alloc] initWithStartingColor:(pressed ? tokens.accent_pressed
                                                           : tokens.button_default_background_top)
                                      endingColor:(pressed ? tokens.accent_pressed
                                                           : tokens.button_default_background_bottom)];
    [gradient drawInBezierPath:RoundedPath(rect, radius) angle:-90];
    [NSGraphicsContext restoreGraphicsState];
    StrokeRounded(rect, radius, tokens.button_border, 0.5);
    return;
  }
  NSColor* fill = pressed ? tokens.button_background_pressed : tokens.button_background;
  FillFaceWithShadow(rect, radius, fill, enabled ? tokens.button_shadow : nil);
  StrokeRounded(rect, radius, tokens.button_border, 0.5);
}

/// The font both the buttons and the check boxes use, from the window's own size.
NSFont* ControlFont(CGFloat size, BOOL bold) {
  return bold ? [NSFont boldSystemFontOfSize:size] : [NSFont systemFontOfSize:size];
}

}  // namespace

// --- the button ------------------------------------------------------------------------

@implementation DazzleButton {
  NSInteger _eventTag;
  NSString* _title;
  BOOL _is_default;
  BOOL _highlighted;
  BOOL _enabled;
  /// The focus ring is for the keyboard: macOS does not draw one after a click.
  BOOL _keyboard_focus;
  BOOL _focusing_from_mouse;
  const MacTokens* _tokens;
  NSString* _key_equivalent;
  __weak id _target;
  SEL _action;
}

+ (instancetype)buttonWithTitle:(NSString*)title
                        isDefault:(BOOL)isDefault
                           target:(id)target
                           action:(SEL)action {
  DazzleButton* button = [[DazzleButton alloc] initWithFrame:NSZeroRect];
  button->_title = [title copy];
  button->_is_default = isDefault;
  button->_enabled = YES;
  button->_target = target;
  button->_action = action;
  button->_tokens = &dazzle::internal::MacCurrentTokens();
  [button sizeToFit];
  return button;
}

- (BOOL)isFlipped {
  return NO;
}

- (void)setEventTag:(NSInteger)eventTag {
  _eventTag = eventTag;
}

- (NSInteger)eventTag {
  return _eventTag;
}

- (void)setEnabled:(BOOL)enabled {
  _enabled = enabled;
  [self setNeedsDisplay:YES];
}

- (BOOL)isEnabled {
  return _enabled;
}

- (void)setTitle:(NSString*)title {
  _title = [title copy];
  [self sizeToFit];
  [self setNeedsDisplay:YES];
}

- (NSString*)title {
  return _title;
}

- (BOOL)isDefaultButton {
  return _is_default;
}

- (void)setKeyEquivalent:(NSString*)key {
  _key_equivalent = [key copy];
}

- (NSString*)keyEquivalent {
  return _key_equivalent;
}

/// The size the face needs: the label plus the token's padding on both sides.
- (void)sizeToFit {
  const CGFloat height = _tokens != nil ? _tokens->button_height : 22;
  NSDictionary* attributes = @{NSFontAttributeName : ControlFont(13, NO)};
  const NSSize text = [_title sizeWithAttributes:attributes];
  const CGFloat padding = _tokens != nil ? _tokens->button_padding_x : 12;
  self.frame = NSMakeRect(0, 0, ceil(text.width) + 2 * padding, height);
}

- (void)viewDidChangeEffectiveAppearance {
  [super viewDidChangeEffectiveAppearance];
  _tokens = &dazzle::internal::MacTokensForAppearance(self.effectiveAppearance);
  [self setNeedsDisplay:YES];
}

- (void)drawRect:(NSRect)dirtyRect {
  const MacTokens& tokens = *_tokens;
  const NSRect rect = NSInsetRect(self.bounds, 0.25, 0.25);
  DrawButtonFace(rect, tokens, _is_default, _highlighted, _enabled);

  NSColor* color = tokens.text;
  if (!_enabled) {
    color = tokens.text_disabled;
  } else if (_is_default) {
    color = tokens.on_accent;
  }
  NSDictionary* attributes = @{
    NSFontAttributeName : ControlFont(13, NO),
    NSForegroundColorAttributeName : color
  };
  const NSSize text = [_title sizeWithAttributes:attributes];
  const NSPoint origin =
      NSMakePoint(NSMidX(self.bounds) - text.width / 2,
                  NSMidY(self.bounds) - text.height / 2 + (_is_default ? 0.5 : 0));
  [_title drawAtPoint:origin withAttributes:attributes];

  if (_keyboard_focus && self.window.firstResponder == self) {
    NSColor* ring = [tokens.accent colorWithAlphaComponent:0.5];
    StrokeRounded(NSInsetRect(self.bounds, -1.5, -1.5), tokens.button_radius >= 999
                                                          ? (self.bounds.size.height + 3) / 2
                                                          : tokens.button_radius + 1.5,
                  ring, 3);
  }
}

- (BOOL)acceptsFirstResponder {
  return _enabled;
}

- (BOOL)becomeFirstResponder {
  _keyboard_focus = !_focusing_from_mouse;
  [self setNeedsDisplay:YES];
  return YES;
}

- (BOOL)resignFirstResponder {
  _keyboard_focus = NO;
  [self setNeedsDisplay:YES];
  return YES;
}

- (void)mouseDown:(NSEvent*)event {
  if (!_enabled) {
    return;
  }
  _focusing_from_mouse = YES;
  [self.window makeFirstResponder:self];
  _focusing_from_mouse = NO;
  _highlighted = YES;
  [self setNeedsDisplay:YES];
}

- (void)mouseDragged:(NSEvent*)event {
  if (!_enabled) {
    return;
  }
  const NSPoint point = [self convertPoint:event.locationInWindow fromView:nil];
  const BOOL inside = NSPointInRect(point, self.bounds);
  if (inside != _highlighted) {
    _highlighted = inside;
    [self setNeedsDisplay:YES];
  }
}

- (void)mouseUp:(NSEvent*)event {
  if (!_enabled) {
    return;
  }
  const BOOL was_highlighted = _highlighted;
  _highlighted = NO;
  [self setNeedsDisplay:YES];
  const NSPoint point = [self convertPoint:event.locationInWindow fromView:nil];
  if (was_highlighted && NSPointInRect(point, self.bounds)) {
    [self performClick];
  }
}

/// Sends the action, as a click or the return key would.
- (void)performClick {
  if (!_enabled) {
    return;
  }
  id target = _target;
  if (target != nil && _action != nil && [target respondsToSelector:_action]) {
    [NSApp sendAction:_action to:target from:self];
  }
}

- (BOOL)accessibilityIsIgnored {
  return NO;
}

- (NSString*)accessibilityRole {
  return NSAccessibilityButtonRole;
}

- (NSString*)accessibilityLabel {
  return _title;
}

- (BOOL)accessibilityPerformPress {
  [self performClick];
  return YES;
}

@end

// --- the check box ---------------------------------------------------------------------

@implementation DazzleCheckbox {
  NSInteger _eventTag;
  NSString* _title;
  BOOL _checked;
  BOOL _enabled;
  const MacTokens* _tokens;
  __weak id _target;
  SEL _action;
}

+ (instancetype)checkboxWithTitle:(NSString*)title target:(id)target action:(SEL)action {
  DazzleCheckbox* checkbox = [[DazzleCheckbox alloc] initWithFrame:NSZeroRect];
  checkbox->_title = [title copy];
  checkbox->_enabled = YES;
  checkbox->_target = target;
  checkbox->_action = action;
  checkbox->_tokens = &dazzle::internal::MacCurrentTokens();
  [checkbox sizeToFit];
  return checkbox;
}

- (BOOL)isFlipped {
  return NO;
}

- (void)setEventTag:(NSInteger)eventTag {
  _eventTag = eventTag;
}

- (NSInteger)eventTag {
  return _eventTag;
}

- (void)setChecked:(BOOL)checked {
  _checked = checked;
  [self setNeedsDisplay:YES];
}

- (BOOL)isChecked {
  return _checked;
}

- (void)setTitle:(NSString*)title {
  _title = [title copy];
  [self sizeToFit];
}

- (NSString*)title {
  return _title;
}

- (void)sizeToFit {
  const CGFloat box = _tokens != nil ? _tokens->checkbox_size : 12;
  const CGFloat gap = _tokens != nil ? _tokens->checkbox_gap : 5;
  NSDictionary* attributes = @{NSFontAttributeName : ControlFont(11, NO)};
  const NSSize text = [_title sizeWithAttributes:attributes];
  const CGFloat height = MAX(box, text.height);
  self.frame = NSMakeRect(0, 0, box + gap + ceil(text.width), ceil(height));
}

- (void)viewDidChangeEffectiveAppearance {
  [super viewDidChangeEffectiveAppearance];
  _tokens = &dazzle::internal::MacTokensForAppearance(self.effectiveAppearance);
  [self setNeedsDisplay:YES];
}

- (void)drawRect:(NSRect)dirtyRect {
  const MacTokens& tokens = *_tokens;
  const CGFloat size = tokens.checkbox_size;
  const CGFloat radius = tokens.checkbox_radius;
  const NSRect box = NSMakeRect(0, NSMidY(self.bounds) - size / 2, size, size);
  const NSRect face = NSInsetRect(box, 0.25, 0.25);

  if (_checked) {
    FillRounded(face, radius, tokens.accent);
    // The check: the prototype's stroked path, 80% of the box, round caps.
    NSBezierPath* check = [NSBezierPath bezierPath];
    const CGFloat inset = size * 0.28;
    [check moveToPoint:NSMakePoint(NSMinX(box) + inset, NSMidY(box) + size * 0.02)];
    [check lineToPoint:NSMakePoint(NSMidX(box) - size * 0.02, NSMinY(box) + inset)];
    [check lineToPoint:NSMakePoint(NSMaxX(box) - inset, NSMaxY(box) - inset)];
    check.lineWidth = 1.8;
    check.lineCapStyle = NSLineCapStyleRound;
    check.lineJoinStyle = NSLineJoinStyleRound;
    [[NSColor whiteColor] setStroke];
    [check stroke];
  } else {
    FillRounded(face, radius, tokens.button_background);
    StrokeRounded(face, radius, tokens.separator, 1.0);
    StrokeRounded(face, radius, tokens.button_border, 0.5);
  }

  NSDictionary* attributes = @{
    NSFontAttributeName : ControlFont(11, NO),
    NSForegroundColorAttributeName : _enabled ? tokens.text : tokens.text_disabled
  };
  const NSSize text = [_title sizeWithAttributes:attributes];
  [_title drawAtPoint:NSMakePoint(NSMaxX(box) + tokens.checkbox_gap,
                                  NSMidY(self.bounds) - text.height / 2)
       withAttributes:attributes];
}

- (BOOL)acceptsFirstResponder {
  return _enabled;
}

- (void)mouseDown:(NSEvent*)event {
  if (!_enabled) {
    return;
  }
  [self.window makeFirstResponder:self];
  _checked = !_checked;
  [self setNeedsDisplay:YES];
  [self sendAction];
}

- (BOOL)performKeyEquivalent:(NSEvent*)event {
  if ([event.charactersIgnoringModifiers isEqualToString:@" "]) {
    _checked = !_checked;
    [self setNeedsDisplay:YES];
    [self sendAction];
    return YES;
  }
  return NO;
}

- (void)sendAction {
  id target = _target;
  if (target != nil && _action != nil && [target respondsToSelector:_action]) {
    [NSApp sendAction:_action to:target from:self];
  }
}

- (NSString*)accessibilityRole {
  return NSAccessibilityCheckBoxRole;
}

- (NSString*)accessibilityLabel {
  return _title;
}

- (id)accessibilityValue {
  return @(_checked);
}

- (BOOL)accessibilityPerformPress {
  _checked = !_checked;
  [self setNeedsDisplay:YES];
  [self sendAction];
  return YES;
}

@end

// --- the progress bar ------------------------------------------------------------------

@implementation DazzleProgressBar {
  const MacTokens* _tokens;
  BOOL _indeterminate;
  double _fraction;
  NSTimer* _animation;
  NSTimeInterval _started;
}

- (instancetype)initWithFrame:(NSRect)frame {
  self = [super initWithFrame:frame];
  if (self != nil) {
    _tokens = &dazzle::internal::MacCurrentTokens();
    _fraction = 0.0;
  }
  return self;
}

- (BOOL)isFlipped {
  return NO;
}

- (void)setIndeterminate:(BOOL)indeterminate {
  if (_indeterminate == indeterminate) {
    return;
  }
  _indeterminate = indeterminate;
  if (indeterminate) {
    [self startAnimating];
  } else {
    [self stopAnimating];
  }
  [self setNeedsDisplay:YES];
}

- (BOOL)isIndeterminate {
  return _indeterminate;
}

- (void)setFraction:(double)fraction {
  _fraction = MAX(0.0, MIN(1.0, fraction));
  [self setNeedsDisplay:YES];
}

- (double)fraction {
  return _fraction;
}

- (void)startAnimating {
  if (_animation != nil) {
    return;
  }
  _started = [NSDate timeIntervalSinceReferenceDate];
  // The prototype's sweep: 30% of the bar, 1.4 s, ease-in-out, repeating.
  __weak DazzleProgressBar* weak_self = self;
  _animation = [NSTimer scheduledTimerWithTimeInterval:1.0 / 60.0
                                               repeats:YES
                                                 block:^(NSTimer* timer) {
                                                   [weak_self setNeedsDisplay:YES];
                                                 }];
}

- (void)stopAnimating {
  [_animation invalidate];
  _animation = nil;
}

- (void)dealloc {
  [_animation invalidate];
}

- (void)viewDidMoveToWindow {
  [super viewDidMoveToWindow];
  if (self.window == nil) {
    [self stopAnimating];
  } else if (_indeterminate) {
    [self startAnimating];
  }
}

- (void)viewDidChangeEffectiveAppearance {
  [super viewDidChangeEffectiveAppearance];
  _tokens = &dazzle::internal::MacTokensForAppearance(self.effectiveAppearance);
  [self setNeedsDisplay:YES];
}

- (NSSize)intrinsicContentSize {
  return NSMakeSize(NSViewNoIntrinsicMetric, _tokens != nil ? _tokens->progress_height : 6);
}

- (void)drawRect:(NSRect)dirtyRect {
  const MacTokens& tokens = *_tokens;
  const CGFloat height = MIN(tokens.progress_height, self.bounds.size.height);
  const NSRect track =
      NSMakeRect(0, NSMidY(self.bounds) - height / 2, self.bounds.size.width, height);
  const CGFloat radius = tokens.progress_radius >= 999 ? height / 2 : tokens.progress_radius;

  FillRounded(track, radius, tokens.progress_track);
  if (tokens.progress_track_border) {
    StrokeRounded(track, radius, Rgba(0, 0, 0, 0.06), 0.5);
  }

  if (_indeterminate) {
    // from { left: -30% } to { left: 100% } over 1.4 s, with the sweep's own width.
    const NSTimeInterval elapsed =
        [NSDate timeIntervalSinceReferenceDate] - _started;
    const double phase = fmod(elapsed, 1.4) / 1.4;
    const double eased = 0.5 - 0.5 * cos(phase * M_PI);  // ease-in-out
    const CGFloat sweep = track.size.width * 0.3;
    const CGFloat left = -sweep + eased * (track.size.width + sweep);
    const NSRect fill = NSIntersectionRect(track, NSMakeRect(left, track.origin.y, sweep, height));
    if (fill.size.width > 0) {
      FillRounded(fill, radius, tokens.accent);
    }
    return;
  }

  const NSRect fill = NSMakeRect(track.origin.x, track.origin.y,
                                 track.size.width * _fraction, height);
  if (fill.size.width > 0.5) {
    FillRounded(fill, radius, tokens.accent);
  }
}

- (NSString*)accessibilityRole {
  return NSAccessibilityProgressIndicatorRole;
}

- (id)accessibilityValue {
  return _indeterminate ? nil : @(_fraction);
}

@end
