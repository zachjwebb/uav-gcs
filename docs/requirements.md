# UAV Ground Control System Requirements

This document defines the functional, safety, protocol, and observability requirements for the UAV ground control system.

**Revision:** 3
**Status:** Baseline for Phase 2 (`libuavlink` implementation)

## Requirement ID Scheme

| Prefix | Scope                                 |
| ------ | ------------------------------------- |
| `SYS-` | System-wide, end-to-end behavior      |
| `TM-`  | Telemetry downlink                    |
| `CMD-` | Command uplink                        |
| `SIM-` | Vehicle simulator behavior            |
| `FMT-` | Wire format and protocol library      |
| `OPS-` | Logging, recording, and observability |

Requirement IDs are stable. A deleted requirement retains its number and is marked
deleted rather than being reused.

---

# 1. System-Wide Requirements

## SYS-001 — Malformed Network Input

**Requirement:**
The system shall reject any received datagram that fails validation under `FMT-001`,
`FMT-002`, `FMT-003`, or `SYS-002` without terminating `uav-sim`, `gcs`, or `gcs-cmd`.

**Rationale:**
Network input cannot be assumed to be valid, so malformed packets must be treated as an
expected operating condition rather than a condition that terminates a process. Defining
"malformed" by reference to specific validation requirements removes ambiguity about what
must be rejected.

**Verification:**
Inject datagrams violating each referenced requirement into every network endpoint and
verify that each process remains running and subsequently processes a valid packet.

---

## SYS-002 — Oversized Datagram Rejection

**Requirement:**
The system shall detect and reject any received UDP datagram larger than `256 bytes`
without interpreting any portion of the datagram as a valid protocol packet, and shall
increment `oversized_packets`.

**Rationale:**
A receive buffer sized exactly to the maximum packet length would cause the operating
system to silently truncate an oversized datagram, allowing a fragment of invalid data to
be interpreted as a valid packet. Oversize must be detectable and not just survivable.

**Verification:**
Send UDP datagrams of `256 bytes` and `257 bytes` and verify that the `256-byte` datagram
is processed and that the `257-byte` datagram is rejected and increments
`oversized_packets` by `1`.

---

## SYS-003 — Bounded Telemetry Queue

**Requirement:**
The GCS shall maintain a telemetry receive queue with a maximum capacity of
`1,024 packets`; when the queue is full, the GCS shall discard the oldest queued telemetry
packet, increment `queue_drops`, and enqueue the newly received packet.

**Rationale:**
A bounded queue prevents unbounded memory growth when the processing stage falls behind
the receive stage. Discarding the oldest packet rather than the newest preserves the
freshest vehicle state, which has greater operational value.

**Verification:**
Fill the queue to `1,024 packets` with the consumer suspended, inject one additional valid
telemetry packet, and verify that the oldest queued packet is removed, the new packet is
retained, `queue_drops` increases by `1`, and the consumer processes packets normally when
resumed.

---

## SYS-004 — Decoder Bounds Safety

**Requirement:**
The protocol decoder shall never read or write outside the bounds of the input or output
buffers for any possible input byte sequence of any length.

**Rationale:**
Network data is untrusted and arbitrary. An out-of-bounds access at the protocol boundary
could terminate a process or corrupt memory, and is the highest-severity defect class in
the system. This is a total property: it must hold for all inputs, not only for inputs the
system expects.

**Verification:**
Execute the decoder against at least `1,000,000` generated inputs,
including truncated, oversized, and randomly mutated packets, under AddressSanitizer and
UndefinedBehaviorSanitizer, and verify zero reported violations.

---

## SYS-005 — Ground-Station Heartbeat Transmission

**Requirement:**
`gcs` shall transmit a ground-station heartbeat message to `uav-sim` every `1,000 ms`
while running, independently of operator command activity.

**Rationale:**
The absence of commands is not evidence of a lost link, because commands are sporadic and
operator-driven. A continuous heartbeat provides the vehicle with an independent and
periodic indication that the ground station remains reachable. The heartbeat originates
from `gcs` rather than `gcs-cmd` because `gcs` runs continuously, so heartbeat presence
represents ground-station availability rather than the lifetime of an individual operator
session.

