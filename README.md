## Hardware and setup

- Connect the probes to the pins below and provide any required signal conditioning. Probe voltages must be safe for the ESP32.
- Using platformio

| Input | ESP32 pin | Default interval | Notes |
|---|---:|---:|---|
| pH | GPIO 33 | 15 minutes | Currently linear ADC-to-pH mapping |
| Temperature | GPIO 32 | 30 seconds | Uses a simple analog voltage conversion; |
| EC | GPIO 34 | 5 minutes | Probe reader is not implemented yet, so the reading is unavailable. |
| Water level | GPIO 35 | 1 second | Reports `1` when the digital input is HIGH and `0` otherwise. |

## Bluetooth commands

Commands can be sent as a number alone for queries, or as a number followed by `:` (or a space) and a value for updates.
| Command | Action |
|---|---|
| `200`-`203` | Read pH, temperature, EC, or water level, respectively. |
| `210`-`213` | Read the corresponding sensor's sampling interval in seconds. |
| `500` | Show all readings, targets, and intervals. |
| `101`-`104` followed by a value | Set the target for pH, temperature, EC, or water level, respectively. |
| `301`-`304` followed by seconds | Set the sampling interval for pH, temperature, EC, or water level, respectively. |

Examples:

```text
200
101:7.6
301:900
500
```

Targets are constrained to pH `0`-`14`, temperature `0`-`100`, EC `0`-`1000`, and water level `0` or `1`. Sampling intervals are constrained by sensor: pH `60`-`86400` seconds, temperature `1`-`86400` seconds, EC `10`-`86400` seconds, and water level `1`-`3600` seconds.

## Current behavior and limitations
- The device periodically samples the four configured inputs and prints a reading over USB serial when it first becomes available or changes beyond its reporting threshold.
- It compares each available reading with its target and prints whether it is below, above, or within tolerance. This is monitoring only; no pump, heater, or other actuator control is implemented.
- Target values and sampling intervals are held in memory and return to defaults after a restart.
- The EC reader currently reports unavailable. The displayed pH and temperature conversions are basic placeholders and need calibration for the actual probes and circuitry.
