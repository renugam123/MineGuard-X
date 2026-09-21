/*
============================================================
                    MINEGUARD-X
              PS39 MINE RESCUE ROBOT
             SIH PROFESSIONAL DASHBOARD
============================================================

SENSORS:
HC-SR04  -> Distance / Obstacle
PIR      -> Motion Detection
MQ-2     -> Gas / Smoke

MOTOR:
L298N    -> Left + Right Motors

COMMUNICATION:
Wi-Fi Access Point
Bluetooth

DASHBOARD:
http://192.168.4.1

FEATURES:
- Real-time sensor monitoring
- Auto / Manual mode
- Manual movement control
- Emergency stop
- Emergency reset
- PIR motion event counter
- Bluetooth control
- Wi-Fi dashboard control
- Professional responsive UI

IMPORTANT:
HC-SR04 ECHO must be voltage-divided before ESP32 GPIO18.
MQ-2 output must be voltage-safe before ESP32 GPIO34.
Do not power motors directly from ESP32.
============================================================
*/

#include <WiFi.h>
#include <WebServer.h>
#include <BluetoothSerial.h>

// ============================================================
// SENSOR PINS
// ============================================================

const int TRIG_PIN = 5;
const int ECHO_PIN = 18;

const int PIR_PIN = 19;

const int MQ2_PIN = 34;


// ============================================================
// L298N MOTOR DRIVER PINS
// ============================================================

// LEFT MOTOR
const int ENA_PIN = 25;
const int IN1_PIN = 26;
const int IN2_PIN = 27;

// RIGHT MOTOR
const int ENB_PIN = 32;
const int IN3_PIN = 14;
const int IN4_PIN = 12;


// ============================================================
// OUTPUT PINS
// ============================================================

const int BUZZER_PIN = 23;

const int GREEN_LED_PIN = 21;
const int YELLOW_LED_PIN = 22;
const int RED_LED_PIN = 4;


// ============================================================
// ROBOT SETTINGS
// ============================================================

const int MOTOR_SPEED = 170;

const float OBSTACLE_DISTANCE = 20.0;

const int GAS_WARNING_LEVEL = 1800;
const int GAS_CRITICAL_LEVEL = 3000;


// ============================================================
// WIFI SETTINGS
// ============================================================

const char* WIFI_SSID = "MineGuard-X";
const char* WIFI_PASSWORD = "mineguard123";


// ============================================================
// SERVER
// ============================================================

WebServer server(80);
BluetoothSerial SerialBT;


// ============================================================
// ROBOT STATE
// ============================================================

bool manualMode = true;
bool emergencyStop = false;


// ============================================================
// SENSOR VALUES
// ============================================================

float currentDistance = 999.0;

int currentMotion = LOW;

int currentGasValue = 0;


// ============================================================
// DASHBOARD VALUES
// ============================================================

String currentDecision = "STARTING";
String currentAction = "STOPPED";


// ============================================================
// PIR EVENT COUNTER
// ============================================================

unsigned long peopleDetected = 0;

int previousMotion = LOW;


// ============================================================
// TIMERS
// ============================================================

unsigned long lastSensorRead = 0;
unsigned long lastAutoUpdate = 0;
unsigned long lastBuzzerToggle = 0;


// ============================================================
// AUTO MODE STATE
// ============================================================

bool buzzerState = false;

bool autoTurning = false;

unsigned long autoTurnUntil = 0;


// ============================================================
// FUNCTION DECLARATIONS
// ============================================================

float readDistance();

String makeDecision(
  float distance,
  int motion,
  int gasValue
);

void moveForward(int speedValue);
void moveBackward(int speedValue);
void turnRight(int speedValue);
void turnLeft(int speedValue);
void stopMotors();

void updateSensors();
void updateIndicators();
void runAutoMode();

void handleBluetoothCommand(char command);

void handleRoot();
void handleStatus();
void handleCommand();


// ============================================================
// SETUP
// ============================================================

void setup() {

  Serial.begin(115200);

  delay(500);


  // ----------------------------------------------------------
  // SENSOR PINS
  // ----------------------------------------------------------

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  pinMode(PIR_PIN, INPUT);

  pinMode(MQ2_PIN, INPUT);


  // ----------------------------------------------------------
  // MOTOR PINS
  // ----------------------------------------------------------

  pinMode(ENA_PIN, OUTPUT);

  pinMode(IN1_PIN, OUTPUT);
  pinMode(IN2_PIN, OUTPUT);

  pinMode(ENB_PIN, OUTPUT);

  pinMode(IN3_PIN, OUTPUT);
  pinMode(IN4_PIN, OUTPUT);


  // ----------------------------------------------------------
  // OUTPUT PINS
  // ----------------------------------------------------------

  pinMode(BUZZER_PIN, OUTPUT);

  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(YELLOW_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);


  // ----------------------------------------------------------
  // SAFE START
  // ----------------------------------------------------------

  stopMotors();

  digitalWrite(BUZZER_PIN, LOW);

  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(YELLOW_LED_PIN, LOW);

  digitalWrite(RED_LED_PIN, HIGH);


  // ----------------------------------------------------------
  // SERIAL INFORMATION
  // ----------------------------------------------------------

  Serial.println();
  Serial.println("======================================");
  Serial.println("             MINEGUARD-X");
  Serial.println("          PS39 MINE RESCUE");
  Serial.println("======================================");

  Serial.println("Initializing system...");


  // ----------------------------------------------------------
  // BLUETOOTH
  // ----------------------------------------------------------

  Serial.println();
  Serial.println("Starting Bluetooth...");

  if (SerialBT.begin("MineGuard-X Rover")) {

    Serial.println("Bluetooth started.");
    Serial.println("Device: MineGuard-X Rover");

    Serial.println("Commands:");
    Serial.println("F = Forward");
    Serial.println("B = Backward");
    Serial.println("L = Left");
    Serial.println("R = Right");
    Serial.println("S = Stop");
    Serial.println("A = Auto");
    Serial.println("M = Manual");

  } else {

    Serial.println("Bluetooth start FAILED!");

  }


  // ----------------------------------------------------------
  // WIFI ACCESS POINT
  // ----------------------------------------------------------

  Serial.println();
  Serial.println("Starting Wi-Fi Access Point...");

  WiFi.mode(WIFI_AP);

  if (WiFi.softAP(WIFI_SSID, WIFI_PASSWORD)) {

    Serial.println("Wi-Fi Access Point started.");

    Serial.print("SSID: ");
    Serial.println(WIFI_SSID);

    Serial.print("Password: ");
    Serial.println(WIFI_PASSWORD);

    Serial.print("IP Address: ");
    Serial.println(WiFi.softAPIP());

  } else {

    Serial.println("Wi-Fi Access Point FAILED!");

  }


  // ----------------------------------------------------------
  // WEB SERVER ROUTES
  // ----------------------------------------------------------

  server.on("/", HTTP_GET, handleRoot);

  server.on(
    "/api/status",
    HTTP_GET,
    handleStatus
  );

  server.on(
    "/api/command",
    HTTP_GET,
    handleCommand
  );


  // ----------------------------------------------------------
  // START SERVER
  // ----------------------------------------------------------

  server.begin();

  Serial.println();
  Serial.println("Web server started.");

  Serial.println("Dashboard:");
  Serial.println("http://192.168.4.1");

  Serial.println();
  Serial.println("======================================");
  Serial.println("          MINEGUARD-X READY");
  Serial.println("======================================");
}


