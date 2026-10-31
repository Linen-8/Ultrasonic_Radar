/*
 * ==========================================================================
 *  ULTRASONIC RADAR - Arduino Uno / Nano - single-file build
 * ==========================================================================
 *
 *  180 degree sweep with obstacle alarm. One sketch, three sensors: pick ONE
 *  line in section 1 (SENSOR_TYPE) and wire the matching sensor.
 *
 *   - Servo steps across 0..180 degrees and pings at every step
 *   - Anything closer than ALARM_CM (15 cm) stops the sweep, sounds the
 *     buzzer, lights the red LED and shows distance + angle on the LCD
 *   - The sweep resumes only after ALARM_MIN_HOLD_MS (2 s) and the path
 *     stays clear for RELEASE_HOLD_MS (200 ms)
 *
 *  LCD flow:
 *    - "Ultrasonic Radar" for SPLASH_MS (3 s)
 *    - "Scanning...." while the servo sweeps
 *    - "DIST : nn cm" / "ANGLE: nnn deg" while the alarm is active
 *
 *  Why it stays stable:
 *    - Servo settles between steps (no lost steps, no random angles)
 *    - Ultrasonic readings are single pings (no median filter for max sensitivity)
 *    - Alarm triggers on the very first reading ≤ ALARM_CM
 *    - Release needs 200 ms of continuous clear readings (17 cm hysteresis)
 *    - No blocking delays in the control flow: loop() is a millis() state
 *      machine, so the buzzer and LCD never stall the sweep
 *
 *  Libraries (only the one you use is required):
 *    - Servo                  (bundled with the IDE)
 *    - LiquidCrystal I2C      by Frank de Brabander
 *    - VL53L0X                by Pololu   (SENSOR_TOF only)
 *
 *  Wiring and calibration: see README.md
 * ==========================================================================
 */

/* ==========================================================================
 *  1. CONFIG - pick the sensor
 * ========================================================================== */

#define SENSOR_ULTRASONIC 0
#define SENSOR_TOF        1
#define SENSOR_IR         2

#ifndef SENSOR_TYPE
  #define SENSOR_TYPE SENSOR_ULTRASONIC   // <-- change this line
#endif

/* ==========================================================================
 *  2. INCLUDES AND PINS
 * ========================================================================== */

#include <Wire.h>
#include <Servo.h>
#include <LiquidCrystal_I2C.h>

#if SENSOR_TYPE == SENSOR_TOF
  #include <VL53L0X.h>
#endif

const uint8_t PIN_SERVO  = 11;
const uint8_t PIN_BUZZER = 12;
const uint8_t PIN_LED    = 13;

#if SENSOR_TYPE == SENSOR_ULTRASONIC
  const uint8_t PIN_TRIG = 9;
  const uint8_t PIN_ECHO = 10;
#elif SENSOR_TYPE == SENSOR_IR
  const uint8_t PIN_IR = A0;
#endif

/* ==========================================================================
 *  3. PER-SENSOR SETTINGS
 * ========================================================================== */

#if SENSOR_TYPE == SENSOR_ULTRASONIC

  const uint8_t  ANGLE_STEP        = 2;
  const uint16_t STEP_INTERVAL_MS  = 60;    // >= HC-SR04's 60 ms cycle time
  const uint16_t MAX_RANGE_CM      = 150;
  const uint16_t ECHO_TIMEOUT_US   = 10000; // ~170 cm round trip
  const uint8_t  SAMPLES_PER_POINT = 1;     // single ping for maximum sensitivity
  const uint8_t  MIN_VALID_CM      = 5;     // closer than this = ringing / self-echo
  const char     SENSOR_NAME[]     = "HC-SR04";

#elif SENSOR_TYPE == SENSOR_TOF

  const uint8_t  ANGLE_STEP        = 3;
  const uint16_t STEP_INTERVAL_MS  = 40;    // > one 20 ms measurement
  const uint16_t MAX_RANGE_CM      = 120;
  const char     SENSOR_NAME[]     = "VL53L0X";

#else // SENSOR_IR

  const uint8_t  ANGLE_STEP       = 3;
  const uint16_t STEP_INTERVAL_MS = 40;
  const uint16_t MAX_RANGE_CM     = 80;

  // distance_cm = CAL_A * volts ^ CAL_B  (see README "Calibrating the IR")
  const float    CAL_A         = 27.86;
  const float    CAL_B         = -1.15;
  const uint8_t  IR_SAMPLES    = 8;
  const float    IR_MIN_VOLT   = 0.25;      // below this: nothing in range
  const uint16_t MIN_RANGE_CM  = 8;         // curve unusable closer
  const char     SENSOR_NAME[] = "GP2Y0A21";

#endif

/* ==========================================================================
 *  4. SHARED SETTINGS
 * ========================================================================== */

