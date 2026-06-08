/*
 * ESP-NOW 遥控器发射端完整代码
 * 硬件配置：
 * - GPIO6,5,4: 三档拨动开关（速度档位1,2,3）
 * - GPIO8(SDA), GPIO9(SCL): 0.96寸 OLED (SSD1306)
 * - GPIO0: 回中摇杆（油门/刹车，ADC输入）
 * - GPIO3: 蜂鸣器（PWM输出）
 * - GPIO7, GPIO10: 两个按钮
 */

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "beep_profiles.h"
#include "control_logic.h"

// ========== 引脚定义 ==========
#define SWITCH_PIN_1 6   // 三档开关档位1
#define SWITCH_PIN_2 5   // 三档开关档位2
#define SWITCH_PIN_3 4   // 三档开关档位3
#define OLED_SDA 8       // OLED SDA
#define OLED_SCL 9       // OLED SCL
#define JOYSTICK_PIN 0   // 回中摇杆（油门/刹车）ADC输入
#define BUZZER_PIN 3     // 蜂鸣器PWM输出
#define BUTTON_1_PIN 7   // 按钮1
#define BUTTON_2_PIN 10  // 按钮2

// ========== 接收机MAC地址（请修改为您的接收机MAC）==========
// #define RECEIVER_MAC { 0x48, 0xF6, 0xEE, 0x23, 0x14, 0xAC }
#define RECEIVER_MAC { 0xAC, 0xEB, 0xE6, 0x44, 0xC5, 0x90 };
// ========== 参数配置 ==========
#define CONTROL_RATE_HZ 100    // 控制频率100Hz
#define TELEMETRY_RATE_HZ 20   // 回传频率20Hz
#define HEARTBEAT_TIMEOUT 500  // 心跳超时500ms
#define JOYSTICK_DEADZONE 50   // 摇杆死区
#define ADC_CENTER 2048        // ADC中心点（12位ADC）
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_ADDR 0x3C

// ========== CW2015寄存器 ==========
#define CW2015_ADDR 0x62         // CW2015 I2C地址 [^43^]
#define CW2015_REG_VCELL 0x02    // 电压寄存器 (14bit, 305uV/bit) [^44^]
#define CW2015_REG_SOC 0x04      // 电量寄存器 (16bit) [^44^]
#define CW2015_REG_MODE 0x0A     // 模式寄存器 [^43^]
#define CW2015_REG_BATINFO 0x10  // 电池建模信息起始地址，共64字节

// 模式位
#define CW2015_MODE_SLEEP 0xC0  // 睡眠模式默认值
#define CW2015_MODE_WAKE 0x00   // 唤醒
#define CW2015_MODE_QSTRT 0x30  // Quick Start (bit 5:4 = 11)
#define CW2015_MODE_POR 0x0F    // 复位 (bit 3:0 = 1111)

#define BATTERY_READ_HZ 2  // 2Hz读取电量
#define LOW_BATTERY_THRESHOLD 20
#define CRITICAL_BATTERY_THRESHOLD 10
float localBatteryVoltage = 0.0;
uint8_t localBatteryPercent = 0;
bool cw2015Available = false;
bool lowBatteryWarned = false;

// ========== 数据结构 ==========
#pragma pack(push, 1)

typedef struct {
  uint8_t head;        // 帧头 0xA5
  uint8_t type;        // 0x01=控制数据
  int16_t throttle;    // 油门/刹车 (-1000~1000，回中=0)
  uint8_t speedLevel;  // 速度档位 1/2/3
  uint8_t buttons;     // 按钮状态 bit0=按钮1, bit1=按钮2
  uint8_t checksum;    // 校验和
} ControlPacket;       // 8字节

typedef struct {
  uint8_t head;         // 帧头 0x5A
  uint8_t type;         // 0x02=状态数据
  int16_t rssi;         // 信号强度
  uint16_t voltage;     // 电池电压(x100)
  uint8_t motorPWM[4];  // 电机PWM值
  uint16_t speed;       //速度值（如km/h×10或RPM）
  uint8_t status;       // 状态标志
  uint8_t checksum;     // 校验和
} StatusPacket;         // 10字节

