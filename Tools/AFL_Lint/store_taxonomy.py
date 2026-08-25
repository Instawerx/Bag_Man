#!/usr/bin/env python3
"""
AFL_Lint_StoreTaxonomy — the store may render ONLY the ruled economy.

WHY THIS EXISTS. The same defect was found three separate times and reported each time as new: the
SKINS tab rendering finishes, bodies and edges; the CAMOS tab rendering weapon-skins; the BEAMS tab
rendering beams. None of those is a product. They are cosmetics a player OWNS -- free is not the same
as listed, and the loadout is the surface for owned things. Each fix removed one tab and left the
mechanism that allowed it, so the next one was rediscovered from scratch.

This is the mechanism. It reads the two independent statements of the ruled set out of
AFLW_FrontEndMarket.cpp and requires them to agree with the ruling recorded here:

  GAFLStoreNamespaces   the allowlist the filter enforces at runtime
  GStoreTabs            the prefixes each physical tab declares

A tab that declares a namespace outside the allowlist fails. A namespace in the allowlist that no
tab can reach fails -- otherwise a ruled product silently has no way to be bought. And an allowlist
that drifts from RULED below fails, so adding a product to the store is a deliberate edit here and
not a side effect of tuning a tab.

    python Tools/AFL_Lint/store_taxonomy.py           # exits non-zero on any disagreement
"""
from __future__ import annotations
import re
import sys
from pathlib import Path

# THE RULED ECONOMY, 2026-08-25. Anything not on this list is not a product.
# Colours, edges, beams, weapon-skins, identities, abilities, stickers and any retired row are
# deliberately absent: they are owned and equipped, never bought.
RULED = {
    "AFL.CreatorSlot.":   "robot packs x1/x3/x8, slot SKUs, the max upgrade",
    "AFL.Weapon.":        "weapons",
    "AFL.WeaponCredit.":  "weapon credits x3",
    "AFL.StickerCredit.": "sticker credits x5 / x10",
    "AFL.FacemaskCredit": "facemask credits x5",
    "AFL.Facemask.":      "facemasks",
    "AFL.Accessory.":     "jewellery",
    "AFL.Emblem.":        "emblems",
    # AFL.Bundle. was REMOVED 2026-08-25. There are no bundle products: the identity bundles retired
    # with the roster cut, AFL.Bundle.FANATICS was deleted, and the only multi-item products left --
    # the .XT hand cannon pairs -- carry AFL.Weapon. ids and render under WEAPONS. The BUNDLES tab
    # was empty in every possible state, which is the empty-tab defect for the fourth time.
    #
    # "AFL.League." lands here when the League SKU is built. Until then its absence is correct, and
    # this comment is the record that it is pending rather than forgotten.
}

MARKET = Path(__file__).resolve().parents[2] / (
    "Plugins/GameFeatures/AFLCombat/Source/AFLCombat/Private/UI/AFLW_FrontEndMarket.cpp"
)


def parse() -> tuple[list[str], list[tuple[str, list[str]]]]:
    """Pull the allowlist and the per-tab prefixes out of the source."""
    src = MARKET.read_text(encoding="utf-8", errors="replace")

    m = re.search(r"GAFLStoreNamespaces\[\]\s*=\s*\{(.*?)\};", src, re.S)
    if not m:
        sys.exit("LINT ERROR: GAFLStoreNamespaces not found — did the store move?")
    allow = re.findall(r'TEXT\("([^"]+)"\)', m.group(1))

    m2 = re.search(r"GStoreTabs\[\d+\]\s*=\s*\{(.*?)\n\t\};", src, re.S)
    if not m2:
        sys.exit("LINT ERROR: GStoreTabs not found — did the store move?")
    tabs: list[tuple[str, list[str]]] = []
    for line in m2.group(1).splitlines():
        if "TEXT(" not in line:
            continue
        names = re.findall(r'TEXT\("([^"]+)"\)', line)
        if len(names) < 2:
            continue
        # first is the widget name, second the label widget, the rest are prefixes
        tabs.append((names[0], [n for n in names[2:] if n.startswith("AFL.")]))
    return allow, tabs


def main() -> int:
    allow, tabs = parse()
    fails: list[str] = []

    # A control before any verdict: if the parse found nothing, every check below would pass
    # vacuously — which is the failure mode this whole file exists to prevent.
    if not allow or len(tabs) != 5:
        sys.exit(f"LINT ERROR: parsed {len(allow)} namespaces and {len(tabs)} tabs (expected 5) — the instrument is broken, not the code")

    for ns in allow:
        if ns not in RULED:
            fails.append(f"  allowlist has {ns!r}, which the ruling does not list as a product")
    for ns in RULED:
        if ns not in allow:
            fails.append(f"  ruled product {ns!r} ({RULED[ns]}) is missing from the allowlist")

    reachable: set[str] = set()
    for tab, prefixes in tabs:
        for p in prefixes:
            reachable.add(p)
            if p not in allow:
                fails.append(f"  tab {tab} renders {p!r}, which is NOT a product — this is the SKINS/CAMOS/BEAMS defect")

    for ns in allow:
        if ns not in reachable:
            fails.append(f"  {ns!r} is a ruled product but no tab can reach it — it cannot be bought")

    print(f"store taxonomy: {len(allow)} allowed namespace(s), {len(tabs)} tab(s)")
    for tab, prefixes in tabs:
        print(f"  {tab:<14} {', '.join(prefixes) if prefixes else '(none)'}")

    if fails:
        print("\nSTORE TAXONOMY LINT FAILED:")
        for f in fails:
            print(f)
        return 1
    print("\nOK — the store renders the ruled economy and nothing else.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
