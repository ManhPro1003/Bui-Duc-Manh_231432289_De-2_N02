// led xanh tượng trưng máy phun sương
// led đỏ tượng trưng cho quạt
// led xanh lá tượng trưng cho máy bơm
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

// ==================================================
// 1. CAU HINH PHAN CUNG
// ==================================================

#define DHT_PIN       4
#define DHT_TYPE      DHT22

#define PUMP_PIN      18
#define FAN_PIN       19
#define MIST_PIN      23

DHT dht(DHT_PIN, DHT_TYPE);

// ==================================================
// 2. CAU HINH WIFI
// ==================================================

const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASSWORD = "";

// ==================================================
// 3. CAU HINH MQTT BROKER EMQX
// ==================================================

const char* MQTT_HOST = "broker.emqx.io";
const int MQTT_PORT = 1883;

const char* MQTT_CLIENT_ID = "esp32_lab3_cafe_4729";

// CAC MQTT TOPIC

const char* TOPIC_TELEMETRY =
  "lab3/garden/cafe_4729/telemetry";

const char* TOPIC_PUMP =
  "lab3/garden/cafe_4729/control/pump/set";

const char* TOPIC_FAN =
  "lab3/garden/cafe_4729/control/fan/set";

const char* TOPIC_STATUS =
  "lab3/garden/cafe_4729/status/actuators";

// ==================================================
// 4. KHOI TAO MQTT
// ==================================================

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// ==================================================
// 5. CAU TRUC DU LIEU
// ==================================================

struct SensorData {
  float temperature;
  float humidity;
  unsigned long sampleTime;
};

// ==================================================
// 6. KHAI BAO QUEUE VA SEMAPHORE
// ==================================================

QueueHandle_t dataQueue;
QueueHandle_t dataAlert;

SemaphoreHandle_t serialMutex;
SemaphoreHandle_t actuatorMutex;

// Trang thai thiet bi

bool pumpState = false;
bool fanState = false;

uint8_t mistPercent = 0;

// Co bao hieu can gui lai trang thai
volatile bool statusDirty = true;

// Thoi diem thu ket noi MQTT
unsigned long lastMqttAttempt = 0;

// Chu ky doc DHT22
const unsigned long SENSOR_INTERVAL = 2000;

// ==================================================
// 7. HAM GHI SERIAL CO BAO VE MUTEX
// ==================================================

void logLine(const char* message) {

  if (serialMutex != NULL &&
      xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100)) == pdTRUE) {

    Serial.println(message);

    xSemaphoreGive(serialMutex);
  }
}

void logSensor(float temperature, float humidity) {

  if (serialMutex != NULL &&
      xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100)) == pdTRUE) {

    Serial.println("===== DU LIEU CAM BIEN =====");

    Serial.print("Nhiet do: ");
    Serial.print(temperature);
    Serial.println(" do C");

    Serial.print("Do am: ");
    Serial.print(humidity);
    Serial.println(" %");

    Serial.println("============================");

    xSemaphoreGive(serialMutex);
  }
}

// ==================================================
// 8. HAM GUI TRANG THAI CO CAU CHAP HANH
// ==================================================

