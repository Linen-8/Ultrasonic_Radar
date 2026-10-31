# Ultrasonic Radar — Arduino Uno / Nano

A 180° scanning radar in a single sketch. One file (`radar.ino`) supports
three sensors — HC-SR04 ultrasonic, VL53L0X time-of-flight, or SHARP
GP2Y0A21YK infrared. Anything closer than **15 cm** halts the sweep, sounds
the buzzer, lights the red LED and reports the angle and distance on a 16×2
I²C LCD. The sweep resumes only after the path has been clear for 200 ms.

---

## 1. Files

```
ultrasonic_radar/
├── radar.ino     ← the whole project: config, state machine, sensors, UI
└── README.md
```

Everything is in `radar.ino` — there are no other source files.

**Folder name:** the Arduino IDE requires the folder to match the sketch
name. If you open `radar.ino` while the folder is called `ultrasonic_radar`,
the IDE offers to create a `radar/` folder and move the file into it — click
OK. Or simply rename this folder to `radar`. Either way the code and this
README stay together.

---

## 2. Pick the sensor

At the top of `radar.ino`:

```cpp
#define SENSOR_ULTRASONIC 0
#define SENSOR_TOF        1
#define SENSOR_IR         2

#define SENSOR_TYPE SENSOR_ULTRASONIC   // <-- change this line
```

| Setting | Sensor | Range | Step | Library needed |
|---------|--------|-------|------|----------------|
| `SENSOR_ULTRASONIC` | HC-SR04 | 2–150 cm (alarm at 15) | 2° / 30 ms | none |
| `SENSOR_TOF` | VL53L0X | 3–120 cm | 3° / 40 ms | VL53L0X |
| `SENSOR_IR` | GP2Y0A21YK | 8–80 cm | 3° / 40 ms | none |

Only the selected sensor's code is compiled and only its library is
included, so you can build the ultrasonic or IR version without installing
the ToF library.

Compiled sizes for the Nano:

| Sensor | Flash | RAM |
|---|---|---|
| HC-SR04 | 9,488 B (30%) | 647 B (31%) |
| VL53L0X | 12,994 B (42%) | 690 B (33%) |
| GP2Y0A21 | 11,218 B (36%) | 647 B (31%) |

---

## 3. Bill of materials

| Qty | Part | Notes |
|----:|------|-------|
| 1 | Arduino Uno or Nano | ATmega328P; Nano clones are fine |
| 1 | HC-SR04 **or** VL53L0X **or** GP2Y0A21YK | pick one |
| 1 | SG90 / MG90S servo | MG90S recommended (metal gears) |
| 1 | 16×2 LCD with I²C backpack | PCF8574, address `0x27` or `0x3F` |
| 1 | Active buzzer (5 V) | passive supported via `PASSIVE_BUZZER` |
| 1 | Red LED + 220 Ω resistor | optional: D13's on-board LED lights too |
| 1 | 1000 µF / 16 V electrolytic | across the servo supply |
| 1 | 5 V buck converter, 2–3 A | MP1584EN or similar |
| 2 | 18650 Li-ion cells + holder | protected cells or a 2S BMS |
| 1 | 2 A fuse + holder | pack protection |
| — | Jumper wires, breadboard / perfboard | |

Optional: 2S balance charger, 5.5 mm barrel jack, small switch.

---

## 4. Hardware and wiring

Five parts share the board: the **servo**, the **HC-SR04**, the **16×2 I²C
LCD**, the **buzzer** and the **status LED**. Pin numbers are identical on
the Uno and the Nano — use the silk labels (`D9`, `D10`, …), not the physical
pin position, because the Nano's header order does not match the Uno's.

### 4.1 Pin map at a glance

