#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <HardwareSerial.h>
#include <DFRobotDFPlayerMini.h>
#include <BluetoothSerial.h>
#include <Preferences.h>

Adafruit_MPU6050 mpu;
HardwareSerial mySerial(1);
DFRobotDFPlayerMini player;
BluetoothSerial SerialBT;
Preferences preferences;

// Dual core task and mutex
TaskHandle_t bluetoothTask;
SemaphoreHandle_t btMutex;

const int touchPins[5] = {15, 2, 4, 5, 18};
int touchStates[5];
int prevTouchStates[5] = {0, 0, 0, 0, 0};

// Enhanced gesture structure with custom text support
struct Gesture {
  char letter;
  int audioFile;      // 0 = custom (play from phone), >0 = SD card file
  String customText;  // Text for custom gestures (played via phone)
  int touch[5];
  float bendX, bendZ;
  float tolerance;
};

// Define gestures - Pre-loaded gestures play from SD card
Gesture gestures[30] = {
  {'A', 21, "", {1,0,0,0,0}, 22.0, 100.0, 18.0},
  {'B', 22, "", {1,0,1,0,0}, 72.0, 63.0, 22.0},
  {'C', 23, "", {0,0,0,0,0}, 39.0, 84.0, 25.0},
  {'D', 24, "", {1,1,0,0,0}, 48.0, 86.0, 18.0},
  {'E', 25, "", {1,1,0,0,0}, 73.5, 68.9, 22.0},
  {'F', 26, "", {0,1,1,0,0}, 31.2, 94.4, 18.0},
  {'G', 27, "", {1,0,1,0,0}, 38.0, 49.0, 28.0},
  {'H', 28, "", {0,0,0,1,1}, 41.0, 90.0, 18.0},
  {'I', 29, "", {0,1,0,0,0}, 60.0, 81.2, 22.0},
  {'J', 30, "", {0,1,0,0,0}, 19.5, 95.3, 25.0},
  {'K', 31, "", {0,0,0,1,0}, 19.3, 95.5, 25.0},
  {'L', 32, "", {0,0,0,1,0}, 71.7, 72.5, 22.0},
  {'M', 33, "", {0,1,1,1,0}, 30.5, 92.8, 18.0},
  {'P', 49, "", {1,0,1,1,1}, 74.5, 67.0, 20.0},
  {'1', 1,  "", {1,1,1,1,1}, 12.3, 99.3, 22.0},
  {'2', 6,  "", {0,0,0,0,0}, 33.0, 27.5, 20.0}
};
int NUM_GESTURES = 16;
const int MAX_GESTURES = 30;

// Smoothing and filtering
const int SMOOTH_SAMPLES = 8;
float bendXBuffer[SMOOTH_SAMPLES] = {0};
float bendZBuffer[SMOOTH_SAMPLES] = {0};
int smoothIndex = 0;

// Gesture confirmation
const int CONFIRMATION_SAMPLES = 5;
int gestureHistory[CONFIRMATION_SAMPLES] = {-1, -1, -1, -1, -1};
int historyIndex = 0;

int lastPlayedGesture = -1; 
unsigned long lastGestureTime = 0;
const unsigned long GESTURE_COOLDOWN = 1500; 

const int CALIBRATION_BUTTON = 13;
const int MIN_CONFIDENCE = 60;

bool dfplayerWorking = false;
bool bluetoothConnected = false;

unsigned long lastMPUCheck = 0;
const unsigned long MPU_CHECK_INTERVAL = 5000;
int mpuFailCount = 0;
const int MAX_FAIL_COUNT = 3;

unsigned long lastTelemetryTime = 0;
const unsigned long TELEMETRY_INTERVAL = 200; 

// ========== FORWARD DECLARATIONS ==========
void processCommand(String command);
void addCustomGesture(String command);
void deleteCustomGesture(String command);
void listGestures();
void sendSensorData();
void saveCustomGestures();
void loadCustomGestures();
bool initDFPlayer();
bool initMPU6050();
void fillInitialBuffer();
bool recoverMPU6050();
bool checkMPUHealth();
int scoreGesture(Gesture &g, float bendX, float bendZ);
float median(float* arr, int size);
void enterCalibrationMode();
float mapFloat(float x, float in_min, float in_max, float out_min, float out_max);
void playGestureHybrid(int gestureIndex);