**Verification:**
Run `gcs` for `60 seconds` with no operator commands and verify that at least `58`
heartbeat messages are transmitted with a p99 inter-heartbeat interval within
`1,000 ms ± 100 ms`.

---

# 2. Telemetry Requirements

## TM-001 — Telemetry Transmission Rate

**Derived from:** `SIM-006`

**Requirement:**
`uav-sim` shall transmit telemetry at a nominal rate of `50 Hz`. During steady-state
operation, the 99th percentile of consecutive inter-packet intervals shall be within
`20 ms ± 2 ms`, and the maximum observed interval shall not exceed `50 ms`. Steady-state
operation excludes the first `100` cycles following process start.

**Rationale:**
A statistical bound reflects what a general-purpose Linux kernel can guarantee. The
scheduler may preempt the simulator process at any time, so a hard bound on every interval
would specify hard real-time behavior that the platform does not provide. Meeting a hard
per-cycle bound would require a `PREEMPT_RT` kernel, CPU isolation, and real-time
scheduling priority, which are outside the scope of this project. Intervals that exceed
the bound are detected and counted under `SIM-006` rather than assumed not to occur.
Excluding startup cycles prevents process initialization from being measured as
steady-state jitter.

**Verification:**
Capture at least `10,000` consecutive steady-state telemetry transmissions, compute the
distribution of inter-packet intervals, and verify the p99 and maximum bounds. Record the
full distribution in the performance report.

---

## TM-002 — Sequence Gap Detection

**Derived from:** `FMT-005`

**Requirement:**
The GCS shall detect a forward telemetry sequence-number gap, increment `packets_lost` by
the number of missing sequence values, and record each missing sequence value in a bounded
outstanding-gap set holding at most `64` entries. When the set is full, the oldest entry
shall be evicted and `gaps_expired` incremented by `1`.

**Rationale:**
UDP does not guarantee delivery, so sequence numbers provide the mechanism for detecting
and measuring telemetry loss. The outstanding-gap set allows a later-arriving packet to be
distinguished from a genuinely lost packet (see `TM-003`). The set is bounded because
unbounded gap tracking would allow sustained loss to consume unbounded memory.

**Verification:**
Deliver telemetry packets with sequence numbers `10` and `13` consecutively and verify
that `packets_lost` increases by `2` and that sequence values `11` and `12` are present in
the outstanding-gap set. Separately, induce more than `64` outstanding gaps and verify that
the set size remains at `64` and that `gaps_expired` increments.

---

## TM-003 — Telemetry Reordering and Gap Recovery

**Derived from:** `TM-002`

**Requirement:**
The GCS shall increment `packets_reordered` on receipt of a valid telemetry packet whose
sequence number is lower than the highest accepted sequence number. If that sequence
number is present in the outstanding-gap set, the GCS shall additionally remove it from
the set and increment `packets_recovered` by `1`. A reordered packet shall not replace
vehicle state derived from a higher sequence number.

**Rationale:**
UDP can deliver packets out of order. A reordered packet is late, not lost, so counting it
only as loss would misreport link quality. Net telemetry loss is defined as
`packets_lost - packets_recovered`, which makes the reported loss rate accurate under
reordering. Applying stale telemetry over newer state would cause the ground station to
display incorrect vehicle information.

**Verification:**
Deliver valid telemetry packets with sequence numbers `10`, `12`, `11` and verify that
after the sequence completes: `packets_lost == 1`, `packets_recovered == 1`,
`packets_reordered == 1`, net loss `== 0`, and displayed vehicle state derives from
sequence `12`.

---

## TM-004 — Telemetry Duplication

**Derived from:** `FMT-005`

**Requirement:**
The GCS shall increment `packets_duplicate` and shall not apply new vehicle state on
receipt of a valid telemetry packet whose sequence number equals the highest accepted
sequence number or is present in neither the outstanding-gap set nor above the highest
accepted sequence number.

**Rationale:**
UDP does not prevent duplicate delivery, and processing duplicates as new telemetry would
distort rate and state-update measurements. Duplicate detection is limited to the highest
accepted sequence number and the bounded outstanding-gap set; a duplicate of a packet
older than the gap window is classified as reordered rather than duplicate. This limit is
accepted because unbounded duplicate detection would require retaining every sequence
number ever received.