| Arduino pin | Connected to | Direction | Used when |
|-------------|--------------|-----------|-----------|
| **D9** | HC-SR04 `TRIG` | output | ultrasonic build |
| **D10** | HC-SR04 `ECHO` | input | ultrasonic build |
| **D11** | servo signal (orange/white) | output (PWM) | always |
| **D12** | buzzer `+` | output | always |
| **D13** | LED anode via 220 Ω | output | always |
| **A0** | GP2Y0A21 `VO` | analogue in | IR build only |
| **A4** | LCD `SDA` (+ VL53L0X `SDA`) | I²C | always |
| **A5** | LCD `SCL` (+ VL53L0X `SCL`) | I²C | always |
| **5V** | LCD `VCC`, HC-SR04 `VCC` | supply | from the buck rail |
| **GND** | every module's ground | — | common ground |

```
 5 V rail ──┬── servo V+  (1000 µF to GND right at the servo)
            ├── HC-SR04 VCC
            ├── LCD VCC
            └── Arduino 5V pin

 GND ───────┴── servo, HC-SR04, LCD, buzzer, LED, Arduino   (one node)

 D9  → HC-SR04 TRIG          A4 ↔ LCD SDA
 D10 ← HC-SR04 ECHO          A5 ↔ LCD SCL
 D11 → servo signal          D12 → buzzer +
 D13 → 220 Ω → LED anode     LED cathode → GND
```

### 4.2 Servo — SG90 / MG90S

| Spec | Value |
|------|-------|
| Operating voltage | 4.8–6.0 V (5 V nominal) |
| Idle current | ~10 mA |
| Moving current | 100–250 mA (SG90) |
| Stall current | ~700 mA (SG90), >1 A (MG90S) |
| Torque | 1.8 kg·cm (SG90), 2.2 kg·cm (MG90S) |
| Control | 50 Hz PWM, 1–2 ms pulse |
| Travel used | 0–180° |

| Servo wire | Colour (typical) | Connect to |
|------------|------------------|------------|
| Signal | orange / yellow / white | **D11** |
| V+ | red | **5 V buck rail** — never the Arduino 5V pin |
| GND | brown / black | GND (common) |

```
   buck 5.1 V ──┬───────────────► servo red
                │
             1000 µF
                │
   GND ─────────┴───────────────► servo brown
   D11 ─────────────────────────► servo orange
```

- The four 100 µF / 25 V capacitors maintain the voltage drop in the energy supplied by the Arduino Nano.
- Use **2 capacitors in parallel** with the servo motor, **1 capacitor** with the LCD, and **1 capacitor** with the ultrasonic sensor.
- If not all 4 capacitors are used, it could cause malfunctioning, such as the servo getting stuck while sweeping, false triggering of the ultrasonic sensor, or the LCD backlight flickering / barely visible display.
- The signal wire carries no current; it can be thin and long. The power
  wires cannot.
- Wire colours vary between clones. If the sweep runs backwards or not at
  all, the signal and ground wires are swapped — check the label on the
  connector, not the colour.
- Code trims the endpoints with `SERVO_MIN_US = 544` and
  `SERVO_MAX_US = 2400` (see §9.1).

### 4.3 HC-SR04 ultrasonic sensor

| Spec | Value |
|------|-------|
| Operating voltage | 5 V (not 3.3 V) |
| Current | ~15 mA |
| Frequency | 40 kHz |
| Range | 2–400 cm (sketch uses 2–150 cm) |
| Beam | ~15° cone |
| Accuracy | ±3 mm typical |
| Interface | 2 digital pins, 5 V logic |

| HC-SR04 pin | Arduino pin | Notes |
|-------------|-------------|-------|
| `VCC` | **5 V** | from the buck rail, not the 3V3 pin |
| `TRIG` | **D9** | 10 µs pulse, generated by the sketch |
| `ECHO` | **D10** | pulse width = round-trip time |
| `GND` | GND | common ground |

```
   HC-SR04                      Arduino
   ┌─────────┐
   │  VCC ●──┼────────────────── 5V
   │ TRIG ●──┼────────────────── D9
   │ ECHO ●──┼────────────────── D10
   │  GND ●──┼────────────────── GND
   └─────────┘
     (most modules: VCC TRIG ECHO GND left→right)
```