#pragma pack(pop)

void beep(uint16_t frequency_hz, uint16_t duration_ms);
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
void onDataRecv(const uint8_t *mac, const uint8_t *data, int len);

// ========== 全局变量 ==========
uint8_t receiverMac[] = RECEIVER_MAC;
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

// 硬件定时器
hw_timer_t *controlTimer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

// 控制标志（volatile用于中断通信）
volatile bool sendControlFlag = false;
volatile bool readBatteryFlag = false;
volatile bool sendTelemetryFlag = false;
volatile uint32_t controlCounter = 0;

// 输入状态
volatile int16_t joystickValue = 0;  // 摇杆值 -1000~1000
volatile uint8_t speedLevel = 1;     // 速度档位 1/2/3
volatile uint8_t buttonState = 0;    // 按钮状态
volatile bool button1Pressed = false;
volatile bool button2Pressed = false;
int joystickCenter = ADC_CENTER;

// 连接状态
volatile bool connected = false;
volatile uint32_t lastRecvTime = 0;
volatile int16_t rssiValue = -100;
volatile uint16_t voltageValue = 0;
volatile uint16_t speedValue = 0;  // ← 新增：存储回传速度

// ========== 硬件定时器中断 ==========
void IRAM_ATTR onControlTimer() {
  portENTER_CRITICAL_ISR(&timerMux);
  sendControlFlag = true;
  controlCounter++;
  // 每5个控制周期发送一次回传请求 (100/5=20Hz)
  if (controlCounter % 5 == 0) {
    sendTelemetryFlag = true;
  }
  // 每50个周期(2Hz)读取电量
  if (controlCounter % 50 == 0) {
    readBatteryFlag = true;
  }
  portEXIT_CRITICAL_ISR(&timerMux);
}

// ========== 初始化函数 ==========
void setupPins() {
  // 三档开关输入（内部上拉）
  pinMode(SWITCH_PIN_1, INPUT_PULLUP);
  pinMode(SWITCH_PIN_2, INPUT_PULLUP);
  pinMode(SWITCH_PIN_3, INPUT_PULLUP);

  // 按钮输入（内部上拉）
  pinMode(BUTTON_1_PIN, INPUT_PULLUP);
  pinMode(BUTTON_2_PIN, INPUT_PULLUP);

  // 蜂鸣器PWM输出
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // 摇杆ADC输入
  analogReadResolution(12);        // 12位ADC 0-4095
  analogSetAttenuation(ADC_11db);  // 0-3.3V全范围

  Serial.println("引脚初始化完成");
}

void calibrateJoystickCenter() {
  const size_t sampleCount = 64;
  int samples[sampleCount];
  for (size_t i = 0; i < sampleCount; i++) {
    samples[i] = analogRead(JOYSTICK_PIN);
    delay(2);
  }
  joystickCenter = calibratedJoystickCenter(samples, sampleCount, ADC_CENTER);
  Serial.printf("摇杆中位校准完成: %d\n", joystickCenter);
}

void setupOLED() {
  // 自定义I2C引脚 - 必须在display.begin()之前调用
  Wire.setPins(OLED_SDA, OLED_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED初始化失败！");
    return;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("ESP-NOW Remote");
  display.println("Initializing...");
  display.display();

  Serial.println("OLED初始化完成");
}

void setupESPNOW() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);  
  // WiFi.setTxPower(WIFI_POWER_19_5dBm);  // 最大发射功率
  WiFi.setTxPower(WIFI_POWER_15dBm);
  // 添加这行：固定信道
  WiFi.channel(1);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW初始化失败！");
    return;
  }

  esp_now_register_send_cb(onDataSent);
  esp_now_register_recv_cb(onDataRecv);

  // 添加配对
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverMac, 6);
  peerInfo.channel = 0;  // 当前信道
  peerInfo.encrypt = false;

  // 先删除可能存在的旧配对
  esp_now_del_peer(receiverMac);

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("添加配对失败！");
    return;
  }

  Serial.println("ESP-NOW初始化完成");
  Serial.print("本机MAC: ");
  Serial.println(WiFi.macAddress());
}

