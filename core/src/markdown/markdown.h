#pragma once

#include <string>

// The Markdown the update windows render (../specs/updater-ui.md, "更新说明的渲染").
//
// Updater hands the release notes over as Markdown (../specs/updater.md), and each
// platform's text control wants a different markup, so the conversion happens here, in
// the cross-platform layer: HTML for AppKit's NSAttributedString today (markdown_html.cpp),
// RTF for the Windows rich edit control when that backend is written (markdown_rtf.cpp).
//
// The subset is the one the spec lists: headings, paragraphs, ordered and unordered
// lists, bold, italic, inline code, code blocks and links. Images and raw HTML are
// dropped rather than shown, so nothing a release notes file contains can make a window
// load a remote resource; a link is only rendered for a scheme the system browser can
// open safely (http, https, mailto).

namespace dazzle_updater {

/// `markdown` as an HTML fragment; "" renders to the empty document.
std::string MarkdownToHtml(const std::string& markdown);

}  // namespace dazzle_updater
