# libuavlink Interface Control Document

**Protocol version:** 1
**Revision:** draft
**Byte order:** big-endian for all multi-byte fields (`FMT-004`)
**Maximum packet size:** 256 bytes (`FMT-002`)
**CRC:** CRC-16-CCITT, poly `0x1021`, init `0xFFFF`, final XOR `0x0000`, no reflection (`FMT-003`)

---

## 1. Common Header

Present at the start of every packet regardless of message type.

| Offset | Size | Type | Field | Units | Encoding | Valid range | Notes |
| -----: | ---: | ---- | ----- | ----- | -------- | ----------- | ----- |
| 0 | 1 | `uint8_t` | version | n/a | literal | 1 | `FMT-001` |
| 1 | 1 | `uint8_t` | msg_type | n/a | unsigned | `1-5` | `FMT-006` |
| 2 | 4 | `uint32_t` | seq | n/a | unsigned, big-endian | `0-4294967295` | `FMT-005` |
| 6 | 1 | `uint8_t` | payload_len | n/a | unsigned | `0-246` | `FMT-002, Payload bytes only; excludes 8-byte header and 2-byte CRC` |
| 7 | 1 | `uint8_t` | reserved | n/a | literal | `0x00` on encode, any value on decode | ignored by v1 decoder |

**Header size:** 8 bytes

---

## 2. Common Trailer

| Offset | Size | Type | Field | Units | Encoding | Valid range | Notes |
| -----: | ---: | ---- | ----- | ----- | -------- | ----------- | ----- |
| `8 + payload_len` | 2 | `uint16_t` | crc16 | n/a | unsigned, big endian | `0x0000–0xFFFF` | Covers bytes `0 .. (8 + payload_len - 1)`; excludes itself (`FMT-003`) |

---

## 3. TELEMETRY Payload

| Offset | Size | Type | Field | Units | Encoding | Valid range | Notes |
| -----: | ---: | ---- | ----- | ----- | -------- | ----------- | ----- |
| 0 | 4 | `uint32_t` | timestamp_ms | ms | Unsigned, Unscaled | `0`–`4294967295` | 	Since vehicle boot; wraps at 49.7 days |
| 4 | 4 | `int32_t`| latitude | deg | Signed, `x10^7`, BE | `-900000000`–`900000000` | `TM-007` |
| 8 | 4 | `int32_t` | longitude | deg | Signed, `x10^7`, BE | `-1800000000`–`1800000000` | `TM-007` |
| 12 | 4 | `int32_t` | altitude_amsl | m | Signed, `x100`, BE | `-50000`–`1000000` | `TM-007` |
| 16 | 2 | `uint16_t` | ground_speed | m/s | Unsigned, `x100`, BE | `0`-`10000` | `TM-007` |
| 18 | 2 | `int16_t` | vertical_speed | m/s | Signed,  `x100`, BE | `-10000`-`1000` | Positive = climbing |
| 20 | 2 | `uint16_t` | heading | deg | Unsigned, `x100`, BE | `0`-`35999` | `TM-007` 360.00 deg excluded |
| 22 | 2 | `int16_t` | roll | deg | Signed,  `x100`, BE | `-18000`-`18000` | Positive = right wing down |
| 24 | 2 | `int16_t` | pitch | deg | Signed,  `x100`, BE | `-9000`-`9000` | Positive = nose up |
| 26 | 2 | `uint16_t` | battery_voltage | V | Unsigned, `x1000`, BE | `0`-`30000` | mV; covers 3S–6S packs |
| 28 | 1 | `uint8_t` | battery_pct | % | Unsigned, unscaled | `0`-`100` | `TM-007` |
| 29 | 1 | `uint8_t` | gps_fix_type | n/a | enum | `0`-`3` | See 7.5 |
| 30 | 1 | `uint8_t` | gps_sat_count | count | Unsigned, Unscaled | `0`-`32` | `TM-007` |
| 31 | 1 | `uint8_t` | flight_mode | n/a | enum | `0`-`6` | `SIM-004` |
| 32 | 1 | `uint8_t` | status_flags | n/a | bitfield | bit 0 = armed; bits 1 - 7 reserved, `0` | Decoder ignores reserved bits |