// ============================================================
// MAIN LOOP
// ============================================================

void loop() {

  // Keep dashboard responsive
  server.handleClient();


  // ----------------------------------------------------------
  // SENSOR UPDATE
  // ----------------------------------------------------------

  if (millis() - lastSensorRead >= 150) {

    lastSensorRead = millis();

    updateSensors();

  }


  // ----------------------------------------------------------
  // BLUETOOTH COMMAND PROCESSING
  // ----------------------------------------------------------

  while (SerialBT.available()) {

    char command = SerialBT.read();


    // Ignore spaces and line endings

    if (
      command == '\n' ||
      command == '\r' ||
      command == ' '
    ) {

      continue;

    }


    // Convert lowercase to uppercase

    if (
      command >= 'a' &&
      command <= 'z'
    ) {

      command = command - 32;

    }


    handleBluetoothCommand(command);

  }


  // ----------------------------------------------------------
  // EMERGENCY STOP
  // ----------------------------------------------------------

  if (emergencyStop) {

    stopMotors();

    currentDecision = "EMERGENCY STOP";

    currentAction = "MOTORS STOPPED";

    updateIndicators();

  }


  // ----------------------------------------------------------
  // AUTO MODE
  // ----------------------------------------------------------

  else if (!manualMode) {

    runAutoMode();

  }


  delay(2);
}


// ============================================================
// SENSOR UPDATE
// ============================================================

void updateSensors() {

  currentDistance = readDistance();

  currentMotion = digitalRead(PIR_PIN);

  currentGasValue = analogRead(MQ2_PIN);


  // ----------------------------------------------------------
  // PIR EVENT COUNTER
  // ----------------------------------------------------------
  //
  // This counts every new PIR activation.
  //
  // IMPORTANT:
  // PIR detects motion.
  // It does NOT identify or count individual humans.
  //
  // Therefore this value represents motion events.
  // ----------------------------------------------------------

  if (
    currentMotion == HIGH &&
    previousMotion == LOW
  ) {

    peopleDetected++;

  }

  previousMotion = currentMotion;


  // ----------------------------------------------------------
  // SYSTEM DECISION
  // ----------------------------------------------------------

  if (emergencyStop) {

    currentDecision = "EMERGENCY STOP";

  } else {

    currentDecision = makeDecision(
      currentDistance,
      currentMotion,
      currentGasValue
    );

  }


  // ----------------------------------------------------------
  // INDICATORS
  // ----------------------------------------------------------

  updateIndicators();
}


// ============================================================
// HC-SR04 DISTANCE
// ============================================================

float readDistance() {

  digitalWrite(TRIG_PIN, LOW);

  delayMicroseconds(2);


  digitalWrite(TRIG_PIN, HIGH);

  delayMicroseconds(10);

  digitalWrite(TRIG_PIN, LOW);


  long duration = pulseIn(
    ECHO_PIN,
    HIGH,
    30000
  );


  // No echo

  if (duration == 0) {

    return 999.0;

  }


  float distance =
    (duration * 0.0343) / 2.0;


  if (
    distance < 0 ||
    distance > 999
  ) {

    return 999.0;

  }


  return distance;
}


// ============================================================
// SYSTEM DECISION
// ============================================================

String makeDecision(
  float distance,
  int motion,
  int gasValue
) {

  // Highest priority:
  // Critical gas

  if (
    gasValue >= GAS_CRITICAL_LEVEL
  ) {

    return "CRITICAL GAS";

  }


  // Gas warning

  if (
    gasValue >= GAS_WARNING_LEVEL
  ) {

    return "GAS HAZARD";

  }


  // Motion

  if (motion == HIGH) {

    return "MOTION DETECTED";

  }


  // Obstacle

  if (
    distance < OBSTACLE_DISTANCE
  ) {

    return "OBSTACLE";

  }


  return "NORMAL";
}


// ============================================================
// AUTO MODE
// ============================================================