void publishActuatorStatus() {

  if (!mqttClient.connected() || !statusDirty) {
    return;
  }

  // Bao hieu da bat dau xu ly trang thai
  statusDirty = false;

  bool pump;
  bool fan;
  uint8_t mist;

  if (xSemaphoreTake(actuatorMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    statusDirty = true;
    return;
  }

  pump = pumpState;
  fan = fanState;
  mist = mistPercent;

  xSemaphoreGive(actuatorMutex);

  StaticJsonDocument<128> doc;

  doc["pump"] = pump;
  doc["fan"] = fan;
  doc["mistPercent"] = mist;

  char payload[128];

  serializeJson(doc, payload, sizeof(payload));

  bool success = mqttClient.publish(
    TOPIC_STATUS,
    payload,
    true
  );

  if (!success) {
    statusDirty = true;
  }
}

// ==================================================
// 9. HAM CAP NHAT MAY BOM VA QUAT
// ==================================================

void setPumpState(bool state) {

  if (xSemaphoreTake(actuatorMutex, pdMS_TO_TICKS(100)) == pdTRUE) {

    pumpState = state;

    digitalWrite(
      PUMP_PIN,
      state ? HIGH : LOW
    );

    statusDirty = true;

    xSemaphoreGive(actuatorMutex);
  }
}

void setFanState(bool state) {

  if (xSemaphoreTake(actuatorMutex, pdMS_TO_TICKS(100)) == pdTRUE) {

    fanState = state;

    digitalWrite(
      FAN_PIN,
      state ? HIGH : LOW
    );

    statusDirty = true;

    xSemaphoreGive(actuatorMutex);
  }
}

// ==================================================
// 10. HAM DIEU KHIEN PWM MAY PHUN SUONG
// ==================================================

void setMistPercent(uint8_t percent) {

  if (percent > 100) {
    percent = 100;
  }

  // Chuyen cong suat tu 0-100% sang gia tri PWM 0-255
  int duty = (percent * 255) / 100;

  analogWrite(MIST_PIN, duty);

  if (xSemaphoreTake(actuatorMutex, pdMS_TO_TICKS(100)) == pdTRUE) {

    mistPercent = percent;
    statusDirty = true;

    xSemaphoreGive(actuatorMutex);
  }
}

// ==================================================
// 11. XAC DINH CONG SUAT VA THOI GIAN PHUN
// ==================================================

bool getMistSetting(
  float humidity,
  uint8_t &percent,
  unsigned long &duration
) {

  if (humidity >= 40.0) {

    percent = 0;
    duration = 0;

    return false;
  }

  if (humidity >= 35.0) {

    percent = 25;
    duration = 10000;

  } else if (humidity >= 30.0) {

    percent = 50;
    duration = 20000;

  } else {

    percent = 100;
    duration = 30000;
  }

  return true;
}

// ==================================================
// 12. CALLBACK NHAN LENH MQTT TU WEBSITE
// ==================================================

void mqttCallback(
  char* topic,
  byte* payload,
  unsigned int length
) {

  StaticJsonDocument<128> doc;

  DeserializationError error =
    deserializeJson(doc, payload, length);

  if (error) {

    logLine("LOI: Khong doc duoc du lieu JSON MQTT!");

    return;
  }

  if (!doc["state"].is<bool>()) {

    logLine("LOI: Lenh dieu khien khong hop le!");

    return;
  }

  bool requestedState = doc["state"].as<bool>();

  if (strcmp(topic, TOPIC_PUMP) == 0) {

    setPumpState(requestedState);

    logLine(requestedState
      ? "DA BAT MAY BOM!"
      : "DA TAT MAY BOM!");

  } else if (strcmp(topic, TOPIC_FAN) == 0) {

    setFanState(requestedState);

    logLine(requestedState
      ? "DA BAT QUAT!"
      : "DA TAT QUAT!");

  } else {

    logLine("LOI: MQTT TOPIC KHONG HOP LE!");

    return;
  }

  publishActuatorStatus();
}

// ==================================================
// 13. HAM KET NOI WIFI
// ==================================================

void connectWiFi() {

  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  logLine("DANG KET NOI WIFI...");

  WiFi.mode(WIFI_STA);

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );

  while (WiFi.status() != WL_CONNECTED) {

    vTaskDelay(pdMS_TO_TICKS(500));
  }

  logLine("DA KET NOI WIFI!");
}

// ==================================================
// 14. HAM KET NOI LAI MQTT
// ==================================================

void reconnectMQTT() {

  if (mqttClient.connected()) {
    return;
  }

  if (millis() - lastMqttAttempt < 5000 &&
      lastMqttAttempt != 0) {
    return;
  }

  lastMqttAttempt = millis();

  logLine("DANG KET NOI DEN MAY CHU MQTT...");

  if (mqttClient.connect(MQTT_CLIENT_ID)) {

    logLine("DA KET NOI MQTT!");

    // Dang ky nhan lenh may bom va quat
    mqttClient.subscribe(TOPIC_PUMP);

    mqttClient.subscribe(TOPIC_FAN);

    logLine("DA DANG KY NHAN LENH MAY BOM VA QUAT!");

    statusDirty = true;

    publishActuatorStatus();

  } else {

    logLine("LOI: KET NOI MQTT THAT BAI!");
  }
}

