#include "markdown.h"

#include <cstring>
#include <string>
#include <vector>

#include "md4c/md4c.h"

// Markdown → HTML, over md4c's parser (core/third_party/md4c). The writer is dazzle_updater's
// own, and renders only what ../specs/updater-ui.md lists: what the subset does not
// cover is dropped rather than passed through.
//
// AppKit turns this HTML into the attributed string the notes box shows, so the output
// is a fragment: no <html>, <head> or <body>, no styling — the window applies the
// platform's own font, size and colors afterwards.

namespace dazzle_updater {
namespace {

/// Escapes the characters that would otherwise be read as markup. `&` is escaped even in
/// text, so a release notes file cannot smuggle an entity in; the parser's own entities
/// arrive through MD_TEXT_ENTITY and are written verbatim.
void AppendEscaped(std::string* out, const char* text, size_t size) {
  for (size_t index = 0; index < size; ++index) {
    switch (text[index]) {
      case '&':
        out->append("&amp;");
        break;
      case '<':
        out->append("&lt;");
        break;
      case '>':
        out->append("&gt;");
        break;
      case '"':
        out->append("&quot;");
        break;
      default:
        out->push_back(text[index]);
        break;
    }
  }
}

/// An MD_ATTRIBUTE (a link's href, an image's src) as text. It arrives as a run of
/// substrings, each with its own type: entities are already markup and keep their
/// spelling, everything else is escaped.
void AppendAttribute(std::string* out, const MD_ATTRIBUTE& attribute) {
  for (int index = 0; attribute.substr_offsets[index] < attribute.size; ++index) {
    const MD_OFFSET offset = attribute.substr_offsets[index];
    const MD_SIZE size = attribute.substr_offsets[index + 1] - offset;
    const char* text = attribute.text + offset;
    if (attribute.substr_types[index] == MD_TEXT_ENTITY ||
        attribute.substr_types[index] == MD_TEXT_NULLCHAR) {
      out->append(text, size);
    } else {
      AppendEscaped(out, text, size);
    }
  }
}

/// Whether the system browser can be handed this link. Anything else — a local file, a
/// custom scheme — is rendered as plain text, so a release notes file cannot make the
/// window open something unexpected.
bool IsOpenableScheme(const std::string& href) {
  return href.compare(0, 7, "http://") == 0 || href.compare(0, 8, "https://") == 0 ||
         href.compare(0, 7, "mailto:") == 0;
}

/// The parser's callbacks, gathering the HTML as they go.
struct HtmlWriter {
  std::string html;
  /// The open blocks, innermost last: a paragraph needs to know whether the list item it
  /// sits in is tight, in which case CommonMark's HTML has no <p> around it.
  std::vector<MD_BLOCKTYPE> blocks;
  /// Whether each open list is tight, innermost last.
  std::vector<bool> tight_lists;
  /// The levels of the open headings, innermost last: the leave callback is not told the
  /// level, so it is remembered from the enter callback.
  std::vector<unsigned> heading_levels;
  /// > 0 while inside an image: its text is the alt text, and images are not shown.
  int images = 0;
  /// Whether the paragraph being written had its <p> left out.
  bool paragraph_bare = false;
};

int EnterBlock(MD_BLOCKTYPE type, void* detail, void* userdata) {
  HtmlWriter& writer = *static_cast<HtmlWriter*>(userdata);
  switch (type) {
    case MD_BLOCK_DOC:
      break;
    case MD_BLOCK_QUOTE:
      writer.html.append("<blockquote>\n");
      break;
    case MD_BLOCK_UL:
      writer.html.append("<ul>\n");
      writer.tight_lists.push_back(static_cast<MD_BLOCK_UL_DETAIL*>(detail)->is_tight != 0);
      break;
    case MD_BLOCK_OL: {
      const MD_BLOCK_OL_DETAIL* ordered = static_cast<MD_BLOCK_OL_DETAIL*>(detail);
      if (ordered->start > 1) {
        writer.html.append("<ol start=\"" + std::to_string(ordered->start) + "\">\n");
      } else {
        writer.html.append("<ol>\n");
      }
      writer.tight_lists.push_back(ordered->is_tight != 0);
      break;
    }
    case MD_BLOCK_LI:
      writer.html.append("<li>");
      break;
    case MD_BLOCK_HR:
      writer.html.append("<hr>\n");
      break;
    case MD_BLOCK_H: {
      const unsigned level = static_cast<MD_BLOCK_H_DETAIL*>(detail)->level;
      writer.html.append("<h" + std::to_string(level) + ">");
      writer.heading_levels.push_back(level);
      break;
    }
    case MD_BLOCK_CODE:
      writer.html.append("<pre><code>");
      break;
    case MD_BLOCK_P: {
      writer.paragraph_bare = !writer.blocks.empty() && writer.blocks.back() == MD_BLOCK_LI &&
                             !writer.tight_lists.empty() && writer.tight_lists.back();
      if (!writer.paragraph_bare) {
        writer.html.append("<p>");
      }
      break;
    }
    case MD_BLOCK_HTML:
      break;  // raw HTML never arrives: the parser runs with MD_FLAG_NOHTML
    default:
      break;  // tables and their parts: the extension is off, so they cannot arrive
  }
  writer.blocks.push_back(type);
  return 0;
}

int LeaveBlock(MD_BLOCKTYPE type, void* /*detail*/, void* userdata) {
  HtmlWriter& writer = *static_cast<HtmlWriter*>(userdata);
  if (!writer.blocks.empty()) {
    writer.blocks.pop_back();
  }
  switch (type) {
    case MD_BLOCK_QUOTE:
      writer.html.append("</blockquote>\n");
      break;
    case MD_BLOCK_UL:
      writer.html.append("</ul>\n");
      if (!writer.tight_lists.empty()) {
        writer.tight_lists.pop_back();
      }
      break;
    case MD_BLOCK_OL:
      writer.html.append("</ol>\n");
      if (!writer.tight_lists.empty()) {
        writer.tight_lists.pop_back();
      }
      break;
    case MD_BLOCK_LI:
      writer.html.append("</li>\n");
      break;
    case MD_BLOCK_H: {
      const unsigned level = writer.heading_levels.empty() ? 1u : writer.heading_levels.back();
      if (!writer.heading_levels.empty()) {
        writer.heading_levels.pop_back();
      }
      writer.html.append("</h" + std::to_string(level) + ">\n");
      break;
    }
    case MD_BLOCK_CODE:
      writer.html.append("</code></pre>\n");
      break;
    case MD_BLOCK_P:
      if (!writer.paragraph_bare) {
        writer.html.append("</p>\n");
      }
      break;
    default:
      break;
  }
  return 0;
}

int EnterSpan(MD_SPANTYPE type, void* detail, void* userdata) {
  HtmlWriter& writer = *static_cast<HtmlWriter*>(userdata);
  switch (type) {
    case MD_SPAN_EM:
      writer.html.append("<em>");
      break;
    case MD_SPAN_STRONG:
      writer.html.append("<strong>");
      break;
    case MD_SPAN_CODE:
      writer.html.append("<code>");
      break;
    case MD_SPAN_A: {
      const MD_SPAN_A_DETAIL* link = static_cast<MD_SPAN_A_DETAIL*>(detail);
      std::string href;
      AppendAttribute(&href, link->href);
      // The text of a link whose target the browser must not be handed stays.
      if (IsOpenableScheme(href)) {
        writer.html.append("<a href=\"" + href + "\">");
      }
      break;
    }
    case MD_SPAN_IMG:
      ++writer.images;
      break;
    default:
      break;
  }
  return 0;
}

int LeaveSpan(MD_SPANTYPE type, void* detail, void* userdata) {
  HtmlWriter& writer = *static_cast<HtmlWriter*>(userdata);
  switch (type) {
    case MD_SPAN_EM:
      writer.html.append("</em>");
      break;
    case MD_SPAN_STRONG:
      writer.html.append("</strong>");
      break;
    case MD_SPAN_CODE:
      writer.html.append("</code>");
      break;
    case MD_SPAN_A: {
      const MD_SPAN_A_DETAIL* link = static_cast<MD_SPAN_A_DETAIL*>(detail);
      std::string href;
      AppendAttribute(&href, link->href);
      if (IsOpenableScheme(href)) {
        writer.html.append("</a>");
      }
      break;
    }
    case MD_SPAN_IMG:
      if (writer.images > 0) {
        --writer.images;
      }
      break;
    default:
      break;
  }
  return 0;
}

int Text(MD_TEXTTYPE type, const MD_CHAR* text, MD_SIZE size, void* userdata) {
  HtmlWriter& writer = *static_cast<HtmlWriter*>(userdata);
  if (writer.images > 0) {
    return 0;  // an image's text is its alt text, and the window shows no images
  }
  switch (type) {
    case MD_TEXT_NULLCHAR:
      writer.html.append("\xEF\xBF\xBD");  // U+FFFD, as CommonMark requires
      break;
    case MD_TEXT_BR:
      writer.html.append("<br>\n");
      break;
    case MD_TEXT_SOFTBR:
      writer.html.push_back('\n');
      break;
    case MD_TEXT_ENTITY:
      writer.html.append(text, size);  // already an entity
      break;
    default:
      AppendEscaped(&writer.html, text, size);
      break;
  }
  return 0;
}

}  // namespace

std::string MarkdownToHtml(const std::string& markdown) {
  HtmlWriter writer;
  MD_PARSER parser;
  std::memset(&parser, 0, sizeof(parser));
  parser.flags = MD_FLAG_NOHTML;  // raw HTML is text, never markup
  parser.enter_block = EnterBlock;
  parser.leave_block = LeaveBlock;
  parser.enter_span = EnterSpan;
  parser.leave_span = LeaveSpan;
  parser.text = Text;
  md_parse(markdown.c_str(), static_cast<MD_SIZE>(markdown.size()), &parser, &writer);
  return writer.html;
}

}  // namespace dazzle_updater
