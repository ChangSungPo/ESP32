#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <esp_now.h>

// Timer and Synchronization 
#include "soc/timer_group_reg.h"

#define TRIG_PIN 4  // Ultrasonic Sensor
#define ECHO_PIN 5  // Ultrasonic Sensor

// RFID
#define RST_PIN  9
#define SS_PIN   10

#define DISTANCE_THRESHOLD_CM 50.0 // Ultrasonic Sensor distance threshold
#define CONSECUTIVE_CLOSE_THRESHOLD 3  // Ultrasonic Sensor activate alarm threshold
#define CONSECUTIVE_FAR_THRESHOLD 8 // Ultrasonic Sensor deactivate alarm threshold
#define RFID_COOLDOWN_TIMEUS_THRESHOLD 2000 // RFID same card cooldown time


#define SYSTEM_HEARTBEAT_CHECK 600

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Declare RFID component

// TODO: Update Node 2 MAC address
uint8_t node2Address[] = {0x74, 0x4D, 0xBD, 0xBD, 0xBD, 0xBD};

typedef struct struct_message {
  char eventType[12]; 
  float distance;     
  char uid[20];       
} struct_message;

QueueHandle_t networkQueue;
TaskHandle_t UltrasonicTaskHandle = NULL;
TaskHandle_t RfidTaskHandle = NULL;
TaskHandle_t NetworkTaskHandle = NULL;

// get current hardware time
uint32_t getHardwareTime() {
  *( (volatile uint32_t *) TIMG_T0UPDATE_REG(0) ) = 1;
  return *( (volatile uint32_t *) TIMG_T0LO_REG(0) );
}