void runAutoMode() {

  if (
    millis() - lastAutoUpdate < 100
  ) {

    return;

  }

  lastAutoUpdate = millis();


  String decision = makeDecision(
    currentDistance,
    currentMotion,
    currentGasValue
  );


  currentDecision = decision;


  // ----------------------------------------------------------
  // CRITICAL GAS
  // ----------------------------------------------------------

  if (
    decision == "CRITICAL GAS"
  ) {

    autoTurning = false;

    stopMotors();

    currentAction = "SAFETY STOP";

    digitalWrite(BUZZER_PIN, HIGH);

    updateIndicators();

    return;
  }


  // ----------------------------------------------------------
  // GAS WARNING
  // ----------------------------------------------------------

  if (
    decision == "GAS HAZARD"
  ) {

    autoTurning = false;

    stopMotors();

    currentAction = "SAFETY STOP";

    digitalWrite(BUZZER_PIN, HIGH);

    updateIndicators();

    return;
  }


  // ----------------------------------------------------------
  // MOTION DETECTED
  // ----------------------------------------------------------

  if (
    decision == "MOTION DETECTED"
  ) {

    autoTurning = false;

    stopMotors();

    currentAction =
      "STOP - MOTION ALERT";


    // Non-blocking buzzer

    if (
      millis() - lastBuzzerToggle >= 250
    ) {

      lastBuzzerToggle = millis();

      buzzerState = !buzzerState;

      digitalWrite(
        BUZZER_PIN,
        buzzerState
      );

    }

    updateIndicators();

    return;
  }


  // ----------------------------------------------------------
  // NORMAL OPERATION
  // ----------------------------------------------------------

  digitalWrite(
    BUZZER_PIN,
    LOW
  );

  buzzerState = false;


  // ----------------------------------------------------------
  // COMPLETE CURRENT TURN
  // ----------------------------------------------------------

  if (autoTurning) {

    if (
      millis() < autoTurnUntil
    ) {

      turnRight(MOTOR_SPEED);

      currentAction =
        "TURNING RIGHT";

      updateIndicators();

      return;

    }


    autoTurning = false;

  }


  // ----------------------------------------------------------
  // OBSTACLE DETECTED
  // ----------------------------------------------------------

  if (
    decision == "OBSTACLE"
  ) {

    stopMotors();

    autoTurning = true;

    autoTurnUntil =
      millis() + 550;

    currentAction =
      "OBSTACLE - TURNING";

    updateIndicators();

    return;
  }


  // ----------------------------------------------------------
  // MOVE FORWARD
  // ----------------------------------------------------------

  moveForward(MOTOR_SPEED);

  currentAction =
    "MOVE FORWARD";

  updateIndicators();
}


// ============================================================
// LED + BUZZER INDICATORS
// ============================================================

void updateIndicators() {

  // ----------------------------------------------------------
  // EMERGENCY
  // ----------------------------------------------------------

  if (emergencyStop) {

    digitalWrite(
      GREEN_LED_PIN,
      LOW
    );

    digitalWrite(
      YELLOW_LED_PIN,
      LOW
    );

    digitalWrite(
      RED_LED_PIN,
      HIGH
    );

    digitalWrite(
      BUZZER_PIN,
      LOW
    );

    return;
  }


  // ----------------------------------------------------------
  // CRITICAL GAS
  // ----------------------------------------------------------

  if (
    currentDecision == "CRITICAL GAS"
  ) {

    digitalWrite(
      GREEN_LED_PIN,
      LOW
    );

    digitalWrite(
      YELLOW_LED_PIN,
      LOW
    );

    digitalWrite(
      RED_LED_PIN,
      HIGH
    );

    return;
  }


  // ----------------------------------------------------------
  // GAS WARNING
  // ----------------------------------------------------------

  if (
    currentDecision == "GAS HAZARD"
  ) {

    digitalWrite(
      GREEN_LED_PIN,
      LOW
    );

    digitalWrite(
      YELLOW_LED_PIN,
      LOW
    );

    digitalWrite(
      RED_LED_PIN,
      HIGH
    );

    return;
  }


  // ----------------------------------------------------------
  // MOTION
  // ----------------------------------------------------------

  if (
    currentDecision == "MOTION DETECTED"
  ) {

    digitalWrite(
      GREEN_LED_PIN,
      LOW
    );

    digitalWrite(
      YELLOW_LED_PIN,
      HIGH
    );

    digitalWrite(
      RED_LED_PIN,
      LOW
    );

    return;
  }


  // ----------------------------------------------------------
  // OBSTACLE
  // ----------------------------------------------------------

  if (
    currentDecision == "OBSTACLE"
  ) {

    digitalWrite(
      GREEN_LED_PIN,
      LOW
    );

    digitalWrite(
      YELLOW_LED_PIN,
      HIGH
    );

    digitalWrite(
      RED_LED_PIN,
      LOW
    );

    return;
  }


  // ----------------------------------------------------------
  // MANUAL MODE
  // ----------------------------------------------------------

  if (manualMode) {

    digitalWrite(
      GREEN_LED_PIN,
      LOW
    );

    digitalWrite(
      YELLOW_LED_PIN,
      HIGH
    );

    digitalWrite(
      RED_LED_PIN,
      LOW
    );

  }


  // ----------------------------------------------------------
  // AUTO MODE
  // ----------------------------------------------------------

  else {

    digitalWrite(
      GREEN_LED_PIN,
      HIGH
    );

    digitalWrite(
      YELLOW_LED_PIN,
      LOW
    );

    digitalWrite(
      RED_LED_PIN,
      LOW
    );

  }
}


// ============================================================
// MOTOR: FORWARD
// ============================================================

void moveForward(int speedValue) {

  digitalWrite(
    IN1_PIN,
    HIGH
  );

  digitalWrite(
    IN2_PIN,
    LOW
  );

  digitalWrite(
    IN3_PIN,
    HIGH
  );

  digitalWrite(
    IN4_PIN,
    LOW
  );


  analogWrite(
    ENA_PIN,
    speedValue
  );

  analogWrite(
    ENB_PIN,
    speedValue
  );
}