void setupTimer() {
  // 使用定时器0，80分频（1MHz），向上计数，自动重载
  controlTimer = timerBegin(0, 80, true);
  timerAttachInterrupt(controlTimer, &onControlTimer, true);

  // 100Hz = 10000us周期
  timerAlarmWrite(controlTimer, 10000, true);
  timerAlarmEnable(controlTimer);

  Serial.println("硬件定时器初始化完成: 100Hz");
}

// ========== 输入读取函数 ==========
void readJoystick() {
  int raw = analogRead(JOYSTICK_PIN);
  joystickValue = joystickToThrottle(raw, joystickCenter, JOYSTICK_DEADZONE);
}

void readSwitches() {
  // 读取三档开关（低电平有效）
  bool sw1 = !digitalRead(SWITCH_PIN_1);
  bool sw2 = !digitalRead(SWITCH_PIN_2);
  bool sw3 = !digitalRead(SWITCH_PIN_3);

  // 确定档位（优先级：3>2>1）
  if (sw3) {
    speedLevel = 3;
  } else if (sw2) {
    speedLevel = 2;
  } else if (sw1) {
    speedLevel = 1;
  } else {
    // 默认档位1
    speedLevel = 1;
  }
}

void readButtons() {
  static bool lastBtn1 = false;
  static bool lastBtn2 = false;
  static uint32_t lastDebounce1 = 0;
  static uint32_t lastDebounce2 = 0;

  bool rawBtn1 = !digitalRead(BUTTON_1_PIN);
  bool rawBtn2 = !digitalRead(BUTTON_2_PIN);

  // 消抖处理（20ms）
  if (rawBtn1 != lastBtn1 && millis() - lastDebounce1 > 20) {
    lastDebounce1 = millis();
    lastBtn1 = rawBtn1;
    if (rawBtn1) {
      button1Pressed = true;
      buttonState |= 0x01;
      beep(BEEP_FREQ_BUTTON, 50);  // 按键音
    } else {
      buttonState &= ~0x01;
    }
  }

  if (rawBtn2 != lastBtn2 && millis() - lastDebounce2 > 20) {
    lastDebounce2 = millis();
    lastBtn2 = rawBtn2;
    if (rawBtn2) {
      button2Pressed = true;
      buttonState |= 0x02;
      beep(BEEP_FREQ_BUTTON, 50);  // 按键音
    } else {
      buttonState &= ~0x02;
    }
  }
}

// ========== 蜂鸣器控制 ==========
void beep(uint16_t frequency_hz, uint16_t duration_ms) {
  // digitalWrite(BUZZER_PIN, HIGH);
  // delay(duration_ms);
  // digitalWrite(BUZZER_PIN, LOW);
  tone(BUZZER_PIN, frequency_hz, duration_ms);  // 自动停止
}

void alarmBeep() {
  // 报警音（快速闪烁）
  static uint32_t last = 0;
  if (millis() - last > 100) {
    last = millis();
    tone(BUZZER_PIN, BEEP_FREQ_LINK_ALERT, 100);  // 100ms鸣叫
  }
}

// ========== 数据发送 ==========
uint8_t calcChecksum(uint8_t *data, uint8_t len) {
  uint8_t sum = 0;
  for (uint8_t i = 0; i < len - 1; i++) {
    sum += data[i];
  }
  return sum;
}

void sendControlData() {
  ControlPacket pkt;
  pkt.head = 0xA5;
  pkt.type = 0x01;
  pkt.throttle = joystickValue;
  pkt.speedLevel = speedLevel;
  pkt.buttons = buttonState;
  pkt.checksum = calcChecksum((uint8_t *)&pkt, sizeof(pkt));

  // 非阻塞发送
  esp_err_t result = esp_now_send(receiverMac, (uint8_t *)&pkt, sizeof(pkt));

  if (result != ESP_OK) {
    Serial.println("发送失败");
  }
}