- **Pin order varies on clones.** Some are `VCC ECHO TRIG GND`. With TRIG
  and ECHO swapped the sketch prints `D=--` forever, because the pulse goes
  into the wrong pin. This is the most common wiring mistake on this project.
- Do not run it from 3.3 V — the transmitter is weak and the echo never
  comes back.
- The 2 cm dead zone is physical: the transducer is still ringing. The
  sketch rejects readings below 2 cm.
- Mount it on the servo horn with the two cans **clear of the bracket**. A
  bracket edge inside the cone shows up as a permanent object at a fixed
  angle.
- Soft, angled or cloth targets scatter the echo. Expect dropouts on those;
  the median-of-3 filter hides most of them.
- No level shifter is needed on a 5 V Arduino: ECHO is 5 V logic, which is
  exactly what the ATmega328P input wants.
- Keep TRIG/ECHO wires short (< 50 cm) and route them away from the servo
  power pair.

### 4.4 LCD 16×2 character with I²C backpack (PCF8574)

| Spec | Value |
|------|-------|
| Display | 16 characters × 2 rows, HD44780 controller |
| Backpack | PCF8574 I²C expander, 4 data pins |
| Operating voltage | 5 V |
| Current | ~20 mA with backlight, ~2 mA without |
| I²C address | `0x27` default, `0x3F` on many modules |
| Bus speed | 100 kHz (default `Wire`) |
| Wires needed | **4** |

| Backpack pin | Arduino pin | Notes |
|--------------|-------------|-------|
| `GND` | GND | common ground |
| `VCC` | **5 V** | from the buck rail |
| `SDA` | **A4** | shared I²C data |
| `SCL` | **A5** | shared I²C clock |

```
   LCD backpack              Arduino
   ┌────────────┐
   │  GND ●─────┼──────────── GND
   │  VCC ●─────┼──────────── 5V
   │  SDA ●─────┼──────────── A4
   │  SCL ●─────┼──────────── A5
   └────────────┘
     (order on the header is often GND VCC SDA SCL — read the silkscreen)
```

- **Backlight on, no characters** → turn the blue contrast potentiometer on
  the backpack until the top row of dark blocks appears, then back it off
  until the blocks just disappear. This is the number-one "dead LCD" cause.
- **Nothing at all** → VCC/GND, or the backlight jumper on the backpack is
  removed.
- **Garbage or half-written text** → wrong address. The sketch uses
  `0x27`; if your module is `0x3F`, change
  `LiquidCrystal_I2C lcd(0x27, 16, 2);` to `lcd(0x3F, 16, 2)`.
- Long I²C wires are unreliable. Keep them under ~20 cm, or add 4.7 kΩ
  pull-ups from SDA and SCL to 5 V.
- The backpack plugs onto the LCD's 16-pin header. If you soldered it
  yourself, all 16 pins must be connected and pin 1 aligned. A single dry
  joint gives a display that lights up but shows blocks or half characters.
- The LCD and the VL53L0X share A4/A5 without conflict: addresses `0x27`
  and `0x29`.

### 4.5 Buzzer

| Spec | Active buzzer | Passive buzzer |
|------|---------------|----------------|
| Drive | DC on/off | square wave (needs `tone()`) |
| Current | ~25–30 mA at 5 V | ~20–30 mA |
| Sketch setting | `PASSIVE_BUZZER = false` | `PASSIVE_BUZZER = true` |
| Detection | sealed top, DC makes it sound | open top, DC makes a click only |

| Buzzer pin | Arduino pin | Notes |
|------------|-------------|-------|
| `+` (long leg / red) | **D12** | driven by `digitalWrite()` or `tone()` |
| `−` (short leg / black) | GND | common ground |

```
   D12 ──────► buzzer +   (red)
   GND ──────► buzzer −   (black)
```

- Polarity matters on a magnetic buzzer. Reversed, it is silent or very
  quiet — it will not be damaged.
- **Active** is the default and what this sketch assumes: the sketch simply
  switches D12 on and off in a 60 ms / 90 ms pattern. No `tone()` needed.