const uint8_t  ANGLE_MIN        = 0;
const uint8_t  ANGLE_MAX        = 180;

const uint16_t ALARM_CM         = 15;    // trigger distance
const uint16_t RELEASE_CM       = 17;    // hysteresis: must clear this far
const uint16_t ALARM_MIN_HOLD_MS = 2000;  // hold at least this long after a hit
const uint16_t RELEASE_HOLD_MS  = 200;   // ...and stay clear this long
const uint8_t  TRIGGER_SAMPLES  = 1;     // alarm on first hit for maximum sensitivity
const uint16_t ALARM_SAMPLE_MS  = 60;    // re-ping rate while stopped

const uint16_t SPLASH_MS            = 3000;
const bool     SPLASH_UNTIL_FIRST_ECHO = false;

const uint16_t LCD_INTERVAL_MS = 200;
const uint8_t  BEEP_ON_MS      = 60;
const uint8_t  BEEP_OFF_MS     = 90;
const bool     PASSIVE_BUZZER  = false;  // true for a passive buzzer
const uint16_t BUZZER_TONE_HZ  = 2200;

const uint16_t SERVO_MIN_US = 544;
const uint16_t SERVO_MAX_US = 2400;

const bool DEBUG_SERIAL = true;

/* ==========================================================================
 *  5. OBJECTS AND STATE
 * ========================================================================== */

LiquidCrystal_I2C lcd(0x27, 16, 2);   // 0x27 usual; try 0x3F if blank
Servo servo;

#if SENSOR_TYPE == SENSOR_TOF
  VL53L0X tof;
  bool sensorOk = false;
#endif

enum Mode { MODE_SPLASH, MODE_SWEEP, MODE_ALARM };

Mode     mode          = MODE_SPLASH;
uint32_t modeSince     = 0;
uint32_t lastStepAt    = 0;
uint32_t lastAlarmPing = 0;
uint32_t clearSince    = 0;
uint32_t lastLcdAt     = 0;
uint32_t buzzerAt      = 0;
uint32_t lastSplashPing = 0;
uint32_t lastAlarmDebug = 0;

bool buzzerOn         = false;
bool clearTimerActive = false;

uint8_t  angle      = ANGLE_MIN;
int8_t   direction  = 1;
uint16_t distanceCm = 0;   // 0 means "no echo"
uint8_t  closeCount = 0;

char lcdCache[2][17] = { "", "" };

/* ==========================================================================
 *  6. SETUP / LOOP
 * ========================================================================== */

void setup() {
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  digitalWrite(PIN_LED, LOW);

  if (DEBUG_SERIAL) {
    uint8_t resetFlags = MCUSR;        // POR / EXTR / BOR / WDR, read before clearing
    MCUSR = 0;
    Serial.begin(115200);
    Serial.print(F("radar: "));
    Serial.println(SENSOR_NAME);
    Serial.print(F("reset flags: 0x"));
    Serial.println(resetFlags, HEX);
  }

  servo.attach(PIN_SERVO, SERVO_MIN_US, SERVO_MAX_US);
  servo.write(90);                 // park in the middle during the splash

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Ultrasonic Radar"));   // exactly 16 chars, fills row 0
  lcd.setCursor(0, 1);
  lcd.print(F("                "));   // row 1 stays blank during the splash

#if SENSOR_TYPE == SENSOR_TOF
  sensorOk = sensorBegin();
  if (!sensorOk) {
    lcd.setCursor(0, 1);
    lcd.print(F(" ToF not found "));
    if (DEBUG_SERIAL) Serial.println(F("VL53L0X not detected - check SDA/SCL/5V"));
  }
#else
  sensorBegin();
#endif

  modeSince = millis();
}

void loop() {
  uint32_t now = millis();

  switch (mode) {
    case MODE_SPLASH: splashStep(now); break;
    case MODE_SWEEP:  sweepStep(now);  break;
    case MODE_ALARM:  alarmStep(now);  break;
  }

  digitalWrite(PIN_LED, mode == MODE_ALARM ? HIGH : LOW);
  serviceBuzzer(now);
  serviceLcd(now);
}

/* ==========================================================================
 *  7. STATE MACHINE
 * ========================================================================== */

/* Boot screen. Optionally waits for the first echo before sweeping. */
void splashStep(uint32_t now) {
  if (SPLASH_UNTIL_FIRST_ECHO && now - lastSplashPing >= 100) {
    lastSplashPing = now;
    int16_t d = readDistanceCm();
    if (d > 0) {
      startSweep(now, d);
      return;
    }
  }
  if (now - modeSince >= SPLASH_MS) startSweep(now, 0);
}