// ========== ESP-NOW回调 ==========
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // 发送完成回调
  // 可用于统计发送成功率
  //   Serial.println(status == ESP_NOW_SEND_SUCCESS ? "发送成功" : "发送失败");
}

void onDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
  if (len == sizeof(StatusPacket)) {
    StatusPacket *pkt = (StatusPacket *)data;

    // 验证帧头和类型
    if (pkt->head != 0x5A || pkt->type != 0x02) return;

    // 校验和验证
    if (calcChecksum((uint8_t *)pkt, sizeof(StatusPacket)) != pkt->checksum) return;

    // 更新状态
    rssiValue = pkt->rssi;
    voltageValue = pkt->voltage;
    speedValue = pkt->speed;  // 保存速度
    lastRecvTime = millis();
    connected = true;
  }
}

// ========== 连接状态检测 ==========
void checkConnection() {
  if (millis() - lastRecvTime > HEARTBEAT_TIMEOUT) {
    if (connected) {
      connected = false;
      Serial.println("⚠️ 接收机断线！");
    }
  }
}


// ========== CW2015驱动 ==========
bool cw2015Init() {
  Wire.beginTransmission(CW2015_ADDR);
  if (Wire.endTransmission() != 0) {
    Serial.println("CW2015未检测到");
    return false;
  }
  // 唤醒CW2015 [^43^]
  Wire.beginTransmission(CW2015_ADDR);
  Wire.write(CW2015_REG_MODE);
  Wire.write(0x00);
  Wire.endTransmission();
  Serial.println("CW2015初始化成功");
  return true;
}

float cw2015ReadVoltage() {
  Wire.beginTransmission(CW2015_ADDR);
  Wire.write(CW2015_REG_VCELL);
  if (Wire.endTransmission(false) != 0) return -1;
  Wire.requestFrom(CW2015_ADDR, 2);
  if (Wire.available() < 2) return -1;
  uint8_t msb = Wire.read();
  uint8_t lsb = Wire.read();
  // uint16_t raw = ((msb << 8) | lsb) >> 2;  // 14bit数据 [^44^]
  uint16_t raw = ((msb << 8) | lsb);
  return raw * 0.000305;  // 305uV/bit
}

uint8_t cw2015ReadPercentage() {
  Wire.beginTransmission(CW2015_ADDR);
  Wire.write(CW2015_REG_SOC);
  if (Wire.endTransmission(false) != 0) return 0;
  Wire.requestFrom(CW2015_ADDR, 2);
  if (Wire.available() < 2) return 0;
  uint8_t soc_msb = Wire.read();  // 整数部分
  uint8_t soc_lsb = Wire.read();  // 小数部分(1/256) [^43^]
  float percent = soc_msb + (float)soc_lsb / 256.0;
  return (uint8_t)constrain(percent, 0, 100);
}

void checkBatteryAlert() {
  if (!cw2015Available) return;
  if (localBatteryPercent <= LOW_BATTERY_THRESHOLD && !lowBatteryWarned) {
    lowBatteryWarned = true;
    for (int i = 0; i < 3; i++) {
      beep(BEEP_FREQ_LOW_BATTERY, 50);
      delay(50);
    }
    beep(BEEP_FREQ_LOW_BATTERY, 200);
  }
  if (localBatteryPercent <= CRITICAL_BATTERY_THRESHOLD) {
    static uint32_t last = 0;
    if (millis() - last > 1000) {
      last = millis();
      beep(BEEP_FREQ_LOW_BATTERY, 500);
    }
  }
  if (localBatteryPercent > LOW_BATTERY_THRESHOLD + 5) lowBatteryWarned = false;
}