- **Passive** needs a frequency. Set `PASSIVE_BUZZER = true` and the sketch
  uses `tone(D12, 2200)` instead. A passive buzzer driven with plain DC only
  clicks.
- If it is too loud, put 100–220 Ω in series with `+`. That also drops the
  current below the 40 mA absolute maximum of a single pin, with margin.
- The buzzer is pulsed from `millis()`, never with `delay()`, so it cannot
  stall the sweep (§7).
- Do not power a large 12 V buzzer from the 5 V rail — it needs its own
  driver transistor. The 5 V magnetic types in the BOM are fine directly.

### 4.6 Status LED

| Spec | Value |
|------|-------|
| Type | 5 mm red, 1.9–2.1 V forward drop |
| Current | ~13 mA with 220 Ω at 5 V |
| Resistor | 220 Ω (150–330 Ω all work) |
| Behaviour | on only during `MODE_ALARM` |

| LED | Connect to |
|-----|------------|
| Anode (long leg) | **D13** through the 220 Ω resistor |
| Cathode (short leg, flat side) | GND |

```
   D13 ──[220 Ω]──►|── GND
                  LED
```

- Resistor on either side of the LED is fine; it must be in series, never
  across the supply.
- Backwards, it simply does not light. It will not be damaged by 5 V through
  220 Ω.
- On the Nano, **D13 also drives the on-board LED**, so the external LED is
  optional — the board's own LED blinks with the alarm.
- D13 is also the SPI clock pin, but nothing else in this project uses SPI,
  so there is no conflict.

### 4.7 Alternative sensors

Both are drop-in replacements — same sketch, one line changed.

**VL53L0X (`SENSOR_TOF`)**

| VL53L0X | Arduino |
|---------|---------|
| `VIN` | 5 V |
| `GND` | GND |
| `SDA` | A4 |
| `SCL` | A5 |
| `XSHUT` | leave unconnected |

3–120 cm, 25° field of view, 1 mm resolution, I²C address `0x29`. Shares the
bus with the LCD without conflict. Mount it on the horn with flexible wires.

**GP2Y0A21YK (`SENSOR_IR`)**

| GP2Y0A21 | Arduino |
|----------|---------|
| `VCC` (red) | 5 V |
| `GND` (black) | GND |
| `VO` (white/yellow) | A0 |

8–80 cm, analogue output, ~15° effective beam. Keep it out of direct
sunlight — ambient IR shifts the reading. Needs the `CAL_A` / `CAL_B`
curve in §9.4.

### 4.8 Power distribution

```
 2S Li-ion pack            buck converter               5 V rail
 7.4 V nominal   ──────►   set to 5.1 V, 2–3 A  ──────┬──► servo +5 V
 (6.0–8.4 V)                 + fuse 2 A               ├──► sensor VCC
                                                       ├──► LCD VCC
                                                       └──► Arduino 5V pin *
                                                             │
 GND ─────────────────── common ground ─────────────────────┘

 1000 µF / 16 V across the servo +5 V and GND, as close to the servo
 as possible. Stripe = negative.
```

\* Feeding the **5V pin** with a regulated 5.1 V bypasses the on-board
regulator and is the efficient choice. Two rules:

- **Never** connect USB and the 5 V rail at the same time.
- **Never** connect the 7.4 V pack to the 5V pin.

If you prefer to keep it foolproof, power the Arduino from USB (or a second
small 5 V source) and use the buck only for the servo.

**Nano note:** the same 5 V pin feed applies. Do not power the Nano through
`Vin` from the 2S pack: near empty (6.0 V) the regulator has barely 1 V of
headroom and the board can brown out mid-sweep. Older Nanos use Mini-B USB,
many current clones USB-C or micro-USB.

> The servo must not be powered from the Arduino's 5V pin. An SG90 pulls
> 100–250 mA while moving and ~700 mA stalled; an MG90S stalls above 1 A.
> Neither the Uno nor the Nano regulator (and no USB port) can supply that.
> The resulting brown-out is the usual cause of "the servo jumps to a random
> angle" and "the board resets when the servo moves".

### 4.9 Before you power up

