// RPM thresholds
const unsigned long GREEN_THRESHOLD = 4900;
const unsigned long YELLOW_THRESHOLD = 5500;
const unsigned long RED_THRESHOLD = 5900;
const unsigned long FLASH_THRESHOLD = 6200;

// How many cylinders?
const unsigned int PULSE_PER_REV = 6;
// used to average over the last X events - increase if not smooth
const unsigned int HISTORY_LENGTH = 48;

// GPIO pins
const byte TACH_PIN = 2;                // D4
const byte GREEN_PIN = 0;               // D3
const byte YELLOW_PIN = 4;              // D2
const byte RED_PIN = 5;                 // D1

const unsigned long FLASH_PERIOD = 50;  // milliseconds
const unsigned long HYSTERESIS = 100;    // rpm

// internals below
unsigned long lastFlashEvent = 0;
bool flashState = false;
byte lightState = 0;

const byte SL_ALL_OFF = 0;
const byte SL_GREEN = 1;
const byte SL_YELLOW = 2;
const byte SL_RED = 4;
const byte SL_ALL_ON = 7;

const unsigned long DEBOUNCE_USEC = 60000000 / PULSE_PER_REV / (FLASH_THRESHOLD * 2);

unsigned long history[HISTORY_LENGTH];
unsigned int historyPosition = 0;

volatile unsigned long previousTach = 0;
volatile unsigned long rpm = 0;

void setup() {
  pinMode(TACH_PIN, INPUT);
  attachInterrupt(
    digitalPinToInterrupt(TACH_PIN),
    tachBounce,
    RISING
  );

  pinMode(GREEN_PIN, OUTPUT);
  pinMode(YELLOW_PIN, OUTPUT);
  pinMode(RED_PIN, OUTPUT);

  startupFlashSequence();

  Serial.begin(115200);
}

// interrupt service routine
void tachBounce() {
  unsigned long current = micros();
  unsigned long period = current - previousTach;
  // debounce signal i.e. ignore anything < DEBOUNCE_USEC
  if (period < DEBOUNCE_USEC) {
    return;
  }
  previousTach = current;

  history[historyPosition++ % HISTORY_LENGTH] = period;
  unsigned long sum = 0;
  for (int i = 0; i < HISTORY_LENGTH; i++) {
    sum += history[i];
  }
  
  if (sum == 0) {
    return;
  }

  rpm = 60000000 / PULSE_PER_REV / (sum / HISTORY_LENGTH);
}


void setLights(byte color) {
  digitalWrite(GREEN_PIN, color & SL_GREEN);
  digitalWrite(YELLOW_PIN, color & SL_YELLOW);
  digitalWrite(RED_PIN, color & SL_RED);
}

// blocking do not use in main loop
void startupFlashSequence() {
  setLights(SL_GREEN);
  delay(300);
  setLights(SL_GREEN | SL_YELLOW);
  delay(300);
  setLights(SL_ALL_ON);
  delay(300);
  for (int i = 0; i < 5; i++) {
    setLights(SL_ALL_OFF);
    delay(FLASH_PERIOD);
    setLights(SL_ALL_ON);
    delay(FLASH_PERIOD);
  }
  setLights(SL_ALL_OFF);
}

void loop() {
  // constantly toggle flasher state
  if (millis() - lastFlashEvent > FLASH_PERIOD) {
    flashState = !flashState;
    lastFlashEvent = millis();
    // piggyback on this event timer to dump current RPM to serial port
    Serial.print("RPM: ");
    Serial.println(rpm);
    Serial.print("LIGHTS: ");
    Serial.println(lightState);
  }

  if (rpm >= FLASH_THRESHOLD) {
    lightState = flashState ? SL_ALL_ON : SL_ALL_OFF;
  } else {
    if (rpm >= RED_THRESHOLD) lightState |= SL_RED;
    if (rpm < RED_THRESHOLD - HYSTERESIS) lightState &= ~SL_RED;
    if (rpm >= YELLOW_THRESHOLD) lightState |= SL_YELLOW;
    if (rpm < YELLOW_THRESHOLD - HYSTERESIS) lightState &= ~SL_YELLOW;
    if (rpm >= GREEN_THRESHOLD) lightState |= SL_GREEN;
    if (rpm < GREEN_THRESHOLD - HYSTERESIS) lightState &= ~SL_GREEN;
  }
  setLights(lightState);
}
