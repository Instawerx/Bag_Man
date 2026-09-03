# COMMS Voice (#2) + Social (#3) — Build-Ready Plan

Everything below is **gated on the Epic portal and/or EOS SDK + a watched PIE proof**, which is why it is a
plan and not committed code: authoring EOS-Voice/Friends SDK wiring blind produces untested code. Build these
with the portal live so each step is testable as written. The portal-independent pieces already landed:
**`PostServerRtcToken`** (server RTC-token mint, `2ee59e81`) and the **COMMS-4B/5B backend** (PROVEN).

## Prerequisite gate — Epic Dev Portal (operator-authorized)

- Product `407bac6a…`: enable **Voice** + **Lobby**.
- Client policy `xyza7891…`: grant **Voice**, **Lobby**, **P2P**.
- Real 64-hex client key into the gitignored `Config/Custom/EOS/DefaultEngine.ini` overlay (operator places
  the secret; Claude never enters it).
- Env for the dedicated server: `AFL_RTC_TOKEN_URL` = the deployed `POST …/rtc/token` (same HMAC key as earn).

---

## #2 — Voice (COMMS-3 party + COMMS-4 server rooms)

### Enable
1. `Bag_Man.uproject`: enable plugins **VoiceChat** + **EOSVoiceChat**.
2. `Config/DefaultEngine.ini`: `[Voice] bEnabled=true`; the EOSVoiceChat factory rows.
3. `AFLOnline.Build.cs`: add `"VoiceChat"` (private).

### `UAFLVoiceSubsystem` (AFLOnline, GameInstanceSubsystem — module-placement law)
- Lifecycle: `IVoiceChat* VC = IVoiceChat::Get();` → `Initialize()` → `Connect()` → `Login(PlatformId, PlayerName, Credentials)` (EOS ProductUserId + token). Client-only; guard `IsRunningDedicatedServer()`.
- Join: `VC->GetUser()->JoinChannel(RoomName, ChannelCredentials, EVoiceChatChannelType::NonPositional (Team) / Positional (Proximity))`. The token = the `/rtc/token` mint (server rooms) or the EOS lobby RTC (party).
- Controls: `SetAudioInputVolume` / `SetAudioInputDeviceMuted` (PTT/mute), `SetPlayerVolume` (per-player), `SetPlayerMuted`.
- Speaking indicators: bind `OnVoiceChatPlayerTalkingUpdated` → drive the faceplate `#1E5AFF` pulse (COMMS-7).
- Party voice: EOS Lobby with RTC enabled, client-created, persists across hub↔match `ExperienceTravel` (do NOT rejoin on travel — the lobby carries it).

### Server-room flow (COMMS-4, token-mint DONE)
- On match start, the **dedicated server** calls `UAFLOnlineSubsystem::PostServerRtcToken({roomName, participantIds:[puid…]})` → `{participants:[{puid,token}]}` → distributes each token to its owning client via an **owner-only Client RPC** (secrets stay off-instance) → client `JoinChannel(roomName, token)`.
- **R1 per-experience proximity flag** (a bool on the AFL experience/gameplay data): Staked **ON**, League **OFF** (all 20 cells), Hub **ON**, Shooting Range **OFF**; future social **ON**, ranked **OFF**. The subsystem reads it before a proximity join.
- Spatialization phased: v1 = distance-attenuated `SetPlayerVolume` (launch); v2 = true-3D RTC frame injection + occlusion (COMMS-7, never blocks v1).

### PIE proof (own operator pass, audio)
2 clients, 2 device IDs: audio both directions · channel survives hub↔match travel · per-player volume · **mute watched** · League cell = silent (R1). `TransmitToSpecificChannels` (PLURAL).

---

## #3 — Social: friends / presence / block-mute / moderation

### Friends / presence / invites (COMMS-5, retires AFL-1105)
- EOS Friends + Presence via `IOnlineFriends`/`IOnlinePresence` (OnlineSubsystemEOS) OR the EOS SDK `EOS_Friends`/`EOS_Presence` directly. Gated on the **Friends** scope in the client policy (already enabled for sign-in). Panel consumes it; DM "start" gains a friends source (today it uses in-session players + inbox partners).

### Block / mute persistence (COMMS-5 wires the existing seam)
- The seam exists: `UAFLChatComponent::IsBlockedBy` (text) returns false; DM `isBlocked` (backend) returns false. To make it real, add a **block-list store** (a small backend table keyed on the blocker's PlayFabId, or PlayFab ReadOnlyData) + a `POST /block` / `GET /blocks`; the text `IsBlockedBy` and the DM `isBlocked` both consult it server-side. Voice mute rides `SetPlayerMuted` (client) + the same block list (server-enforced). **Mute must land with voice (COMMS-3)** — no player ever faces an unmutable stranger.

### Moderation (COMMS-6, may trail beta)
- Reports: EOS **Player Reports** submission from the scoreboard + the social panel (`EOS_Reports_SendPlayerBehaviorReport`).
- Sanctions: EOS **Sanctions** checked at login (`EOS_Sanctions_QueryActivePlayerSanctions`); a comms-ban hard-mutes **every** send path server-side (text `ServerSendChat`, DM `sendMessage`, voice channel join).
- Chat logs already ride the dedicated-server → CloudWatch pipeline (`AFL_CHAT[…]`).

---

## What is committed vs planned

| Piece | State |
|---|---|
| `/rtc/token` server mint (`PostServerRtcToken`) | **DONE** `2ee59e81` |
| COMMS-4B RTC + COMMS-5B DM backend | **PROVEN** (deployed) |
| DM client transport + live DM UI | **DONE** (`41d8268d`, `08e40890`), live-unproven |
| Voice `IVoiceChat` client wiring + party lobby + distribution + R1 | **planned** — needs portal + audio PIE |
| Friends / presence / block-persistence / reports / sanctions | **planned** — needs EOS scopes + a block store |
