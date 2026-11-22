#!/usr/bin/env python3
"""Audit local Markdown file links in a checked-out repository.

Offline verification only: external HTTPS links, heading anchors, native
builds and project runtime behavior are not checked by this script.

Usage:
    python3 scripts/audit_teaching_links.py .
    python3 scripts/audit_teaching_links.py ../KairoAssets
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path
import re
import sys
from urllib.parse import unquote, urlsplit


# Matches ordinary inline Markdown links and images, with optional title.
# For nested-parenthesis URLs or complex HTML use a full Markdown parser.
_LINK = re.compile(r'!?\[[^\]\n]+\]\(\s*(<[^>]+>|[^\s)]+)(?:\s+"[^"]*")?\s*\)')
_REFERENCE = re.compile(r'^\s*\[[^\]]+\]:\s*(<[^>]+>|\S+)')
_SKIP_DIRS = frozenset({
    ".git", "build", "dist", "node_modules", "vendor", "third_party",
    ".venv", "target", "__pycache__", "out",
})


@dataclass(frozen=True)
class BrokenLink:
    """A missing file/dir and the document that referenced it."""

    source: Path
    line: int
    target: str

    def __str__(self) -> str:
        return f"{self.source}:{self.line}: missing local target: {self.target}"


@dataclass(frozen=True)
class AuditResult:
    """The three counts are specific to the examined local checkout."""

    markdown_files: int
    local_links: int
    skipped_links: int
    broken: tuple[BrokenLink, ...]


def _markdown_files(root: Path):
    """Walk first-party Markdown while skipping vendored/generated trees."""
    for path in sorted(root.rglob("*.md")):
        if any(part in _SKIP_DIRS for part in path.relative_to(root).parts):
            continue
        if path.is_file():
            yield path


def _targets(text: str):
    """Yield (line number, target) outside fenced examples."""
    fence: str | None = None
    for line_number, line in enumerate(text.splitlines(), start=1):
        stripped = line.lstrip()
        if stripped.startswith((chr(96) * 3, "~~~")):
            token = stripped[:3]
            if fence is None:
                fence = token
            elif token == fence:
                fence = None
            continue
        if fence is not None:
            continue

        for match in _LINK.finditer(line):
            yield line_number, match.group(1).strip("<>")
        match = _REFERENCE.match(line)
        if match is not None:
            yield line_number, match.group(1).strip("<>")


def _resolve(source: Path, target: str) -> Path | None:
    """Resolve a local Markdown target relative to its source document."""
    if not target or target.startswith("#") or target.startswith("//"):
        return None
    parsed = urlsplit(target)
    if parsed.scheme or parsed.netloc or not parsed.path:
        return None  # Remote links need network verification.
    relative = unquote(parsed.path)
    if relative.startswith("/"):
        # Reject machine-specific paths even if they happen to exist.
        return source.parent / "__INVALID_ABSOLUTE_LINK__"
    return source.parent / relative


def scan(root: Path) -> AuditResult:
    """Return missing local targets without following remote links."""
    root = root.resolve()
    if not root.is_dir():
        raise NotADirectoryError(root)

    broken: list[BrokenLink] = []
    local_links = 0
    skipped_links = 0
    count = 0

    for source in _markdown_files(root):
        count += 1
        try:
            content = source.read_text(encoding="utf-8")
        except (OSError, UnicodeError) as exc:
            broken.append(BrokenLink(source.relative_to(root), 0, f"unreadable: {exc}"))
            continue

        for line, target in _targets(content):
            dest = _resolve(source, target)
            if dest is None:
                skipped_links += 1
                continue
            local_links += 1
            if not dest.exists():
                broken.append(BrokenLink(source.relative_to(root), line, target))

    return AuditResult(count, local_links, skipped_links, tuple(broken))


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "repository", nargs="?", type=Path, default=Path("."),
        help="repository checkout root (default: current directory)"
    )
    args = parser.parse_args(argv)
    try:
        result = scan(args.repository)
    except (OSError, ValueError) as exc:
        parser.error(str(exc))

    for broken in result.broken:
        print(broken, file=sys.stderr)
    print(
        f"Markdown files: {result.markdown_files}; "
        f"local links checked: {result.local_links}; "
        f"remote/fragment skipped: {result.skipped_links}; "
        f"missing local targets: {len(result.broken)}"
    )
    return 1 if result.broken else 0


if __name__ == "__main__":
    raise SystemExit(main())