// ========== OLED显示 ==========
void updateDisplay() {
  static uint32_t lastUpdate = 0;
  // if (millis() - lastUpdate < 50) return;  // 20Hz刷新率
  if (millis() - lastUpdate < 100) return;  // 10Hz刷新率
  // if (millis() - lastUpdate < 200) return;  // 5Hz刷新率
  lastUpdate = millis();

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);

  // 第1行：连接状态和信号
  if (connected) {
    display.printf("[OK]  BAT:%.2fV\n", voltageValue / 100.0);
  } else {
    display.println("[LOST]");
  }

  // 第2行：速度档位和电压
  // display.printf("SPD:%d\n", speedLevel);

  // 第3行：油门/刹车值
  const char *direction = joystickValue > 0 ? "THR" : "BRK";
  // display.printf("%s:%4d  BTN:%02X\n", direction, abs(joystickValue), buttonState);
  display.printf("SPD:%d  %s:%4d\n", speedLevel, direction, abs(joystickValue));
  display.setTextSize(4);
  // display.println("30 KM");
  if (connected) {
    display.printf("%2d KM\n", speedValue);
  } else {
    display.printf("N/A\n");
  }

  display.setTextSize(1);

  // 第4行: 电量百分比 + 本地电池电压
  display.printf("%s:%3d%% BAT:%.2fV\n", cw2015Available ? "SOC" : "N/A", localBatteryPercent, localBatteryVoltage);

  // 第5行：可视化条
  int barWidth = map(abs(joystickValue), 0, 1000, 0, 60);
  display.print(direction);
  display.print(":");
  for (int i = 0; i < barWidth / 6; i++) display.print("=");
  display.println();

  display.display();
}

// ========== 主程序 ==========
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=================================");
  Serial.println("ESP-NOW 遥控器发射端启动");
  Serial.println("=================================");

  setupPins();
  calibrateJoystickCenter();
  setupOLED();
  cw2015Available = cw2015Init();
  if (cw2015Available) {
    localBatteryVoltage = cw2015ReadVoltage();
    localBatteryPercent = cw2015ReadPercentage();
    Serial.printf("初始电量: %.2fV %d%%\n", localBatteryVoltage, localBatteryPercent);
  }
  setupESPNOW();
  setupTimer();

  // 启动提示音
  beep(BEEP_FREQ_STARTUP, 100);
  delay(100);
  beep(BEEP_FREQ_STARTUP, 100);

  Serial.println("系统就绪！");
}

void loop() {
  // 1. 读取所有输入（每次循环都读，保证实时性）
  readJoystick();
  readSwitches();
  readButtons();
  if (readBatteryFlag && cw2015Available) {
    portENTER_CRITICAL(&timerMux);
    readBatteryFlag = false;
    portEXIT_CRITICAL(&timerMux);
    localBatteryVoltage = cw2015ReadVoltage();
    localBatteryPercent = cw2015ReadPercentage();
    checkBatteryAlert();
  }

  // 2. 处理定时发送（100Hz控制）
  if (sendControlFlag) {
    portENTER_CRITICAL(&timerMux);
    sendControlFlag = false;
    portEXIT_CRITICAL(&timerMux);

    sendControlData();

    // 调试输出（每100次打印一次，即1秒一次）
    static uint16_t debugCount = 0;
    if (++debugCount >= 100) {
      debugCount = 0;
      Serial.printf("THR:%4d SPD:%d BTN:%02X %s\n",
                    joystickValue, speedLevel, buttonState,
                    connected ? "[OK]" : "[LOST]");
    }
  }

  // 3. 处理回传请求（20Hz）
  if (sendTelemetryFlag) {
    portENTER_CRITICAL(&timerMux);
    sendTelemetryFlag = false;
    portEXIT_CRITICAL(&timerMux);

    // 接收机收到控制数据后会自动回传
    // 这里可以添加显式请求逻辑
  }

  // 4. 检查连接状态
  checkConnection();

  // 5. 断线报警
  if (!connected) {
    alarmBeep();
  }

  // 6. 更新OLED显示
  updateDisplay();

  // 7. 短暂延时防止看门狗
  delay(1);
}