// Core 0: Ultrasonic Sensor Task
void ultrasonicTask(void *pvParameters) {
  uint32_t lastLoopTime = getHardwareTime();
  uint32_t interval = 20000; // 50 Hz -> every 20000 us

  float distance = 0.0;
  long duration = 0;
  struct_message msg;
  strcpy(msg.eventType, "ULTRASONIC"); 
  strcpy(msg.uid, "N/A");

  int heartbeatCounter = 0;

  bool isAlertActive = false; // flag for alert
  int consecutiveCloseCount = 0;
  int consecutiveFarCount = 0;

  while (1) {
    uint32_t currentTime = getHardwareTime();

    if (currentTime - lastLoopTime >= interval) {
      lastLoopTime = currentTime;

      heartbeatCounter++;
      if (heartbeatCounter >= SYSTEM_HEARTBEAT_CHECK) {
        Serial.printf("[Core %d] Ultrasonic heartbeat -> Distance: %.2f cm\n", xPortGetCoreID(), distance);
        heartbeatCounter = 0;
      }

      digitalWrite(TRIG_PIN, LOW);
      delayMicroseconds(2);
      digitalWrite(TRIG_PIN, HIGH);
      delayMicroseconds(10);
      digitalWrite(TRIG_PIN, LOW);

      duration = pulseIn(ECHO_PIN, HIGH, 12000); // Measure round-trip Time-of-Flight (us)
      
      if (duration > 0) {
        distance = (duration * 0.0343) / 2.0;  // Calculate the distance using Speed ​​of sound in air (0.0343 cm/us) after send out the pulse
          
        if (distance < DISTANCE_THRESHOLD_CM) {
          consecutiveCloseCount++;
          consecutiveFarCount = 0; 

          if (consecutiveCloseCount >= CONSECUTIVE_CLOSE_THRESHOLD) {  // object has to stay long enough to trigger alarm
            if (!isAlertActive) {
              msg.distance = distance;
              xQueueSend(networkQueue, &msg, 0); // push into queue
              Serial.printf("[Core %d] Alarm!!! Too close -> Distance: %.2f cm\n", xPortGetCoreID(), distance);
              isAlertActive = true; // set the flag to true until the object is left
            }
          }
        } else {
          consecutiveFarCount++;
          consecutiveCloseCount = 0;

          if (consecutiveFarCount >= CONSECUTIVE_FAR_THRESHOLD) { // object has to leave long enough to deactiave alarm
            if (isAlertActive) {
              Serial.printf("[Core %d] Object left, Alarm deactivate\n", xPortGetCoreID());
              isAlertActive = false; 
            }
          }
        }
      } else { // sensor not receive, consider object gone
        consecutiveFarCount++;
        consecutiveCloseCount = 0;

        if (consecutiveFarCount >= CONSECUTIVE_FAR_THRESHOLD) {
          if (isAlertActive) {
            Serial.printf("[Core %d] Object left, Alarm deactivate\n", xPortGetCoreID());
            isAlertActive = false;
          }
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

// Core 0: RFID Task
void rfidTask(void *pvParameters) {
  uint32_t lastLoopTime = getHardwareTime();
  uint32_t interval = 31250; // 32 Hz -> every 31250 us

  struct_message msg;
  strcpy(msg.eventType, "RFID");
  msg.distance = -1.0;

  String lastTriggeredUid = "";  // record last sensing UID
  uint32_t lastTriggerTimeUs = 0; // record laset sensing time

  int heartbeatCounter = 0;

  while (1) {
    uint32_t currentTime = getHardwareTime();

    if (currentTime - lastLoopTime >= interval) {
      lastLoopTime = currentTime;

      heartbeatCounter++;
      if (heartbeatCounter >= SYSTEM_HEARTBEAT_CHECK) { 
        Serial.printf("[Core %d] RFID heartbeat check\n", xPortGetCoreID());
        heartbeatCounter = 0;
      }

      if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) { // read the card information

        String uidString = "";
        for (byte i = 0; i < mfrc522.uid.size; i++) {
          uidString += String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : "");
          uidString += String(mfrc522.uid.uidByte[i], HEX);
        }
        uidString.toUpperCase();

        if (uidString == lastTriggeredUid && (currentTime - lastTriggerTimeUs < RFID_COOLDOWN_TIMEUS_THRESHOLD)) {
          // no-op, this card is in cool time
        } else {
          Serial.print("[Core " + String(xPortGetCoreID()) + "] Card approaching, UID:");
          Serial.println(uidString);

          uidString.toCharArray(msg.uid, sizeof(msg.uid));
          xQueueSend(networkQueue, &msg, 0); // push into queue

          lastTriggeredUid = uidString;
          lastTriggerTimeUs = currentTime;
        }

        // stop sensing until next timer
        mfrc522.PICC_HaltA();
        mfrc522.PCD_StopCrypto1();
      } else { // unable to read card
        if (currentTime - lastTriggerTimeUs >= RFID_COOLDOWN_TIMEUS_THRESHOLD) { // clear tracking card after cool time
          lastTriggeredUid = "";
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(1));
  }
}


// Core 1 ESP-NOW send task
void networkTask(void *pvParameters) {
  struct_message receivedMsg;

  while (1) {
    // Queue has new event
    if (xQueueReceive(networkQueue, &receivedMsg, portMAX_DELAY) == pdTRUE) {

      // send to Node 2
      esp_err_t result = esp_now_send(node2Address, (uint8_t *) &receivedMsg, sizeof(receivedMsg));
        
      if (result == ESP_OK) {
        Serial.printf("[Core 1] %s event send to Node 2\n", receivedMsg.eventType);
      } else {
        Serial.println("[Core 1] event send to Node 2 fail\n");
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // initial sensor pin
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  uint32_t config_val = 0;
  config_val |= (1UL << 31);   // Enable: 1
  config_val |= (1UL << 30);   // INCREASE: 1
  config_val |= (1UL << 29);   // AUTORELOAD: 1
  config_val |= (80UL << 13);  // DIVIDER: 80
  *( (volatile uint32_t *) TIMG_T0CONFIG_REG(0) ) = config_val;

  // initial RFID module
  SPI.begin();
  mfrc522.PCD_Init();

  // wait initialization
  delay(4);

  // initial ESP-NOW wireless communication
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != ESP_OK) { 
    return; 
  }

  esp_now_peer_info_t peerInfo;  // initial for Node 2 connection
  memset(&peerInfo, 0, sizeof(peerInfo)); // clear memory
  memcpy(peerInfo.peer_addr, node2Address, 6);  // set Node 2 address with 6 bytes
  peerInfo.channel = 0;  // auto connection and align wifi channel
  peerInfo.encrypt = false; // no need to encrypt
  if (esp_now_add_peer(&peerInfo) != ESP_OK) { // set connection information into board
    return; 
  }

  // initial queue with 10 slots
  networkQueue = xQueueCreate(10, sizeof(struct_message));
 
  // Core 0: Ultrasonic task
  xTaskCreatePinnedToCore(
    ultrasonicTask,
    "UltrasonicTask",
    4096,
    NULL,
    3,
    &UltrasonicTaskHandle,
    0 // Core 0
  );

  // Core 0: RFID task
  xTaskCreatePinnedToCore(
    rfidTask,
    "RfidTask",
    4096,
    NULL,
    3,
    &RfidTaskHandle,
    0 // Core 0
  );

  // Core 1: ESP-NOW task
  xTaskCreatePinnedToCore(
    networkTask,
    "NetworkTask",
    4096,
    NULL,
    2,
    &NetworkTaskHandle,
    1 // Core 1
  );

  Serial.println("Node 1 setting finish");
}

void loop() {
  // put your main code here, to run repeatedly:
  vTaskDelay(pdMS_TO_TICKS(1000));
}
