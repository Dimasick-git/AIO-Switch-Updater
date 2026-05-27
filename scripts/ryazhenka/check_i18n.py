#!/usr/bin/env python3
"""Verify Russian and English i18n files keep the same keyset.

Walks resources/i18n/en-US/ and resources/i18n/ru/ recursively, parses every
JSON file, and compares the set of leaf keys (dotted paths). Reports missing
or extra keys. Exit code is non-zero if anything is missing in ru/.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
I18N_DIR = ROOT / "resources" / "i18n"
BASE_LOCALE = "en-US"
TARGET_LOCALE = "ru"


def flatten(prefix: str, node) -> dict[str, str]:
    out: dict[str, str] = {}
    if isinstance(node, dict):
        for k, v in node.items():
            key = f"{prefix}.{k}" if prefix else k
            out.update(flatten(key, v))
    else:
        out[prefix] = str(node)
    return out


def load_locale(locale: str) -> dict[str, str]:
    locale_dir = I18N_DIR / locale
    if not locale_dir.is_dir():
        sys.exit(f"missing locale directory: {locale_dir}")
    keys: dict[str, str] = {}
    for json_path in sorted(locale_dir.rglob("*.json")):
        rel = json_path.relative_to(locale_dir).with_suffix("").as_posix()
        try:
            data = json.loads(json_path.read_text(encoding="utf-8"))
        except json.JSONDecodeError as exc:
            sys.exit(f"{json_path}: {exc}")
        for k, v in flatten(rel, data).items():
            keys[k] = v
    return keys


def main() -> int:
    base = load_locale(BASE_LOCALE)
    target = load_locale(TARGET_LOCALE)

    missing = sorted(set(base) - set(target))
    extra = sorted(set(target) - set(base))

    if missing:
        print(f"[{TARGET_LOCALE}] missing {len(missing)} keys:")
        for k in missing[:50]:
            print(f"  - {k}")
        if len(missing) > 50:
            print(f"  ... and {len(missing) - 50} more")

    if extra:
        print(f"[{TARGET_LOCALE}] has {len(extra)} extra keys not in {BASE_LOCALE}:")
        for k in extra[:20]:
            print(f"  + {k}")

    if missing:
        return 1
    print(f"i18n keysets match between {BASE_LOCALE} and {TARGET_LOCALE} "
          f"({len(base)} keys)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