**Payload size:** 33 bytes
**Total packet size:** 43 bytes

---

## 4. COMMAND Payload

Payload offsets are relative to the start of the payload (absolute offset = `8 + payload offset`).

| Offset | Size | Type | Field | Units | Encoding | Valid range | Notes |
| -----: | ---: | ---- | ----- | ----- | -------- | ----------- | ----- |
| 0 | 4 | `uint32_t` | session_id | n/a | unsigned, unscaled, BE | `0`–`4294967295` | `CMD-005` |
| 4 | 1 | `uint8_t` | cmd_type | n/a | enum | `0`–`4` | See 7.3 |
| 5 | 4 | `int32_t` | param1 | varies | signed, scaled, BE | see 7.3 | Meaning depends on `cmd_type` |
| 9 | 4 | `int32_t` | param2 | varies | signed, scaled, BE | see 7.3 | Meaning depends on `cmd_type` |
| 13 | 4 | `int32_t` | param3 | varies | signed, scaled, BE | see 7.3 | Meaning depends on `cmd_type` |

**Payload size: 17 bytes**
**Total packet size: 8 + 17 + 2 = 27 bytes**

The command's sequence number is carried in the common header (`seq`), not in the payload.
Unused parameters shall be set to `0` on encode and ignored on decode.

---

## 5. HEARTBEAT Payload

| Offset | Size | Type | Field | Units | Encoding | Valid range | Notes |
| -----: | ---: | ---- | ----- | ----- | -------- | ----------- | ----- |
| — | — | — | — | — | — | — | No payload fields |

**Payload size: 0 bytes**
**Total packet size: 8 + 0 + 2 = 10 bytes**

A heartbeat carries no data because its arrival is itself the information. The vehicle
requires only evidence that the ground station is reachable (`SIM-002`), which the packet's
existence provides. The common header supplies liveness ordering via `seq`.

This makes `payload_len = 0` legal, and 10 bytes the minimum possible packet size — the
lower bound the decoder checks before reading any length field.

---

## 6. ACK / NACK Payload

| Offset | Size | Type | Field | Units | Encoding | Valid range | Notes |
| -----: | ---: | ---- | ----- | ----- | -------- | ----------- | ----- |
| 0 | 4 | `uint32_t` | session_id | n/a | unsigned, unscaled, BE | `0`–`4294967295` | Echoes the command's session (`CMD-001`) |
| 4 | 4 | `uint32_t` | ack_seq | n/a | unsigned, unscaled, BE | `0`–`4294967295` | Echoes the command's header `seq` (`CMD-001`) |
| 8 | 1 | `uint8_t` | reason_code | n/a | enum | `0`–`4` | See 7.4. `0` on ACK |

**Payload size: 9 bytes**
**Total packet size: 8 + 9 + 2 = 19 bytes**

ACK and NACK share an identical payload layout and are distinguished by `msg_type`. The
echoed `session_id` and `ack_seq` are what allow `gcs-cmd` to correlate a response with the
command that produced it, rather than assuming responses arrive in order.

---

## 7. Enumerations

### 7.1 Message Type (`msg_type`)

| Value | Name | Direction |
| ----: | ---- | --------- |
| 0 | *(invalid)* | — |
| 1 | `TELEMETRY` | `uav-sim` → `gcs` |
| 2 | `COMMAND` | `gcs-cmd` → `uav-sim` |
| 3 | `HEARTBEAT` | `gcs` → `uav-sim` |
| 4 | `ACK` | `uav-sim` → `gcs-cmd` |
| 5 | `NACK` | `uav-sim` → `gcs-cmd` |

Value `0` is deliberately unassigned so that an all-zero buffer is rejected as malformed
rather than being interpreted as a valid message type.

### 7.2 Flight Mode (`flight_mode`)

