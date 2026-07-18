/*
 * ESP-NOW 接收器完整代码（固定MAC版）
 * 
 * 硬件配置：
 * - PPM输出 : GPIO4
 * - 状态LED : GPIO2
 * - UART    : GPIO6, 7
 * - 蜂鸣器  : GPIO3
 */
#include <Arduino.h>
#include <VescUart.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include "beep_profiles.h"
#include "control_logic.h"
#include "diagnostic_protocol.h"
#include "protocol.h"
#include "receiver_frame.h"
#ifdef FAN_CONTROLLER_HIL
#include "hil_protocol.h"
#include "hil_safety.h"
#endif
// ========== 引脚定义 ==========
#define PWM_OUT_1 4  //SERVO
#define LED_STATUS 2
#define BUZZER_PIN 3

// ========== PWM配置 ==========
#define PWM_FREQ 50
#define PWM_RES 10
// #define PWM_MIN 512
// #define PWM_MAX 1024

#define PWM_MIN 51   // 1.0ms @ 50Hz 10bit
#define PWM_MAX 102  // 2.0ms @ 50Hz 10bit

// ========== 参数配置 ==========
#define TELEMETRY_RATE_HZ 20
#define BATTERY_READ_HZ 5
#define FAILSAFE_TIMEOUT 2000
#define LINK_ALERT_INTERVAL 2000
#define LINK_ALERT_BEEP_MS 200
#define REMOTE_HORN_MAX_MS 3000
#define CONNECTION_BEEP_MS 160
#define ESPNOW_CHANNEL 1

VescUart VESC;

// ========== 发射器MAC地址 ==========
// C3 基础版: AC:EB:E6:44:D5:54
// S3 高级版: 48:CA:43:9A:A9:B0
uint8_t transmitterMac[] = { 0xAC, 0xEB, 0xE6, 0x44, 0xD5, 0x54 };
uint8_t s3TransmitterMac[] = { 0x48, 0xCA, 0x43, 0x9A, 0xA9, 0xB0 };

// ========== 数据结构 ==========
#pragma pack(push, 1)

typedef struct {
  uint8_t head;
  uint8_t type;
  uint8_t version;
  uint16_t sequence;
  int16_t rssi;
  uint16_t voltage;
  uint8_t motorPWM[4];
  uint16_t speed;  //速度值（如km/h×10或RPM）
  uint8_t status;
  uint8_t crc;
} StatusPacket;

typedef struct {
  uint8_t head;
  uint8_t type;
  int16_t rssi;
  uint16_t voltage;
  uint8_t motorPWM[4];
  uint16_t speed;
  uint8_t status;
  uint8_t checksum;
} LegacyStatusPacket;

#pragma pack(pop)

// ========== 全局变量 ==========
hw_timer_t *timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

volatile bool sendTelemetryFlag = false;
volatile bool readBatteryFlag = false;
volatile uint32_t timerCounter = 0;

volatile bool connected = false;
volatile uint32_t lastRecvTime = 0;
volatile int16_t lastRssi = -100;

volatile int16_t throttle = 0;
volatile int16_t speed = 0;
volatile uint8_t speedLevel = 1;
volatile uint8_t buttons = 0;
uint8_t statusTargetMac[6] = {};
bool hasStatusTarget = false;
bool statusTargetUsesLegacyProtocol = false;
uint16_t lastControlSequence = 0;
bool hasControlSequence = false;
uint16_t statusSequence = 0;
uint8_t stableControlPacketCount = 0;
uint32_t diagnosticReceivedPackets = 0;
uint32_t diagnosticLostPackets = 0;
uint32_t diagnosticFaultCount = 0;
uint32_t diagnosticIgnoredPackets = 0;
uint32_t diagnosticMaxPacketGapMs = 0;
uint32_t diagnosticLastPacketMs = 0;
uint32_t diagnosticFailsafeCount = 0;
ControllerSourceState controllerSource = {};

int16_t smoothedThrottle = 0;
int16_t lastPwmValue = -1;
// uint8_t motorPWMValues[4] = { 0, 0, 0, 0 };
float batteryVoltage = 0.0;
uint16_t batteryVoltageX100 = 0;
bool failsafeActive = false;
bool protocolFault = false;
bool vescTelemetryValid = false;
bool sourceOutputLocked = true;
uint32_t failsafeBeepUntil = 0;
uint32_t lastLinkAlertTime = 0;
bool linkAlertHasFired = false;
uint32_t remoteHornStartTime = 0;
bool remoteHornActive = false;
bool remoteHornTimeoutReported = false;
volatile bool connectionBeepPending = false;
uint32_t buzzerHoldUntil = 0;

#ifdef FAN_CONTROLLER_HIL
static constexpr uint32_t HIL_OUTPUT_WATCHDOG_MS = 10000;
HilOutputGate hilOutputGate = hilInitialOutputGate((PWM_MIN + PWM_MAX) / 2);
char hilLastError[24] = "ok";
uint32_t hilLastSequence = 0;
uint8_t hilLastFrame[sizeof(ReceiverControlFrameV2)] = {};
size_t hilLastFrameLength = 0;
uint8_t hilLastFrameMac[6] = {};
ReceiverFrameResult hilLastFrameResult = RECEIVER_FRAME_BAD_LENGTH;
enum HilVescMode : uint8_t { HIL_VESC_PHYSICAL, HIL_VESC_VALUE, HIL_VESC_FAULT };
HilVescMode hilVescMode = HIL_VESC_PHYSICAL;
int32_t hilVescVoltageX100 = 0;
int32_t hilVescRpm = 0;
#endif

