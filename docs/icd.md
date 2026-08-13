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

| Offset | Size | Type | Field | Units | Encoding | Valid range | Notes |
| -----: | ---: | ---- | ----- | ----- | -------- | ----------- | ----- |
| 0 | 4 | `uint32_t` | session_id | n/a | unsigned, BE | `0`–`4294967295` | `CMD-005` |
| 4 | 1 | `uint8_t` | cmd_type | n/a | enum | `0`–`4` | See 7.3 |
| 5 | 4 | `int32_t` | param1 | varies | signed, BE | see 7.3 | Meaning depends on `cmd_type` |
| 9 | 4 | `int32_t` | param2 | varies | signed, BE | see 7.3 | Unused params encoded as `0` |
| 13 | 4 | `int32_t` | param3 | varies | signed, BE | see 7.3 | Unused params encoded as `0` |

**Payload size: 17 bytes. Total packet: 8 + 17 + 2 = 27 bytes.**

---

## 5. HEARTBEAT Payload

No payload. `payload_len = 0`. Total packet: 8 + 0 + 2 = 10 bytes.

The arrival of the heartbeat is itself the information (`SYS-005`); no additional content is
required to establish ground-station liveness. This is the minimum-size packet the protocol
can produce and therefore defines the decoder's minimum length check.

---

## 6. ACK / NACK Payload

| Offset | Size | Type | Field | Units | Encoding | Valid range | Notes |
| -----: | ---: | ---- | ----- | ----- | -------- | ----------- | ----- |
| 0 | 4 | `uint32_t` | session_id | n/a | unsigned, BE | `0`–`4294967295` | Echoed from command (`CMD-001`) |
| 4 | 4 | `uint32_t` | ack_seq | n/a | unsigned, BE | `0`–`4294967295` | Echoed from command (`CMD-001`) |
| 8 | 1 | `uint8_t` | reason_code | n/a | enum | `0`–`3` | See 7.4 |

**Payload size: 9 bytes. Total packet: 8 + 9 + 2 = 19 bytes.**

ACK and NACK share an identical payload layout and are distinguished by `msg_type` in the
header. `reason_code` is `0` (`ACCEPTED`) in an ACK and non-zero in a NACK.

---

## 7. Enumerations

### 7.1 Message Type (`msg_type`)

| Value | Name | Direction | Payload size |
| ----: | ---- | --------- | -----------: |
| 1 | `TELEMETRY` | `uav-sim` → `gcs` | 33 |
| 2 | `COMMAND` | `gcs-cmd` → `uav-sim` | 17 |
| 3 | `HEARTBEAT` | `gcs` → `uav-sim` | 0 |
| 4 | `ACK` | `uav-sim` → `gcs-cmd` | 9 |
| 5 | `NACK` | `uav-sim` → `gcs-cmd` | 9 |

Value `0` is not assigned, so an all-zero datagram is rejected by the message-type check.

### 7.2 Flight Mode (`flight_mode`)

| Value | Name | Flying mode |
| ----: | ---- | ----------- |
| 0 | `BOOT` | no |
| 1 | `PREFLIGHT` | no |
| 2 | `ARMED` | yes |
| 3 | `AUTO` | yes |
| 4 | `RTL` | yes |
| 5 | `LANDING` | yes |
| 6 | `LANDED` | no |

Flying-mode classification per `SIM-004`.

### 7.3 Command Type (`cmd_type`)

| Value | Name | param1 | param2 | param3 |
| ----: | ---- | ------ | ------ | ------ |
| 0 | `ARM` | unused (`0`) | unused (`0`) | unused (`0`) |
| 1 | `DISARM` | unused (`0`) | unused (`0`) | unused (`0`) |
| 2 | `SET_MODE` | target mode (`0`–`6`) | unused (`0`) | unused (`0`) |
| 3 | `GOTO_WAYPOINT` | latitude, ×10⁷ | longitude, ×10⁷ | altitude AMSL, ×100 |
| 4 | `RTL` | unused (`0`) | unused (`0`) | unused (`0`) |

Waypoint parameters use the same scale factors as the corresponding telemetry fields so that
a single conversion routine serves both directions.

### 7.4 NACK Reason Code (`reason_code`)

| Value | Name | Source requirement |
| ----: | ---- | ------------------ |
| 0 | `ACCEPTED` | `CMD-001` (ACK only) |
| 1 | `UNKNOWN_COMMAND` | `CMD-004` |
| 2 | `ILLEGAL_TRANSITION` | `SIM-001` |
| 3 | `STALE_SEQUENCE` | `CMD-006` |

### 7.5 GPS Fix Type (`gps_fix_type`)