// ========== BLUETOOTH TASK (Core 0) ==========
void bluetoothTaskFunction(void *parameter) {
  // Set explicit dynamic stream serial processing timeouts to prevent WDT locks
  SerialBT.setTimeout(150);
  for (;;) {
    if (SerialBT.available()) {
      String command = SerialBT.readStringUntil('\n');
      command.trim();
      if (command.length() > 0) {
        bluetoothConnected = true;
        // Strict thread synchronization lock across hardware boundaries
        if (xSemaphoreTake(btMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
          processCommand(command);
          xSemaphoreGive(btMutex);
        } else {
          Serial.println("[BT Link] Mutex Lock Busy - Dropping Frame");
        }
      }
    }
    vTaskDelay(20 / portTICK_PERIOD_MS); 
  }
}

// ========== SETUP ==========
void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin();
  Wire.setClock(100000);

  mySerial.begin(9600, SERIAL_8N1, 16, 17);
  Serial.println("\n=== Smart Glove - ASL SYSTEM PRO ===");

  btMutex = xSemaphoreCreateMutex();
  if (btMutex == NULL) {
    Serial.println("CRITICAL: Semaphore Error!");
    delay(3000);
    ESP.restart();
  }

  SerialBT.begin("SmartGlove_ASL", false);
  Serial.println("✓ Bluetooth Engine Initialized");

  dfplayerWorking = initDFPlayer();
  if (!dfplayerWorking) {
    Serial.println("WARNING: DFPlayer Unresponsive - Using Bluetooth Audio Exclusively");
  }

  if (!initMPU6050()) {
    Serial.println("CRITICAL: IMU Bus Unresponsive!");
    delay(3000);
    ESP.restart();
  }

  for (int i = 0; i < 5; i++) {
    pinMode(touchPins[i], INPUT);
  }
  pinMode(CALIBRATION_BUTTON, INPUT_PULLUP);

  if (xSemaphoreTake(btMutex, portMAX_DELAY) == pdTRUE) {
    loadCustomGestures();
    xSemaphoreGive(btMutex);
  }
  
  fillInitialBuffer();

  xTaskCreatePinnedToCore(
    bluetoothTaskFunction,
    "BluetoothTask",
    4096,
    NULL,
    2, // Higher priority execution mapping
    &bluetoothTask,
    0
  );
  Serial.println("\n=== System Operational ===");
}