const int pwmChannels[] = { 0, 1, 2, 3 };

bool isBoundTransmitter(const uint8_t *mac) {
  return memcmp(mac, transmitterMac, 6) == 0 || memcmp(mac, s3TransmitterMac, 6) == 0;
}

const char *controllerNameForMac(const uint8_t *mac) {
  if (controllerMacEquals(mac, transmitterMac)) {
    return "c3";
  }
  if (controllerMacEquals(mac, s3TransmitterMac)) {
    return "s3";
  }
  return "unknown";
}

const uint8_t *macForControllerName(const char *name) {
  if (strcmp(name, "c3") == 0 || strcmp(name, "transmitter") == 0 || strcmp(name, "a") == 0) {
    return transmitterMac;
  }
  if (strcmp(name, "s3") == 0 || strcmp(name, "s3_transmitter") == 0 || strcmp(name, "b") == 0) {
    return s3TransmitterMac;
  }
  return nullptr;
}

void zeroReceiverOutputForSourceSwitch() {
  throttle = 0;
  speedLevel = 1;
  buttons = 0;
  smoothedThrottle = 0;
  stableControlPacketCount = 0;
  sourceOutputLocked = true;
}

bool activeControllerIsOnline(uint32_t nowMs) {
  return controllerSource.hasActiveController &&
         nowMs - controllerSource.lastSeenMs <= FAILSAFE_TIMEOUT &&
         !failsafeActive;
}

void applyControlPacketFromSource(
  const uint8_t *mac,
  int16_t receivedThrottle,
  uint8_t receivedSpeedLevel,
  uint8_t receivedButtons,
  uint8_t receivedFlags,
  bool legacyPacket,
  bool hasSequence,
  uint16_t receivedSequence
) {
  if (!isBoundTransmitter(mac)) {
    return;
  }

  const uint32_t nowMs = millis();
  releaseActiveControllerIfTimedOut(controllerSource, nowMs, FAILSAFE_TIMEOUT);

  const bool takeoverRequested = !legacyPacket && ((receivedFlags & CONTROL_FLAG_TAKEOVER_REQUEST) != 0);
  const bool wasActive = controllerSourceIsActive(mac, controllerSource);
  const bool activeOnline = activeControllerIsOnline(nowMs);
  if (!controllerSourceAllowsPacket(mac, controllerSource, activeOnline, takeoverRequested)) {
    diagnosticIgnoredPackets++;
    return;
  }

  const bool resetForNewSource = !wasActive || controllerSourceShouldResetForTakeover(mac, controllerSource, takeoverRequested);
  if (resetForNewSource) {
    zeroReceiverOutputForSourceSwitch();
    hasControlSequence = false;
  }

  if (hasSequence) {
    if (!protocolSequenceIsFresh(receivedSequence, lastControlSequence, hasControlSequence)) {
      protocolFault = true;
      diagnosticFaultCount++;
      return;
    }
    protocolRememberSequence(receivedSequence, lastControlSequence, hasControlSequence);
  }
  updateReceiverPacketGapDiagnostics(nowMs, diagnosticLastPacketMs, diagnosticMaxPacketGapMs);

  const bool signalConnectionSuccess = shouldSignalReceiverConnectionSuccess(connected, failsafeActive);

  rememberActiveController(mac, controllerSource, legacyPacket, nowMs);
  rememberStatusTarget(mac, statusTargetMac, hasStatusTarget);
  statusTargetUsesLegacyProtocol = legacyPacket;
  sourceOutputLocked = legacyPacket ? false : (receivedFlags & STATUS_FLAG_OUTPUT_LOCKED) != 0;
  if (stableControlPacketCount < 3) {
    stableControlPacketCount++;
  }
  if (stableControlPacketCount < 3) {
    throttle = 0;
    speedLevel = 1;
    buttons = 0;
    lastRecvTime = nowMs;
    protocolFault = false;
    return;
  }
  throttle = receivedThrottle;
  speedLevel = receivedSpeedLevel;
  buttons = receivedButtons;
  diagnosticReceivedPackets++;
  lastRecvTime = nowMs;
  connected = true;
  failsafeActive = false;
  protocolFault = false;
  if (signalConnectionSuccess) {
    connectionBeepPending = true;
  }

  digitalWrite(LED_STATUS, !digitalRead(LED_STATUS));
}

ReceiverFrameResult processControlFrame(const uint8_t *mac, const uint8_t *data, size_t len) {
  if (!isBoundTransmitter(mac)) {
    diagnosticIgnoredPackets++;
    return RECEIVER_FRAME_BAD_HEADER;
  }
  ReceiverDecodedControl decoded = {};
  const ReceiverFrameResult result = receiverDecodeControlFrame(data, len, decoded);
  if (result != RECEIVER_FRAME_OK) {
    protocolFault = true;
    diagnosticFaultCount++;
    return result;
  }
  applyControlPacketFromSource(mac,
                               decoded.throttle,
                               decoded.speedLevel,
                               decoded.buttons,
                               decoded.flags,
                               decoded.legacy,
                               decoded.hasSequence,
                               decoded.sequence);
  return RECEIVER_FRAME_OK;
}

// ========== 硬件定时器中断 ==========
void IRAM_ATTR onTimer() {
  portENTER_CRITICAL_ISR(&timerMux);
  timerCounter++;
  if (timerCounter % 5 == 0) sendTelemetryFlag = true;
  if (timerCounter % 20 == 0) readBatteryFlag = true;
  portEXIT_CRITICAL_ISR(&timerMux);
}