| Value | Name | Flying mode (`SIM-004`) |
| ----: | ---- | ----------------------- |
| 0 | `BOOT` | No |
| 1 | `PREFLIGHT` | No |
| 2 | `ARMED` | Yes |
| 3 | `AUTO` | Yes |
| 4 | `RTL` | Yes |
| 5 | `LANDING` | Yes |
| 6 | `LANDED` | No |

### 7.3 Command Type (`cmd_type`)

| Value | Name | param1 | param2 | param3 |
| ----: | ---- | ------ | ------ | ------ |
| 0 | `ARM` | unused (`0`) | unused (`0`) | unused (`0`) |
| 1 | `DISARM` | unused (`0`) | unused (`0`) | unused (`0`) |
| 2 | `SET_MODE` | target mode (`0`–`6`) | unused (`0`) | unused (`0`) |
| 3 | `GOTO_WAYPOINT` | latitude ×10⁷ | longitude ×10⁷ | altitude AMSL ×100 |
| 4 | `RTL` | unused (`0`) | unused (`0`) | unused (`0`) |

Parameter scale factors match the corresponding telemetry fields so that a waypoint and a
reported position use identical encoding.

### 7.4 NACK Reason Code (`reason_code`)

| Value | Name | Source requirement |
| ----: | ---- | ------------------ |
| 0 | `ACCEPTED` | Used in `ACK` only |
| 1 | `UNKNOWN_COMMAND` | `CMD-004` |
| 2 | `ILLEGAL_TRANSITION` | `SIM-001` |
| 3 | `STALE_SEQUENCE` | `CMD-006` |
| 4 | `INVALID_PARAM` | `TM-007` range rules applied to command parameters |

### 7.5 GPS Fix Type (`gps_fix_type`)

| Value | Name | Meaning |
| ----: | ---- | ------- |
| 0 | `NO_FIX` | No position solution |
| 1 | `FIX_2D` | Horizontal position only |
| 2 | `FIX_3D` | Horizontal and vertical position |
| 3 | `RTK` | Real-time kinematic, centimetre-class |

---

## 8. Design Decisions

### 8.1 Field ordering

Fields are grouped by function — time, position, velocity, attitude, vehicle health, mode —
so that a hex dump can be read against this document without counting bytes. Within the
payload, ordering does not affect packet size, because explicit byte-by-byte serialization
inserts no padding. Ordering largest-to-smallest is nonetheless retained so that the
corresponding in-memory C structure is also compact, avoiding compiler-inserted padding
between fields.

### 8.2 Scaled integers versus floating point

All non-integer quantities are transmitted as scaled integers rather than IEEE 754 floats.
Scaled integers have an identical representation on every architecture and require no
assumption about the host's floating-point format, whereas transmitting a float requires
either a strict-aliasing violation or a `memcpy` through an integer and an assumption that
both endpoints implement IEEE 754.

| Field | Type | Scale | Resolution | Rationale |
| ----- | ---- | ----- | ---------- | --------- |
| latitude, longitude | `int32_t` | ×10⁷ | ~1.1 cm | Sub-metre positioning; ±1.8×10⁹ fits `int32_t` (max 2.147×10⁹) |
| altitude_amsl | `int32_t` | ×100 | 1 cm | Adequate for altitude hold; finer resolution would be false precision |
| ground_speed, vertical_speed | `int16_t` / `uint16_t` | ×100 | 0.01 m/s | Below the noise floor of any real airspeed or barometric sensor |
| heading, roll, pitch | `int16_t` / `uint16_t` | ×100 | 0.01° | Smooth attitude display without float encoding |
| battery_voltage | `uint16_t` | ×1000 | 1 mV | Cell-level voltage monitoring; 0–30 V covers 3S–6S packs |
| battery_pct | `uint8_t` | none | 1% | Sub-percent battery estimates are not physically meaningful |
| gps_sat_count | `uint8_t` | none | 1 | Integer count |

### 8.3 Timestamp representation

`timestamp_ms` is a `uint32_t` counting milliseconds since vehicle boot, sourced from a
monotonic clock.