// ============================================================
// MOTOR: BACKWARD
// ============================================================

void moveBackward(int speedValue) {

  digitalWrite(
    IN1_PIN,
    LOW
  );

  digitalWrite(
    IN2_PIN,
    HIGH
  );

  digitalWrite(
    IN3_PIN,
    LOW
  );

  digitalWrite(
    IN4_PIN,
    HIGH
  );


  analogWrite(
    ENA_PIN,
    speedValue
  );

  analogWrite(
    ENB_PIN,
    speedValue
  );
}


// ============================================================
// MOTOR: RIGHT
// ============================================================

void turnRight(int speedValue) {

  digitalWrite(
    IN1_PIN,
    HIGH
  );

  digitalWrite(
    IN2_PIN,
    LOW
  );

  digitalWrite(
    IN3_PIN,
    LOW
  );

  digitalWrite(
    IN4_PIN,
    HIGH
  );


  analogWrite(
    ENA_PIN,
    speedValue
  );

  analogWrite(
    ENB_PIN,
    speedValue
  );
}


// ============================================================
// MOTOR: LEFT
// ============================================================

void turnLeft(int speedValue) {

  digitalWrite(
    IN1_PIN,
    LOW
  );

  digitalWrite(
    IN2_PIN,
    HIGH
  );

  digitalWrite(
    IN3_PIN,
    HIGH
  );

  digitalWrite(
    IN4_PIN,
    LOW
  );


  analogWrite(
    ENA_PIN,
    speedValue
  );

  analogWrite(
    ENB_PIN,
    speedValue
  );
}


// ============================================================
// MOTOR: STOP
// ============================================================

void stopMotors() {

  digitalWrite(
    IN1_PIN,
    LOW
  );

  digitalWrite(
    IN2_PIN,
    LOW
  );

  digitalWrite(
    IN3_PIN,
    LOW
  );

  digitalWrite(
    IN4_PIN,
    LOW
  );


  analogWrite(
    ENA_PIN,
    0
  );

  analogWrite(
    ENB_PIN,
    0
  );
}


// ============================================================
// BLUETOOTH COMMAND
// ============================================================

void handleBluetoothCommand(
  char command
) {

  // ----------------------------------------------------------
  // AUTO MODE
  // ----------------------------------------------------------

  if (command == 'A') {

    if (!emergencyStop) {

      manualMode = false;

      autoTurning = false;

      stopMotors();

      currentDecision =
        "AUTO MODE";

      currentAction =
        "AUTO READY";

      updateIndicators();

      SerialBT.println(
        "AUTO MODE"
      );

    }

    return;
  }


  // ----------------------------------------------------------
  // MANUAL MODE
  // ----------------------------------------------------------

  if (command == 'M') {

    if (!emergencyStop) {

      manualMode = true;

      autoTurning = false;

      stopMotors();

      currentDecision =
        "MANUAL MODE";

      currentAction =
        "READY";

      updateIndicators();

      SerialBT.println(
        "MANUAL MODE"
      );

    }

    return;
  }


  // ----------------------------------------------------------
  // EMERGENCY LOCK
  // ----------------------------------------------------------

  if (emergencyStop) {

    stopMotors();

    SerialBT.println(
      "EMERGENCY STOP ACTIVE"
    );

    SerialBT.println(
      "RESET FROM DASHBOARD"
    );

    return;
  }


  // Movement command
  // automatically selects manual mode

  manualMode = true;

  autoTurning = false;


  // ----------------------------------------------------------
  // FORWARD
  // ----------------------------------------------------------

  if (command == 'F') {

    moveForward(MOTOR_SPEED);

    currentAction =
      "MOVE FORWARD";

    SerialBT.println(
      "FORWARD"
    );

  }


  // ----------------------------------------------------------
  // BACKWARD
  // ----------------------------------------------------------

  else if (command == 'B') {

    moveBackward(MOTOR_SPEED);

    currentAction =
      "MOVE BACKWARD";

    SerialBT.println(
      "BACKWARD"
    );

  }


  // ----------------------------------------------------------
  // LEFT
  // ----------------------------------------------------------

  else if (command == 'L') {

    turnLeft(MOTOR_SPEED);

    currentAction =
      "TURN LEFT";

    SerialBT.println(
      "LEFT"
    );

  }


  // ----------------------------------------------------------
  // RIGHT
  // ----------------------------------------------------------

  else if (command == 'R') {

    turnRight(MOTOR_SPEED);

    currentAction =
      "TURN RIGHT";

    SerialBT.println(
      "RIGHT"
    );

  }


  // ----------------------------------------------------------
  // STOP
  // ----------------------------------------------------------

  else if (command == 'S') {

    stopMotors();

    currentAction =
      "STOPPED";

    SerialBT.println(
      "STOP"
    );

  }


  // ----------------------------------------------------------
  // UNKNOWN COMMAND
  // ----------------------------------------------------------

  else {

    SerialBT.println(
      "Commands: F B L R S"
    );

    SerialBT.println(
      "A = AUTO"
    );

    SerialBT.println(
      "M = MANUAL"
    );

  }


  updateIndicators();
}


// ============================================================
// PROFESSIONAL WEB DASHBOARD
// ============================================================