void beep(uint16_t frequency_hz, uint16_t duration_ms) {
#ifdef FAN_CONTROLLER_HIL
  hilOutputGate.expectedBuzzer = true;
  if (!hilActualBuzzer(hilOutputGate)) {
    noTone(BUZZER_PIN);
    buzzerHoldUntil = millis() + duration_ms;
    return;
  }
#endif
  tone(BUZZER_PIN, frequency_hz, duration_ms);  // 自动停止
  buzzerHoldUntil = millis() + duration_ms;
}

void alarmBeep() {
  static uint32_t last = 0;
  if (millis() - last > 100) {
    last = millis();
    tone(BUZZER_PIN, BEEP_FREQ_FAILSAFE, 100);  // 100ms鸣叫
  }
}

void stopBuzzer() {
#ifdef FAN_CONTROLLER_HIL
  hilOutputGate.expectedBuzzer = false;
#endif
  noTone(BUZZER_PIN);
}

// ========== 初始化 ==========
void setupPins() {
  // for (int i = 0; i < 4; i++) {
  //   ledcSetup(pwmChannels[i], PWM_FREQ, PWM_RES);
  // }
  // ledcAttachPin(PWM_OUT_1, pwmChannels[0]);
  // ledcAttachPin(PWM_OUT_2, pwmChannels[1]);
  // ledcAttachPin(PWM_OUT_3, pwmChannels[2]);
  // ledcAttachPin(PWM_OUT_4, pwmChannels[3]);
  ledcSetup(pwmChannels[0], PWM_FREQ, PWM_RES);
  ledcAttachPin(PWM_OUT_1, pwmChannels[0]);

  pinMode(LED_STATUS, OUTPUT);
  digitalWrite(LED_STATUS, LOW);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // analogReadResolution(12);
  // analogSetAttenuation(ADC_11db);
}

void setupESPNOW() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  // 固定信道，必须与发射端一致。
  WiFi.channel(ESPNOW_CHANNEL);
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW初始化失败！");
    return;
  }

  // 添加固定发射器配对
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, transmitterMac, 6);
  peer.channel = ESPNOW_CHANNEL;
  peer.encrypt = false;

  if (esp_now_add_peer(&peer) != ESP_OK) {
    Serial.println("添加发射器失败！");
    return;
  }

  esp_now_peer_info_t s3Peer = {};
  memcpy(s3Peer.peer_addr, s3TransmitterMac, 6);
  s3Peer.channel = ESPNOW_CHANNEL;
  s3Peer.encrypt = false;
  esp_now_del_peer(s3TransmitterMac);
  if (esp_now_add_peer(&s3Peer) != ESP_OK) {
    Serial.println("添加S3发射器失败！");
  }
  Serial.println("发射器配对成功");

  // 接收回调
  esp_now_register_recv_cb([](const uint8_t *mac, const uint8_t *data, int len) {
    // Serial.printf("收到数据！len=%d, mac=%02X:%02X:%02X:%02X:%02X:%02X\n",
    //               len, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    // MAC过滤：只接受绑定的发射器
    if (!isBoundTransmitter(mac)) {
      return;
    }
    // // 使用底层API获取RSSI
    wifi_ap_record_t ap_info;
    esp_wifi_sta_get_ap_info(&ap_info);
    lastRssi = ap_info.rssi;  // 这个方法更可靠

    processControlFrame(mac, data, static_cast<size_t>(len));
    lastRssi = WiFi.RSSI();
  });

  // 发送回调
  esp_now_register_send_cb([](const uint8_t *, esp_now_send_status_t) {});

  Serial.println("ESP-NOW初始化完成");
}

void setupTimer() {
  timer = timerBegin(1, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 10000, true);
  timerAlarmEnable(timer);
}

// ========== 电机控制 ==========
void updateMotors() {
  // int maxThrottle = 1000;
  // switch (speedLevel) {
  //   case 1: maxThrottle = 500; break;
  //   case 2: maxThrottle = 750; break;
  //   case 3: maxThrottle = 1000; break;
  // }

  // int limitedThrottle = constrain(throttle, -maxThrottle, maxThrottle);
  // int pwmValue = map(limitedThrottle, -1000, 1000, PWM_MIN, PWM_MAX);

  // for (int i = 0; i < 4; i++) {
  //   ledcWrite(pwmChannels[i], pwmValue);
  //   motorPWMValues[i] = map(pwmValue, PWM_MIN, PWM_MAX, 0, 255);
  // }

  // static uint32_t lastPrint = 0;
  // if (millis() - lastPrint > 1000) {
  //   lastPrint = millis();
  //   Serial.printf("THR:%4d SPD:%d BTN:%02X %s\n",
  //                 limitedThrottle, speedLevel, buttons,
  //                 failsafeActive ? "[FAILSAFE]" : "");
  // }


  int limitedThrottle = constrainedThrottle(throttle, speedLevel);
  int pwmValue = pwmDutyForThrottle(throttle, speedLevel, PWM_MIN, PWM_MAX);

  // 仅输出到 GPIO4 (VESC servo)
#ifdef FAN_CONTROLLER_HIL
  hilSetExpectedPwm(hilOutputGate, pwmValue);
  pwmValue = hilActualPwm(hilOutputGate);
#endif
  ledcWrite(pwmChannels[0], pwmValue);
  lastPwmValue = pwmValue;
  // motorPWMValues[0] = map(pwmValue, PWM_MIN, PWM_MAX, 0, 255);
  
  
  static uint32_t lastPrint = 0;
  if (millis() - lastPrint > 1000) {
    lastPrint = millis();
    Serial.printf("THR:%4d SPD:%d BTN:%02X %s\n",
                  limitedThrottle, speedLevel, buttons,
                  failsafeActive ? "[FAILSAFE]" : "");

    Serial.printf("PWM value: %d (throttle=%d)\n", pwmValue, limitedThrottle);
  }
}

