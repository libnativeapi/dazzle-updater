#include "updater_ui_notes_linux.h"

#include <cstring>
#include <string>
#include <vector>

#include "md4c/md4c.h"

// Markdown → a GtkTextBuffer over md4c's parser (core/third_party/md4c). The tags it
// applies are the GTK equivalent of the HTML the other platforms hand their rich text
// controls, and the sizes are the prototype's (specs/updater-ui.md, "外观"): body 13 px,
// a heading 16 px, an h3 14 px, code 12 px.
//
// md4c reports the document as blocks and spans with the text in between, so the writer
// keeps a small state: the tag of the block being drawn, and the stack of tags the
// current span opened. Text is inserted with all of them applied, which is what makes
// bold inside a heading, or a link inside a list item, come out right.

namespace dazzle_updater {
namespace internal {
namespace {

const char* const kHeadingTag = "heading";
const char* const kSubheadingTag = "subheading";
const char* const kParagraphTag = "paragraph";
const char* const kEmphasisTag = "emphasis";
const char* const kStrongTag = "strong";
const char* const kCodeTag = "code";
const char* const kCodeBlockTag = "code-block";
const char* const kListItemTag = "list-item";

/// The scheme test the spec asks for: only a link the system browser can open safely is
/// a link. Anything else — `file:`, `javascript:`, an application's own scheme — is drawn
/// as plain text.
bool IsLinkableScheme(const std::string& url) {
  return url.compare(0, 7, "http://") == 0 || url.compare(0, 8, "https://") == 0 ||
         url.compare(0, 7, "mailto:") == 0;
}

struct RenderState {
  GtkTextBuffer* buffer = nullptr;
  std::vector<std::string>* urls = nullptr;
  const char* link_color = "#1c71d8";
  /// The tag of the block being drawn, applied to everything inside it.
  std::string block_tag;
  /// The tags the enclosing spans opened, innermost last.
  std::vector<std::string> span_tags;
  bool in_code_block = false;
  /// An image's alt text is not shown either: the spec drops images entirely.
  bool swallowing_image = false;
  int list_depth = 0;
  /// One counter per open ordered list; unordered lists push a false.
  std::vector<bool> ordered;
  std::vector<int> counters;
};

/// Creates the tags once per buffer; every size and margin is the prototype's.
void CreateTags(GtkTextBuffer* buffer) {
  gtk_text_buffer_create_tag(buffer, "body", "size-points", 9.75, nullptr);  // 13 px
  gtk_text_buffer_create_tag(buffer, kHeadingTag, "weight", PANGO_WEIGHT_BOLD, "size-points",
                             12.0, "pixels-above-lines", 12, "pixels-below-lines", 6, nullptr);
  gtk_text_buffer_create_tag(buffer, kSubheadingTag, "weight", PANGO_WEIGHT_BOLD, "size-points",
                             10.5, "pixels-above-lines", 10, "pixels-below-lines", 4, nullptr);
  gtk_text_buffer_create_tag(buffer, kParagraphTag, "pixels-above-lines", 4, "pixels-below-lines",
                             4, nullptr);
  gtk_text_buffer_create_tag(buffer, kEmphasisTag, "style", PANGO_STYLE_ITALIC, nullptr);
  gtk_text_buffer_create_tag(buffer, kStrongTag, "weight", PANGO_WEIGHT_BOLD, nullptr);
  gtk_text_buffer_create_tag(buffer, kCodeTag, "family", "monospace", "size-points", 9.0, nullptr);
  gtk_text_buffer_create_tag(buffer, kCodeBlockTag, "family", "monospace", "size-points", 9.0,
                             "indent", 12, "pixels-above-lines", 6, "pixels-below-lines", 6,
                             nullptr);
  gtk_text_buffer_create_tag(buffer, kListItemTag, "indent", 24, "pixels-above-lines", 2,
                             "pixels-below-lines", 2, nullptr);
}

/// Inserts `text` at the end of the buffer with the body tag, the block's tag and every
/// open span's. The start is kept as an offset: gtk_text_buffer_insert() moves the iterator
/// it is given to the end of what it inserted, and a tag applied from there covers nothing.
void Append(RenderState* state, const std::string& text) {
  if (text.empty()) {
    return;
  }
  GtkTextIter start;
  GtkTextIter end;
  gtk_text_buffer_get_end_iter(state->buffer, &end);
  const int start_offset = gtk_text_iter_get_offset(&end);
  gtk_text_buffer_insert(state->buffer, &end, text.c_str(), static_cast<int>(text.size()));
  gtk_text_buffer_get_iter_at_offset(state->buffer, &start, start_offset);
  gtk_text_buffer_get_end_iter(state->buffer, &end);
  gtk_text_buffer_apply_tag_by_name(state->buffer, "body", &start, &end);
  if (!state->block_tag.empty()) {
    gtk_text_buffer_apply_tag_by_name(state->buffer, state->block_tag.c_str(), &start, &end);
  }
  for (const std::string& tag : state->span_tags) {
    gtk_text_buffer_apply_tag_by_name(state->buffer, tag.c_str(), &start, &end);
  }
}

/// Starts a new line for the next block, unless the buffer is empty or already ends in
/// one: blocks are separated by exactly one newline, and the spacing between them is the
/// tags' pixels-above/below-lines. The separator carries no block tag, so it stays part of
/// the line it ends.
void BeginLine(RenderState* state) {
  GtkTextIter end;
  gtk_text_buffer_get_end_iter(state->buffer, &end);
  if (gtk_text_iter_is_start(&end)) {
    return;
  }
  GtkTextIter last = end;
  gtk_text_iter_backward_char(&last);
  if (gtk_text_iter_get_char(&last) == '\n') {
    return;
  }
  gtk_text_buffer_insert(state->buffer, &end, "\n", 1);
}

int EnterBlock(MD_BLOCKTYPE type, void* detail, void* userdata) {
  RenderState* state = static_cast<RenderState*>(userdata);
  switch (type) {
    case MD_BLOCK_H: {
      const auto* heading = static_cast<const MD_BLOCK_H_DETAIL*>(detail);
      BeginLine(state);
      state->block_tag = heading->level <= 2 ? kHeadingTag : kSubheadingTag;
      break;
    }
    case MD_BLOCK_P:
      // A paragraph inside a list item (a loose list) continues the item's own line.
      if (state->block_tag != kListItemTag) {
        BeginLine(state);
        state->block_tag = kParagraphTag;
      }
      break;
    case MD_BLOCK_CODE:
      BeginLine(state);
      state->in_code_block = true;
      state->block_tag = kCodeBlockTag;
      break;
    case MD_BLOCK_UL:
      ++state->list_depth;
      state->ordered.push_back(false);
      state->counters.push_back(0);
      break;
    case MD_BLOCK_OL: {
      const auto* list = static_cast<const MD_BLOCK_OL_DETAIL*>(detail);
      ++state->list_depth;
      state->ordered.push_back(true);
      state->counters.push_back(static_cast<int>(list->start) - 1);
      break;
    }
    case MD_BLOCK_LI:
      BeginLine(state);
      state->block_tag = kListItemTag;
      if (!state->ordered.empty() && state->ordered.back()) {
        state->counters.back() += 1;
        Append(state, std::to_string(state->counters.back()) + ". ");
      } else {
        Append(state, "• ");
      }
      break;
    case MD_BLOCK_QUOTE:
      BeginLine(state);
      state->block_tag = kParagraphTag;
      break;
    default:
      break;
  }
  return 0;
}

int LeaveBlock(MD_BLOCKTYPE type, void* detail, void* userdata) {
  (void)detail;
  RenderState* state = static_cast<RenderState*>(userdata);
  switch (type) {
    case MD_BLOCK_P:
      if (state->block_tag == kListItemTag) {
        break;  // the item's own line goes on until the item ends
      }
      state->block_tag.clear();
      break;
    case MD_BLOCK_H:
    case MD_BLOCK_CODE:
      state->in_code_block = false;
      // Back to the enclosing list item's style, when the block sat inside one.
      state->block_tag = state->list_depth > 0 ? kListItemTag : "";
      break;
    case MD_BLOCK_UL:
    case MD_BLOCK_OL:
      if (!state->ordered.empty()) {
        state->ordered.pop_back();
        state->counters.pop_back();
      }
      if (state->list_depth > 0) {
        --state->list_depth;
      }
      state->block_tag = state->list_depth > 0 ? kListItemTag : "";
      break;
    case MD_BLOCK_LI:
      state->block_tag.clear();
      break;
    default:
      break;
  }
  return 0;
}

int EnterSpan(MD_SPANTYPE type, void* detail, void* userdata) {
  RenderState* state = static_cast<RenderState*>(userdata);
  switch (type) {
    case MD_SPAN_EM:
      state->span_tags.push_back(kEmphasisTag);
      break;
    case MD_SPAN_STRONG:
      state->span_tags.push_back(kStrongTag);
      break;
    case MD_SPAN_CODE:
      state->span_tags.push_back(kCodeTag);
      break;
    case MD_SPAN_IMG:
      state->swallowing_image = true;  // images are dropped, alt text included
      break;
    case MD_SPAN_A: {
      const auto* link = static_cast<const MD_SPAN_A_DETAIL*>(detail);
      std::string url;
      for (int index = 0; link->href.substr_offsets[index] < link->href.size; ++index) {
        const MD_OFFSET offset = link->href.substr_offsets[index];
        const MD_SIZE size = link->href.substr_offsets[index + 1] - offset;
        url.append(link->href.text + offset, size);
      }
      if (!IsLinkableScheme(url) || state->urls == nullptr) {
        break;  // drawn as plain text
      }
      state->urls->push_back(url);
      const std::string tag = "link-" + std::to_string(state->urls->size() - 1);
      GtkTextTag* text_tag = gtk_text_buffer_create_tag(state->buffer, tag.c_str(), "foreground",
                                                        state->link_color, "underline",
                                                        PANGO_UNDERLINE_SINGLE, nullptr);
      // The index travels on the tag, so a click can find its target without parsing names
      // (GTK 4 has no gtk_text_tag_get_name()).
      g_object_set_data(G_OBJECT(text_tag), "dazzle_updater-link-index",
                        GINT_TO_POINTER(static_cast<int>(state->urls->size())));
      state->span_tags.push_back(tag);
      break;
    }
    default:
      break;
  }
  return 0;
}

int LeaveSpan(MD_SPANTYPE type, void* detail, void* userdata) {
  (void)detail;
  RenderState* state = static_cast<RenderState*>(userdata);
  switch (type) {
    case MD_SPAN_EM:
    case MD_SPAN_STRONG:
    case MD_SPAN_CODE:
    case MD_SPAN_A:
      if (!state->span_tags.empty()) {
        state->span_tags.pop_back();
      }
      break;
    case MD_SPAN_IMG:
      state->swallowing_image = false;
      break;
    default:
      break;
  }
  return 0;
}

int TextCallback(MD_TEXTTYPE type, const MD_CHAR* text, MD_SIZE size, void* userdata) {
  RenderState* state = static_cast<RenderState*>(userdata);
  if (state->swallowing_image || type == MD_TEXT_HTML || type == MD_TEXT_NULLCHAR) {
    return 0;  // raw HTML is dropped, the way images are
  }
  switch (type) {
    case MD_TEXT_BR:
      Append(state, "\n");
      break;
    case MD_TEXT_SOFTBR:
      // A line wrapped in the source is one line of text, as in the HTML the other
      // platforms render; only a code block keeps its lines.
      Append(state, state->in_code_block ? "\n" : " ");
      break;
    case MD_TEXT_CODE:
      // An inline code span inside a code block is already the block's own text.
      if (state->in_code_block) {
        Append(state, std::string(text, size));
      } else {
        state->span_tags.push_back(kCodeTag);
        Append(state, std::string(text, size));
        state->span_tags.pop_back();
      }
      break;
    default:
      Append(state, std::string(text, size));
      break;
  }
  return 0;
}

}  // namespace

void RenderNotesInto(GtkTextBuffer* buffer, const std::string& markdown,
                     std::vector<std::string>* urls, const char* link_color) {
  if (buffer == nullptr) {
    return;
  }
  gtk_text_buffer_set_text(buffer, "", 0);
  CreateTags(buffer);

  RenderState state;
  state.buffer = buffer;
  state.urls = urls;
  if (link_color != nullptr) {
    state.link_color = link_color;
  }

  MD_PARSER parser;
  std::memset(&parser, 0, sizeof(parser));
  parser.flags = MD_DIALECT_GITHUB;
  parser.enter_block = EnterBlock;
  parser.leave_block = LeaveBlock;
  parser.enter_span = EnterSpan;
  parser.leave_span = LeaveSpan;
  parser.text = TextCallback;
  md_parse(markdown.c_str(), static_cast<MD_SIZE>(markdown.size()), &parser, &state);
}

}  // namespace internal
}  // namespace dazzle_updater