**Verification:**
Deliver the same telemetry packet twice consecutively and verify that the second
increments `packets_duplicate` by `1` and produces no state update. Separately, deliver a
duplicate of a packet older than the gap window and verify it is classified as reordered.

---

## TM-005 — Telemetry CRC Failure

**Derived from:** `FMT-003`

**Requirement:**
The GCS shall discard telemetry packets whose computed CRC does not equal the transmitted
CRC, increment `crc_errors` by `1`, and shall not update vehicle state or sequence
tracking from the rejected packet.

**Rationale:**
Corrupted telemetry must not become ground-station state. Sequence tracking is excluded
because the sequence number of a corrupted packet is itself untrustworthy and could
corrupt loss accounting.

**Verification:**
Corrupt one byte in a valid telemetry packet and verify rejection, a one-count increase in
`crc_errors`, unchanged vehicle state, and unchanged sequence-tracking state.

---

## TM-006 — Stale Telemetry Detection

**Requirement:**
The GCS shall declare the vehicle link stale when no valid telemetry packet has been
accepted for `2,000 ms`, shall log the condition at `WARN`, shall mark all displayed and
reported vehicle state as stale, and shall increment `link_stale_events` by `1`. The GCS
shall clear the stale condition and log at `INFO` on receipt of the next valid telemetry
packet.

**Rationale:**
The vehicle detects loss of the ground station via heartbeat timeout (`SIM-002`); the
ground station requires the equivalent capability in the opposite direction. Continuing to
present the last received position without indicating its age would allow an operator to
act on arbitrarily stale vehicle state. The `2,000 ms` threshold corresponds to `100`
missed telemetry packets at the `50 Hz` nominal rate, which tolerates substantial
loss without producing alarms.

**Verification:**
Establish telemetry flow, suppress all telemetry, and verify that the stale condition is
declared within `2,000 ms ± 100 ms`, state is marked stale, and `link_stale_events`
increments. Resume telemetry and verify the condition clears.

---

## TM-007 — Telemetry Field Range Validation

**Requirement:**
The GCS shall reject any decoded telemetry packet containing a field outside the following
ranges, increment `range_errors` by `1`, log at `WARN`, and shall not update vehicle state
from the rejected packet:

| Field                | Valid range                          |
| -------------------- | ------------------------------------ |
| Latitude             | `-90.0` to `+90.0` degrees           |
| Longitude            | `-180.0` to `+180.0` degrees         |
| Altitude (AMSL)      | `-500 m` to `+10,000 m`              |
| Battery percentage   | `0` to `100`                         |
| GPS satellite count  | `0` to `32`                          |
| Heading              | `0.0` inclusive to `360.0` exclusive |
| Ground speed         | `0.0` to `100.0` m/s                 |

Any field containing a NaN or infinite floating-point value shall be rejected under this
requirement.

**Rationale:**
CRC validation detects corruption introduced in transit but cannot detect a validly encoded
impossible value produced by a defect in the transmitting software. Range validation is
specified as ground-station behavior rather than protocol-library behavior because the
valid ranges are operational policy that depends on the vehicle and mission, whereas
`libuavlink` defines only the wire format and must remain policy-free. Altitude is
specified above mean sea level; a datum must be stated explicitly because AMSL and
above-ground-level differ by the terrain elevation. NaN comparisons are always false, so
NaN would silently pass a naive range check and must be excluded explicitly.

**Verification:**
For each field, generate telemetry at both range boundaries and at one value outside each
boundary, and verify that boundary values are accepted and out-of-range values are
rejected with a `range_errors` increment. Additionally verify rejection of NaN and infinity
in every floating-point field.

---

# 3. Command Requirements

## CMD-001 — Valid Command Acknowledgment

**Requirement:**
`uav-sim` shall respond to every command that passes validation under `SYS-001` with an
`ACK` or `NACK` containing the command's session identifier and sequence number.
`uav-sim` shall not respond to a datagram that fails validation.

**Rationale:**
The operator must be able to distinguish command acceptance, command rejection, and
communication loss. Commands that fail validation are not acknowledged because the session
identifier and sequence number in an invalid packet cannot be trusted, and responding to
unvalidated input would echo attacker-controlled data. The retry logic in `CMD-002` handles
the resulting absence of a response.

**Verification:**
Send each defined command in both accepted and rejected states and verify that each
produces a response carrying the originating session identifier and sequence number.
Separately, send a command with an invalid CRC and verify that no response is transmitted.