| Value | Name | Meaning |
| ----: | ---- | ------- |
| 0 | `NO_FIX` | No position solution |
| 1 | `FIX_2D` | Horizontal position only |
| 2 | `FIX_3D` | Horizontal and vertical position |
| 3 | `FIX_RTK` | Real-time kinematic, centimetre-level |

---

## 8. Design Decisions

### 8.1 Field ordering

Fields are ordered largest-first within each payload. On the wire this has no effect on size,
because explicit byte-by-byte serialisation inserts no padding. The ordering exists so that the
corresponding in-memory structures are naturally compact under the platform ABI, and so that a
hex dump is readable in aligned groups.

### 8.2 Scaled integers versus floating point

All physical quantities are transmitted as scaled integers rather than IEEE 754 floats. Integer
encoding is bit-exact across compilers and architectures, requires no assumption about
floating-point representation, and permits explicit range validation on the wire value. The cost
is a fixed resolution per field, chosen below to exceed the precision the underlying quantity can
meaningfully carry.

| Field | Encoding | Scale | Resolution | Rationale |
| ----- | -------- | ----- | ---------- | --------- |
| latitude / longitude | `int32_t` | ×10⁷ | ~1.1 cm | Sub-metre positioning; ±1.8×10⁹ fits `int32_t` (max 2.147×10⁹) |
| altitude AMSL | `int32_t` | ×100 | 1 cm | Adequate for altitude hold; finer would be false precision |
| ground / vertical speed | `int16_t`, `uint16_t` | ×100 | 0.01 m/s | Below the noise floor of any real airspeed sensor |
| heading, roll, pitch | `int16_t`, `uint16_t` | ×100 | 0.01° | Smooth attitude display without float encoding |
| battery voltage | `uint16_t` | ×1000 | 1 mV | Cell-level voltage monitoring resolution |
| battery percentage | `uint8_t` | none | 1% | Sub-percent battery estimates are not physically meaningful |

### 8.3 Timestamp representation

`timestamp_ms` is milliseconds since vehicle boot, encoded as `uint32_t`, wrapping after
`2³² / 1000 / 86400 ≈ 49.7 days` of continuous operation. No plausible flight approaches this,
so wraparound handling is out of scope. Time since boot is used rather than Unix epoch
milliseconds because the vehicle has no wall-clock source, and epoch milliseconds would require
`uint64_t`, doubling the field width for information the vehicle does not possess.

A consequence is that absolute one-way latency cannot be computed, because vehicle and ground
clocks share no epoch. `OPS-001` therefore records a ground receive timestamp alongside the
vehicle timestamp; the pair yields inter-arrival jitter and relative timing, which is what the
performance analysis requires. Measuring true one-way latency would require clock
synchronisation, which is outside the scope of this project.

### 8.4 Fixed versus variable payload

Each message type has a fixed payload layout; variability exists only between types, resolved by
`msg_type` in the common header. This permits the decoder to validate the declared
`payload_len` against the expected length for the type before interpreting any payload byte.
The alternative, a type-length-value payload, would be more extensible but would require the
parser to iterate over untrusted length fields, enlarging the attack surface that `SYS-004`
constrains.

### 8.5 Reserved space and extensibility

Byte 7 of the header is reserved. Encoders write `0x00`; decoders accept any value and ignore it.
This asymmetry is deliberate: a version 1 decoder that rejected non-zero reserved bytes would
make the field unusable by a future version 2 sender, defeating its purpose. It is a considered
exception to the system's general posture of rejecting unexpected input — the field is defined as
carrying no meaning in version 1, so skipping it is safe. `status_flags` bits 1–7 are reserved on
the same terms.

### 8.6 Frame synchronisation

No sync or magic byte is present. Frame synchronisation exists to locate message boundaries in a
continuous byte stream; UDP is datagram-oriented and delivers whole messages, so a packet always
begins at offset 0. The version and message-type fields together reject foreign traffic on a
shared port at negligible cost.

### 8.7 Decode ordering constraint

`payload_len` determines the CRC offset, so it must be validated before the CRC can be located.
The decoder therefore performs checks in a fixed order:

1. Reject if received length < 10 bytes (minimum packet: 8 header + 0 payload + 2 CRC)
2. Reject if `version != 1` (`FMT-001`)
3. Reject if `msg_type` is not 1–5 (`FMT-006`)
4. Reject if `8 + payload_len + 2 != bytes_received` (`FMT-002`)
5. Reject if `payload_len` does not match the expected length for `msg_type`
6. Reject if computed CRC over bytes `0 .. 7 + payload_len` != transmitted CRC (`FMT-003`)
7. Only then interpret payload fields

Performing step 6 before step 4 would compute a CRC offset from an unvalidated length field and
read beyond the supplied buffer, violating `SYS-004`.