// ==================================================
// 15. TASK DOC DU LIEU CAM BIEN - CORE 1
// ==================================================

void Task_ReadData(void* parameter) {

  TickType_t lastWakeTime = xTaskGetTickCount();

  for (;;) {

    float temperature = dht.readTemperature();

    float humidity = dht.readHumidity();

    if (isnan(temperature) || isnan(humidity)) {

      logLine("LOI: KHONG DOC DUOC DU LIEU TU DHT22!");

    } else {

      // Hien thi du lieu qua Serial
      logSensor(temperature, humidity);

      // Tao cau truc du lieu day du
      SensorData data;

      data.temperature = temperature;
      data.humidity = humidity;
      data.sampleTime = millis();

      // Gui du lieu vao dataQueue
      if (xQueueSend(dataQueue, &data, 0) != pdTRUE) {

        // Neu Queue day, bo mau cu nhat
        SensorData oldData;

        xQueueReceive(dataQueue, &oldData, 0);

        xQueueSend(dataQueue, &data, 0);
      }

      // Gui rieng do am vao dataAlert
      // Queue nay co do dai 1 nen dung xQueueOverwrite
      xQueueOverwrite(dataAlert, &humidity);
    }

    // Doc cam bien moi 2 giay
    vTaskDelayUntil(
      &lastWakeTime,
      pdMS_TO_TICKS(SENSOR_INTERVAL)
    );
  }
}

// ==================================================
// 16. TASK CANH BAO VA DIEU KHIEN PHUN SUONG - CORE 1
// ==================================================