---

## CMD-002 — Command Retry Limit

**Requirement:**
`gcs-cmd` shall retransmit an unacknowledged command `750 ms` after the previous
transmission and shall transmit a given command no more than `3` times before reporting
command failure to the operator.

**Rationale:**
Bounded application-level retries provide command reliability without allowing a lost link
to block the operator indefinitely. The `750 ms` interval accommodates a modeled round-trip
time of up to approximately `500 ms` plus vehicle processing latency; a shorter interval
would classify in-flight responses as failures on a high-latency link. Three attempts
bound total command resolution time to approximately `2.25 s`.

**Verification:**
Suppress all ACK packets and verify exactly three transmissions at `750 ms ± 50 ms`
intervals followed by a reported command failure.

---

## CMD-003 — Duplicate Command Handling

**Derived from:** `CMD-006`

**Requirement:**
`uav-sim` shall execute a command identified by a given session identifier and sequence
number at most once. On receipt of a command whose session identifier and sequence number
are present in the deduplication window, `uav-sim` shall retransmit the previously
generated response without re-executing the command.

**Rationale:**
An acknowledgment can be lost after the vehicle has executed a command, causing `gcs-cmd`
to retry a command that already took effect. Deduplication makes retries idempotent.
Retransmitting the original response rather than generating a new one ensures the operator
receives the outcome of the original execution.

**Verification:**
Send a state-changing command twice with identical session identifier and sequence number
and verify exactly one state change and one response per received copy, with both
responses identical.

---

## CMD-004 — Unknown Command Handling

**Requirement:**
`uav-sim` shall respond to a valid packet containing an unsupported command type with a
`NACK` carrying reason code `UNKNOWN_COMMAND`, shall increment `unknown_commands` by `1`,
and shall not modify vehicle state.

**Rationale:**
An unsupported command may indicate a version mismatch between ground and vehicle
software. Rejecting it observably allows the condition to be diagnosed, whereas silently
ignoring it would present as an unexplained command failure.

**Verification:**
Send a validly framed packet containing an undefined command type and verify the
`UNKNOWN_COMMAND` NACK, the counter increment, and unchanged vehicle state.

---

## CMD-005 — Command Session Identification

**Requirement:**
`gcs-cmd` shall generate a new `32-bit` session identifier at process start, shall
initialize its command sequence number to `0`, and shall include both values in every
transmitted command.

**Rationale:**
A restarted `gcs-cmd` process begins numbering commands from zero. Without a session
identifier, the vehicle's deduplication state would treat those commands as retries of
previously executed commands and silently discard them.

**Verification:**
Start `gcs-cmd` twice and verify that the session identifiers differ and that the first
command of each session carries sequence number `0`.

---

## CMD-006 — Bounded Command Deduplication State

**Requirement:**
`uav-sim` shall maintain deduplication state for at most one active command session and at
most the `16` most recently received sequence numbers within that session. On receipt of a
valid command bearing a session identifier different from the active session, `uav-sim`
shall replace the active session, clear the deduplication window, and log the session
change at `INFO`. `uav-sim` shall respond to a command whose sequence number precedes the
deduplication window with a `NACK` carrying reason code `STALE_SEQUENCE` and shall not
execute it.

**Rationale:**
Retaining every observed sequence number would consume unbounded memory, which is
inconsistent with the bounded-resource requirements applied elsewhere in the system. A
`16`-entry window covers substantially more than the `3` transmissions permitted by
`CMD-002`. Supporting a single active session reflects the operational model of one
operator at a time; rejecting rather than executing a command older than the window is the
safe outcome, because such a command cannot be distinguished from a delayed retry of an
already-executed command.

**Verification:**
Send `17` commands within one session, then retransmit the first, and verify a
`STALE_SEQUENCE` NACK with no state change. Separately, send a command with a new session
identifier and verify that the window is cleared, the session change is logged, and the
command executes.

---

# 4. Vehicle Simulator Requirements

## SIM-001 — Flight-Mode Transition Table

**Requirement:**
`uav-sim` shall implement exactly the transitions listed below, and shall permit no other
transition between flight modes.

