# uav-gcs — UAV Ground Control Station Simulator

**Status:** 🚧 Work in progress — Phase 2 (protocol codec) in development

A simulated ground control station (GCS) system for an unmanned aerial vehicle,
built from scratch in C99/C++17 to explore the kinds of engineering problems
found in real telemetry/command-and-control systems: binary wire protocols,
UDP networking, data integrity, and reliable command delivery over an
unreliable transport.

This is a learning-through-building portfolio project targeting entry-level
roles in defense, aerospace, embedded, and systems software engineering. It
is **not** a certified or safety-critical system — it's an honest simulation
of the *engineering practices* used to build those systems.

---

## Table of Contents

- [Why This Project](#why-this-project)
- [System Overview](#system-overview)
- [Architecture](#architecture)
- [Wire Protocol (ICD)](#wire-protocol-icd)
- [Key Design Decisions](#key-design-decisions)
- [Project Status](#project-status)
- [Repository Structure](#repository-structure)
- [Building](#building)
- [Testing](#testing)
- [Skills Demonstrated](#skills-demonstrated)
- [Roadmap](#roadmap)
- [License](#license)

---

## Why This Project

Ground control stations sit at the intersection of several hard engineering
problems: parsing untrusted binary data safely, moving telemetry over a
lossy network in real time, and guaranteeing that commands sent to a vehicle
either arrive or are known to have failed — never silently lost, never
applied twice. Those constraints (data integrity, bounded resource usage,
deterministic behavior, graceful failure) show up throughout defense,
aerospace, and embedded software, which is why this project is built around
them rather than around a UI or a database.

## System Overview

The system is split into four independently buildable components that
communicate over UDP using a custom binary protocol (`libuavlink`):

| Component     | Language | Role                                                            |
|----------------|----------|------------------------------------------------------------------|
| `libuavlink`   | C99      | Shared wire protocol: message encode/decode, CRC, validation     |
| `uav-sim`      | C++17    | Simulated vehicle — generates telemetry, receives commands       |
| `gcs`          | C++17    | Ground station — receives/displays telemetry, sends commands     |
| `gcs-cmd`      | C++17    | CLI tool for sending one-off commands to the vehicle              |

`libuavlink` is intentionally written in C99 rather than C++, since it
represents the kind of low-level, dependency-free protocol library that
gets linked into flight software, embedded firmware, or cross-language
tooling — portability and a stable ABI matter more there than convenience.

## Architecture

```
                    UDP / uavlink protocol
   ┌──────────┐  ──── TELEMETRY (43B) ────▶   ┌─────────┐
   │ uav-sim  │  ◀──── COMMAND (27B) ──────    │   gcs   │
   │ (vehicle)│  ──── ACK/NACK (19B) ─────▶    │ (ground │
   │          │  ◀──── HEARTBEAT (10B) ────▶   │ station)│
   └──────────┘                                └─────────┘
        ▲                                            ▲
        │              libuavlink (shared)           │
        └────────────────────┬───────────────────────┘
                              │
                        ┌───────────┐
                        │ gcs-cmd   │  (one-shot command CLI)
                        └───────────┘
```

Telemetry flows one-way over plain UDP — it's high-rate and time-sensitive,
so a dropped packet should be superseded by the next one rather than
retransmitted. Commands are low-rate and safety-relevant, so they're wrapped
in an **application-layer ARQ** (acknowledgment + retransmission) on top of
UDP to get reliability without the overhead or head-of-line blocking of TCP.

## Wire Protocol (ICD)

All messages share an 8-byte header (type, length, sequence number) and a
2-byte CRC-16-CCITT trailer for integrity checking. Multi-byte fields are
serialized big-endian, byte-by-byte — never via struct casting — so the
wire format is independent of host endianness, alignment, and struct
padding.

| Message    | Size (bytes) | Purpose                                      |
|------------|:------------:|-----------------------------------------------|
| TELEMETRY  | 43           | Position, attitude, speed, battery state, etc. |
| COMMAND    | 27           | Commanded setpoints / mode changes             |
| HEARTBEAT  | 10           | Liveness / link-status signaling               |
| ACK        | 19           | Positive acknowledgment of a received command  |
| NACK       | 19           | Negative acknowledgment (with reason code)     |

Numeric fields use **scaled integers** rather than floating point on the
wire (e.g., latitude/longitude ×10⁷, speed/angles ×100, battery voltage
×1000). This guarantees deterministic, platform-independent representation
and avoids IEEE-754 rounding/comparison pitfalls in a protocol context.

Maximum payload size (246 bytes) is derived directly from the 256-byte
packet cap minus the 8-byte header and 2-byte CRC trailer — not an arbitrary
constant.

The full Interface Control Document, finalized after three adversarial
review passes, lives in [`docs/ICD.md`](docs/ICD.md).

## Key Design Decisions

| Decision | Rationale |
|---|---|
| C99 for `libuavlink`, C++17 elsewhere | Protocol library stays portable/dependency-free; application code gets modern C++ ergonomics |
| UDP for telemetry, app-layer ARQ for commands | Match transport reliability guarantees to what each message type actually needs |
| Scaled integers on the wire | Deterministic cross-platform representation; avoids float rounding/comparison issues |
| Big-endian, byte-by-byte (de)serialization | Wire format independent of host endianness, struct padding, and alignment |
| `uavlink_result_t` return type across codec functions | Explicit, uniform error signaling instead of sentinel values or exceptions in the C library |
| Commit-on-success decoding | A decode call only mutates the caller's output struct after *all* validation passes — no partially-populated state on failure |

## Project Status

- [x] **Phase 0 — Toolchain fundamentals:** struct layout, memory-bug
      analysis with ASan/gdb, repo scaffolding
- [x] **Phase 1 — Requirements & architecture:** ICD finalized across all
      five message types after three adversarial review cycles
- [ ] **Phase 2 — `libuavlink` codec** *(in progress)*
  - [x] CRC-16-CCITT implementation, verified against the standard check value
  - [x] Header encoder/decoder with full validation and commit-on-success semantics
  - [ ] Telemetry payload encoder/decoder
  - [ ] Command, heartbeat, and ACK/NACK payload codecs
  - [ ] Full packet round-trip tests (header + payload + CRC)
- [ ] **Phase 3 — `uav-sim`:** simulated vehicle, telemetry generation, UDP send loop
- [ ] **Phase 4 — `gcs`:** telemetry receive/display, command dispatch
- [ ] **Phase 5 — `gcs-cmd`:** command-line uplink tool
- [ ] **Phase 6 — Reliability:** application-layer ARQ for commands, fault injection/handling
- [ ] **Phase 7 — Concurrency:** multithreaded I/O (receive, process, log/display)
- [ ] **Phase 8 — Testing & CI:** unit/integration test suite, sanitizers in CI
- [ ] **Phase 9 — Performance:** profiling, measured latency/throughput
- [ ] **Phase 10 — Docs & polish:** architecture diagrams, this README finalized

## Repository Structure

> Proposed layout — update to match the actual repo if it differs.

```
uav-gcs/
├── CMakeLists.txt
├── README.md
├── docs/
│   ├── ICD.md              # Interface Control Document (wire protocol spec)
│   └── architecture.md     # Design decisions and tradeoffs
├── libuavlink/
│   ├── include/uavlink/
│   ├── src/
│   └── tests/
├── uav-sim/
│   ├── include/
│   └── src/
├── gcs/
│   ├── include/
│   └── src/
├── gcs-cmd/
│   └── src/
└── tests/
    └── integration/
```

## Building

Developed and tested on Linux (via WSL2/Ubuntu on Windows). Requires:

- CMake ≥ 3.16
- GCC or Clang with C99/C++17 support
- GDB and AddressSanitizer for debugging/memory-safety checks

```bash
git clone https://github.com/<your-username>/uav-gcs.git
cd uav-gcs
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build
```

## Testing

- **Unit tests** for `libuavlink`: known-answer tests for CRC-16-CCITT
  against the standard check value, header encode/decode round-trips, and
  payload codecs as they're completed.
- **AddressSanitizer** builds used throughout development to catch
  memory-safety issues (buffer overruns, use-after-free) at the point they
  occur rather than downstream.
- **Round-trip tests**: encode a message with distinct, non-default values
  in every field, decode it, and assert the output matches the original —
  catches field-offset and byte-order bugs that all-zero or repeated-value
  test data would hide.

## Skills Demonstrated

- C99 / C++17 systems programming
- Binary protocol design and implementation (ICD authoring, wire framing)
- Data integrity (CRC-16-CCITT)
- UDP networking and application-layer reliability (ARQ)
- Defensive programming (input validation, commit-on-success semantics)
- Debugging and memory-safety tooling (GDB, AddressSanitizer)
- CMake build systems
- Git-based iterative development

## Roadmap

See [Project Status](#project-status) above. Near-term: finish the
`libuavlink` payload codecs for all five message types, then move to
`uav-sim` and real UDP transport.

## License

---

*Maintained by Zach as a portfolio project. Feedback and issues welcome.*