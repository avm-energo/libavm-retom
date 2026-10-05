# Session handoff — RETOM-51 zero-voltage investigation (2026-08-25)

Written so a fresh Claude Code session (or a human) on a different machine can pick this up with
full context. The user moved the physical device + laptop from a Windows dev stand to a Linux PC
mid-investigation; this file bridges the two, since chat history/memory doesn't sync across
machines automatically.

## TL;DR — what's broken right now

## Update 2026-10-05 (evening): root cause located — device state, cured by mains power-cycle

Supersedes the "real lead" in the earlier 2026-10-05 update (the `0x804`/`0x803`/`PLUG` differences
are the same in working and failing Linux runs — a red herring).

- RETOM is `/dev/ttyUSB3` (udev model `USB_Link_-_Dynamics_tm_`); the Energomonitor is
  `/dev/ttyUSB0` (FT232R A602YD53), read with `libavm-emon/build/examples/get_analog_inputs <port> usb`.
- **Discriminator:** 5th payload byte of each record in the reply to `MINI 0x1c` (`0x806`; decoded
  from the old `44 03` frame). `1` in every run that produced voltage (08-26 Linux, Windows 3-phase,
  Windows cold start after bootstrap), `0` in every 0 V run. Our frames never change it: identical TX
  and ACKs, same flag outcome depends on device state at connect.
- **Fix: mains power-cycle the RETOM.** Before: flag 0 throughout, Energomonitor Ua=0.0000 V. After:
  flag 1 from `enable()` on, Energomonitor Ua=57.85 V (requested 57.74 V), Ia 4.998 (see note).
- Subsequent sessions without a power-cycle keep working (flag stays 1): clean rerun, 11 s watchdog
  expiry, and SIGKILL mid-output followed by a new session all still gave Ua=57.85 V. So the
  trigger that puts the device into the flag-0 state is **not yet identified** (prime suspects: a long
  idle/power-on time, or something another tool — AVM Tester, retomhold — did to it).
- New diagnostics: `RetomDriver::readStatus()`, `voltageStageReady()`, `tools/status_probe`
  (`status_probe <port> [idle_s] [active_s] [Ua]`; polls the status once a second, no keep-alive
  during the idle phase, so the device's ~11 s watchdog expiry is visible).
- Note: Energomonitor Ia read 4.998 A for a 0.5 A request this time vs 0.4998 A earlier today — likely
  a different current range/ratio on the Energomonitor; unchecked.


**Update 2026-10-05 (AVM Tester bench, different machine, same symptom):**
Re-confirmed, and one hypothesis is now eliminated with independent
instrument confirmation of where the fault actually is.

1. **`pinoutSignals` matches Windows exactly.** Captured today against the
   real RETOM-51 on `/dev/ttyUSB3`: `dtrOk=1 rtsOk=1 pinoutSignals=0xd4` —
   byte-for-byte identical to `retom_debug_windows3.log`'s `COM3` capture.
   **The leading hypothesis (Linux QSerialPort not setting the same control
   lines) is dead.** (For comparison, the Energomonitor's unrelated FTDI
   chip on the same host reports `0xdc` — confirming the value really is
   per-device, not a host-wide Linux quirk.)
2. **FTDI `latency_timer` dropped to 1 ms (from the Linux default 16) — no
   change.** Voltage still reads 0 V. That hypothesis is dead too.
3. **Frame-sequence diff against `retom_debug_windows3.log`**: TX sequences
   match once keep-alive (`0x902`/`ReadInputContacts`) interleaving is
   discounted — that's just polling-timing noise, not a protocol
   divergence. One real payload difference found in the RX stream: a
   `0x804` block reads back **fully zeroed** on Linux (`00 33 f8 09 00 00
   ...`) where Windows reads it **populated** (`01 33 f8 09 aa aa aa aa ee
   ee ee ee ...`). Consistent with the calibration block (fix #2,
   `kEnableCalibration`) genuinely not having completed/settled on this
   run — worth checking whether it needs to run *again* after a specific
   trigger this session didn't hit, or whether `0x804`'s first byte is a
   readiness flag worth polling before trusting `apply()`.
4. **Independent confirmation via the Energomonitor (separate instrument,
   separate library, no DUT involved at all):** applied `Ua=57.74V,
   Ia=0.5A` on RETOM and read both the Energomonitor (`libavm-emon`,
   USB link) and an unrelated AVM-KIV module (Modbus) at the same moment:
   ```
   Energomonitor   Ia = 0.4998 A   Ua = 0.0000 V
   AVM-KIV (DUT)   IA = 49.81 mA   UA = 0.0000 kV
   ```
   Both agree: current is present and correctly scaled by each instrument's
   own ratio; voltage is completely absent from the physical output
   terminals. **This rules out everything downstream of RETOM** — no lead,
   adapter, or reading instrument can explain it, since two independent
   instruments on two independent libraries both see nothing. The fault is
   in RETOM's own voltage generation (or in what this driver sends to
   produce it), full stop.