| From        | To          | Trigger type | Trigger condition                              |
| ----------- | ----------- | ------------ | ---------------------------------------------- |
| `BOOT`      | `PREFLIGHT` | Condition    | Power-on self-test passes                      |
| `PREFLIGHT` | `ARMED`     | Operator     | `ARM` command                                   |
| `ARMED`     | `PREFLIGHT` | Operator     | `DISARM` command                                |
| `ARMED`     | `AUTO`      | Operator     | `GOTO_WAYPOINT` command                         |
| `ARMED`     | `RTL`       | Failsafe     | Heartbeat timeout or battery `<= 20%`           |
| `ARMED`     | `LANDING`   | Failsafe     | Battery `<= 5%`                                 |
| `AUTO`      | `RTL`       | Operator     | `RTL` command                                   |
| `AUTO`      | `RTL`       | Failsafe     | Heartbeat timeout or battery `<= 20%`           |
| `AUTO`      | `LANDING`   | Failsafe     | Battery `<= 5%`                                 |
| `RTL`       | `LANDING`   | Condition    | Vehicle within `5 m` horizontally of home       |
| `RTL`       | `LANDING`   | Failsafe     | Battery `<= 5%`                                 |
| `LANDING`   | `LANDED`    | Condition    | Vehicle altitude within `1 m` of ground         |
| `LANDED`    | `PREFLIGHT` | Operator     | `DISARM` command                                |

A command requesting a transition not listed above shall be rejected with a `NACK` carrying
reason code `ILLEGAL_TRANSITION`, shall increment `illegal_transitions` by `1`, and shall
not modify the flight mode.

**Rationale:**
An explicit and complete transition table makes vehicle behavior reviewable, prevents
undefined flight states, and defines the exhaustive test matrix for the state machine. The
trigger-type column distinguishes operator-commanded transitions, autonomous failsafe
transitions, and autonomous transitions arising from normal vehicle condition; all three
categories are required for the vehicle to progress from power-on to landing without
becoming stuck in an unreachable state.

**Verification:**
Exercise every listed transition and verify it occurs. Exercise every unlisted ordered pair
of modes as an operator command and verify rejection with `ILLEGAL_TRANSITION` and no mode
change.

---

## SIM-002 — Heartbeat-Based Lost-Link Failsafe

**Derived from:** `SYS-005`, `SIM-001`

**Requirement:**
`uav-sim` shall transition to `RTL` when no valid ground-station heartbeat has been
received for `3,000 ms` while in `ARMED` or `AUTO`, and shall log the transition at `WARN`.
While already in `RTL`, `LANDING`, `LANDED`, `BOOT`, or `PREFLIGHT`, a heartbeat timeout
shall increment `heartbeat_timeouts` and shall not cause a mode transition.

**Rationale:**
The vehicle cannot infer link status from command traffic because commands are sporadic.
The `3,000 ms` timeout corresponds to three consecutive missed heartbeats at the `1 Hz`
rate specified in `SYS-005`. Tolerating two missed heartbeats prevents isolated packet loss
on an unreliable link from triggering a spurious failsafe, while three missed heartbeats
provides a timely response to a genuine link failure. The condition does not apply in `RTL`
because the vehicle is already executing the failsafe response, and does not apply on the
ground because no autonomous action is required.

**Verification:**
Place the simulator in `AUTO`, suppress heartbeat transmission, and verify transition to
`RTL` within `3,000 ms` plus one control cycle. Suppress a single heartbeat and verify no
transition. Repeat with the vehicle already in `RTL` and verify that `heartbeat_timeouts`
increments with no mode change.

---

## SIM-003 — Low-Battery Failsafe

**Derived from:** `SIM-001`

**Requirement:**
`uav-sim` shall transition to `RTL` when simulated battery percentage falls to `20%` or
below while in `ARMED` or `AUTO`, and shall transition to `LANDING` when simulated battery
percentage falls to `5%` or below while in `ARMED`, `AUTO`, or `RTL`. Both transitions
shall be logged at `WARN`.

**Rationale:**
A two-threshold policy provides an escalating response. At `20%` the vehicle has sufficient
energy to return to its launch point. At `5%` it does not, so the objective changes from
returning home to reaching the ground under control rather than depleting the battery in
flight. Neither threshold applies in `LANDING`, because the vehicle is already descending
and interrupting a landing would not improve the outcome.

