# libavm-retom — work plan

What this library still needs, written from what its consumers need. The first
consumer is AVM Tester (`repos/avm-tester`), which drives RETOM-51 as the
stimulus source and discrete I/O of an automated test bench.

The byte-level reverse-engineering history stays in the `linretom` repository
(`docs/RE_LOG.md`, `PLAN.md`, `docs/PROTOCOL.md`), as `SESSION_HANDOFF.md`
explains. This file does not duplicate it; it points to it.

## Where the library stands

Live-confirmed on a real RETOM-51 through the library's own FTDI transport, with
no RTDI:

- `connect` / `disconnect`;
- `enable` / `disable`, including the once-per-power-on calibration block;
- `apply` — three-phase U and I, a phase angle per voltage and current channel,
  and frequency — with the keep-alive started automatically;
- `off` / `safetyStop`;
- `readInputContacts` (a 32-bit mask) and `setOutContact`.

Guard limits of 120 V and 20 A are enforced on the host side.

## 1. Identify the device on a shared USB bus — needed first

`findPort()` returns the **first** serial port whose VID/PID is `0403:6001`.
That is FTDI's generic identifier, not RETOM's:

- the Energomonitor ЭМ-3.1КМ's USB link enumerates as the same `0403:6001`
  (`libavm-emon`, `md/mindflow.md`);
- common USB→RS-485 adapters use the same chip.

On a bench carrying several of them, first-match can open the wrong device.

- [ ] Add a read-only probe that confirms whether a given port is a RETOM-51,
  for example `static bool probe(const QString &port)`. It must **never enable
  outputs**.
  - Known signature: the `Lock` frame (`0x00F`) reliably gets reply `0x0501`
    with payload `01 00 00 00` (`linretom`, `docs/RE_LOG.md`, Phase D).
  - Caution from the same phase: a bare `Lock` without the rest of the connect
    bootstrap left the device ignoring every later frame. The probe must go
    through the full `connect(false)` → `disconnect()` path, or otherwise prove
    that it releases the device cleanly.
- [ ] Make `findPort()` verify each `0403:6001` candidate with the probe instead
  of taking the first match — or match on the FTDI chip's serial number
  (`QSerialPortInfo::serialNumber()`), which is unique per device.
- [ ] Confirm on the bench that probing never disturbs the other devices on the
  candidate ports. The Energomonitor's USB link runs at 3,000,000 baud 8N2
  against RETOM's 115200 8N1, so it should see only line noise.

## 2. `ServerInfo` — `linretom` Phase F.1

Returns `Producer`, `Version`, `DeviceNumber`, `maxU`, `maxI`,
`NumberInputContacts`, `NumberOutputContacts`, `minU`, `minI`, `deltaU`,
`deltaI`, `SetsType`.

- [ ] Capture and implement it. The candidate `0x804` is already ruled out: it
  is a heartbeat, not the response structure.
- [ ] Expose it to consumers as `serverInfo()`, with `getNumberOfInputContacts()`
  for the discrete input count — the names AVM Tester checks for.

It gives AVM Tester two things:

- positive identification of the device, superseding the probe signature in §1;
- the discrete contact counts. AVM Tester currently configures its wiring against
  provisional bounds (inputs 0–15, outputs 0–3).

## 3. Trip-time timer — `linretom` Phase F.4

`StartTimer` / `StopTimer` / `IsStopedTimer`: hardware timing from stimulus to
contact. AVM Tester's timing tests — relay and event delays — need it; the use
case is described in `kiv-testing`, `docs/REPORT_RETOM_TIMER_ANALYSIS.md`.

- [ ] Capture and implement it after §2.

## 4. Later, when a test needs it

From `linretom` Phase F, lower priority:

- **F.2 — harmonics and arbitrary waveform (`IAnalogOutputs`). A test now wants
  this.** AVM Tester's AVM-KIV catalogue (suite S2) would check the module's
  harmonic values — `UhA`…, `IhA`…, the crest factors `KrF_*` — against a signal
  with a known harmonic content, which needs RETOM to produce one. Nothing in
  this library exposes it, and `linretom`'s own `PLAN.md` records the protocol as
  entirely un-reversed: "ни одного захвата с этим интерфейсом ещё не снято", with
  the previously mentioned "working harmonics" belonging to the Windows COM
  reference, not to our protocol level. So the order is: a Windows capture of
  `NewAnalogChannels()` / `SetSignal` / `AddHarmonica` (linretom F.2), then the
  frames, then a driver call here. Until then those AVM-KIV checks stay out of
  the catalogue.
- F.3 — `OutEx` / `OutSteps` / `OutSeq` (hardware ramps and step sequences);
- extended binary I/O (`IExtBinIO`, up to 64 inputs and 32 outputs) — only if a
  bench needs more contacts than the base set.

## 5. Housekeeping

- [x] **2026-10-08: the 0 V "fault" below is resolved — it was never in the
  hardware.** `apply()`'s own `voltageStageReady()` guard was a false
  negative: the readiness flag it polled genuinely reads "ready" for about a
  second after `enable()`, then reverts, and the guard's own status check was
  itself enough extra latency to miss that window almost every time. Fixed by
  removing the guard from `apply()` entirely (`src/driver.cpp`); real voltage
  confirmed stable on the Energomonitor with the fix shipped, no bypass flag.
  Full detail in `docs/SESSION_HANDOFF.md`'s 2026-10-08 update, at the top.
- [x] `docs/SESSION_HANDOFF.md` (2026-08-25) still describes a 0 V fault on
  Linux as open. **Confirmed on a Linux bench 2026-10-05 — the fault is
  real, not stale.** The "it's working now" commit (`2a3b6f0`) only added a
  three-phase mode to the smoke test; it did not change `src/`, and this
  build (from `master`, carrying fixes 1-5) still reproduces 0 V exactly as
  described. Two of the handoff's Linux-specific hypotheses were tested and
  eliminated (`pinoutSignals` identical to Windows; FTDI `latency_timer` at
  1 ms made no difference), and the fault was independently confirmed via
  the Energomonitor (separate instrument, separate library, no DUT
  involved) — current is correctly produced, voltage is completely absent
  from the physical output terminals. See `docs/SESSION_HANDOFF.md`'s
  2026-10-05 update for the full detail, including a real RX payload
  difference (`0x804` zeroed on Linux vs populated on Windows) that is the
  first concrete, non-"maybe it's Linux" lead.

## Rules for every item

- Each new command follows `linretom`'s cycle: live capture → frame layout →
  protocol → driver → live check (`linretom` `PLAN.md`, Phase F).
- Agree bench safety for each item before working with a powered instrument.
- Nothing that only identifies or reads may enable an output.