Continuity check with a multimeter, power off:

1. **5 V rail to GND** — should read a few hundred ohms rising as the 1000 µF
   charges, never a dead short.
2. **Servo red to GND** — must go to the buck output, not to the Arduino 5V
   pin.
3. **Every module's GND** — all continuous with the pack negative.
4. **D9 → TRIG, D10 → ECHO** — and not the other way round.
5. **LCD SDA → A4, SCL → A5** — swapped SDA/SCL gives a blank display.
6. **Buzzer `+` → D12, `−` → GND** — reversed polarity is silent, not fatal.
7. **LED** — 220 Ω in series, cathode (flat side) to GND.
8. **Capacitor** — stripe on the 1000 µF to GND, not to +5 V.

Then power up with the servo unplugged first. If the LCD splash and Serial
output are correct, plug the servo in. A reset or LCD corruption the moment
the servo moves means the supply or ground is wrong, not the code.

---

## 5. Libraries

Installed in `~/Documents/Arduino/libraries`:

| Header | Library | Version | When |
|--------|---------|---------|------|
| `Wire.h` | Wire | 1.0 | built into the AVR core |
| `Servo.h` | Servo | 1.3.0 | always |
| `LiquidCrystal_I2C.h` | LiquidCrystal I2C (Frank de Brabander) | 1.1.2 | always |
| `VL53L0X.h` | VL53L0X (Pololu) | 1.3.1 | `SENSOR_TOF` only |

`Wire.h` ships with the `arduino:avr` board package — nothing to download.
To reinstall the rest on another machine:

```bash
arduino-cli lib install "Servo" "LiquidCrystal I2C" "VL53L0X"
```

Or use **Tools → Manage Libraries** and search each name. Restart the IDE if
it was open while the libraries were installed.

---

## 6. Upload

1. Set `SENSOR_TYPE` in `radar.ino` to match the sensor you wired.
2. **Tools → Board → Arduino Nano**
3. **Tools → Processor → `ATmega328P`**. If the upload fails with
   `avrdude: stk500_recv(): programmer is not responding`, choose
   `ATmega328P (Old Bootloader)` — most clones need it.
4. **Tools → Port** → your board (clones usually need the CH340 driver)
5. Open the sketch folder and press **Upload**
6. **Tools → Serial Monitor → 115200** shows one line per step

The Uno equivalent is **Board → Arduino Uno** with no processor option.

---

## 7. How it works

`loop()` is a single non-blocking state machine driven by `millis()`. The
control flow never waits on the buzzer, the LCD or the servo; the only
blocking call is the 3 ms gap between the ultrasonic pings inside one
measurement.

| State | What happens | Exit |
|-------|--------------|------|
| `SPLASH` | LCD shows **`Ultrasonic Radar`**, servo parked at 90° | after 3 s (or first echo with `SPLASH_UNTIL_FIRST_ECHO`) |
| `SWEEP` | LCD shows **`Scanning....`**; ping → evaluate → step on a fixed interval | 2 consecutive readings ≤ 15 cm |
| `ALARM` | Servo holds, buzzer 60 ms on / 90 ms off, LED on, LCD shows **`DIST : nn cm`** on row 0 and **`ANGLE: nnn deg`** on row 1 | at least 2 s after the hit, then clear (> 17 cm or no echo) for 200 ms |

**Display:** three screens, one per state.

| State | Row 0 | Row 1 |
|-------|-------|-------|
| `SPLASH` | `Ultrasonic Radar` | blank |
| `SWEEP` | `  Scanning....  ` | blank |
| `ALARM` | `DIST :  12 cm` | `ANGLE:  90 deg` |

The LCD is refreshed every 200 ms, and a line is only written when its text
changed. Rewriting the LCD every loop is what makes these displays flicker
and drags the sweep timing.

**Alarm sequence:** the sweep stops on the angle that triggered it, the red
LED on D13 goes solid, the buzzer pulses 60 ms on / 90 ms off, and the LCD
switches to the measured distance and the angle the servo is holding. The
distance shown is the filtered reading, rounded to whole cm. The sweep does
not resume for at least **2 s** after the hit, even if the object is removed
sooner; after that it waits for 200 ms of continuous clear readings.