**Verification:**
Set battery to `20%` in `AUTO` and verify transition to `RTL`. Set battery to `5%` in `RTL`
and verify transition to `LANDING`. Set battery to `5%` while in `LANDING` and verify no
mode change.

---

## SIM-004 — Flight-Mode Definition

**Requirement:**
`uav-sim` shall implement the flight modes `BOOT`, `PREFLIGHT`, `ARMED`, `AUTO`, `RTL`,
`LANDING`, and `LANDED`. The modes `ARMED`, `AUTO`, `RTL`, and `LANDING` shall be
classified as flying modes. `uav-sim` shall report its current flight mode in every
telemetry packet.

**Rationale:**
An explicit mode set and an explicit definition of "flying mode" prevent different
components from interpreting the failsafe requirements differently. Reporting mode in
telemetry allows the ground station to observe autonomous transitions, which are otherwise
invisible to the operator because they are not the result of a command.

**Verification:**
Drive the simulator through all seven modes and verify that each is represented in
telemetry and that the flying-mode classification matches this requirement.

---

## SIM-005 — Autonomous Transition Independence

**Derived from:** `SIM-001`

**Requirement:**
`uav-sim` shall execute every transition of trigger type `Failsafe` or `Condition` in
`SIM-001` without requiring an operator command and without requiring ground-station
connectivity.

**Rationale:**
Failsafe transitions exist precisely for conditions in which the ground station is
unreachable. A failsafe that depended on ground connectivity would be unavailable in the
scenario it is designed to handle.

**Verification:**
With all ground-station network traffic suppressed, trigger each failsafe and condition
transition and verify that each occurs.

---

## SIM-006 — Control Cycle Overrun Detection

**Requirement:**
`uav-sim` shall compute each control cycle's intended wake time as a fixed `20 ms`
increment of the previous intended wake time using a monotonic clock. When the actual wake
time exceeds the intended wake time by more than `2 ms`, `uav-sim` shall increment
`cycle_overruns` by `1` and record the overrun magnitude. When cumulative drift exceeds one
full cycle, `uav-sim` shall skip the missed cycle rather than executing it late, and shall
increment `cycles_skipped`.

**Rationale:**
Computing wake times as absolute increments of a fixed schedule prevents the cumulative
drift that results from sleeping for a fixed duration each cycle, because per-cycle
execution time and scheduling latency do not accumulate. Because the target platform is a
general-purpose Linux kernel, overruns will occur and cannot be prevented; a cyclic
executive is therefore required to detect and report deadline misses rather than to assume
their absence. A monotonic clock is required because a wall-clock adjustment during
operation would otherwise corrupt the schedule.

**Verification:**
Run `uav-sim` for `60 seconds` under artificial CPU contention and verify that
`cycle_overruns` is non-zero, that each overrun is logged with its magnitude, and that the
mean telemetry rate remains within `1%` of `50 Hz` despite overruns.

---

# 5. Wire Format / Protocol Library Requirements

## FMT-001 — Protocol Version

**Requirement:**
`libuavlink` shall encode protocol version `1` in every packet and shall reject any packet
whose version field is not `1` during decoding, reporting a distinct version-mismatch
error.

**Rationale:**
A version field allows an incompatible wire format to be detected rather than decoded
using the wrong field layout. A distinct error code allows a version mismatch to be
diagnosed separately from corruption, since the two have entirely different remedies.

**Verification:**
Encode a packet and verify the version field equals `1`. Decode packets with version fields
`0`, `1`, and `2` and verify that only version `1` is accepted and that the others produce
a version-mismatch error.

---

## FMT-002 — Maximum Packet Length

**Derived from:** `SYS-002`

**Requirement:**
`libuavlink` shall reject any packet whose total encoded length exceeds `256 bytes` or
whose declared payload length is inconsistent with the number of bytes supplied to the
decoder.

**Rationale:**
A protocol-level size limit provides a deterministic upper bound for buffer allocation and
decoder work. Validating the declared length against the actual byte count prevents a
crafted length field from directing the decoder to read beyond the supplied buffer, which
is the most common exploitation path in binary protocol parsers.

**Verification:**
Decode packets of `256` and `257` bytes and verify acceptance and rejection respectively.
Decode a packet whose declared payload length exceeds the supplied buffer and verify
rejection without an out-of-bounds read under AddressSanitizer.