// ========== 失控保护 ==========
void checkFailsafe() {
  if (millis() - lastRecvTime > FAILSAFE_TIMEOUT && !failsafeActive) {
    ReceiverControlState safeState = {
      throttle,
      speedLevel,
      buttons,
      connected,
      failsafeActive,
    };
    applyReceiverFailsafe(safeState);
    throttle = safeState.throttle;
    speedLevel = safeState.speedLevel;
    buttons = safeState.buttons;
    connected = safeState.connected;
    failsafeActive = safeState.failsafeActive;
    stableControlPacketCount = 0;
    controllerSource.hasActiveController = false;
    hasControlSequence = false;
    sourceOutputLocked = true;
    failsafeBeepUntil = millis() + 1000;
    diagnosticFailsafeCount++;
    Serial.println("失控保护启动！");
    beep(BEEP_FREQ_FAILSAFE, 1000);
    // for (int i = 0; i < 5; i++) {
    //   beep(100);
    //   delay(100);
    // }
  }
}

void updateLinkAlert() {
  if (shouldEmitReceiverLinkAlert(connected, failsafeActive, millis(), lastLinkAlertTime, linkAlertHasFired, LINK_ALERT_INTERVAL)) {
    Serial.println("遥控器未连接，接收端提示音");
    beep(BEEP_FREQ_LINK_ALERT, LINK_ALERT_BEEP_MS);
    failsafeBeepUntil = millis() + LINK_ALERT_BEEP_MS;
  }
}

bool updateRemoteHorn() {
  const bool requested = buttons & 0x02;
  const bool allowed = shouldAllowRemoteHorn(requested, millis(), remoteHornStartTime, remoteHornActive, REMOTE_HORN_MAX_MS);

  if (!requested) {
    remoteHornTimeoutReported = false;
    return false;
  }

  if (!allowed && !remoteHornTimeoutReported) {
    remoteHornTimeoutReported = true;
    Serial.println("远程蜂鸣超时保护，已停止");
  }

  return allowed;
}

void updateConnectionBeep() {
  if (!connectionBeepPending) {
    return;
  }

  connectionBeepPending = false;
  Serial.println("遥控器连接成功，接收端提示音");
  beep(BEEP_FREQ_CONNECTED, CONNECTION_BEEP_MS);
}

// ========== 电池检测 ==========
void readBattery() {
  // long sum = 0;
  // for (int i = 0; i < 10; i++) {
  //   sum += analogRead(BATTERY_PIN);
  //   delayMicroseconds(100);
  // }
  // int raw = sum / 10;
  // batteryVoltage = raw * 3.3 / 4095.0 * 2.0;
  // batteryVoltageX100 = (uint16_t)(batteryVoltage * 100);

  // if (batteryVoltage < 6.0) {
  //   static uint32_t lastAlarm = 0;
  //   if (millis() - lastAlarm > 2000) {
  //     lastAlarm = millis();
  //     beep(200);
  //     Serial.printf("电池低压: %.2fV\n", batteryVoltage);
  //   }
  // }
  batteryVoltageX100 = 4800;
}

// ========== 回传数据 ==========
void sendTelemetry() {
  // lastRssi = WiFi.RSSI();
  if (!hasStatusTarget) {
    return;
  }
  if (statusTargetUsesLegacyProtocol) {
    LegacyStatusPacket pkt = {};
    pkt.head = STATUS_PACKET_HEAD;
    pkt.type = STATUS_PACKET_TYPE;
    pkt.rssi = lastRssi;
    pkt.voltage = batteryVoltageX100;
    pkt.motorPWM[0] = (uint8_t)clampInt(mapLong(lastPwmValue, PWM_MIN, PWM_MAX, 0, 255), 0, 255);
    pkt.speed = speed;
    pkt.status = failsafeActive ? STATUS_FLAG_FAILSAFE : 0;
    pkt.checksum = protocolLegacyChecksum((const uint8_t *)&pkt, sizeof(LegacyStatusPacket));
    esp_now_send(statusTargetMac, (uint8_t *)&pkt, sizeof(pkt));
    return;
  }

  StatusPacket pkt = {};
  pkt.head = STATUS_PACKET_HEAD;
  pkt.type = STATUS_PACKET_TYPE;
  pkt.version = STATUS_PROTOCOL_VERSION;
  pkt.sequence = statusSequence++;
  pkt.rssi = lastRssi;
  pkt.voltage = batteryVoltageX100;
  pkt.motorPWM[0] = (uint8_t)clampInt(mapLong(lastPwmValue, PWM_MIN, PWM_MAX, 0, 255), 0, 255);
  pkt.speed = speed;  // ← 新增：示例：0-100km/h
  pkt.status = (failsafeActive ? STATUS_FLAG_FAILSAFE : 0) |
               (vescTelemetryValid ? STATUS_FLAG_VESC_VALID : 0) |
               (protocolFault ? STATUS_FLAG_PROTOCOL_FAULT : 0) |
               (sourceOutputLocked ? STATUS_FLAG_OUTPUT_LOCKED : 0);
  pkt.crc = protocolCrc8((const uint8_t *)&pkt, sizeof(StatusPacket) - 1);

  esp_now_send(statusTargetMac, (uint8_t *)&pkt, sizeof(pkt));
}