**Timing:** 91 readings per sweep at 2°/30 ms ≈ 2.7–3.5 s (ultrasonic,
slower when nothing is in range because each ping waits out its 10 ms echo
timeout). The ToF and IR builds use 3°/40 ms, about 2.4 s per sweep.

**Filtering:** the ultrasonic build takes the **median of 3 pings** per
angle, so one spike cannot move the result. The alarm additionally needs
**2 consecutive** hits, so a single bad reading never stops the sweep. The
IR build averages 8 ADC samples; the ToF returns the sensor's latest
measurement.

**Hysteresis:** trigger below 15 cm, release only above 17 cm and only after
200 ms of continuous clear readings. The 2 cm gap stops chatter when an
object sits at the threshold; the 200 ms window stops a single dropout from
restarting the sweep into the same object.

**Buzzer:** an active buzzer is assumed. For a passive one set
`PASSIVE_BUZZER = true`; the sketch then uses `tone()` / `noTone()`.

**ToF health:** if `tof.init()` fails, the LCD shows `ToF not found` and the
state machine stays idle instead of alarming on garbage readings.

---

## 8. Tuning

All at the top of `radar.ino`.

| Constant | Default | Effect |
|----------|---------|--------|
| `SENSOR_TYPE` | `SENSOR_ULTRASONIC` | sensor selection |
| `ALARM_CM` | 15 | trigger distance |
| `RELEASE_CM` | 17 | must clear this far to release |
| `ALARM_MIN_HOLD_MS` | 2000 | minimum hold after a hit, even if cleared |
| `RELEASE_HOLD_MS` | 200 | how long it must stay clear |
| `TRIGGER_SAMPLES` | 2 | consecutive hits needed to alarm |
| `ALARM_SAMPLE_MS` | 40 | re-ping rate while stopped |
| `STEP_INTERVAL_MS` | 30 / 40 | servo settle time per step |
| `ANGLE_STEP` | 2 / 3 | degrees per step |
| `MAX_RANGE_CM` | 150 / 120 / 80 | farther reads as "---" |
| `SAMPLES_PER_POINT` | 3 | ultrasonic median filter (keep odd) |
| `ECHO_TIMEOUT_US` | 10000 | ultrasonic ping timeout |
| `SPLASH_MS` | 3000 | boot screen duration (3 s) |
| `SPLASH_UNTIL_FIRST_ECHO` | false | hold splash until something is seen |
| `SERVO_MIN_US` / `SERVO_MAX_US` | 544 / 2400 | servo endpoint trim |
| `LCD_INTERVAL_MS` | 200 | display refresh rate |
| `BEEP_ON_MS` / `BEEP_OFF_MS` | 60 / 90 | alarm beep pattern |
| `PASSIVE_BUZZER` | false | set true for a passive buzzer |
| `DEBUG_SERIAL` | true | per-step Serial output; false for the final build |
| `CAL_A` / `CAL_B` | 27.86 / -1.15 | IR curve (see below) |

If the sweep is too slow, raise `ANGLE_STEP` before shrinking
`STEP_INTERVAL_MS` — the settle time is what keeps the servo from losing
steps.

---

## 9. Calibration

### 9.1 Servo endpoints

Mount the horn so that at 90° the sensor points straight ahead. If 0° or
180° buzzes against a mechanical stop, narrow `SERVO_MIN_US` / `SERVO_MAX_US`
(e.g. 600 / 2300). If the sweep does not reach the ends, widen slightly
(500 / 2450). Change one at a time.

The angle shown is the commanded servo angle. A sensor mounted off-axis on
the horn adds a fixed offset to every reading — align the sensor face with
the servo pivot.

### 9.2 Detection distance

`ALARM_CM` is compared against the filtered reading in whole centimetres.
Measure the real trigger point with a ruler and adjust. Keep `RELEASE_CM`
about 2 cm above `ALARM_CM`.