---

## FMT-003 — CRC-16-CCITT

**Requirement:**
`libuavlink` shall compute a CRC-16-CCITT using polynomial `0x1021`, initial value
`0xFFFF`, and final XOR value `0x0000`, with no input or output reflection. The CRC shall
cover the complete packet header and payload and shall exclude the CRC field itself. The
CRC shall be transmitted as the final field of the packet.

**Rationale:**
A fully specified CRC — polynomial, initial value, final XOR, reflection, and covered byte
range — is required for independent implementations to agree. Two correct but differently
parameterized CRC implementations reject every one of each other's packets, and the
resulting failure is indistinguishable from total link corruption.

**Verification:**
Compute the CRC of the ASCII string `123456789` and verify the result equals `0x29B1`,
which is the published check value for this parameterization. Additionally encode a packet
containing known field values, compare the complete byte sequence against a stored golden
vector, corrupt one byte, and verify that decoding reports a CRC error.

---

## FMT-004 — Network Byte Order

**Requirement:**
`libuavlink` shall encode all multi-byte integer fields in big-endian byte order and shall
decode them using the same byte order, on all host architectures. Serialization shall be
performed byte by byte; the library shall not copy a structure directly into or out of a
packet buffer.

**Rationale:**
Byte order is a property of the interface contract, not of the host. Even where both
endpoints currently share an architecture, the vehicle and ground software may be built
for different targets by different teams over the system's lifetime. Explicit byte-by-byte
serialization additionally eliminates two further hazards of direct structure copying:
compiler-inserted padding bytes, which vary between compilers and would both change the
wire layout and transmit indeterminate memory contents, and unaligned access, which is
undefined when a received buffer does not satisfy a structure's alignment requirement.

**Verification:**
Encode the `32-bit` value `0x01020304` and verify the byte sequence `01 02 03 04` appears
in the buffer at the expected offset. Verify that the encoded packet length matches the sum
of the specified field widths, confirming that no padding is present.

---

## FMT-005 — Sequence Number Width

**Requirement:**
`libuavlink` shall encode telemetry and command sequence numbers as unsigned `32-bit`
integers.

**Rationale:**
A `32-bit` sequence space at the `50 Hz` telemetry rate wraps after approximately
`2.7 years` of continuous operation, which exceeds any plausible session duration for this
system and therefore removes wraparound handling from the loss-detection logic. A `16-bit`
field would wrap every `22 minutes`, requiring every sequence comparison in `TM-002`
through `TM-004` to implement modular arithmetic.

**Verification:**
Encode sequence values `0`, `1`, and `4294967295` and verify each occupies four bytes and
round-trips exactly.

---

## FMT-006 — Message Type Identification

**Requirement:**
`libuavlink` shall encode a message type field in every packet identifying the packet as
one of `TELEMETRY`, `COMMAND`, `HEARTBEAT`, `ACK`, or `NACK`, and shall reject any packet
whose message type is not one of these values.

**Rationale:**
`gcs` and `gcs-cmd` both transmit to the same vehicle UDP port, so the vehicle must
distinguish heartbeat messages from command messages by content rather than by port. A
message type field also allows the payload layout to be selected before the payload is
interpreted, which is a prerequisite for validating the declared length under `FMT-002`.

**Verification:**
Encode one packet of each defined type and verify the type field round-trips. Decode a
packet with an undefined type value and verify rejection.

---

# 6. Operations / Observability Requirements

## OPS-001 — Telemetry Recording

**Requirement:**
The GCS shall record every accepted telemetry packet with its ground receive timestamp,
vehicle timestamp, sequence number, flight mode, position, velocity, attitude, battery
state, GPS state, and protocol version.

**Rationale:**
A recording containing both vehicle and ground timestamps supports post-run analysis of
vehicle behavior and of end-to-end link latency, and enables deterministic replay of a
session for debugging.

**Verification:**
Run the GCS against telemetry for `10 seconds` and verify that the number of recorded
entries equals the number of accepted packets and that every specified field is present and
correct in each entry.

---

## OPS-002 — Bounded Recording Storage

**Requirement:**
The GCS shall limit each telemetry recording file to `100 MB` and shall begin a new file
before exceeding that limit. The GCS shall retain at most `10` recording files, deleting
the oldest file when creating an eleventh, and shall log each deletion at `INFO`.

