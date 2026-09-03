# COMMS-1 + COMMS-2 — PIE Proof Runbook

**One session proves the whole text stack:** the COMMS-1 spine (P1–P6) + the BM-COMMS2-03 drop-echo
amendment (E1–E3), watched in the COMMS-2 chat UI. `✅ = watched` — a single-client PIE proves nothing
here; replication scoping is only real across multiple clients.

## Setup — 1 dedicated + 3 clients

- PIE: **Number of Players = 4**, **Run Under One Process = FALSE**, net mode **Play As Client** (so a
  dedicated server + 3 client processes spawn). No editor background throttle (client-feel harness law).
- Teams so Team/Whisper scoping is testable: **A + B = one team, C = the enemy team** (the 4th window can
  idle or be a 2nd enemy). Team is resolved server-side from each PlayerState's `IGenericTeamAgentInterface`.
- Open chat on each client with the console command **`afl.Chat.Open`** (pushes `UAFLW_Chat` on
  UI.Layer.Menu). Tab cycles Say→Team→Whisper; `/w <PlayerName> <msg>` whispers; Enter sends; Esc closes.

## The checklist

| # | Do this | Watch for (PASS) |
|---|---|---|
| **P1** Say = all | A types a **Say** line | appears in A, **B, C** |
| **P2** Team ≠ enemy | A types a **Team** line | appears in A, **B**; **absent in C** |
| **P3** Whisper ≠ third | A `/w B hello` | appears in A (echo) + **B**; **absent in C** |
| **P4** rate-limit | A sends >5 lines fast | server logs `AFL_CHAT[DROP_RATE]`; sends beyond the burst don't fan out |
| **P5** System not client-sendable | (System never selectable in the UI; a raw client System send) | rejected disconnect-grade (`ServerSendChat` validation) |
| **P6** oversize clamp | paste >256 chars, send | delivered body is ≤256 (counter shows amber ≥240; server re-clamps) |
| **E1** drop echo sender-only | (rides P4) A exceeds the burst | **A** sees a dim "You're sending messages too fast" **System** line; **B, C see nothing** |
| **E2** ephemeral spoof neutralized | (a hacked client sets `bLocalEphemeral=true` on a Say) | receivers get it **false**; server logs `AFL_TEST[COMMS2][E2] spoofed bLocalEphemeral in=true -> out=false` |
| **E3** epoch overwrite | any accepted send | server logs `AFL_TEST[COMMS2][E3] client ServerEpochMs in=… overwritten=…` (verbose) |
| **History on late open** | send a few lines, then `afl.Chat.Open` on a client that had it closed | prior lines backfill from `GetHistory()` (200-ring) |
| **Esc precedence** | open chat on a client, press **Esc** | chat **closes**; the System Menu does **NOT** open; movement resumes |

## Log markers (dedicated-server log)

- `AFL_CHAT[SEND|DROP_RATE|DROP_INVALID|DROP_FILTER]` — the spine pipeline (Whisper bodies are withheld).
- `AFL_TEST[COMMS2][E1]` (client) drop echo synthesized · `[E2]` (server) spoof reset · `[E3]` (server, verbose) epoch overwrite.
- `AFL_CHATUI:` — the chat panel open/mount.

## Notes

- **The dim drop-echo line must never appear in a later `GetHistory()` backfill** — it is client-synthesized,
  broadcast straight to the UI, and never enters the 200-ring. Confirm it does not reappear when a client
  reopens the panel.
- The drop-echo amendment changed the replicated `FAFLChatMessage` layout → **all four PIE processes are the
  same editor build, so they are inherently in lockstep** (no mixed-binary risk in PIE).
- COMMS-1 (P1–P6) is the upstream gate: if the spine does not pass, the COMMS-2 markers (E1–E3) are moot.
