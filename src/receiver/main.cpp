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
#include "control_logic.h"
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

VescUart VESC;

// ========== 发射器MAC地址（必须修改为你的发射器MAC！）==========
// 格式: {0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX} ac:eb:e6:44:c5:90
uint8_t transmitterMac[] = { 0xAC, 0xEB, 0xE6, 0x44, 0xD5, 0x54 };

// ========== 数据结构 ==========
#pragma pack(push, 1)

typedef struct {
  uint8_t head;
  uint8_t type;
  int16_t throttle;
  uint8_t speedLevel;
  uint8_t buttons;
  uint8_t checksum;
} ControlPacket;

typedef struct {
  uint8_t head;
  uint8_t type;
  int16_t rssi;
  uint16_t voltage;
  uint8_t motorPWM[4];
  uint16_t speed;  //速度值（如km/h×10或RPM）
  uint8_t status;
  uint8_t checksum;
} StatusPacket;

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

int16_t smoothedThrottle = 0;
int16_t lastPwmValue = -1;
// uint8_t motorPWMValues[4] = { 0, 0, 0, 0 };
float batteryVoltage = 0.0;
uint16_t batteryVoltageX100 = 0;
bool failsafeActive = false;
uint32_t failsafeBeepUntil = 0;
uint32_t lastLinkAlertTime = 0;
bool linkAlertHasFired = false;
uint32_t remoteHornStartTime = 0;
bool remoteHornActive = false;
bool remoteHornTimeoutReported = false;

const int pwmChannels[] = { 0, 1, 2, 3 };

// ========== 硬件定时器中断 ==========
void IRAM_ATTR onTimer() {
  portENTER_CRITICAL_ISR(&timerMux);
  timerCounter++;
  if (timerCounter % 5 == 0) sendTelemetryFlag = true;
  if (timerCounter % 20 == 0) readBatteryFlag = true;
  portEXIT_CRITICAL_ISR(&timerMux);
}

void beep(uint16_t duration_ms) {
  tone(BUZZER_PIN, 2000, duration_ms);  // 2kHz，自动停止
}

void alarmBeep() {
  static uint32_t last = 0;
  if (millis() - last > 100) {
    last = millis();
    tone(BUZZER_PIN, 2000, 100);  // 100ms鸣叫
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
  // 添加这行：固定信道
  WiFi.channel(1);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW初始化失败！");
    return;
  }

  // 添加固定发射器配对
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, transmitterMac, 6);
  peer.channel = 0;
  peer.encrypt = false;

  if (esp_now_add_peer(&peer) != ESP_OK) {
    Serial.println("添加发射器失败！");
    return;
  }
  Serial.println("发射器配对成功");

  // 接收回调
  esp_now_register_recv_cb([](const uint8_t *mac, const uint8_t *data, int len) {
    // Serial.printf("收到数据！len=%d, mac=%02X:%02X:%02X:%02X:%02X:%02X\n",
    //               len, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    // MAC过滤：只接受绑定的发射器
    if (memcmp(mac, transmitterMac, 6) != 0) {
      return;
    }
    // // 使用底层API获取RSSI
    wifi_ap_record_t ap_info;
    esp_wifi_sta_get_ap_info(&ap_info);
    lastRssi = ap_info.rssi;  // 这个方法更可靠

    if (len != sizeof(ControlPacket)) return;
    ControlPacket *pkt = (ControlPacket *)data;
    if (pkt->head != 0xA5 || pkt->type != 0x01) return;

    // 校验和
    uint8_t sum = 0;
    for (uint8_t i = 0; i < sizeof(ControlPacket) - 1; i++) {
      sum += ((uint8_t *)pkt)[i];
    }
    if (sum != pkt->checksum) return;

    // 更新数据
    throttle = pkt->throttle;
    speedLevel = pkt->speedLevel;
    buttons = pkt->buttons;
    lastRssi = WiFi.RSSI();
    lastRecvTime = millis();
    connected = true;
    failsafeActive = false;

    digitalWrite(LED_STATUS, !digitalRead(LED_STATUS));
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
    failsafeBeepUntil = millis() + 1000;
    Serial.println("失控保护启动！");
    beep(1000);
    // for (int i = 0; i < 5; i++) {
    //   beep(100);
    //   delay(100);
    // }
  }
}

void updateLinkAlert() {
  if (shouldEmitReceiverLinkAlert(connected, failsafeActive, millis(), lastLinkAlertTime, linkAlertHasFired, LINK_ALERT_INTERVAL)) {
    Serial.println("遥控器未连接，接收端提示音");
    beep(LINK_ALERT_BEEP_MS);
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
  StatusPacket pkt = {};
  pkt.head = 0x5A;
  pkt.type = 0x02;
  pkt.rssi = lastRssi;
  pkt.voltage = batteryVoltageX100;
  pkt.motorPWM[0] = (uint8_t)clampInt(mapLong(lastPwmValue, PWM_MIN, PWM_MAX, 0, 255), 0, 255);
  pkt.speed = speed;  // ← 新增：示例：0-100km/h
  pkt.status = failsafeActive ? 0x01 : 0x00;

  uint8_t sum = 0;
  for (uint8_t i = 0; i < sizeof(StatusPacket) - 1; i++) {
    sum += ((uint8_t *)&pkt)[i];
  }
  pkt.checksum = sum;

  esp_now_send(transmitterMac, (uint8_t *)&pkt, sizeof(pkt));
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

  beep(100);
  delay(200);
  beep(100);

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
  checkFailsafe();
  updateLinkAlert();
  updateMotors();

  // 按钮2控制蜂鸣器（按下响，松开停）
  if (updateRemoteHorn()) {
    tone(BUZZER_PIN, 2000);  // 持续响，不指定时长
  } else if (failsafeActive && millis() < failsafeBeepUntil) {
    // 失控提示音由 checkFailsafe() 启动，这里避免被按钮逻辑立即打断。
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
      Serial.println("❌ 读取 VESC 失败，请检查：接线、波特率、共地、IO6/IO7 是否可用");
    }
  }


  delay(1);
}
