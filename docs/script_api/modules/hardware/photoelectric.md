# Handle `photoelectric`

One photoelectric sensor: calls a function once per object that passed the beam.

[Back to module `hardware`](README.md)

```c
import hardware as hw;

handle photo = hw.PE_SENSOR();   // created closed
```

## Attributes

| Attribute | Type | Meaning |
| :--- | :--- | :--- |
| `sensor` | `int` | Sensor number of the last successful `open()`, `0` before the first one. Read only |
| `min_present_ms` | `int` | The beam must stay blocked this long to start an object (default `30`) |
| `min_gap_ms` | `int` | The beam must stay clear this long to end an object (default `200`) |

- `min_present_ms` and `min_gap_ms` accept any `int >= 0`.
- They are given to the sensor by `open()`. Set them **before** `open()`: setting them while open is an error.

## Methods

| Method | Returns | Description |
| :--- | :--- | :--- |
| `open(int sensor, func on_object)` | `string` | Start the sensor. `on_object(int duration_ms)` is called once per object |
| `close()` | `string` | Stop the sensor and remove `on_object`. The handle can be opened again |

Methods returning `string` give `"OK"` or the error text.

## How an object is counted

| Beam | Result |
| :--- | :--- |
| Blocked shorter than `min_present_ms` | Ignored (noise) |
| Blocked at least `min_present_ms` | An object starts |
| Clear shorter than `min_gap_ms` during an object | Same object (for example a hole in the shoe) |
| Clear at least `min_gap_ms` | The object ends: `on_object(duration_ms)` is called |

- `duration_ms` is the time from the first to the last blocked sample of the object.
- `on_object` runs `min_gap_ms` after the object left the beam, not when it arrives.
- The sensor is sampled every 10 ms.

## Usage

```c
void on_object(int duration_ms) {
    print("Object: ", duration_ms, " ms");
}

photo.min_present_ms = 30;
photo.min_gap_ms     = 200;
print(photo.open(1, on_object));
while (true) {
    delay(1000);                 // on_object runs while the script waits
}
```

## Sensor and pin

- Sensor `n` uses the pin option `PHOTO<n>_IN` in the pin configuration. Only sensor `1` exists now.
- A sensor has one owner at a time: a second `open()` on the same sensor returns `"Photo sensor already started"`.
- The firmware treats a `LOW` input as "beam blocked" (`PHOTO_SENSOR_BLOCKED_LEVEL`), which matches the opto inputs of the PLC MINI board.

## Errors

| Case | Result |
| :--- | :--- |
| `sensor` out of range, `on_object` not a defined function | Raises |
| `open()` on an open handle, `close()` on a closed handle | Raises |
| Setting `min_present_ms` or `min_gap_ms` while open, a value `< 0` or not an `int`, setting `sensor` | Raises |
| Sensor used by another handle | `"Photo sensor already started"` |
| Pin not configured | `"Photo sensor has no pin configured"` |
| Firmware built without `USE_PHOTO_SENSOR` | `hw.PE_SENSOR` does not exist |

## Example

[`examples/hardware/photoelectric.ti`](../../../../examples/hardware/photoelectric.ti)
