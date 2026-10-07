"""Reads the public C++ headers through clang's JSON AST dump."""

from __future__ import annotations

import json
import os
import re
import subprocess
from pathlib import Path

from .model import BOOL, F64, I32, I64, STRING, VOID, Class, Doc, Method, Param

NAMESPACE = "dazzle_updater"

_TYPES = {
    "void": VOID,
    "bool": BOOL,
    "int": I32,
    "int32_t": I32,
    "int64_t": I64,
    "double": F64,
    "std::string": STRING,
}
# Parameters that may be passed by const reference.
_CONST_REF = {"const std::string &": STRING}


class Warnings(list):
    def skip(self, where: str, why: str) -> None:
        self.append(f"skipped {where}: {why}")


def parse_header(core_src: Path, header: str, warnings: Warnings) -> list[Class]:
    ast = _dump_ast(core_src / header)
    classes = []
    for node in ast:
        if node.get("kind") == "NamespaceDecl" and node.get("name") == NAMESPACE:
            for child in node.get("inner", []):
                if child.get("kind") == "CXXRecordDecl" and "inner" in child:
                    cls = _parse_class(child, header, warnings)
                    if cls:
                        classes.append(cls)
    return classes


def _dump_ast(path: Path) -> list[dict]:
    clang = os.environ.get("CLANG", "clang++")
    command = [
        clang, "-x", "c++-header", "-std=c++17", "-fsyntax-only",
        "-Xclang", "-ast-dump=json", "-Xclang", f"-ast-dump-filter={NAMESPACE}", str(path),
    ]
    try:
        result = subprocess.run(command, capture_output=True, text=True, check=True)
    except FileNotFoundError:
        raise SystemExit(f"codegen: {clang} not found; install clang or set CLANG")
    except subprocess.CalledProcessError as error:
        raise SystemExit(f"codegen: {clang} failed on {path}:\n{error.stderr}")
    # The filter prints one JSON document per matching top-level declaration.
    decoder, text, index, nodes = json.JSONDecoder(), result.stdout, 0, []
    while True:
        while index < len(text) and text[index].isspace():
            index += 1
        if index >= len(text):
            return nodes
        node, index = decoder.raw_decode(text, index)
        nodes.append(node)


def _parse_class(node: dict, header: str, warnings: Warnings) -> Class | None:
    name = node["name"]
    access = "private" if node.get("tagUsed") == "class" else "public"
    is_singleton = False
    methods = []
    doc = Doc()
    for child in node["inner"]:
        kind = child.get("kind")
        if kind == "FullComment":
            doc = _parse_doc(child)
        elif kind == "AccessSpecDecl":
            access = child["access"]
        elif kind != "CXXMethodDecl" or access != "public" or child.get("isImplicit"):
            continue
        elif child.get("explicitlyDeleted") or child["name"].startswith("operator"):
            continue
        elif child.get("storageClass") == "static":
            if child["name"] == "GetInstance" and _return_type(child) == f"{name} &":
                is_singleton = True
            else:
                warnings.skip(f"{name}::{child['name']}", "static methods are not supported")
        else:
            method = _parse_method(name, child, warnings)
            if method:
                methods.append(method)
    if not is_singleton:
        warnings.skip(name, "only singleton classes (static GetInstance()) are supported")
        return None
    return Class(name, NAMESPACE, header, doc, methods)


def _parse_method(class_name: str, node: dict, warnings: Warnings) -> Method | None:
    where = f"{class_name}::{node['name']}"
    returns = _TYPES.get(_return_type(node))
    if returns is None:
        warnings.skip(where, f"unsupported return type '{_return_type(node)}'")
        return None
    params, doc = [], Doc()
    for child in node.get("inner", []):
        if child.get("kind") == "ParmVarDecl":
            written = child["type"]["qualType"]
            type_ = _CONST_REF.get(written) or (_TYPES.get(written) if written != "std::string" else None)
            if type_ is None or type_ == VOID:
                hint = " (pass strings as const std::string&)" if written == "std::string" else ""
                warnings.skip(where, f"unsupported parameter type '{written}'{hint}")
                return None
            params.append(Param(child.get("name") or f"arg{len(params)}", type_))
        elif child.get("kind") == "FullComment":
            doc = _parse_doc(child)
    is_const = node["type"]["qualType"].endswith(" const")
    return Method(node["name"], params, returns, is_const, doc)


def _return_type(node: dict) -> str:
    return node["type"]["qualType"].split("(", 1)[0].strip()


def _parse_doc(node: dict) -> Doc:
    doc = Doc()
    for child in node.get("inner", []):
        kind = child.get("kind")
        if kind == "ParagraphComment":
            text = _paragraph(child)
            if text:
                doc.paragraphs.append(text)
        elif kind == "BlockCommandComment":
            text = _paragraph(child["inner"][0]) if child.get("inner") else ""
            command = child.get("name")
            if command == "brief":
                doc.brief = text
            elif command in ("return", "returns"):
                doc.returns = text
            elif command == "note":
                doc.notes.append(text)
            elif text:
                doc.paragraphs.append(text)
        elif kind == "ParamCommandComment":
            text = _paragraph(child["inner"][0]) if child.get("inner") else ""
            doc.params[child.get("param", "")] = text
        elif kind == "VerbatimBlockComment":
            # @code holds C++ and is not carried over; neither is its "Typical usage:" lead-in.
            if doc.paragraphs and doc.paragraphs[-1].endswith(":"):
                doc.paragraphs.pop()
    if not doc.brief and doc.paragraphs:
        doc.brief = doc.paragraphs.pop(0)
    return doc


def _paragraph(node: dict) -> str:
    lines = [c.get("text", "") for c in node.get("inner", []) if c.get("kind") == "TextComment"]
    # Doxygen text keeps the space after "*"; drop that one space, keep deeper indentation.
    lines = [line[1:] if line.startswith(" ") else line for line in lines]
    text = "\n".join(line.rstrip() for line in lines).strip("\n")
    return text if text.strip() else ""


def join_lines(text: str) -> str:
    """Folds a paragraph's wrapped lines into one; list items keep their own line."""
    out: list[str] = []
    for line in text.split("\n"):
        if out and not re.match(r"\s*[-*] ", line):
            out[-1] = out[-1] + " " + line.strip()
        else:
            out.append(line.rstrip())
    return "\n".join(out)