void printDiagnosticStatus() {
  char line[160] = {};
  diagnosticFormatStatusLine(line,
                             sizeof(line),
                             "receiver",
                             connected,
                             diagnosticReceivedPackets,
                             diagnosticLostPackets,
                             diagnosticFaultCount);
  Serial.printf("%s ignored=%lu active=%s max_gap_ms=%lu failsafes=%lu\n",
                line,
                (unsigned long)diagnosticIgnoredPackets,
                controllerSource.hasActiveController ? controllerNameForMac(controllerSource.activeMac) : "none",
                (unsigned long)diagnosticMaxPacketGapMs,
                (unsigned long)diagnosticFailsafeCount);
}

void handleDiagnosticCommand(const String &line) {
  if (line == "DIAG PING") {
    Serial.println("DIAG PONG role=receiver protocol=2 legacy=1");
    return;
  }
  if (line == "DIAG STATUS") {
    printDiagnosticStatus();
    return;
  }
#ifdef FAN_CONTROLLER_HIL
  if (line.startsWith("DIAG SIMCTRL ")) {
    int throttleValue = 0;
    int speedValue = 1;
    int buttonValue = 0;
    int flagValue = 0;
    if (sscanf(line.c_str(), "DIAG SIMCTRL %d %d %d %d", &throttleValue, &speedValue, &buttonValue, &flagValue) != 4) {
      Serial.println("DIAG ERR simctrl");
      diagnosticFaultCount++;
      return;
    }
    for (uint8_t i = 0; i < 3; i++) {
      applyControlPacketFromSource(transmitterMac,
                                   (int16_t)clampInt(throttleValue, -1000, 1000),
                                   (uint8_t)clampInt(speedValue, 1, 3),
                                   (uint8_t)clampInt(buttonValue, 0, 255),
                                   (uint8_t)clampInt(flagValue, 0, 255),
                                   false,
                                   false,
                                   0);
    }
    Serial.println("DIAG OK simctrl");
    return;
  }
  if (line.startsWith("DIAG SIMCTRLFROM ")) {
    char sourceName[20] = {};
    int throttleValue = 0;
    int speedValue = 1;
    int buttonValue = 0;
    int flagValue = 0;
    if (sscanf(line.c_str(), "DIAG SIMCTRLFROM %19s %d %d %d %d", sourceName, &throttleValue, &speedValue, &buttonValue, &flagValue) != 5) {
      Serial.println("DIAG ERR simctrlfrom");
      diagnosticFaultCount++;
      return;
    }
    const uint8_t *sourceMac = macForControllerName(sourceName);
    if (sourceMac == nullptr) {
      Serial.println("DIAG ERR source");
      diagnosticFaultCount++;
      return;
    }
    applyControlPacketFromSource(sourceMac,
                                 (int16_t)clampInt(throttleValue, -1000, 1000),
                                 (uint8_t)clampInt(speedValue, 1, 3),
                                 (uint8_t)clampInt(buttonValue, 0, 255),
                                 (uint8_t)clampInt(flagValue, 0, 255),
                                 false,
                                 false,
                                 0);
    Serial.println("DIAG OK simctrlfrom");
    return;
  }
#endif
  if (line.length() > 0) {
    Serial.println("DIAG ERR unknown");
    diagnosticFaultCount++;
  }
}

void updateDiagnosticSerial() {
  static String line;
  while (Serial.available() > 0) {
    const char c = (char)Serial.read();
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      handleDiagnosticCommand(line);
      line = "";
    } else if (line.length() < 96) {
      line += c;
    } else {
      line = "";
      diagnosticFaultCount++;
      Serial.println("DIAG ERR overflow");
    }
  }
}

#ifdef FAN_CONTROLLER_HIL
void sendHilAck(uint32_t sequence, bool ok, const char *error = nullptr) {
  Serial.printf("{\"type\":\"ack\",\"sequence\":%lu,\"ok\":%s",
                static_cast<unsigned long>(sequence), ok ? "true" : "false");
  if (error != nullptr) Serial.printf(",\"error\":\"%s\"", error);
  Serial.println("}");
}

bool hilDecodeHex(const char *text, uint8_t *output, size_t capacity, size_t &length) {
  length = 0;
  if (text == nullptr) return false;
  const size_t textLength = strlen(text);
  if (textLength == 0 || (textLength & 1u) != 0 || textLength / 2 > capacity) return false;
  for (size_t i = 0; i < textLength; i += 2) {
    char pair[3] = {text[i], text[i + 1], '\0'};
    char *end = nullptr;
    const unsigned long value = strtoul(pair, &end, 16);
    if (end != pair + 2 || value > 255) return false;
    output[length++] = static_cast<uint8_t>(value);
  }
  return true;
}

void rememberHilFrame(const uint8_t *mac, const uint8_t *data, size_t length, ReceiverFrameResult result) {
  memcpy(hilLastFrameMac, mac, sizeof(hilLastFrameMac));
  hilLastFrameLength = min(length, sizeof(hilLastFrame));
  memcpy(hilLastFrame, data, hilLastFrameLength);
  hilLastFrameResult = result;
}

ReceiverFrameResult injectHilFrame(const uint8_t *mac, const uint8_t *data, size_t length) {
  const ReceiverFrameResult result = processControlFrame(mac, data, length);
  rememberHilFrame(mac, data, length, result);
  return result;
}

