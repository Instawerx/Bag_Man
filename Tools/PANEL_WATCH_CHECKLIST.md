Huge capability discovery — the bridge has a full UMG Widget Blueprint authoring API: add_widget(type, opts), configure_widget(name, props), add_custom_event, the graph ops, compile, save. So I can build the store widget tree + bindings via MCP, not just operator-GUI. The WBP was created parented to LyraActivatableWidget ✅.
This changes the plan — I can author the widget. But a full virtualized UCommonListView store with tile entries + async thumbnails + event-graph buy-wiring is a LOT of widget authoring, and per the operator's "first AFL menu as the template" + the prove-one-layer discipline, I should build it incrementally and watchable, not one giant blind push (same lesson as everywhere).


PAWN SHOP} Original Buy No Transfers 95% buyback rate / Other items 20-75% of Market Value based on rarity-popularity / So we must track all metrics original price-owner-etc - transaction history - current market evaluation - issue pawn value  (ENTER THROUGH LOBBY-Signage Vintage)

Mini Game - Boxing Tyson Punchout Styled - Our Robots beat up famous styled bots like Tesla etc


# 1. Close the Unreal editor (quiesces the tree; AIK goes down — that's expected).
# 2. Run this (fresh dated folder — ADD, never /MIR over an existing backup):
$src = "C:\Dev\Bag_Man"
$dst = "C:\Backups\Bag_Man_BACKUP_2026-06-07b"   # 'b' = the 2nd backup today; fresh folder
robocopy $src $dst /E /COPY:DAT /R:2 /W:5 /XD "Intermediate" "DerivedDataCache" "Saved\Autosaves"
# .git INCLUDED (captures all 3 commits); regenerable junk excluded to stay lean.
# 3. Verify the backup captured the commits (the critical proof):
Test-Path "$dst\.git\HEAD"                              # -> True
git --git-dir="$dst\.git" log --oneline -3             # -> 132ab380, 5cbbd804, b88a06ca
Test-Path = True and the 3 commits in the backup's log = the