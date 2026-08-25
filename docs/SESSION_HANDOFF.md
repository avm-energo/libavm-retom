# Session handoff — RETOM-51 zero-voltage investigation (2026-08-25)

Written so a fresh Claude Code session (or a human) on a different machine can pick this up with
full context. The user moved the physical device + laptop from a Windows dev stand to a Linux PC
mid-investigation; this file bridges the two, since chat history/memory doesn't sync across
machines automatically.

## TL;DR — what's broken right now

On the **Linux PC**, running the current code (both repos pulled to latest `master`):
- `apply(Ua=1.0V, Ia=1.0A)` held for 20s → **current is correct (1.0A the whole time)**, but
  **voltage reads 0V the entire time**.
- The run completes with **no exceptions/timeouts** — every protocol frame (including the new
  calibration block, see below) gets ACKed normally.
- This is the *same symptom class* solved twice already on the Windows stand this session (see
  "Fix history" below) — but the exact same code that worked reliably there (many confirmed live
  tests, including fresh mains power-cycles) does not work on Linux.

**Immediate next step (in progress when this was written):** `RETOM_DEBUG=1` env var now dumps
every TX/RX frame to stderr — added specifically because there's no live device access from the
machine continuing this investigation. Get a capture from the Linux side:

```sh
RETOM_DEBUG=1 ./build/tools/live_smoke_test /dev/ttyUSB0 10 1.0 2> retom_debug.log
```

(adjust binary path/port/args as needed for however it's being invoked there), then compare the
byte sequences against what's documented as working in `docs/RE_LOG.md` in the `linretom` repo
(if that repo is also cloned on this machine — see "Repo layout" below) — specifically the sections
about the arm-preamble and calibration-block fixes.

## Repo layout — TWO separate git repos, both touched this session

- **`libavm-retom`** (this repo) — the C++/Qt driver. Where all the code fixes for this
  investigation live.
- **`linretom`** — a *separate* repo containing the original Python driver, plus **all the
  reverse-engineering documentation** (`docs/RE_LOG.md`, `PLAN.md`, `docs/PROTOCOL.md`,
  `capture/*.pcap` USB captures). If you don't have `linretom` cloned on this machine, ask the user
  for it — it has the full byte-level protocol history this fix is built on. This C++ repo does not
  duplicate that documentation (by design — see the "move to separate repo" note in `linretom`'s
  `PLAN.md`), except for this handoff file.

## Fix history this session (all live-verified on the Windows stand, same physical device)

Chronological, each one found by live differential USB/serial capture, not guessing:

1. **`kOutArmPreamble`** (commit `02602c3`) — `apply()` sent protocol-correct `0x815`/`0x818` but
   voltage stayed 0V while current worked. Root cause: a short per-`Out()` "arm" sequence (14
   frames, notably a `0x0814`×3 channel-select step) was missing entirely. Found via a live
   differential capture of a second vendor program (not RTDI) doing the same operation.

2. **`kEnableCalibration`** (commit `ac49301`) — even with fix 1, voltage was 0V unless a
   third-party program had already "warmed up" the device in the same mains power session. Root
   cause: a real hardware channel self-calibration block (34 frames: `0x0824`×8 + `0x0806 "MINI
   0x20"`×8, twice) needs to run once per power-on, and our own `enable()` never sent it. Found via
   a live capture across a **genuine mains power-cycle** of the device (not just USB
   unplug/replug) — this is the key technique if the Linux symptom turns out to be the same class
   of bug again.

3. **Watchdog `keepAlive()`** (commit `ac49301`, same as #2) — even with fixes 1+2, the signal reset
   itself to 0 after ~10–13s of protocol silence. Added a method that polls `readInputContacts()`
   periodically to keep the device from timing out.

4. **Automatic keep-alive + real threading fix** (commit `51c8a7b`) — per user request, made
   `apply()` start the keep-alive automatically (no more manual polling from the caller). This
   required a real architecture fix along the way: `QSerialPort` cannot be driven from a thread
   other than the one that constructed it, and that thread needs an actual running Qt event loop —
   a first attempt using a bare `std::thread` hung live with `QObject: Cannot create children for a
   parent that is in a different thread` / `Timers can only be used with threads started with
   QThread`. Fixed with a proper `QThread` + `moveToThread()` + `QMetaObject::invokeMethod(...,
   Qt::BlockingQueuedConnection)`. Side effect: any consumer now needs a real `QCoreApplication` in
   the process (the offline test switched from `QTEST_APPLESS_MAIN` to `QTEST_MAIN`).

5. **`isOutputActive()`** (commit `6ef8a6e`) — client-side tracking of whether output is currently
   commanded on. Confirmed (via the full `RTLink.dll` method catalog and `IAnalogOutputs` COM
   interface in `linretom`'s docs) that the protocol has **no real hardware readback** for this —
   not even RTDI has it.

6. **`RETOM_DEBUG=1` logging** (commit `c46d713`) — added for exactly this situation: diagnosing a
   remote machine with no live access from here.

All of 1–5 were live-verified working repeatedly on the Windows stand, including fresh power-cycles
with zero third-party software priming. Fix 2 in particular was specifically designed and tested to
solve "works after external program touches it first, not on a truly cold device" — which is a
plausible hypothesis to re-check for the Linux symptom too, though the current run already went
through `enable()` (which includes the calibration block) without error.

## Python driver status

`linretom`'s Python driver (`src/linretom/driver.py`) has fixes 1–3 and 5 ported (commits `9e7f240`,
`88db590`, `ba44aef` in `linretom`). Fix 4 (automatic keepalive/threading) was **deliberately not
ported** — Python keeps a manual `keep_alive()` the caller must poll, per explicit user request.

## Hypotheses worth checking for the Linux-specific symptom (not yet investigated)

- FTDI Linux driver's `latency_timer` (`/sys/bus/usb-serial/devices/ttyUSB0/latency_timer`, often
  defaults to 16ms) could affect timing-sensitive sequences differently than the Windows FTDI VCP
  driver — worth checking/comparing, though the clean (no-timeout) run argues against a *fatal*
  timing issue.
- Whether this is truly the *same* physical RETOM-51 unit's calibration state that's somehow
  different now vs. when last tested on Windows (confirmed same unit, per the user).
- Get the `RETOM_DEBUG=1` capture and diff the calibration-block and arm-preamble ACK payloads
  against a known-good Windows capture — if the *content* of the ACKs differs (not just "did it ACK
  at all"), that's the fastest way to find what's actually different about this run.

## Physical setup notes

- Windows dev stand: RETOM-51 was on `COM10` (a second unrelated FTDI adapter is `COM11` — ignore
  it). `tools/live_smoke_test.cpp` and `examples/connect_wait_disconnect.cpp` both default to
  `/dev/ttyUSB0` in committed code — the Windows port gets swapped in locally and reverted before
  each commit (do the same discipline on Linux: don't commit a hardcoded local port).
- The device is mains-powered independently of USB — a USB unplug/replug is NOT the same as a
  mains power-cycle, and fix #2 above specifically depends on mains power state.