void sendHilStatus(uint32_t sequence) {
  const int16_t expectedPwm = hilOutputGate.expectedPwm;
  const int16_t actualPwm = hilActualPwm(hilOutputGate);
  const bool expectedBuzzer = updateRemoteHorn() || millis() < buzzerHoldUntil;
  Serial.printf("{\"type\":\"status\",\"sequence\":%lu,\"ok\":true,"
                "\"firmware\":\"fan-controller-receiver\",\"protocol\":%u,\"role\":\"receiver\","
                "\"uptime_ms\":%lu,\"last_sequence\":%lu,\"last_result\":\"%s\",",
                static_cast<unsigned long>(sequence), HIL_PROTOCOL_VERSION,
                static_cast<unsigned long>(millis()), static_cast<unsigned long>(hilLastSequence), hilLastError);
  Serial.printf("\"outputs_unlocked\":%s,\"watchdog_remaining_ms\":%lu,"
                "\"connected\":%s,\"failsafe\":%s,\"protocol_fault\":%s,"
                "\"active_controller\":\"%s\",\"stable_packets\":%u,",
                hilOutputGate.unlocked ? "true" : "false",
                hilOutputGate.unlocked ? static_cast<unsigned long>(HIL_OUTPUT_WATCHDOG_MS - min(static_cast<uint32_t>(millis() - hilOutputGate.lastCommandAt), HIL_OUTPUT_WATCHDOG_MS)) : 0UL,
                connected ? "true" : "false", failsafeActive ? "true" : "false", protocolFault ? "true" : "false",
                controllerSource.hasActiveController ? controllerNameForMac(controllerSource.activeMac) : "none",
                stableControlPacketCount);
  Serial.printf("\"remote\":{\"frame_length\":%u,\"frame_result\":%u,\"sequence\":%u,"
                "\"throttle\":%d,\"speed_level\":%u,\"buttons\":%u},",
                static_cast<unsigned>(hilLastFrameLength), static_cast<unsigned>(hilLastFrameResult),
                lastControlSequence, throttle, speedLevel, buttons);
  Serial.printf("\"vesc\":{\"mode\":\"%s\",\"valid\":%s,\"voltage_x100\":%u,\"speed\":%d},",
                hilVescMode == HIL_VESC_PHYSICAL ? "physical" : (hilVescMode == HIL_VESC_VALUE ? "value" : "fault"),
                vescTelemetryValid ? "true" : "false", batteryVoltageX100, speed);
  Serial.printf("\"diagnostics\":{\"received\":%lu,\"lost\":%lu,\"faults\":%lu,\"ignored\":%lu,"
                "\"failsafes\":%lu,\"max_gap_ms\":%lu},",
                static_cast<unsigned long>(diagnosticReceivedPackets), static_cast<unsigned long>(diagnosticLostPackets),
                static_cast<unsigned long>(diagnosticFaultCount), static_cast<unsigned long>(diagnosticIgnoredPackets),
                static_cast<unsigned long>(diagnosticFailsafeCount), static_cast<unsigned long>(diagnosticMaxPacketGapMs));
  Serial.printf("\"expected_outputs\":{\"pwm\":%d,\"buzzer\":%s},"
                "\"actual_outputs\":{\"pwm\":%d,\"buzzer\":%s}}\n",
                expectedPwm, expectedBuzzer ? "true" : "false", actualPwm,
                (hilOutputGate.unlocked && expectedBuzzer) ? "true" : "false");
}

void resetHilReceiverState() {
  throttle = 0;
  speedLevel = 1;
  buttons = 0;
  connected = false;
  failsafeActive = true;
  protocolFault = false;
  stableControlPacketCount = 0;
  controllerSource = {};
  hasControlSequence = false;
  hasStatusTarget = false;
  sourceOutputLocked = true;
  hilOutputGate = hilInitialOutputGate((PWM_MIN + PWM_MAX) / 2);
  hilLastFrameLength = 0;
  hilLastFrameResult = RECEIVER_FRAME_BAD_LENGTH;
  strcpy(hilLastError, "ok");
  updateMotors();
  noTone(BUZZER_PIN);
}