5. Session log: `retom_debug_linux.log` (committed alongside this update) —
   captured via `retomhold`, a small driven-hold tool built against this
   library directly (not the `tools/live_smoke_test` binary, whose
   hard-coded `/dev/ttyUSB0` would have grabbed the Energomonitor on this
   particular bench, not RETOM — ports are per-bench, always verify by
   VID:PID/model string before trusting a hard-coded default).

Full write-up, including the Energomonitor cross-check methodology, is in
the `avm-tester` repo: `ai/AVM_KIV_TEST_CATALOG.md`, section "S2 first run
against the real bench" and the Energomonitor-confirmation paragraph right
after it.

**Where this leaves the investigation:** both of the two Linux-specific
hypotheses this handoff named (control lines, latency timer) are now
eliminated by direct test. The `0x804` zeroed-block difference (point 3) is
the first concrete lead that isn't "maybe it's Linux" — it points back at
calibration/readiness state on the device itself, which is exactly the
class of bug fix #2 was built to solve. The next step is probably a live
differential capture around *that* block specifically, the same technique
that found fixes #1 and #2 originally.


On the **Linux PC**, running the current code (both repos pulled to latest `master`):
- `apply(Ua=1.0V, Ia=1.0A)` held for 20s → **current is correct (1.0A the whole time)**, but
  **voltage reads 0V the entire time**.
- The run completes with **no exceptions/timeouts** — every protocol frame (including the new
  calibration block, see below) gets ACKed normally.
- This is the *same symptom class* solved twice already on the Windows stand this session (see
  "Fix history" below) — but the exact same code that worked reliably there (many confirmed live
  tests, including fresh mains power-cycles) does not work on Linux.

**Update 2026-08-25 (later same day):** re-confirmed on the Windows stand a third time, this time
with the newer instrumented build (commits `68ed555` timestamped debug output, `da5530b`
DTR/RTS/pinoutSignals-on-open logging) — see `retom_debug_windows3.log` (committed `3df4f41`).
Result unchanged: user physically confirmed Ua=1.0V/Ia=1.0A steady for the full 10s hold. Note the
device had been reassigned from `COM10` to `COM3` by Windows since the earlier captures (same
physical device, just a different enumerated port — don't be alarmed if it moves again). New data
point logged on port open: `dtrOk=1 rtsOk=1 pinoutSignals=0xd4` (bit-decoded: CTS|RTS|DSR|DTR all
high, DCD/RI low). Every TX/RX line now also carries a `[elapsed_ms]` timestamp.

**Immediate next step:** get the equivalent capture from the Linux side with this same
instrumented build, so the `dtrOk`/`rtsOk`/`pinoutSignals` values and per-frame timestamps can be
diffed directly against `retom_debug_windows3.log`:

```sh
RETOM_DEBUG=1 ./build/tools/live_smoke_test /dev/ttyUSB0 10 1.0 2> retom_debug_linux.log
```

(adjust binary path/port/args as needed for however it's being invoked there — pull to latest
`master` first to pick up commits `68ed555`/`da5530b`). Compare:
1. `pinoutSignals` value on port open — if it differs from `0xd4`, that's the strongest lead yet
   (QSerialPort's Linux backend may not expose/set all the same lines as the Windows VCP driver).
2. Per-frame `[elapsed_ms]` deltas against `retom_debug_windows3.log`, in case something is
   timing-sensitive despite the earlier "no timeout" observation.
3. Byte sequences against what's documented as working in `docs/RE_LOG.md` in the `linretom` repo
   (if that repo is also cloned on this machine — see "Repo layout" below) — specifically the
   sections about the arm-preamble and calibration-block fixes.

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
