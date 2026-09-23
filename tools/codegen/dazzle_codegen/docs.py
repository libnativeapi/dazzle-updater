"""Turns a parsed Doxygen comment into the paragraphs a binding's doc comment shows."""

from __future__ import annotations

import re

from .model import STRING, Doc, Method, Property


def _lower_first(text: str) -> str:
    return text[:1].lower() + text[1:] if text[:1].isupper() and text[1:2].islower() else text


def _indent_continuation(text: str, indent: str) -> str:
    first, *rest = text.split("\n")
    return "\n".join([first, *(indent + line.strip() for line in rest)])


def blocks(doc: Doc, refs: dict[str, str], *, params: bool = True, returns: bool = True) -> list[str]:
    """Paragraphs in reading order: summary, body, parameters, return value, notes.

    `refs` maps C++ member names to what the binding calls them, so "SetFeedUrl()" in
    the header reads as that language's name.
    """
    out = [p for p in [doc.brief, *doc.paragraphs] if p]
    if params and doc.params:
        out.append("\n".join(_indent_continuation(f"- `{n}`: {t}", "  ") for n, t in doc.params.items()))
    if returns and doc.returns:
        out.append("Returns " + _lower_first(doc.returns))
    out.extend(doc.notes)
    return [_rename(p, refs) for p in out]


def _rename(text: str, refs: dict[str, str]) -> str:
    def replace(match: re.Match) -> str:
        name = refs.get(match.group(1))
        return f"`{name}`" if name else match.group(0)

    return re.sub(r"`?\b(\w+)\(\)`?", replace, text)


def method_blocks(method: Method, refs: dict[str, str], empty_string: str) -> list[str]:
    """Blocks for a method; `empty_string` names what an empty string comes back as."""
    out = blocks(method.doc, refs)
    if method.returns == STRING:
        out.append(f"An empty string comes back as {empty_string}.")
    return out


def property_blocks(prop: Property, refs: dict[str, str], empty_string: str) -> list[str]:
    """Blocks for a property: the getter's comment, then the setter's."""
    out = blocks(prop.getter.doc, refs)
    if prop.setter:
        out.extend(blocks(prop.setter.doc, refs, params=False))
    if prop.type == STRING:
        out.append(f"An empty string comes back as {empty_string}.")
    return out


def line_comment(prefix: str, paragraphs: list[str]) -> list[str]:
    """Renders paragraphs as `prefix` comment lines, blank comment lines between them."""
    lines: list[str] = []
    for index, paragraph in enumerate(paragraphs):
        if index:
            lines.append(prefix.rstrip())
        lines.extend((prefix + line).rstrip() for line in paragraph.split("\n"))
    return lines
