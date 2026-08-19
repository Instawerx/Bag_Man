"""
CC-6.1 PRICE-DRIFT LINT -- run inside the UE editor's Python.

WHAT IT GUARDS (economy-store SS8.2): the catalog entry is the SOLE price authority, and the PlayFab
seed manifest must be DERIVED from it or absent. The failure mode is specific and silent: a seed price
and a catalog price that disagree mean the player is shown one price and charged another. Whichever is
lower is an unintended discount; whichever is higher is a support ticket. Worse, the discrepancy
appears ONLY for items whose price was later CHANGED -- so it survives every test written against the
original value. Derivation at authoring time does not protect you; the next price edit is the risk.

WHY A LINT AND NOT A CONVENTION: "we derived it" is a claim about a past action. This is a check that
can fail today, on the current bytes, every time it is run.

Usage (editor python):
    exec(open(r"C:/Dev/Bag_Man/Tools/AFL_Lint/price_drift.py").read())
"""
import unreal, json, io, os

MANIFEST = r"C:\Dev\Bag_Man_Backend\config\economy-catalog.json"
CATALOG = "/AFLBagMan/Cosmetics/DA_AFL_CosmeticCatalog"


def run(manifest_path=MANIFEST):
    if not os.path.exists(manifest_path):
        print("PRICE_DRIFT: VOID -- manifest not found at %s (this is not a pass)" % manifest_path)
        return False

    man = json.load(io.open(manifest_path, encoding="utf-8"))
    m_by = {it["itemId"]: it.get("virtualCurrencyPrices", {}) for it in man["items"]}

    cat = unreal.load_asset(CATALOG)
    c_by = {}
    for e in cat.get_editor_property("Entries"):
        cid = str(e.get_editor_property("CosmeticId"))
        c_by[cid] = (int(e.get_editor_property("PriceVolts")),
                     int(e.get_editor_property("PriceWatts")))

    # POSITIVE CONTROL: the comparison must be exercising real overlap. Zero shared ids means the
    # lint is comparing nothing and "no drift" would be meaningless.
    shared = sorted(set(m_by) & set(c_by))
    if not shared:
        print("PRICE_DRIFT: VOID -- no manifest id matches a catalog id; nothing was compared")
        return False

    drift, orphan, fixtures = [], [], []
    for iid, prices in sorted(m_by.items()):
        if iid not in c_by:
            # AFL.Test.* are DELIBERATE fixtures, declared as such in the manifest's own _comment
            # (A1.2 purchase-proof and spend-spoof proof). Reported, but they do not fail the lint --
            # otherwise the check is permanently red and a real orphan hides in the noise.
            (fixtures if iid.startswith("AFL.Test.") else orphan).append(iid)
            continue
        vo, wa = c_by[iid]
        want = {}
        if vo > 0:
            want["VO"] = vo
        if wa > 0:
            want["WA"] = wa
        if want != prices:
            drift.append({"id": iid, "catalog": want, "manifest": prices})

    print("PRICE_DRIFT: compared=%d  drift=%d  orphan=%d  test_fixtures=%d"
          % (len(shared), len(drift), len(orphan), len(fixtures)))
    for d in drift:
        print("  DRIFT   %-28s catalog=%s manifest=%s" % (d["id"], d["catalog"], d["manifest"]))
    for o in orphan:
        print("  ORPHAN  %-28s in manifest, no catalog row -> no price authority" % o)
    for f in fixtures:
        print("  fixture %-28s declared test item, not a failure" % f)

    # Catalog rows priced but absent from the manifest are NOT drift: they are simply not seeded to
    # PlayFab yet. Reported separately because they are ship-time dead, not mispriced.
    unseeded = [c for c, (vo, wa) in sorted(c_by.items())
                if (vo > 0 or wa > 0) and c not in m_by]
    print("  note: %d priced catalog rows are not in the manifest (ship-time dead, not drift)"
          % len(unseeded))

    ok = not drift and not orphan
    print("PRICE_DRIFT: %s" % ("PASS" if ok else "FAIL"))
    return ok


run()