void handleRoot() {

  String html = R"rawliteral(

<!DOCTYPE html>

<html lang="en">

<head>

<meta charset="UTF-8">

<meta
  name="viewport"
  content="width=device-width, initial-scale=1.0"
>

<meta
  http-equiv="Cache-Control"
  content="no-cache, no-store, must-revalidate"
>

<title>
MineGuard-X | Rescue Dashboard
</title>


<style>

/* ==========================================================
   GLOBAL
   ========================================================== */

:root {

  --bg: #07111f;

  --panel: #101d31;

  --card: #17263e;

  --card-hover: #1c2e4b;

  --text: #f4f7fb;

  --muted: #8fa3bd;

  --green: #20b866;

  --yellow: #e5a500;

  --red: #e33d4d;

  --blue: #3c8cff;

  --border: rgba(255,255,255,0.08);

}


* {

  box-sizing: border-box;

}


body {

  margin: 0;

  min-height: 100vh;

  font-family:
    "Segoe UI",
    Arial,
    sans-serif;

  background:
    radial-gradient(
      circle at top,
      #142742 0%,
      #07111f 48%,
      #040a12 100%
    );

  color: var(--text);

}


/* ==========================================================
   MAIN CONTAINER
   ========================================================== */

.container {

  width: min(1120px, 94%);

  margin: auto;

  padding:
    30px
    0
    45px;

}


/* ==========================================================
   HEADER
   ========================================================== */

.header {

  display: flex;

  justify-content: space-between;

  align-items: center;

  gap: 20px;

  margin-bottom: 25px;

}


.brand h1 {

  margin: 0;

  font-size: 32px;

  letter-spacing: 2px;

  font-weight: 900;

}


.brand p {

  margin:
    7px
    0
    0;

  color: var(--muted);

  font-size: 14px;

}


.connection {

  display: flex;

  align-items: center;

  gap: 9px;

  padding:
    9px
    14px;

  border-radius: 30px;

  background: #111f34;

  color: #b7c5d8;

  font-size: 12px;

  font-weight: 700;

}


.connection-dot {

  width: 10px;

  height: 10px;

  border-radius: 50%;

  background: var(--green);

  box-shadow:
    0 0 12px
    rgba(32,184,102,0.7);

}


/* ==========================================================
   PANELS
   ========================================================== */

.panel {

  background:
    rgba(16,29,49,0.95);

  border:
    1px solid var(--border);

  border-radius: 20px;

  padding: 23px;

  margin-bottom: 18px;

  box-shadow:
    0 15px 40px
    rgba(0,0,0,0.18);

}


.panel-title {

  display: flex;

  align-items: center;

  justify-content: space-between;

  gap: 15px;

  margin-bottom: 18px;

}


.panel-title h2 {

  margin: 0;

  font-size: 18px;

  letter-spacing: 0.8px;

}


.badge {

  padding:
    7px
    12px;

  border-radius: 20px;

  background: #243650;

  color: #b9c8da;

  font-size: 10px;

  font-weight: 800;

  letter-spacing: 0.7px;

}


/* ==========================================================
   ROBOT STATUS
   ========================================================== */

.status-grid {

  display: grid;

  grid-template-columns:
    repeat(4, 1fr);

  gap: 13px;

}


.status-card {

  background: var(--card);

  border:
    1px solid var(--border);

  border-radius: 15px;

  padding: 17px;

  min-height: 100px;

  transition:
    0.2s ease;

}


.status-card:hover {

  background:
    var(--card-hover);

  transform:
    translateY(-2px);

}


.status-wide {

  grid-column:
    span 2;

}


.status-label {

  color: var(--muted);

  font-size: 10px;

  font-weight: 800;

  text-transform: uppercase;

  letter-spacing: 1px;

}


.status-value {

  margin-top: 10px;

  font-size: 19px;

  font-weight: 900;

  line-height: 1.2;

  word-break: break-word;

}


/* ==========================================================
   PEOPLE / MOTION COUNTER
   ========================================================== */

.counter-panel {

  text-align: center;

}


.counter-content {

  padding:
    15px
    0
    5px;

}


.counter-number {

  font-size: 64px;

  font-weight: 900;

  line-height: 1;

  letter-spacing: 2px;

}


.counter-description {

  margin-top: 10px;

  color: var(--muted);

  font-size: 12px;

}


/* ==========================================================
   OPERATING MODE
   ========================================================== */

.mode-grid {

  display: grid;

  grid-template-columns:
    1fr
    1fr;

  gap: 15px;

}


button {

  border: none;

  border-radius: 13px;

  padding:
    15px
    18px;

  font-size: 14px;

  font-weight: 800;

  cursor: pointer;

  transition:
    transform 0.15s ease,
    opacity 0.15s ease,
    box-shadow 0.15s ease;

}


button:hover {

  transform:
    translateY(-2px);

  opacity: 0.93;

}


button:active {

  transform:
    scale(0.97);

}


.mode-btn {

  min-height: 60px;

  background: #263752;

  color: white;

  border:
    1px solid var(--border);

}


.mode-btn.active.auto {

  background: var(--green);

  box-shadow:
    0 0 25px
    rgba(32,184,102,0.22);

}


.mode-btn.active.manual {

  background: var(--yellow);

  color: #111;

  box-shadow:
    0 0 25px
    rgba(229,165,0,0.22);

}


/* ==========================================================
   MANUAL ROBOT CONTROL
   ========================================================== */

.controls {

  width: 100%;

  max-width: 430px;

  margin:
    0
    auto;

}


.control-row {

  display: flex;

  justify-content: center;

  align-items: center;

  gap: 12px;

  margin:
    12px
    0;

}


.control-btn {

  width: 125px;

  min-height: 58px;

  background: #e9eef5;

  color: #152238;

  border:
    1px solid
    #cbd4df;

}


.control-btn:hover {

  background: white;

}


.control-btn.stop {

  background: var(--red);

  color: white;

  border-color:
    var(--red);

  box-shadow:
    0 0 20px
    rgba(227,61,77,0.18);

}


.control-btn:disabled {

  opacity: 0.35;

  cursor: not-allowed;

  transform: none;

}


/* ==========================================================
   EMERGENCY CONTROL
   ========================================================== */

.emergency {

  display: grid;

  grid-template-columns:
    1fr
    1fr;

  gap: 18px;

  align-items: center;

}


.emergency-status {

  background: var(--card);

  border:
    1px solid var(--border);

  border-radius: 15px;

  padding: 18px;

}


.emergency-label {

  color: var(--muted);

  font-size: 10px;

  font-weight: 800;

  letter-spacing: 1px;

}


.emergency-value {

  margin-top: 8px;

  font-size: 20px;

  font-weight: 900;

}


.emergency-value.safe {

  color: var(--green);

}


.emergency-value.danger {

  color: var(--red);

}


.emergency-btn {

  width: 100%;

  min-height: 60px;

  background:
    #b7192b;

  color: white;

  box-shadow:
    0 0 25px
    rgba(227,61,77,0.18);

}


.reset-btn {

  width: 100%;

  margin-top: 10px;

  background:
    #293b58;

  color: white;

}


/* ==========================================================
   FOOTER
   ========================================================== */

.footer {

  text-align: center;

  color: #71849d;

  font-size: 11px;

  padding:
    8px;

}


/* ==========================================================
   RESPONSIVE
   ========================================================== */

@media(max-width:850px) {

  .status-grid {

    grid-template-columns:
      repeat(2, 1fr);

  }

  .status-wide {

    grid-column:
      span 1;

  }

}


@media(max-width:600px) {

  .container {

    width: 94%;

    padding-top: 18px;

  }


  .header {

    flex-direction: column;

    align-items: flex-start;

  }


  .brand h1 {

    font-size: 25px;

  }


  .status-grid {

    grid-template-columns:
      1fr;

  }


  .status-wide {

    grid-column:
      span 1;

  }


  .mode-grid {

    grid-template-columns:
      1fr;

  }


  .emergency {

    grid-template-columns:
      1fr;

  }


  .control-btn {

    width: 95px;

  }


  .panel {

    padding: 17px;

  }

}

</style>

</head>


<body>


<div class="container">


<!-- ======================================================
     HEADER
     ====================================================== -->

<header class="header">

  <div class="brand">

    <h1>MINEGUARD-X</h1>

    <p>
      Autonomous First-Responder Intelligence
      | SIH PS39
    </p>

  </div>


  <div class="connection">

    <span
      class="connection-dot"
      id="connectionDot">
    </span>

    <span id="connectionText">
      ESP32 CONNECTED
    </span>

  </div>

</header>


<!-- ======================================================
     ROBOT STATUS
     ====================================================== -->

<section class="panel">

  <div class="panel-title">

    <h2>ROBOT STATUS</h2>

    <span
      class="badge"
      id="modeBadge">
      MANUAL
    </span>

  </div>


  <div class="status-grid">


    <div class="status-card">

      <div class="status-label">
        Operating Mode
      </div>

      <div
        class="status-value"
        id="mode">
        ---
      </div>

    </div>


    <div class="status-card">

      <div class="status-label">
        Distance
      </div>

      <div
        class="status-value"
        id="distance">
        ---
      </div>

    </div>


    <div class="status-card">

      <div class="status-label">
        PIR Sensor
      </div>

      <div
        class="status-value"
        id="motion">
        ---
      </div>

    </div>


    <div class="status-card">

      <div class="status-label">
        MQ-2 Gas / Smoke
      </div>

      <div
        class="status-value"
        id="gas">
        ---
      </div>

    </div>


    <div class="status-card status-wide">

      <div class="status-label">
        System Decision
      </div>

      <div
        class="status-value"
        id="decision">
        ---
      </div>

    </div>


    <div class="status-card status-wide">

      <div class="status-label">
        Current Robot Action
      </div>

      <div
        class="status-value"
        id="action">
        ---
      </div>

    </div>


  </div>

</section>


<!-- ======================================================
     PEOPLE / MOTION COUNTER
     ====================================================== -->

<section class="panel counter-panel">

  <div class="panel-title">

    <h2>PEOPLE DETECTED</h2>

    <span class="badge">
      PIR EVENTS
    </span>

  </div>


  <div class="counter-content">

    <div
      class="counter-number"
      id="people">
      00
    </div>

    <div class="counter-description">

      Motion detection events recorded
      since ESP32 startup

    </div>

  </div>

</section>


<!-- ======================================================
     OPERATING MODE
     ====================================================== -->

<section class="panel">

  <div class="panel-title">

    <h2>OPERATING MODE</h2>

    <span
      class="badge"
      id="modeHint">
      SELECT MODE
    </span>

  </div>


  <div class="mode-grid">


    <button
      class="mode-btn auto"
      id="autoBtn"
      onclick="sendCommand('auto')">

      AUTO MODE

    </button>


    <button
      class="mode-btn manual"
      id="manualBtn"
      onclick="sendCommand('manual')">

      MANUAL MODE

    </button>


  </div>

</section>


<!-- ======================================================
     MANUAL ROBOT CONTROL
     ====================================================== -->

<section class="panel">

  <div class="panel-title">

    <h2>MANUAL ROBOT CONTROL</h2>

    <span
      class="badge"
      id="manualBadge">
      MANUAL ONLY
    </span>

  </div>


  <div class="controls">


    <!-- FORWARD -->

    <div class="control-row">

      <button
        class="control-btn"
        id="forwardBtn"
        onclick="sendCommand('forward')">

        FORWARD

      </button>

    </div>


    <!-- LEFT / STOP / RIGHT -->

    <div class="control-row">

      <button
        class="control-btn"
        id="leftBtn"
        onclick="sendCommand('left')">

        LEFT

      </button>


      <button
        class="control-btn stop"
        id="stopBtn"
        onclick="sendCommand('stop')">

        STOP

      </button>


      <button
        class="control-btn"
        id="rightBtn"
        onclick="sendCommand('right')">

        RIGHT

      </button>

    </div>


    <!-- BACKWARD -->

    <div class="control-row">

      <button
        class="control-btn"
        id="backBtn"
        onclick="sendCommand('back')">

        BACKWARD

      </button>

    </div>


  </div>

</section>


<!-- ======================================================
     EMERGENCY CONTROL
     ====================================================== -->

<section class="panel">

  <div class="panel-title">

    <h2>EMERGENCY CONTROL</h2>

    <span class="badge">
      SAFETY
    </span>

  </div>


  <div class="emergency">


    <div class="emergency-status">

      <div class="emergency-label">
        EMERGENCY STATUS
      </div>

      <div
        class="emergency-value safe"
        id="emergencyStatus">

        SYSTEM SAFE

      </div>

    </div>


    <div>

      <button
        class="emergency-btn"
        onclick="sendCommand('emergency')">

        EMERGENCY STOP

      </button>


      <button
        class="reset-btn"
        onclick="sendCommand('reset_emergency')">

        RESET EMERGENCY

      </button>

    </div>


  </div>

</section>


<!-- ======================================================
     FOOTER
     ====================================================== -->

<div class="footer">

  MineGuard-X Prototype
  | ESP32 Local Rescue Dashboard
  | Real-Time Monitoring

</div>


</div>


<script>

/* ==========================================================
   MANUAL CONTROL BUTTONS
   ========================================================== */

const controlIds = [

  'forwardBtn',

  'leftBtn',

  'stopBtn',

  'rightBtn',

  'backBtn'

];


/* ==========================================================
   ENABLE / DISABLE MANUAL CONTROLS
   ========================================================== */

function setControlsEnabled(enabled) {

  controlIds.forEach(function(id) {

    const button =
      document.getElementById(id);

    if (button) {

      button.disabled = !enabled;

    }

  });

}


/* ==========================================================
   SEND COMMAND TO ESP32
   ========================================================== */

function sendCommand(command) {

  fetch(
    '/api/command?cmd=' +
    encodeURIComponent(command),
    {
      method: 'GET',
      cache: 'no-store'
    }
  )

  .then(function(response) {

    if (!response.ok) {

      throw new Error(
        'Command failed'
      );

    }

    return response.text();

  })

  .then(function() {

    updateStatus();

  })

  .catch(function() {

    setDisconnected();

  });

}


/* ==========================================================
   SHOW ESP32 DISCONNECTED
   ========================================================== */

function setDisconnected() {

  document.getElementById(
    'connectionText'
  ).innerText =
    'ESP32 DISCONNECTED';


  document.getElementById(
    'connectionDot'
  ).style.background =
    '#e33d4d';

}


/* ==========================================================
   UPDATE DASHBOARD
   ========================================================== */

function updateStatus() {

  fetch(
    '/api/status',
    {
      cache: 'no-store'
    }
  )

  .then(function(response) {

    if (!response.ok) {

      throw new Error(
        'Status error'
      );

    }

    return response.json();

  })

  .then(function(data) {


    /* Connection */

    document.getElementById(
      'connectionText'
    ).innerText =
      'ESP32 CONNECTED';


    document.getElementById(
      'connectionDot'
    ).style.background =
      '#20b866';


    /* Mode */

    document.getElementById(
      'mode'
    ).innerText =
      data.mode;


    document.getElementById(
      'modeBadge'
    ).innerText =
      data.mode;


    document.getElementById(
      'modeHint'
    ).innerText =
      data.mode;


    /* Distance */

    document.getElementById(
      'distance'
    ).innerText =
      Number(data.distance)
      .toFixed(1) +
      ' cm';


    /* PIR */

    document.getElementById(
      'motion'
    ).innerText =
      data.motion;


    /* MQ2 */

    document.getElementById(
      'gas'
    ).innerText =
      data.gas +
      ' - ' +
      data.gasStatus;


    /* Decision */

    document.getElementById(
      'decision'
    ).innerText =
      data.decision;


    /* Action */

    document.getElementById(
      'action'
    ).innerText =
      data.action;


    /* People / Motion counter */

    document.getElementById(
      'people'
    ).innerText =
      String(data.people)
      .padStart(2, '0');


    /* Emergency status */

    const emergency =
      document.getElementById(
        'emergencyStatus'
      );


    if (data.emergency) {

      emergency.innerText =
        'EMERGENCY ACTIVE';

      emergency.className =
        'emergency-value danger';

    }

    else {

      emergency.innerText =
        'SYSTEM SAFE';

      emergency.className =
        'emergency-value safe';

    }


    /* Mode buttons */

    const autoBtn =
      document.getElementById(
        'autoBtn'
      );

    const manualBtn =
      document.getElementById(
        'manualBtn'
      );


    autoBtn.classList.toggle(
      'active',
      data.mode === 'AUTO'
    );


    manualBtn.classList.toggle(
      'active',
      data.mode === 'MANUAL'
    );


    /* Manual controls */

    setControlsEnabled(
      data.mode === 'MANUAL' &&
      !data.emergency
    );

  })

  .catch(function() {

    setDisconnected();

  });

}


/* ==========================================================
   INITIAL LOAD
   ========================================================== */

setControlsEnabled(true);

updateStatus();


/* ==========================================================
   REAL-TIME UPDATE
   ========================================================== */

setInterval(
  updateStatus,
  500
);

</script>


</body>

</html>

)rawliteral";


  server.send(
    200,
    "text/html",
    html
  );
}