void Task_Alert(void* parameter) {

  float humidity;

  for (;;) {

    // Cho den khi nhan du lieu do am
    if (xQueueReceive(
          dataAlert,
          &humidity,
          portMAX_DELAY
        ) == pdTRUE) {

      uint8_t percent;

      unsigned long duration;

      // Do am tu 40% tro len: tat may phun suong
      if (!getMistSetting(humidity, percent, duration)) {

        setMistPercent(0);

        logLine("DO AM >= 40%: TAT MAY PHUN SUONG!");

        continue;
      }

      setMistPercent(percent);

      if (percent == 25) {

        logLine("DO AM TU 35% DEN DUOI 40%: PHUN 25% TRONG 10 GIAY!");

      } else if (percent == 50) {

        logLine("DO AM TU 30% DEN DUOI 35%: PHUN 50% TRONG 20 GIAY!");

      } else {

        logLine("DO AM DUOI 30%: PHUN 100% TRONG 30 GIAY!");
      }

      unsigned long startTime = millis();

      // Theo doi thoi gian phun
      while (millis() - startTime < duration) {

        float newHumidity;

        // Kiem tra du lieu moi neu co
        if (xQueueReceive(
              dataAlert,
              &newHumidity,
              0
            ) == pdTRUE) {

          uint8_t newPercent;

          unsigned long newDuration;

          // Neu do am dat nguong thi dung ngay
          if (!getMistSetting(
                newHumidity,
                newPercent,
                newDuration
              )) {

            setMistPercent(0);

            logLine("DO AM DA DAT 40%: TAT MAY PHUN SUONG!");

            break;
          }

          // Neu thay doi khoang do am,
          // cap nhat cong suat va thoi gian phun
          if (newPercent != percent) {

            percent = newPercent;

            duration = newDuration;

            startTime = millis();

            setMistPercent(percent);

            logLine("DO AM THAY DOI: DA CAP NHAT CONG SUAT PHUN!");
          }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
      }

      // Ket thuc chu ky phun
      setMistPercent(0);

      logLine("DA HOAN THANH CHU KY PHUN SUONG!");
    }
  }
}

// ==================================================
// 17. TASK TRUYEN DU LIEU MQTT - CORE 0
// ==================================================

void Task_Transmit(void* parameter) {

  // Cau hinh MQTT
  mqttClient.setServer(
    MQTT_HOST,
    MQTT_PORT
  );

  mqttClient.setCallback(mqttCallback);

  mqttClient.setBufferSize(512);

  mqttClient.setKeepAlive(60);

  SensorData data;

  for (;;) {

    // 1. Kiem tra va ket noi WiFi
    if (WiFi.status() != WL_CONNECTED) {

      connectWiFi();
    }

    // 2. Duy tri ket noi MQTT
    if (!mqttClient.connected()) {

      reconnectMQTT();
    }

    if (mqttClient.connected()) {

      // 3. Xu ly cac ban tin MQTT nhan duoc
      mqttClient.loop();

      // 4. Gui trang thai co cau chap hanh neu thay doi
      publishActuatorStatus();

      // 5. Nhan du lieu tu dataQueue
      if (xQueueReceive(
            dataQueue,
            &data,
            pdMS_TO_TICKS(100)
          ) == pdTRUE) {

        StaticJsonDocument<128> doc;

        doc["temperature"] = data.temperature;

        doc["humidity"] = data.humidity;

        doc["sampleTime"] = data.sampleTime;

        char payload[128];

        serializeJson(
          doc,
          payload,
          sizeof(payload)
        );

        // 6. Gui du lieu len MQTT Broker
        bool success = mqttClient.publish(
          TOPIC_TELEMETRY,
          payload
        );

        if (success) {

          logLine("DA GUI DU LIEU CAM BIEN LEN MQTT!");

        } else {

          logLine("LOI: GUI DU LIEU CAM BIEN LEN MQTT THAT BAI!");
        }
      }

    } else {

      // Neu MQTT mat ket noi, khong doc Queue
      vTaskDelay(pdMS_TO_TICKS(100));
    }
  }
}

// ==================================================
// 18. SETUP
// ==================================================

void setup() {

  Serial.begin(115200);

  // Khoi tao GPIO
  pinMode(PUMP_PIN, OUTPUT);

  pinMode(FAN_PIN, OUTPUT);

  pinMode(MIST_PIN, OUTPUT);

  digitalWrite(PUMP_PIN, LOW);

  digitalWrite(FAN_PIN, LOW);

  analogWrite(MIST_PIN, 0);

  // Khoi tao DHT22
  dht.begin();

  delay(2000);

  // Tao Queue
  dataQueue = xQueueCreate(
    10,
    sizeof(SensorData)
  );

  dataAlert = xQueueCreate(
    1,
    sizeof(float)
  );

  // Tao Mutex Semaphore
  serialMutex = xSemaphoreCreateMutex();

  actuatorMutex = xSemaphoreCreateMutex();

  // Kiem tra khoi tao
  if (dataQueue == NULL ||
      dataAlert == NULL ||
      serialMutex == NULL ||
      actuatorMutex == NULL) {

    Serial.println("LOI: KHONG TAO DUOC QUEUE HOAC SEMAPHORE!");

    while (true) {
      delay(1000);
    }
  }

  Serial.println("DA KHOI TAO QUEUE VA SEMAPHORE!");

  // Tao Task canh bao - Core 1
  BaseType_t alertTask = xTaskCreatePinnedToCore(
    Task_Alert,
    "Task_Alert",
    4096,
    NULL,
    2,
    NULL,
    1
  );

  // Tao Task doc du lieu - Core 1
  BaseType_t readTask = xTaskCreatePinnedToCore(
    Task_ReadData,
    "Task_ReadData",
    4096,
    NULL,
    2,
    NULL,
    1
  );

  // Tao Task truyen du lieu - Core 0
  BaseType_t transmitTask = xTaskCreatePinnedToCore(
    Task_Transmit,
    "Task_Transmit",
    6144,
    NULL,
    2,
    NULL,
    0
  );

  if (alertTask != pdPASS ||
      readTask != pdPASS ||
      transmitTask != pdPASS) {

    Serial.println("LOI: KHONG TAO DUOC MOT HOAC NHIEU TASK!");

  } else {

    Serial.println("DA KHOI DONG THANH CONG CA 3 TASK FREERTOS!");
  }
}

// ==================================================
// 19. LOOP
// ==================================================

void loop() {

  // Cac cong viec chinh da duoc xu ly trong 3 Task
  vTaskDelay(pdMS_TO_TICKS(1000));
}
