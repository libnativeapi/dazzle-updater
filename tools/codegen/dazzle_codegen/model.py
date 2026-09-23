"""The API model the parser produces and every emitter consumes."""

from __future__ import annotations

import re
from dataclasses import dataclass, field

# The C++ types the generator can carry across the C ABI.
VOID, BOOL, I32, I64, F64, STRING = "void", "bool", "i32", "i64", "f64", "string"


@dataclass
class Doc:
    """A Doxygen comment, split into the parts every language renders."""

    brief: str = ""
    paragraphs: list[str] = field(default_factory=list)
    params: dict[str, str] = field(default_factory=dict)
    returns: str = ""
    notes: list[str] = field(default_factory=list)


@dataclass
class Param:
    name: str  # snake_case, as written in the C++ header
    type: str


@dataclass
class Method:
    name: str  # PascalCase, as written in the C++ header
    params: list[Param]
    returns: str
    is_const: bool
    doc: Doc


@dataclass
class Property:
    """A GetX()/IsX() const getter, optionally paired with SetX(value)."""

    name: str  # PascalCase without the Get/Is prefix, e.g. "FeedUrl"
    getter: Method
    setter: Method | None

    @property
    def type(self) -> str:
        return self.getter.returns

    @property
    def is_bool(self) -> bool:
        return self.getter.name.startswith("Is")


@dataclass
class Class:
    """A singleton class: one process-wide instance reached via GetInstance()."""

    name: str
    namespace: str
    header: str  # file name below core/src/, e.g. "updater.h"
    doc: Doc
    methods: list[Method]

    @property
    def module(self) -> str:
        return snake(self.name)

    def properties_and_actions(self) -> tuple[list[Property], list[Method]]:
        """Pairs getters with their setters; everything else is an action."""
        by_name = {m.name: m for m in self.methods}
        properties: list[Property] = []
        used: set[str] = set()
        for method in self.methods:
            match = re.fullmatch(r"(Get|Is)([A-Z]\w*)", method.name)
            if not match or method.params or not method.is_const or method.returns == VOID:
                continue
            if match.group(1) == "Is" and method.returns != BOOL:
                continue
            name = match.group(2)
            setter = by_name.get("Set" + name)
            if setter and not (
                len(setter.params) == 1
                and setter.params[0].type == method.returns
                and setter.returns == VOID
            ):
                setter = None
            properties.append(Property(name, method, setter))
            used.add(method.name)
            if setter:
                used.add(setter.name)
        actions = [m for m in self.methods if m.name not in used]
        return properties, actions


def snake(name: str) -> str:
    """FeedUrl -> feed_url, GetURLString -> get_url_string."""
    name = re.sub(r"([A-Z]+)([A-Z][a-z])", r"\1_\2", name)
    name = re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", name)
    return name.lower()


def camel(name: str) -> str:
    """feed_url or FeedUrl -> feedUrl."""
    parts = snake(name).split("_")
    return parts[0] + "".join(p.capitalize() for p in parts[1:])


def pascal(name: str) -> str:
    """feed_url or feedUrl -> FeedUrl."""
    return "".join(p.capitalize() for p in snake(name).split("_"))


def c_function(cls: Class, method: Method) -> str:
    """The C ABI symbol for a method: dazzle_<module>_<method>."""
    return f"dazzle_{cls.module}_{snake(method.name)}"
