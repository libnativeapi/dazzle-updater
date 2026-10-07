#pragma once

#include <gtk/gtk.h>

#include <string>
#include <vector>

// The release notes as GTK text: ../../../specs/updater-ui.md, "更新说明的渲染".
//
// The parser is the vendored md4c the other two platforms use (src/markdown/), but the
// macOS and Windows writers produce HTML and RTF strings for their rich text controls,
// while GTK's own text control has no markup language at all — it holds text with
// `GtkTextTag`s applied to ranges. So this writer lives with the GTK windows rather than
// in src/markdown/, and it writes into the buffer instead of returning a string.
//
// The subset is the one the spec lists, with the same handling as the other platforms:
// headings, paragraphs, ordered and unordered lists, bold, italic, inline code, code
// blocks and links. Images and raw HTML are dropped, and only http, https and mailto
// links become links.

namespace dazzle_updater {
namespace internal {

/// Renders `markdown` into `buffer`, appending one entry to `urls` per link it draws.
/// Each link's tag is named "link-<n>" and carries n as its "dazzle_updater-link-index" data,
/// which is how the backend finds the target of a click without keeping its own map.
void RenderNotesInto(GtkTextBuffer* buffer, const std::string& markdown,
                     std::vector<std::string>* urls);

}  // namespace internal
}  // namespace dazzle_updater