// ========== MAIN LOOP (Core 1) ==========
void loop() {
  unsigned long now = millis();

  // MPU health check
  if (now - lastMPUCheck > MPU_CHECK_INTERVAL) {
    lastMPUCheck = now;
    if (!checkMPUHealth()) {
      mpuFailCount++;
      if (mpuFailCount >= MAX_FAIL_COUNT) {
        Serial.println("[IMU] Hardware link lost. Triggering bus restart...");
        delay(500);
        ESP.restart();
      }
    } else {
      mpuFailCount = 0;
    }
  }

  // Calibration physical check
  if (digitalRead(CALIBRATION_BUTTON) == LOW) {
    enterCalibrationMode();
    return;
  }

  // Update hardware arrays
  for (int i = 0; i < 5; i++) {
    touchStates[i] = digitalRead(touchPins[i]);
  }

  sensors_event_t a, g, temp;
  if (!mpu.getEvent(&a, &g, &temp)) {
    if (!recoverMPU6050() || !mpu.getEvent(&a, &g, &temp)) {
      delay(30);
      return;
    }
  }

  bendXBuffer[smoothIndex] = mapFloat(abs(a.acceleration.x), 0, 10, 0, 100);
  bendZBuffer[smoothIndex] = mapFloat(abs(a.acceleration.z), 0, 10, 0, 100);
  smoothIndex = (smoothIndex + 1) % SMOOTH_SAMPLES;

  float bendX = median(bendXBuffer, SMOOTH_SAMPLES);
  float bendZ = median(bendZBuffer, SMOOTH_SAMPLES);

  int bestGesture = -1;
  int bestScore = 0;
  int secondBestScore = 0;

  // Protect read loop matching arrays with Thread Mutex Block Guard
  if (xSemaphoreTake(btMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
    for (int i = 0; i < NUM_GESTURES; i++) {
      int score = scoreGesture(gestures[i], bendX, bendZ);
      if (score > bestScore) {
        secondBestScore = bestScore;
        bestScore = score;
        bestGesture = i;
      } else if (score > secondBestScore) {
        secondBestScore = score;
      }
    }
    xSemaphoreGive(btMutex);
  }

  int confidenceMargin = bestScore - secondBestScore;

  if (bestGesture != -1 && bestScore >= MIN_CONFIDENCE && confidenceMargin >= 12) {
    gestureHistory[historyIndex] = bestGesture;
    historyIndex = (historyIndex + 1) % CONFIRMATION_SAMPLES;

    int matchCount = 0;
    for (int i = 0; i < CONFIRMATION_SAMPLES; i++) {
      if (gestureHistory[i] == bestGesture) matchCount++;
    }

    if (matchCount >= 4) {
      if (bestGesture != lastPlayedGesture) {
        if (xSemaphoreTake(btMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
          playGestureHybrid(bestGesture);
          xSemaphoreGive(btMutex);
        }
        lastPlayedGesture = bestGesture; 
        lastGestureTime = now;
      }
    }
  } else {
    if (bestScore < (MIN_CONFIDENCE - 10)) {
      for (int i = 0; i < CONFIRMATION_SAMPLES; i++) {
        gestureHistory[i] = -1;
      }
      lastPlayedGesture = -1; 
    }
  }

  // Synchronized telemetry packet pacing
  if (now - lastTelemetryTime > TELEMETRY_INTERVAL) {
    lastTelemetryTime = now;
    if (SerialBT.hasClient()) {
      SerialBT.print("SENSOR|");
      for (int i = 0; i < 5; i++) {
        SerialBT.print(touchStates[i]);
        if (i < 4) SerialBT.print(",");
      }
      SerialBT.print("|");
      SerialBT.print(bendX, 1);
      SerialBT.print("|");
      SerialBT.println(bendZ, 1);
    }
  }

  delay(25); 
}

// ========== HYBRID PLAYBACK SYSTEM ==========
void playGestureHybrid(int gestureIndex) {
  Gesture &g = gestures[gestureIndex];
  if (g.audioFile > 0) {
    if (dfplayerWorking) {
      player.play(g.audioFile);
    }
  } else {
    // Write exactly matching layout protocol mapping pattern cleanly
    SerialBT.print("PLAY|");
    SerialBT.println(g.customText);
  }
}

// ========== BLUETOOTH COMMAND PROCESSING ==========
void processCommand(String command) {
  if (command.startsWith("ADD|")) {
    addCustomGesture(command);
  } else if (command.startsWith("DELETE|")) {
    deleteCustomGesture(command);
  } else if (command == "LIST") {
    listGestures();
  } else if (command == "PING") {
    SerialBT.println("PONG");
  } else if (command == "CALIBRATE") {
    sendSensorData();
  }
}

void addCustomGesture(String command) {
  if (NUM_GESTURES >= MAX_GESTURES) {
    SerialBT.println("SUCCESS|Max storage capacity met");
    return;
  }

  int lastPos = 4; 
  String letter = "";
  String touchStr = "";
  float bX = 0, bZ = 0, tol = 20.0;
  String customText = "";

  int pos1 = command.indexOf('|', lastPos);
  if (pos1 == -1) { SerialBT.println("SUCCESS|Invalid parse"); return; }
  letter = command.substring(lastPos, pos1);
  lastPos = pos1 + 1;

  int pos2 = command.indexOf('|', lastPos);
  if (pos2 == -1) { SerialBT.println("SUCCESS|Invalid parse"); return; }
  touchStr = command.substring(lastPos, pos2);
  lastPos = pos2 + 1;

  int pos3 = command.indexOf('|', lastPos);
  if (pos3 == -1) { SerialBT.println("SUCCESS|Invalid parse"); return; }
  bX = command.substring(lastPos, pos3).toFloat();
  lastPos = pos3 + 1;

  int pos4 = command.indexOf('|', lastPos);
  if (pos4 == -1) { SerialBT.println("SUCCESS|Invalid parse"); return; }
  bZ = command.substring(lastPos, pos4).toFloat();
  lastPos = pos4 + 1;

  int pos5 = command.indexOf('|', lastPos);
  if (pos5 == -1) { SerialBT.println("SUCCESS|Invalid parse"); return; }
  tol = command.substring(lastPos, pos5).toFloat();
  lastPos = pos5 + 1;

  customText = command.substring(lastPos);

  int tArr[5] = {0, 0, 0, 0, 0};
  int idx = 0;
  // Fallback parsing engine logic matches comma separated input arrays seamlessly
  for (int i = 0; i < touchStr.length() && idx < 5; i++) {
    if (touchStr[i] == '0' || touchStr[i] == '1') {
      tArr[idx++] = touchStr[i] - '0';
    }
  }

  Gesture g;
  g.letter = (letter.length() > 0) ? letter.charAt(0) : 'X';
  g.audioFile = 0;
  g.customText = customText;
  for (int i = 0; i < 5; i++) g.touch[i] = tArr[i];
  g.bendX = bX;
  g.bendZ = bZ;
  g.tolerance = tol;

  gestures[NUM_GESTURES] = g;
  NUM_GESTURES++;

  saveCustomGestures();
  SerialBT.print("SUCCESS|Saved gesture ");
  SerialBT.println(g.letter);
}

void deleteCustomGesture(String command) {
  if (command.length() < 8) return;
  char target = command.charAt(7);
  bool found = false;

  for (int i = 0; i < NUM_GESTURES; i++) {
    if (gestures[i].letter == target && gestures[i].audioFile == 0) {
      for (int j = i; j < NUM_GESTURES - 1; j++) {
        gestures[j] = gestures[j + 1];
      }
      NUM_GESTURES--;
      found = true;
      break;
    }
  }

  if (found) {
    saveCustomGestures();
    SerialBT.print("SUCCESS|Removed gesture ");
    SerialBT.println(target);
  } else {
    SerialBT.println("SUCCESS|Gesture non-existent");
  }
}

void listGestures() {
  SerialBT.println("GESTURES_START");
  for (int i = 0; i < NUM_GESTURES; i++) {
    SerialBT.print(gestures[i].letter);
    SerialBT.print("|");
    SerialBT.print(gestures[i].audioFile > 0 ? "SD" : "CUSTOM");
    SerialBT.print("|");
    SerialBT.println(gestures[i].customText);
  }
  SerialBT.println("GESTURES_END");
}

void sendSensorData() {
  for (int i = 0; i < 5; i++) {
    touchStates[i] = digitalRead(touchPins[i]);
  }
  sensors_event_t a, g, temp;
  if (mpu.getEvent(&a, &g, &temp)) {
    float bX = mapFloat(abs(a.acceleration.x), 0, 10, 0, 100);
    float bZ = mapFloat(abs(a.acceleration.z), 0, 10, 0, 100);

    SerialBT.print("CALIBRATE_RESULT|");
    for (int i = 0; i < 5; i++) {
      SerialBT.print(touchStates[i]);
      if (i < 4) SerialBT.print(",");
    }
    SerialBT.print("|");
    SerialBT.print(bX, 1);
    SerialBT.print("|");
    SerialBT.println(bZ, 1);
  } else {
    SerialBT.println("SUCCESS|IMU communication exception");
  }
}

void saveCustomGestures() {
  preferences.begin("glove", false);
  int customCount = 0;
  for (int i = 0; i < NUM_GESTURES; i++) {
    if (gestures[i].audioFile == 0) {
      String key = "g" + String(customCount);
      String data = String(gestures[i].letter) + "|";
      for (int j = 0; j < 5; j++) data += String(gestures[i].touch[j]) + ",";
      data += "|" + String(gestures[i].bendX, 1);
      data += "|" + String(gestures[i].bendZ, 1);
      data += "|" + String(gestures[i].tolerance, 1);
      data += "|" + gestures[i].customText;
      
      preferences.putString(key.c_str(), data);
      customCount++;
    }
  }
  preferences.putInt("count", customCount);
  preferences.end();
}

void loadCustomGestures() {
  preferences.begin("glove", true);
  int customCount = preferences.getInt("count", 0);

  for (int i = 0; i < customCount && NUM_GESTURES < MAX_GESTURES; i++) {
    String key = "g" + String(i);
    String data = preferences.getString(key.c_str(), "");

    if (data.length() > 0) {
      int pos = 0;
      int nextPos = data.indexOf('|', pos);

      Gesture g;
      g.letter = data.substring(pos, nextPos).charAt(0);
      pos = nextPos + 1;

      nextPos = data.indexOf('|', pos);
      String touchStr = data.substring(pos, nextPos);
      
      int tArr[5] = {0,0,0,0,0};
      int tIdx = 0;
      for(int k=0; k<touchStr.length() && tIdx<5; k++) {
        if(touchStr[k] == '0' || touchStr[k] == '1') {
          tArr[tIdx++] = touchStr[k] - '0';
        }
      }
      for(int m=0; m<5; m++) g.touch[m] = tArr[m];
      pos = nextPos + 1;

      nextPos = data.indexOf('|', pos);
      g.bendX = data.substring(pos, nextPos).toFloat();
      pos = nextPos + 1;

      nextPos = data.indexOf('|', pos);
      g.bendZ = data.substring(pos, nextPos).toFloat();
      pos = nextPos + 1;
      
      nextPos = data.indexOf('|', pos);
      g.tolerance = data.substring(pos, nextPos).toFloat();
      pos = nextPos + 1;

      g.customText = data.substring(pos);
      g.audioFile = 0;
      
      gestures[NUM_GESTURES] = g;
      NUM_GESTURES++;
    }
  }
  preferences.end();
}

bool initDFPlayer() {
  delay(100);
  mySerial.begin(9600, SERIAL_8N1, 16, 17);
  if (player.begin(mySerial, false, false)) {  
    delay(200);
    player.volume(30);
    return true;
  }
  return false;
}

bool initMPU6050() {
  int retries = 3;
  while (retries > 0) {
    if (mpu.begin()) {
      mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
      mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
      return true;
    }
    retries--;
    Wire.end();
    delay(50);
    Wire.begin();
    Wire.setClock(100000);
    delay(50);
  }
  return false;
}

void fillInitialBuffer() {
  for (int i = 0; i < SMOOTH_SAMPLES; i++) {
    sensors_event_t a, g, temp;
    if (mpu.getEvent(&a, &g, &temp)) {
      bendXBuffer[i] = mapFloat(abs(a.acceleration.x), 0, 10, 0, 100);
      bendZBuffer[i] = mapFloat(abs(a.acceleration.z), 0, 10, 0, 100);
    }
    delay(10);
  }
}

bool recoverMPU6050() {
  Wire.end();
  delay(50);
  Wire.begin();
  Wire.setClock(100000);
  delay(50);

  if (mpu.begin()) {
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    return true;
  }
  return false;
}

bool checkMPUHealth() {
  sensors_event_t a, g, temp;
  return mpu.getEvent(&a, &g, &temp);
}

int scoreGesture(Gesture &g, float bendX, float bendZ) {
  int score = 0;
  int touchMatches = 0;
  int touchTotal = 0;

  for (int i = 0; i < 5; i++) {
    if (g.touch[i] != -1) {
      touchTotal++;
      if (touchStates[i] == g.touch[i]) {
        touchMatches++;
      }
    }
  }

  if (touchTotal > 0) {
    score += (touchMatches * 50) / touchTotal;
  } else {
    score += 50;
  }

  float tol = (g.tolerance <= 0.0) ? 5.0 : g.tolerance;
  float xDiff = abs(bendX - g.bendX);
  float zDiff = abs(bendZ - g.bendZ);

  int xScore = 0;
  int zScore = 0;

  if (xDiff < tol) {
    xScore = (int)(25 * (1.0 - (xDiff / tol)));
  }
  if (zDiff < tol) {
    zScore = (int)(25 * (1.0 - (zDiff / tol)));
  }

  score += xScore + zScore;
  return min(score, 100);
}

float median(float* arr, int size) {
  float sorted[size];
  memcpy(sorted, arr, size * sizeof(float));

  for (int i = 0; i < size - 1; i++) {
    for (int j = 0; j < size - i - 1; j++) {
      if (sorted[j] > sorted[j + 1]) {
        float temp = sorted[j];
        sorted[j] = sorted[j + 1];
        sorted[j + 1] = temp;
      }
    }
  }
  return sorted[size / 2];
}

void enterCalibrationMode() {
  float sumX = 0, sumZ = 0;
  int samples = 50;
  int successfulSamples = 0;
  for (int i = 0; i < samples; i++) {
    sensors_event_t a, g, temp;
    if (mpu.getEvent(&a, &g, &temp)) {
      sumX += mapFloat(abs(a.acceleration.x), 0, 10, 0, 100);
      sumZ += mapFloat(abs(a.acceleration.z), 0, 10, 0, 100);
      successfulSamples++;
    }
    delay(10);
  }

  if (successfulSamples > 0) {
    float avgX = sumX / successfulSamples;
    float avgZ = sumZ / successfulSamples;

    for (int i = 0; i < 5; i++) {
      touchStates[i] = digitalRead(touchPins[i]);
    }

    SerialBT.print("CALIBRATE_RESULT|");
    for (int i = 0; i < 5; i++) {
      SerialBT.print(touchStates[i]);
      if (i < 4) SerialBT.print(",");
    }
    SerialBT.print("|");
    SerialBT.print(avgX, 1);
    SerialBT.print("|");
    SerialBT.println(avgZ, 1);
  }
}

float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}