The HC-SR04 has a ~2 cm dead zone and a ~15° cone; the ToF has a 25° field
of view and a 3 cm blind spot, so its angular reading is coarser.

### 9.3 LCD

Blank with the backlight on → turn the contrast potentiometer until the top
row of blocks appears, then back off. Blank with no backlight → check the
address (`0x27` vs `0x3F`) and SDA/SCL. Garbage characters → shorten the I²C
wires or add 4.7 kΩ pull-ups.

### 9.4 IR sensor constants (`SENSOR_IR` only)

The Sharp output is a curve, not a line:

```
distance_cm = CAL_A * volts ^ CAL_B        // defaults 27.86 and -1.15
```

1. Upload with `DEBUG_SERIAL = true` and open the Serial Monitor at 115200.
2. Put a flat **white** card at exactly 20 cm and note the distance `d1`;
   move it to 40 cm and note `d2` (or temporarily print `volts`).
3. Solve `CAL_B = ln(d1/d2) / ln(V1/V2)` and `CAL_A = d1 / V1^CAL_B`.
4. Put the constants back and re-check at 15, 25 and 35 cm.

Expect ±2–3 cm on white paper, worse on dark or shiny surfaces. The sensor
is usable from about 8 to 80 cm; the sketch rejects everything outside that.

---

## 10. Power supply notes

### 10.1 The recommended 2S design

Two 18650 cells in series give 7.4 V nominal (8.4 V full, 6.0 V empty).

- A 2–3 A buck set to **5.1 V** feeds the servo rail.
- The 1000 µF capacitor absorbs the servo's current spikes; without it the
  rail dips on every direction change and the Arduino can reset.
- The Arduino is fed from the same 5.1 V into the **5V pin**, or from USB
  while you develop.
- All grounds must be common. A missing ground between the buck and the
  Arduino is a very common cause of phantom behaviour.

**Runtime:** a 2×2600 mAh pack at ~150 mA average lasts well over 10 hours.
The servo dominates the consumption, not the Arduino.

**Cell protection:** protected cells or a 2S BMS with a 6.0 V cut-off, plus
a 2 A fuse. Never discharge below ~3.0 V/cell, never charge without a proper
2S balance charger, never charge unattended.

### 10.2 The parallel + boost alternative

Two cells in parallel (3.7 V) plus a 5 V boost also works and makes charging
simpler, but the boost must supply the full servo current from 3.7 V — about
1.5 A input for a 1 A load and 2.5 A for an MG90S stall. A small MT3608 is
not enough; use a 5 V/3 A boost and a 1S protection board. The 1000 µF
capacitor is still required.

### 10.3 Capacitor rating

1000 µF is fine and better than 100 µF here. Check:

- **Polarity** — stripe to GND; reversed electrolytics vent.
- **Voltage** — 16 V or 25 V (anything ≥ 10 V).
- **Ripple** — low-ESR is better, a generic part still helps.
- **Size** — a 1000 µF/16 V radial is roughly 10 × 20 mm.

---

## 11. Troubleshooting

