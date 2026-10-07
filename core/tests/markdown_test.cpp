#include "test_support.h"

#include "../src/markdown/markdown.h"

#include <string>

// The Markdown subset the notes box renders (specs/updater-ui.md, "更新说明的渲染"),
// over the vendored parser. AppKit turns this HTML into the attributed string of the
// macOS notes box and the Windows rich edit control will be fed its RTF sibling, so the
// shape of the markup matters, not only the text. The expectations spell out the
// writer's own layout: a block ends with a newline, a soft line break stays one.

using dazzle_updater::MarkdownToHtml;

DAZZLE_TEST(markdown_html_renders_headings_and_paragraphs) {
  EXPECT_EQ(MarkdownToHtml("# Title\n\nSome text.\n"), "<h1>Title</h1>\n<p>Some text.</p>\n");
  EXPECT_EQ(MarkdownToHtml("### Three\n"), "<h3>Three</h3>\n");
  // A paragraph keeps the source's soft line break; a blank line starts a new block.
  EXPECT_EQ(MarkdownToHtml("One\ntwo.\n\n## Two\n"), "<p>One\ntwo.</p>\n<h2>Two</h2>\n");
}

DAZZLE_TEST(markdown_html_renders_emphasis_code_and_links) {
  EXPECT_EQ(MarkdownToHtml("**bold** and *italic* and `code`"),
            "<p><strong>bold</strong> and <em>italic</em> and <code>code</code></p>\n");
  EXPECT_EQ(MarkdownToHtml("[notes](https://example.com/a)"),
            "<p><a href=\"https://example.com/a\">notes</a></p>\n");
  EXPECT_EQ(MarkdownToHtml("[mail](mailto:dev@example.com)"),
            "<p><a href=\"mailto:dev@example.com\">mail</a></p>\n");
  // An autolink is a link whose text is its own address.
  EXPECT_EQ(MarkdownToHtml("<https://example.com>"),
            "<p><a href=\"https://example.com\">https://example.com</a></p>\n");
}

DAZZLE_TEST(markdown_html_refuses_links_the_browser_must_not_be_handed) {
  // The text stays; the anchor does not, so a release notes file cannot make the window
  // open a local file or a custom scheme.
  EXPECT_EQ(MarkdownToHtml("[open](file:///etc/passwd)"), "<p>open</p>\n");
  EXPECT_EQ(MarkdownToHtml("[run](javascript:alert(1))"), "<p>run</p>\n");
}

DAZZLE_TEST(markdown_html_renders_lists) {
  EXPECT_EQ(MarkdownToHtml("- one\n- two\n"), "<ul>\n<li>one</li>\n<li>two</li>\n</ul>\n");
  EXPECT_EQ(MarkdownToHtml("1. one\n2. two\n"), "<ol>\n<li>one</li>\n<li>two</li>\n</ol>\n");
  EXPECT_EQ(MarkdownToHtml("3. three\n4. four\n"),
            "<ol start=\"3\">\n<li>three</li>\n<li>four</li>\n</ol>\n");
  // A loose list keeps its paragraphs, a tight one has none, as CommonMark's HTML does.
  EXPECT_EQ(MarkdownToHtml("- one\n\n- two\n"),
            "<ul>\n<li><p>one</p>\n</li>\n<li><p>two</p>\n</li>\n</ul>\n");
}

DAZZLE_TEST(markdown_html_renders_code_blocks) {
  EXPECT_EQ(MarkdownToHtml("```\nint main() {}\n```\n"),
            "<pre><code>int main() {}\n</code></pre>\n");
  EXPECT_EQ(MarkdownToHtml("```cpp\nint x = 1 < 2;\n```\n"),
            "<pre><code>int x = 1 &lt; 2;\n</code></pre>\n");
}

DAZZLE_TEST(markdown_html_drops_images) {
  // No <img>, and no alt text either: the notes box shows no images.
  EXPECT_EQ(MarkdownToHtml("![a chart](https://example.com/chart.png)"), "<p></p>\n");
  EXPECT_EQ(MarkdownToHtml("before ![a chart](https://example.com/c.png) after"),
            "<p>before  after</p>\n");
}

DAZZLE_TEST(markdown_html_escapes_what_is_not_markup) {
  EXPECT_EQ(MarkdownToHtml("a < b & c > d"), "<p>a &lt; b &amp; c &gt; d</p>\n");
  // Raw HTML is text, not markup: a release notes file cannot inject a tag.
  EXPECT_EQ(MarkdownToHtml("<script>alert(1)</script>"),
            "<p>&lt;script&gt;alert(1)&lt;/script&gt;</p>\n");
  EXPECT_EQ(MarkdownToHtml("<img src=\"https://example.com/x.png\">"),
            "<p>&lt;img src=&quot;https://example.com/x.png&quot;&gt;</p>\n");
  // An entity the notes file wrote itself is already markup and keeps its spelling.
  EXPECT_EQ(MarkdownToHtml("Fish &amp; Chips"), "<p>Fish &amp; Chips</p>\n");
}

DAZZLE_TEST(markdown_html_renders_nothing_for_empty_notes) {
  EXPECT_EQ(MarkdownToHtml(""), "");
  EXPECT_EQ(MarkdownToHtml("\n  \n"), "");
}
