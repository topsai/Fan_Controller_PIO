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
  int16_t throttle;
  uint8_t speedLevel;
  uint8_t buttons;
  uint8_t flags;
  uint8_t crc;
} ControlPacket;

typedef struct {
  uint8_t head;
  uint8_t type;
  int16_t throttle;
  uint8_t speedLevel;
  uint8_t buttons;
  uint8_t checksum;
} LegacyControlPacket;

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

// ========== 硬件定时器中断 ==========
void IRAM_ATTR onTimer() {
  portENTER_CRITICAL_ISR(&timerMux);
  timerCounter++;
  if (timerCounter % 5 == 0) sendTelemetryFlag = true;
  if (timerCounter % 20 == 0) readBatteryFlag = true;
  portEXIT_CRITICAL_ISR(&timerMux);
}

void beep(uint16_t frequency_hz, uint16_t duration_ms) {
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

    int16_t receivedThrottle = 0;
    uint8_t receivedSpeedLevel = 1;
    uint8_t receivedButtons = 0;
    uint8_t receivedFlags = 0;
    bool legacyPacket = false;
    bool hasSequence = false;
    uint16_t receivedSequence = 0;

    if (len == sizeof(ControlPacket)) {
      ControlPacket *pkt = (ControlPacket *)data;
      if (pkt->head != CONTROL_PACKET_HEAD || pkt->type != CONTROL_PACKET_TYPE || pkt->version != CONTROL_PROTOCOL_VERSION) {
        protocolFault = true;
        diagnosticFaultCount++;
        return;
      }

      if (protocolCrc8((const uint8_t *)pkt, sizeof(ControlPacket) - 1) != pkt->crc) {
        protocolFault = true;
        diagnosticFaultCount++;
        return;
      }
      hasSequence = true;
      receivedSequence = pkt->sequence;
      receivedThrottle = pkt->throttle;
      receivedSpeedLevel = pkt->speedLevel;
      receivedButtons = pkt->buttons;
      receivedFlags = pkt->flags;
    } else if (len == sizeof(LegacyControlPacket)) {
      LegacyControlPacket *pkt = (LegacyControlPacket *)data;
      if (pkt->head != CONTROL_PACKET_HEAD || pkt->type != CONTROL_PACKET_TYPE ||
          !protocolLegacyChecksumIsValid((const uint8_t *)pkt, sizeof(LegacyControlPacket))) {
        protocolFault = true;
        diagnosticFaultCount++;
        return;
      }
      legacyPacket = true;
      receivedThrottle = pkt->throttle;
      receivedSpeedLevel = pkt->speedLevel;
      receivedButtons = pkt->buttons;
    } else {
      protocolFault = true;
      diagnosticFaultCount++;
      return;
    }

    applyControlPacketFromSource(mac,
                                 receivedThrottle,
                                 receivedSpeedLevel,
                                 receivedButtons,
                                 receivedFlags,
                                 legacyPacket,
                                 hasSequence,
                                 receivedSequence);
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
  Serial.printf("%s ignored=%lu active=%s\n",
                line,
                (unsigned long)diagnosticIgnoredPackets,
                controllerSource.hasActiveController ? controllerNameForMac(controllerSource.activeMac) : "none");
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
  updateDiagnosticSerial();
  checkFailsafe();
  updateLinkAlert();
  updateConnectionBeep();
  updateMotors();

  // 按钮2控制蜂鸣器（按下响，松开停）
  if (updateRemoteHorn()) {
    tone(BUZZER_PIN, BEEP_FREQ_REMOTE_HORN);  // 持续响，由超时保护停止
  } else if (millis() < buzzerHoldUntil || (failsafeActive && millis() < failsafeBeepUntil)) {
    // 定时提示音由 tone(..., duration) 自动停止，这里避免被按钮逻辑立即打断。
  } else {
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