void handleHilCommand(const HilCommand &command) {
  hilLastSequence = command.sequence;
  strcpy(hilLastError, "ok");
  hilOutputGate.lastCommandAt = millis();
  switch (command.type) {
    case HIL_COMMAND_PING:
      sendHilAck(command.sequence, true);
      return;
    case HIL_COMMAND_STATUS:
      sendHilStatus(command.sequence);
      return;
    case HIL_COMMAND_OUTPUTS_LOCK:
      hilSetOutputsUnlocked(hilOutputGate, false, millis());
      updateMotors();
      noTone(BUZZER_PIN);
      sendHilAck(command.sequence, true);
      return;
    case HIL_COMMAND_OUTPUTS_UNLOCK:
      hilSetOutputsUnlocked(hilOutputGate, true, millis());
      updateMotors();
      sendHilAck(command.sequence, true);
      return;
    case HIL_COMMAND_REMOTE_CONTROL: {
      const uint8_t *mac = macForControllerName(command.text);
      if (mac == nullptr || command.values[0] < -1000 || command.values[0] > 1000 ||
          command.values[1] < 1 || command.values[1] > 3 || command.values[2] < 0 || command.values[2] > 255 ||
          command.values[3] < 0 || command.values[3] > 255) {
        sendHilAck(command.sequence, false, "invalid_argument");
        return;
      }
      ReceiverControlFrameV2 frame = {};
      frame.head = CONTROL_PACKET_HEAD;
      frame.type = CONTROL_PACKET_TYPE;
      frame.version = CONTROL_PROTOCOL_VERSION;
      frame.sequence = static_cast<uint16_t>(command.sequence);
      frame.throttle = static_cast<int16_t>(command.values[0]);
      frame.speedLevel = static_cast<uint8_t>(command.values[1]);
      frame.buttons = static_cast<uint8_t>(command.values[2]);
      frame.flags = static_cast<uint8_t>(command.values[3]);
      frame.crc = protocolCrc8(reinterpret_cast<const uint8_t *>(&frame), sizeof(frame) - 1);
      const ReceiverFrameResult result = injectHilFrame(mac, reinterpret_cast<const uint8_t *>(&frame), sizeof(frame));
      if (result != RECEIVER_FRAME_OK) {
        sendHilAck(command.sequence, false, "invalid_argument");
        return;
      }
      sendHilAck(command.sequence, true);
      return;
    }
    case HIL_COMMAND_REMOTE_FRAME: {
      const uint8_t *mac = macForControllerName(command.text);
      uint8_t bytes[sizeof(ReceiverControlFrameV2)] = {};
      size_t length = 0;
      if (mac == nullptr || !hilDecodeHex(command.data, bytes, sizeof(bytes), length)) {
        sendHilAck(command.sequence, false, "invalid_argument");
        return;
      }
      const ReceiverFrameResult result = injectHilFrame(mac, bytes, length);
      if (result != RECEIVER_FRAME_OK) {
        sendHilAck(command.sequence, false, result == RECEIVER_FRAME_BAD_LENGTH ? "invalid_argument" : "frame_rejected");
        return;
      }
      sendHilAck(command.sequence, true);
      return;
    }
    case HIL_COMMAND_REMOTE_REPEAT:
      if (command.values[0] < 1 || command.values[0] > 100 || hilLastFrameLength == 0) {
        sendHilAck(command.sequence, false, "invalid_argument");
        return;
      }
      for (int32_t i = 0; i < command.values[0]; i++) injectHilFrame(hilLastFrameMac, hilLastFrame, hilLastFrameLength);
      sendHilAck(command.sequence, true);
      return;
    case HIL_COMMAND_REMOTE_INVALID: {
      const uint8_t *sourceMac = macForControllerName(command.text);
      uint8_t unknownMac[6] = {1, 2, 3, 4, 5, 6};
      if (sourceMac == nullptr) { sendHilAck(command.sequence, false, "invalid_argument"); return; }
      ReceiverControlFrameV2 frame = {};
      frame.head = CONTROL_PACKET_HEAD;
      frame.type = CONTROL_PACKET_TYPE;
      frame.version = CONTROL_PROTOCOL_VERSION;
      frame.sequence = static_cast<uint16_t>(command.sequence);
      frame.speedLevel = 1;
      frame.crc = protocolCrc8(reinterpret_cast<const uint8_t *>(&frame), sizeof(frame) - 1);
      const uint8_t *mac = sourceMac;
      size_t length = sizeof(frame);
      if (strcmp(command.data, "crc") == 0) frame.crc ^= 0xFF;
      else if (strcmp(command.data, "unknown_address") == 0) mac = unknownMac;
      else if (strcmp(command.data, "unknown_command") == 0) { frame.type = 0x7F; frame.crc = protocolCrc8(reinterpret_cast<const uint8_t *>(&frame), sizeof(frame) - 1); }
      else if (strcmp(command.data, "truncated") == 0) length -= 2;
      else if (strcmp(command.data, "stale") == 0) frame.sequence = lastControlSequence;
      else { sendHilAck(command.sequence, false, "invalid_argument"); return; }
      injectHilFrame(mac, reinterpret_cast<const uint8_t *>(&frame), length);
      sendHilAck(command.sequence, true);
      return;
    }
    case HIL_COMMAND_VESC_PHYSICAL:
      hilVescMode = HIL_VESC_PHYSICAL;
      sendHilAck(command.sequence, true);
      return;
    case HIL_COMMAND_VESC_FAULT:
      hilVescMode = HIL_VESC_FAULT;
      vescTelemetryValid = false;
      sendHilAck(command.sequence, true);
      return;
    case HIL_COMMAND_VESC_VALUE:
      if (command.values[0] < 0 || command.values[0] > 10000) {
        sendHilAck(command.sequence, false, "invalid_argument");
        return;
      }
      hilVescMode = HIL_VESC_VALUE;
      hilVescVoltageX100 = command.values[0];
      hilVescRpm = command.values[1];
      vescTelemetryValid = true;
      batteryVoltageX100 = static_cast<uint16_t>(hilVescVoltageX100);
      speed = static_cast<int16_t>(hilVescRpm * 0.00207f);
      sendHilAck(command.sequence, true);
      return;
    case HIL_COMMAND_RESET:
      resetHilReceiverState();
      sendHilAck(command.sequence, true);
      return;
    case HIL_COMMAND_REBOOT:
      hilSetOutputsUnlocked(hilOutputGate, false, millis());
      updateMotors();
      noTone(BUZZER_PIN);
      sendHilAck(command.sequence, true);
      Serial.flush();
      delay(50);
      ESP.restart();
      return;
    default:
      strcpy(hilLastError, "unsupported");
      sendHilAck(command.sequence, false, "unsupported");
      return;
  }
}

