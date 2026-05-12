#include "Wire.h"
#include "I2Cdev.h"
#include "MPU6050.h"

MPU6050 mpu;

#define THRESHOLD_ENTER  5000
#define THRESHOLD_EXIT   3000
#define NUM_SAMPLES        15

int16_t ax, ay, az;

int32_t xBuf[NUM_SAMPLES];
int32_t yBuf[NUM_SAMPLES];
int     bufIdx    = 0;
int     bufFilled = 0;

bool stateFwd   = false;
bool stateBack  = false;
bool stateLeft  = false;
bool stateRight = false;

bool prevFwd   = false;
bool prevBack  = false;
bool prevLeft  = false;
bool prevRight = false;

// Runtime offset — zeroed from your hand position
int32_t offsetX = 0;
int32_t offsetY = 0;

int32_t bufAvg(int32_t* buf) {
  int32_t s = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) s += buf[i];
  return s / NUM_SAMPLES;
}

void updateState(int32_t x, int32_t y) {
  if      (y >  THRESHOLD_ENTER) stateFwd  = true;
  else if (y <  THRESHOLD_EXIT)  stateFwd  = false;

  if      (y < -THRESHOLD_ENTER) stateBack = true;
  else if (y > -THRESHOLD_EXIT)  stateBack = false;

  if      (x < -THRESHOLD_ENTER) stateLeft  = true;
  else if (x > -THRESHOLD_EXIT)  stateLeft  = false;

  if      (x >  THRESHOLD_ENTER) stateRight = true;
  else if (x <  THRESHOLD_EXIT)  stateRight = false;
}

// Call this any time to re-zero from current hand position
void calibrateFromHand() {
  int32_t sumX = 0, sumY = 0;
  for (int i = 0; i < 200; i++) {
    mpu.getAcceleration(&ax, &ay, &az);
    sumX += ax;
    sumY += ay;
    delay(5);
  }
  offsetX = sumX / 200;
  offsetY = sumY / 200;
}

void setup() {
  Wire.begin();
  Wire.setClock(400000);
  Serial.begin(115200);
  while (!Serial);

  mpu.initialize();
  if (!mpu.testConnection()) {
    Serial.println("ERR");
    while (true);
  }

  mpu.setXAccelOffset(0);
  mpu.setYAccelOffset(0);
  mpu.setZAccelOffset(0);
  mpu.setXGyroOffset(0);
  mpu.setYGyroOffset(0);
  mpu.setZGyroOffset(0);

  // Signal: hold your hand in idle/neutral position now
  Serial.println("HOLD your hand in neutral position...");
  delay(3000);  // 3 seconds to get ready

  calibrateFromHand();

  Serial.print("Offset X="); Serial.print(offsetX);
  Serial.print("  Y="); Serial.println(offsetY);

  for (int i = 0; i < NUM_SAMPLES; i++) { xBuf[i] = 0; yBuf[i] = 0; }

  Serial.println("READY");
}

void loop() {
  mpu.getAcceleration(&ax, &ay, &az);

  // Subtract the hand's natural resting offset
  int32_t correctedX = ax - offsetX;
  int32_t correctedY = ay - offsetY;

  xBuf[bufIdx] = correctedX;
  yBuf[bufIdx] = correctedY;
  bufIdx = (bufIdx + 1) % NUM_SAMPLES;
  if (bufFilled < NUM_SAMPLES) { bufFilled++; return; }

  int32_t smoothX = bufAvg(xBuf);
  int32_t smoothY = bufAvg(yBuf);

  updateState(smoothX, smoothY);

  // Re-zero on demand: send any character over Serial
  if (Serial.available()) {
    Serial.read();
    Serial.println("Re-calibrating...");
    calibrateFromHand();
    Serial.println("READY");
  }

  if (stateFwd   != prevFwd  ) { Serial.println(stateFwd   ? "FWD:1"   : "FWD:0");   prevFwd   = stateFwd;   }
  if (stateBack  != prevBack ) { Serial.println(stateBack  ? "BACK:1"  : "BACK:0");  prevBack  = stateBack;  }
  if (stateLeft  != prevLeft ) { Serial.println(stateLeft  ? "LEFT:1"  : "LEFT:0");  prevLeft  = stateLeft;  }
  if (stateRight != prevRight) { Serial.println(stateRight ? "RIGHT:1" : "RIGHT:0"); prevRight = stateRight; }
}