void startSweep(uint32_t now, int16_t initialDistance) {
  mode        = MODE_SWEEP;
  modeSince   = now;
  lastStepAt  = now;
  angle       = 90;          // start mid-travel: no 90->0 slam at sweep start
  direction   = 1;
  closeCount  = 0;
  distanceCm  = initialDistance > 0 ? (uint16_t)initialDistance : 0;
  servo.write(angle);

  lcd.clear();
  lcdCache[0][0] = '\0';
  lcdCache[1][0] = '\0';

  if (DEBUG_SERIAL) Serial.println(F("-- sweep start --"));
}

/* One servo step per STEP_INTERVAL_MS: ping, decide, then move. */
void sweepStep(uint32_t now) {
  if (now - lastStepAt < STEP_INTERVAL_MS) return;
  lastStepAt = now;

  int16_t d = readDistanceCm();
  distanceCm = d > 0 ? (uint16_t)d : 0;

  if (DEBUG_SERIAL) {
    Serial.print(F("A="));
    Serial.print(angle);
    Serial.print(F(" D="));
    if (d > 0) Serial.println(d); else Serial.println(F("--"));
  }

  if (d > 0 && (uint16_t)d <= ALARM_CM) {
    if (closeCount < 255) closeCount++;
  } else {
    closeCount = 0;
  }

  if (closeCount >= TRIGGER_SAMPLES) {
    enterAlarm(now);
    return;
  }

  advanceAngle();
}

/* Hold the angle, keep measuring, leave only after a stable clear. */
void alarmStep(uint32_t now) {
  if (now - lastAlarmPing < ALARM_SAMPLE_MS) return;
  lastAlarmPing = now;

  int16_t d = readDistanceCm();
  bool clear = (d < 0) || ((uint16_t)d > RELEASE_CM);

  if (DEBUG_SERIAL && now - lastAlarmDebug >= 500) {
    lastAlarmDebug = now;
    Serial.print(F("  hold A="));
    Serial.print(angle);
    Serial.print(F(" D="));
    if (d > 0) Serial.println(d); else Serial.println(F("--"));
  }

  // Minimum hold: stay stopped for ALARM_MIN_HOLD_MS even if the object is removed.
  if (now - modeSince < ALARM_MIN_HOLD_MS) {
    if (d > 0) distanceCm = (uint16_t)d;   // keep showing the latest distance
    return;
  }

  if (clear) {
    if (!clearTimerActive) {
      clearTimerActive = true;
      clearSince = now;
    } else if (now - clearSince >= RELEASE_HOLD_MS) {
      exitAlarm(now);
      return;
    }
  } else {
    clearTimerActive = false;
    distanceCm = (uint16_t)d;
  }
}

void enterAlarm(uint32_t now) {
  mode             = MODE_ALARM;
  modeSince        = now;
  lastAlarmPing    = now;
  clearTimerActive = false;
  buzzerOn         = true;
  buzzerAt         = now;
  outputBuzzer(true);

  if (DEBUG_SERIAL) {
    Serial.print(F("!! ALARM at "));
    Serial.print(angle);
    Serial.print(F(" deg, "));
    Serial.print(distanceCm);
    Serial.println(F(" cm"));
  }
}

void exitAlarm(uint32_t now) {
  mode             = MODE_SWEEP;
  modeSince        = now;
  lastStepAt       = now;
  closeCount       = 0;
  clearTimerActive = false;
  buzzerOn         = false;
  outputBuzzer(false);

  advanceAngle();          // resume from here, same direction

  if (DEBUG_SERIAL) Serial.println(F("-- clear, sweep resumes --"));
}

/* Bounce between 0 and 180 and write the new angle to the servo. */
void advanceAngle() {
  if (direction > 0) {
    if (angle >= ANGLE_MAX - ANGLE_STEP) {
      angle = ANGLE_MAX;
      direction = -1;
    } else {
      angle += ANGLE_STEP;
    }
  } else {
    if (angle <= ANGLE_MIN + ANGLE_STEP) {
      angle = ANGLE_MIN;
      direction = 1;
    } else {
      angle -= ANGLE_STEP;
    }
  }
  servo.write(angle);
}

/* ==========================================================================
 *  8. SENSOR LAYER - only the selected implementation is compiled
 * ========================================================================== */

bool sensorBegin() {
#if SENSOR_TYPE == SENSOR_ULTRASONIC
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  digitalWrite(PIN_TRIG, LOW);
  return true;

#elif SENSOR_TYPE == SENSOR_TOF
  Wire.begin();
  tof.setTimeout(500);
  if (!tof.init()) return false;
  tof.setMeasurementTimingBudget(20000);   // 20 ms, ~1.2 m
  tof.startContinuous();
  return true;

#else // SENSOR_IR
  pinMode(PIN_IR, INPUT);
  return true;
#endif
}