```
2^32 - 1 = 4,294,967,295 ms
         ÷ 1000   = 4,294,967 s
         ÷ 3600   = 1,193 h
         ÷ 24     = 49.7 days
```

Wraparound occurs after 49.7 days of continuous vehicle uptime, which no flight approaches.
A `uint64_t` would be required only for a wall-clock epoch representation, which is
deliberately not used: it would require the vehicle to know absolute time, and this system
has no clock-synchronisation mechanism.

**Consequence for latency measurement.** Because vehicle and ground clocks are not
synchronised, the difference between `timestamp_ms` and the ground receive timestamp
recorded under `OPS-001` is not a valid one-way latency measurement. What the pair does
support is inter-arrival jitter and relative latency variation, which is what the
performance analysis in Phase 9 reports. Absolute one-way latency would require either
clock synchronisation or a round-trip measurement.

### 8.4 Fixed versus variable payload

Each message type has a fixed payload layout, selected by `msg_type` in the common header.
The payload length varies between message types but not within one.

This trades extensibility for simplicity: adding a field to an existing message type
requires a protocol version increment, whereas a type-length-value scheme would allow
fields to be added without breaking older receivers. The fixed layout was chosen because it
makes the decoder's bounds checking straightforward — the expected length for a given
`msg_type` is a compile-time constant that can be compared against `payload_len` before any
field is read, which directly supports `SYS-004`.

### 8.5 Reserved space and extensibility

Header byte 7 is reserved. It is written as `0x00` on encode and **ignored** on decode.

This is a deliberate exception to the system's general policy of rejecting unexpected
input. A version 1 decoder that rejected a non-zero reserved byte would make the field
useless as an extension point, because a future version 2 sender using it could not
interoperate with version 1 receivers. The general rule is therefore "reject what cannot be
safely interpreted"; a byte designated as carrying no meaning is safe to skip.

`status_flags` bits 1–7 follow the same convention: written as `0`, ignored on decode.

### 8.6 Absence of a synchronisation marker

The protocol has no sync or magic byte. Sync markers exist to locate frame boundaries
within a continuous byte stream, as on a serial link. UDP is datagram-oriented — `recvfrom`
returns a complete datagram or nothing — so a packet always begins at byte 0 and the
framing problem a sync marker solves does not arise.

Rejection of foreign traffic arriving on a shared port is provided instead by the version
field (`FMT-001`), the message type field (`FMT-006`, where `0` is invalid), and the CRC
(`FMT-003`).

### 8.7 Decoder validation order

`payload_len` counts payload bytes only, excluding the 8-byte header and 2-byte CRC. Because
the CRC's location is derived from `payload_len`, the field must be validated before the CRC
can be located. The decoder therefore validates in this fixed order, and no step may read
bytes whose presence has not yet been established:

1. Received length ≥ 10 (minimum packet: 8 header + 0 payload + 2 CRC)
2. `version` == 1 (`FMT-001`)
3. `msg_type` in 1–5 (`FMT-006`)
4. `payload_len` ≤ 246, and `8 + payload_len + 2` == bytes received (`FMT-002`)
5. `payload_len` equals the fixed length defined for this `msg_type`
6. CRC over bytes `0` .. `(8 + payload_len - 1)` equals the transmitted CRC (`FMT-003`)
7. Only then, interpret payload fields

Step 4 is what prevents a crafted `payload_len` from directing a read beyond the supplied
buffer, and is the primary defence verified by the fuzz testing required under `SYS-004`.

---

## 9. Packet Size Summary

| Message type | Header | Payload | CRC | Total |
| ------------ | -----: | ------: | --: | ----: |
| `TELEMETRY` | 8 | 33 | 2 | **43** |
| `COMMAND` | 8 | 17 | 2 | **27** |
| `HEARTBEAT` | 8 | 0 | 2 | **10** |
| `ACK` / `NACK` | 8 | 9 | 2 | **19** |

Telemetry at 50 Hz consumes 43 × 50 = 2,150 bytes/s ≈ **17.2 kbps** before UDP/IP overhead