| Symptom | Likely cause | Fix |
|---------|--------------|-----|
| Servo jumps to random angles / buzzes | Supply sag, missing cap, shared ground | Separate 5 V servo rail, 1000 µF at the servo, common GND, 2 A buck |
| Servo jitters at one angle | Step too small for the load, mechanical bind | `ANGLE_STEP` 3, `STEP_INTERVAL_MS` 35–40, free the horn |
| Board resets when the servo moves | Servo on the Arduino 5V pin / USB current limit | Move the servo to the buck rail |
| Buzzer chirps with nothing there | Single-ping noise | Keep median-of-3, raise `TRIGGER_SAMPLES` to 3 |
| Alarm never clears | Object inside the 15–17 cm hysteresis band | Increase `RELEASE_CM`, or move the object |
| Distance always `---` (ultrasonic) | TRIG/ECHO swapped, sensor unpowered, timeout too short | Check D9/D10, 5 V, raise `ECHO_TIMEOUT_US` |
| Distance reads about 2× the real value | Stray second echo, or the conversion was changed | Keep the `us * 10 / 583` formula; discard readings > `MAX_RANGE_CM` |
| Readings jump ±2 cm | Normal for HC-SR04 | Raise `SAMPLES_PER_POINT` to 5 |
| LCD blank, backlight on | Contrast pot | Turn until blocks appear |
| LCD blank, no backlight | Address / wiring | Try `0x3F`, check SDA=A4, SCL=A5 |
| LCD shows garbage | Loose or long I²C wires | Shorten, twist with GND, add 4.7 kΩ pull-ups |
| LCD says `ToF not found` | Sensor not answering on I²C | Check 5 V, GND, SDA/SCL, run an I²C scanner for `0x29` |
| IR readings drift | Ambient light / target colour | Shade the sensor, calibrate on the real target |
| Upload fails with `stk500_recv()` | Wrong processor option on a Nano clone | Tools → Processor → `ATmega328P (Old Bootloader)` |
| No serial port appears | CH340 driver missing | Install the CH340 driver, use a data cable |

---

## 12. Sensor comparison

| | HC-SR04 | VL53L0X (ToF) | GP2Y0A21YK (IR) |
|---|---|---|---|
| Range | 2–400 cm | 3 cm–1.2 m (2 m long-range) | 10–80 cm |
| Resolution | 1 cm | 1 mm | ~1 cm |
| Accuracy | ±1 cm typical | ±3% | ±10%, target-dependent |
| Beam | ~15° cone | 25° field of view | ~15° effective |
| Interface | 2 digital pins | I²C | 1 analogue pin |
| Soft / angled targets | poor (echo lost) | good | fair |
| Ambient light | immune | good | poor (sunlight) |
| Cost | ~$2 | ~$6 | ~$10 |
| Best for | general radar, long range | precise short range, soft targets | budget, fast response |

The 15 cm alarm sits comfortably inside all three ranges. The HC-SR04 is the
cheapest way to get a wide sweep; the ToF is the most accurate but its 25°
field of view blurs angular detail; the IR sensor is fast but needs shading
and calibration.

---

## 13. Why each choice was made

| Decision | Where |
|----------|-------|
| Distance rounded to whole cm | `pingOnce()`: `(us * 10 + 291) / 583` (us / 58.3, rounded) |
| "Ultrasonic Radar" boot screen, 3 s | `setup()` + `SPLASH_MS`; optional `SPLASH_UNTIL_FIRST_ECHO` |
| Servo no longer sticks at random angles | 2° steps, 30 ms settle, one write per step, plus the power fixes |
| Sensor noise no longer triggers the buzzer | median of 3 + 2 consecutive hits + 200 ms release hold |
| Buzzer no longer blocks the sweep | `serviceBuzzer()` toggles on `millis()`, no buzzer `delay()` |
| LCD no longer flickers | 200 ms refresh, only changed lines are written |
| Alarm survives a single bad echo | release timer resets on any close reading |
| Whole loop is non-blocking | `switch (mode)` state machine in `loop()` |
| One file, three sensors | `SENSOR_TYPE` + conditional includes and sensor layer |

---

## 14. Test checklist

1. Power up: LCD shows `Ultrasonic Radar` for 3 s, then `Scanning....` as
   the servo starts.
2. Connect the servo: the horn sweeps smoothly to 180° and back.
3. Hand at 50 cm: no alarm.
4. Hand at 10 cm: sweep stops at that angle, LED on solid, buzzer beeps,
   LCD shows `DIST : nn cm` and `ANGLE: nnn deg`.
5. Remove the hand: the sweep stays stopped for ~2 s, then resumes from the
   same angle and the LCD returns to `Scanning....`.
6. Hold a hand at 16 cm: alarm stays on (inside the hysteresis band). Move to
   19 cm: sweep resumes.
7. Wave a hand quickly across the beam: no false alarm (2-hit requirement).
8. Run for 10 minutes: no resets, no servo drift, LCD stable.
9. Measure the 5 V rail while the servo sweeps: it should stay above ~4.8 V.