**Rationale:**
File rotation alone bounds individual file size but not total storage, so a continuous
telemetry stream would still exhaust the filesystem. A retention limit bounds total
recording storage to approximately `1 GB`. Exhausting the filesystem would affect every
process on the host, not only the ground station, so the recorder must not be capable of
unbounded growth.

**Verification:**
Generate telemetry until `11` files would be created and verify that no file exceeds
`100 MB`, that exactly `10` files are retained, that the retained files are the most
recent, and that each deletion is logged.

---

## OPS-003 — Failure Counters

**Requirement:**
The GCS shall maintain separate monotonically increasing counters for: `crc_errors`,
`version_errors`, `malformed_packets`, `oversized_packets`, `range_errors`,
`packets_lost`, `packets_recovered`, `packets_reordered`, `packets_duplicate`,
`gaps_expired`, `queue_drops`, and `link_stale_events`. `uav-sim` shall maintain separate
counters for `heartbeat_timeouts`, `illegal_transitions`, `unknown_commands`,
`cycle_overruns`, and `cycles_skipped`. Every counter shall be reported at a configurable
interval with a default of `10 seconds`.

**Rationale:**
Distinct counters allow corruption, version incompatibility, malformed input, oversize
input, invalid values, loss, recovery, reordering, duplication, gap-tracking exhaustion,
processing overload, and link outage to be distinguished from one another. A single
aggregate error count would identify that the system was degraded without identifying the
cause, which is the primary purpose of the counters.

**Verification:**
Inject one instance of each defined failure condition and verify that only the
corresponding counter increments.

---

## OPS-004 — Command Failure Logging

**Derived from:** `CMD-002`

**Requirement:**
`gcs-cmd` shall log at `ERROR` every command that fails after `3` unsuccessful
transmission attempts, including the session identifier, sequence number, command type, and
failure reason.

**Rationale:**
A command that was never confirmed represents an operator intent that may or may not have
taken effect on the vehicle. Recording these events provides the audit trail required to
reconstruct what was requested versus what the vehicle actually did.

**Verification:**
Suppress all ACKs for a command and verify an `ERROR` log entry containing all four
specified values after the third attempt.

---

## OPS-005 — Log Levels and Format

**Requirement:**
All components shall emit log records containing a monotonic timestamp with millisecond or
finer resolution, a severity level, a component identifier, and a message. Severity levels
shall be `DEBUG`, `INFO`, `WARN`, and `ERROR`, assigned as follows:

| Level   | Usage                                                                         |
| ------- | ----------------------------------------------------------------------------- |
| `DEBUG` | Per-packet detail; disabled by default                                        |
| `INFO`  | Normal operational events: startup, mode changes, session changes, rotation   |
| `WARN`  | Expected abnormal conditions: packet loss, CRC errors, failsafe transitions   |
| `ERROR` | Conditions requiring operator attention: command failure, unrecoverable I/O   |

The minimum emitted level shall be configurable at startup with a default of `INFO`.

**Rationale:**
Consistent levels allow a filter to isolate operationally significant events from
per-packet detail. Packet loss is classified as `WARN` rather than `ERROR` because it is an
expected condition on an unreliable link, and classifying expected conditions as errors
causes genuine errors to be overlooked. Per-packet logging is disabled by default because
at `50 Hz` it would dominate both the log volume and the processing budget.

**Verification:**
Configure each minimum level and verify that only records at or above it are emitted.
Verify that every emitted record contains all four required fields. Trigger one condition
at each level and verify the assigned severity.

---

# 7. Open Items

The following require decisions before the affected component is implemented.

| Item                                                                                     | Blocks    |
| ---------------------------------------------------------------------------------------- | --------- |
| ACK/NACK destination: reply to the command datagram's source address, or a fixed port      | Phase 7   |
| Behavior when two `gcs-cmd` instances transmit concurrently with different session IDs     | Phase 7   |
| Whether `gcs` continues recording when the vehicle link is stale                           | Phase 4   |
| Power-on self-test content and failure behavior (`BOOT` has no exit path on failure)       | Phase 3   |
| Geofence definition and whether `GOTO_WAYPOINT` validates against it                       | Phase 7   |