// ============================================================
// STATUS API
// ============================================================

void handleStatus() {

  String gasStatus;


  // ----------------------------------------------------------
  // GAS STATUS
  // ----------------------------------------------------------

  if (
    currentGasValue >=
    GAS_CRITICAL_LEVEL
  ) {

    gasStatus = "CRITICAL";

  }

  else if (
    currentGasValue >=
    GAS_WARNING_LEVEL
  ) {

    gasStatus = "WARNING";

  }

  else {

    gasStatus = "NORMAL";

  }


  // ----------------------------------------------------------
  // JSON
  // ----------------------------------------------------------

  String json = "{";


  // MODE

  json += "\"mode\":\"";

  json +=
    manualMode
    ? "MANUAL"
    : "AUTO";

  json += "\",";


  // DISTANCE

  json += "\"distance\":";

  json +=
    String(
      currentDistance,
      1
    );

  json += ",";


  // PIR

  json += "\"motion\":\"";

  json +=
    currentMotion == HIGH
    ? "DETECTED"
    : "NO MOTION";

  json += "\",";


  // GAS

  json += "\"gas\":";

  json +=
    String(
      currentGasValue
    );

  json += ",";


  // GAS STATUS

  json += "\"gasStatus\":\"";

  json += gasStatus;

  json += "\",";


  // DECISION

  json += "\"decision\":\"";

  json += currentDecision;

  json += "\",";


  // ACTION

  json += "\"action\":\"";

  json += currentAction;

  json += "\",";


  // PEOPLE / MOTION EVENTS

  json += "\"people\":";

  json +=
    String(
      peopleDetected
    );

  json += ",";


  // EMERGENCY

  json += "\"emergency\":";

  json +=
    emergencyStop
    ? "true"
    : "false";


  json += "}";


  server.send(
    200,
    "application/json",
    json
  );
}