#if SENSOR_TYPE == SENSOR_ULTRASONIC
/* Single HC-SR04 ping, rounded to whole cm. Returns -1 on timeout. */
int16_t pingOnce() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  unsigned long us = pulseIn(PIN_ECHO, HIGH, ECHO_TIMEOUT_US);
  if (us == 0) return -1;

  // cm = us / 58.3 (speed of sound ~343 m/s at 20 C), rounded to nearest
  int16_t cm = (int16_t)((us * 10 + 291) / 583);
  if (cm < MIN_VALID_CM || (uint16_t)cm > MAX_RANGE_CM) return -1;
  return cm;
}
#endif

/* Latest distance in whole cm, or -1 when there is no usable reading. */
int16_t readDistanceCm() {
#if SENSOR_TYPE == SENSOR_ULTRASONIC

  int16_t v[SAMPLES_PER_POINT];

  for (uint8_t i = 0; i < SAMPLES_PER_POINT; i++) {
    v[i] = pingOnce();
    delay(25);                      // HC-SR04 needs >= 60 ms between pings;
                                    // 3 pings x 25 ms = 75 ms per step
  }

  // insertion sort (tiny array)
  for (uint8_t i = 1; i < SAMPLES_PER_POINT; i++) {
    int16_t key = v[i];
    int8_t j = i - 1;
    while (j >= 0 && v[j] > key) {
      v[j + 1] = v[j];
      j--;
    }
    v[j + 1] = key;
  }

  return v[SAMPLES_PER_POINT / 2];

#elif SENSOR_TYPE == SENSOR_TOF

  if (!sensorOk) return -1;

  uint16_t mm = tof.readRangeContinuousMillimeters();

  if (tof.timeoutOccurred()) {
    tof.startContinuous();          // resync after a timeout
    return -1;
  }

  if (mm < 30) return -1;           // < 3 cm: sensor blind spot
  uint16_t cm = (mm + 5) / 10;      // mm -> cm, rounded
  if (cm > MAX_RANGE_CM) return -1;

  return (int16_t)cm;

#else // SENSOR_IR

  uint16_t raw = 0;
  for (uint8_t i = 0; i < IR_SAMPLES; i++) {
    raw += analogRead(PIN_IR);
    delayMicroseconds(200);
  }
  float volts = (raw / (float)IR_SAMPLES) * 5.0f / 1023.0f;

  if (volts < IR_MIN_VOLT) return -1;          // nothing within ~80 cm

  float cm = CAL_A * pow(volts, CAL_B);

  if (cm < MIN_RANGE_CM || cm > MAX_RANGE_CM) return -1;
  return (int16_t)(cm + 0.5f);                 // round to nearest cm

#endif
}

/* ==========================================================================
 *  9. OUTPUT - buzzer and LCD
 * ========================================================================== */

void outputBuzzer(bool on) {
  if (PASSIVE_BUZZER) {
    if (on) tone(PIN_BUZZER, BUZZER_TONE_HZ);
    else    noTone(PIN_BUZZER);
  } else {
    digitalWrite(PIN_BUZZER, on ? HIGH : LOW);
  }
}

/* On/off pattern driven by millis(), so the sweep never waits on it. */
void serviceBuzzer(uint32_t now) {
  if (mode != MODE_ALARM) {
    if (buzzerOn) {
      buzzerOn = false;
      outputBuzzer(false);
    }
    return;
  }

  uint16_t interval = buzzerOn ? BEEP_ON_MS : BEEP_OFF_MS;
  if (now - buzzerAt >= interval) {
    buzzerAt = now;
    buzzerOn = !buzzerOn;
    outputBuzzer(buzzerOn);
  }
}

void setLcdLine(uint8_t row, const char *text) {
  char padded[17];
  snprintf(padded, sizeof(padded), "%-16.16s", text);

  if (strcmp(padded, lcdCache[row]) == 0) return;   // only touch I2C on change

  strcpy(lcdCache[row], padded);
  lcd.setCursor(0, row);
  lcd.print(padded);
}

void serviceLcd(uint32_t now) {
  if (now - lastLcdAt < LCD_INTERVAL_MS) return;
  lastLcdAt = now;

  char line[17];

#if SENSOR_TYPE == SENSOR_TOF
  if (!sensorOk) {
    setLcdLine(0, "ToF not found");
    setLcdLine(1, "Check I2C 0x29");
    return;
  }
#endif

  if (mode == MODE_ALARM) {
    if (distanceCm > 0) {
      snprintf(line, sizeof(line), "DIST : %3u cm", (unsigned)distanceCm);
    } else {
      snprintf(line, sizeof(line), "DIST : --- cm");
    }
    setLcdLine(0, line);

    snprintf(line, sizeof(line), "ANGLE: %3u deg", (unsigned)angle);
    setLcdLine(1, line);

  } else if (mode == MODE_SPLASH) {
    // static boot text is drawn once in setup()

  } else {
    setLcdLine(0, "  Scanning....  ");
    setLcdLine(1, "");
  }
}