void pollHilSerial() {
  static char line[HIL_MAX_LINE_LENGTH] = {};
  static size_t length = 0;
  static bool discarding = false;
  while (Serial.available() > 0) {
    const char value = static_cast<char>(Serial.read());
    if (value == '\r') continue;
    if (value == '\n') {
      if (discarding) {
        discarding = false;
        length = 0;
        sendHilAck(0, false, "line_too_long");
        continue;
      }
      if (length == 0) { sendHilAck(0, false, "empty"); continue; }
      line[length] = '\0';
      HilCommand command = {};
      const HilParseResult result = hilParseCommand(line, command);
      length = 0;
      if (result == HIL_PARSE_OK) handleHilCommand(command);
      else {
        hilLastSequence = command.sequence;
        strncpy(hilLastError, hilParseError(result), sizeof(hilLastError) - 1);
        hilLastError[sizeof(hilLastError) - 1] = '\0';
        sendHilAck(command.sequence, false, hilParseError(result));
      }
      continue;
    }
    if (discarding) continue;
    if (length + 1 >= sizeof(line)) {
      discarding = true;
      continue;
    }
    line[length++] = value;
  }
  if (hilApplyOutputWatchdog(hilOutputGate, millis(), HIL_OUTPUT_WATCHDOG_MS)) {
    updateMotors();
    noTone(BUZZER_PIN);
  }
}
#endif


// ========== 主程序 ==========
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=================================");
  Serial.println("ESP-NOW 接收器(固定MAC版)");
  Serial.println("=================================");

  setupPins();
  setupESPNOW();
  setupTimer();

  beep(BEEP_FREQ_STARTUP, 100);
  delay(200);
  beep(BEEP_FREQ_STARTUP, 100);

  Serial.println("系统就绪");
  Serial.print("本机MAC: ");
  Serial.println(WiFi.macAddress());
  Serial.print("绑定发射器: ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", transmitterMac[i]);
    if (i < 5) Serial.print(":");
  }
  Serial.println();
  Serial1.begin(115200, SERIAL_8N1, 7, 6);
  VESC.setSerialPort(&Serial1);
}

void loop() {
  static uint32_t lastVescRead = 0;
#ifdef FAN_CONTROLLER_HIL
  pollHilSerial();
#else
  updateDiagnosticSerial();
#endif
  checkFailsafe();
  updateLinkAlert();
  updateConnectionBeep();
  updateMotors();

  // 按钮2控制蜂鸣器（按下响，松开停）
  if (updateRemoteHorn()) {
#ifdef FAN_CONTROLLER_HIL
    hilOutputGate.expectedBuzzer = true;
    if (hilActualBuzzer(hilOutputGate)) tone(BUZZER_PIN, BEEP_FREQ_REMOTE_HORN);
    else noTone(BUZZER_PIN);
#else
    tone(BUZZER_PIN, BEEP_FREQ_REMOTE_HORN);  // 持续响，由超时保护停止
#endif
  } else if (millis() < buzzerHoldUntil || (failsafeActive && millis() < failsafeBeepUntil)) {
    // 定时提示音由 tone(..., duration) 自动停止，这里避免被按钮逻辑立即打断。
  } else {
#ifdef FAN_CONTROLLER_HIL
    hilOutputGate.expectedBuzzer = false;
#endif
    noTone(BUZZER_PIN);  // 停止
  }

  if (sendTelemetryFlag) {
    portENTER_CRITICAL(&timerMux);
    sendTelemetryFlag = false;
    portEXIT_CRITICAL(&timerMux);
    sendTelemetry();
  }

  // LED状态指示
  if (failsafeActive) {
    static uint32_t last = 0;
    if (millis() - last > 100) {
      last = millis();
      digitalWrite(LED_STATUS, !digitalRead(LED_STATUS));
    }
  } else if (!connected) {
    static uint32_t last = 0;
    if (millis() - last > 500) {
      last = millis();
      digitalWrite(LED_STATUS, !digitalRead(LED_STATUS));
    }
  }

  if (millis() - lastVescRead > 1000) {
    lastVescRead = millis();
#ifdef FAN_CONTROLLER_HIL
    if (hilVescMode == HIL_VESC_VALUE) {
      vescTelemetryValid = true;
      batteryVoltageX100 = static_cast<uint16_t>(hilVescVoltageX100);
      speed = static_cast<int16_t>(hilVescRpm * 0.00207f);
    } else if (hilVescMode == HIL_VESC_FAULT) {
      vescTelemetryValid = false;
    } else
#endif
    if (VESC.getVescValues()) {
      vescTelemetryValid = true;
      // Serial.println("======== VESC 数据 ========");
      // Serial.print("输入电压: ");
      // Serial.print(VESC.data.inpVoltage);
      batteryVoltageX100 = VESC.data.inpVoltage * 100;
      // Serial.println(" V");
      // Serial.print("电机电流: ");
      // Serial.print(VESC.data.avgMotorCurrent);
      // Serial.println(" A");
      // Serial.print("电池电流: ");
      // Serial.print(VESC.data.avgInputCurrent);
      // Serial.println(" A");
      // Serial.print("转速:     ");
      // Serial.print(VESC.data.rpm);
      // Serial.println(" eRPM");
      speed = VESC.data.rpm * 0.00207;
      // Serial.print("占空比:   ");
      // Serial.print(VESC.data.dutyCycleNow * 100);
      // Serial.println(" %");
      // Serial.print("MOS温度:  ");
      // Serial.print(VESC.data.tempMosfet);
      // Serial.println(" °C");
      // Serial.print("电机温度: ");
      // Serial.print(VESC.data.tempMotor);
      // Serial.println(" °C");
      // Serial.print("累计耗电: ");
      // Serial.print(VESC.data.ampHours);
      // Serial.println(" Ah");
      // Serial.println();
    } else {
      vescTelemetryValid = false;
      Serial.println("❌ 读取 VESC 失败，请检查：接线、波特率、共地、IO6/IO7 是否可用");
    }
  }


  delay(1);
}