// ============================================================
// COMMAND API
// ============================================================

void handleCommand() {

  // ----------------------------------------------------------
  // CHECK COMMAND
  // ----------------------------------------------------------

  if (!server.hasArg("cmd")) {

    server.send(
      400,
      "text/plain",
      "Missing command"
    );

    return;
  }


  String command =
    server.arg("cmd");

  command.toLowerCase();


  // ----------------------------------------------------------
  // EMERGENCY STOP
  // ----------------------------------------------------------

  if (
    command == "emergency"
  ) {

    emergencyStop = true;

    manualMode = true;

    autoTurning = false;


    // IMMEDIATE MOTOR STOP

    stopMotors();


    currentDecision =
      "EMERGENCY STOP";

    currentAction =
      "MOTORS STOPPED";


    // RED LED

    digitalWrite(
      GREEN_LED_PIN,
      LOW
    );

    digitalWrite(
      YELLOW_LED_PIN,
      LOW
    );

    digitalWrite(
      RED_LED_PIN,
      HIGH
    );


    // Buzzer off

    digitalWrite(
      BUZZER_PIN,
      LOW
    );


    server.send(
      200,
      "text/plain",
      "EMERGENCY STOP ACTIVATED"
    );

    return;
  }


  // ----------------------------------------------------------
  // RESET EMERGENCY
  // ----------------------------------------------------------

  if (
    command ==
    "reset_emergency"
  ) {

    emergencyStop = false;

    manualMode = true;

    autoTurning = false;


    stopMotors();


    currentDecision =
      "MANUAL MODE";

    currentAction =
      "READY";


    updateIndicators();


    server.send(
      200,
      "text/plain",
      "EMERGENCY RESET"
    );

    return;
  }


  // ----------------------------------------------------------
  // AUTO MODE
  // ----------------------------------------------------------

  if (
    command == "auto"
  ) {

    if (emergencyStop) {

      server.send(
        403,
        "text/plain",
        "RESET EMERGENCY FIRST"
      );

      return;
    }


    manualMode = false;

    autoTurning = false;


    stopMotors();


    currentDecision =
      makeDecision(
        currentDistance,
        currentMotion,
        currentGasValue
      );


    currentAction =
      "AUTO READY";


    updateIndicators();


    server.send(
      200,
      "text/plain",
      "AUTO MODE"
    );

    return;
  }


  // ----------------------------------------------------------
  // MANUAL MODE
  // ----------------------------------------------------------

  if (
    command == "manual"
  ) {

    if (emergencyStop) {

      server.send(
        403,
        "text/plain",
        "RESET EMERGENCY FIRST"
      );

      return;
    }


    manualMode = true;

    autoTurning = false;


    stopMotors();


    currentDecision =
      "MANUAL MODE";

    currentAction =
      "READY";


    updateIndicators();


    server.send(
      200,
      "text/plain",
      "MANUAL MODE"
    );

    return;
  }


  // ----------------------------------------------------------
  // BLOCK MOVEMENT DURING EMERGENCY
  // ----------------------------------------------------------

  if (emergencyStop) {

    stopMotors();


    server.send(
      403,
      "text/plain",
      "MOVEMENT BLOCKED - EMERGENCY STOP ACTIVE"
    );

    return;
  }


  // ----------------------------------------------------------
  // MOVEMENT COMMANDS
  // ----------------------------------------------------------

  manualMode = true;

  autoTurning = false;


  // FORWARD

  if (
    command == "forward"
  ) {

    moveForward(
      MOTOR_SPEED
    );

    currentAction =
      "MOVE FORWARD";

  }


  // BACKWARD

  else if (
    command == "back"
  ) {

    moveBackward(
      MOTOR_SPEED
    );

    currentAction =
      "MOVE BACKWARD";

  }


  // LEFT

  else if (
    command == "left"
  ) {

    turnLeft(
      MOTOR_SPEED
    );

    currentAction =
      "TURN LEFT";

  }


  // RIGHT

  else if (
    command == "right"
  ) {

    turnRight(
      MOTOR_SPEED
    );

    currentAction =
      "TURN RIGHT";

  }


  // STOP

  else if (
    command == "stop"
  ) {

    stopMotors();

    currentAction =
      "STOPPED";

  }


  // UNKNOWN

  else {

    server.send(
      400,
      "text/plain",
      "UNKNOWN COMMAND"
    );

    return;
  }


  updateIndicators();


  server.send(
    200,
    "text/plain",
    "COMMAND EXECUTED: " +
    command
  );
}