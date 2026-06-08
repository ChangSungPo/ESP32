/**
 * @file FinalProject.ino
 * @brief This is the node 1 with 2 sensor component code file
 * @author Sung-Po Chang
 * @date 2026-06-01
 */

#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <esp_now.h>
#include "soc/timer_group_reg.h"

/** @name Ultrasonic Sensor Configuration
 * Pins configuration for the ultrasonic distance sensor.
 */
 ///@{
#define TRIG_PIN 4  ///< Trigger pin for the ultrasonic sensor
#define ECHO_PIN 5  ///< Echo pin for the ultrasonic sensor
///@}

/** @name RFID Configuration
 * Pins configuration for the RFID module.
 */
///@{
#define RST_PIN  9  ///< Reset pin for the RFID module
#define SS_PIN   10 ///< Slave Select (SS) pin for the RFID module
///@}

/** @name Ultrasonic Sensor Thresholds
 * Threshold configurations for distance measurement and alarm logic.
 */
///@{
#define DISTANCE_THRESHOLD_CM 50.0 ///< Distance threshold in centimeters
#define CONSECUTIVE_CLOSE_THRESHOLD 3  ///< Consecutive close readings required to activate the alarm
#define CONSECUTIVE_FAR_THRESHOLD 8 ///< Consecutive far readings required to deactivate the alarm
///@}

/** @name RFID Thresholds
 * Parameter configurations for RFID card processing.
 */
///@{
#define RFID_COOLDOWN_TIMEUS_THRESHOLD 2000 ///< Cooldown time in microseconds for reading the same card
///@}

#define SYSTEM_HEARTBEAT_CHECK 600

MFRC522 mfrc522(SS_PIN, RST_PIN);

uint8_t node2Address[] = {0x80, 0xB5, 0x4E, 0xE3, 0x0F, 0x04};


/**
 * @brief Structure to hold message data for inter-task communication.
 *
 * This structure encapsulates the event type, measured distance, and RFID UID
 * to be transmitted across tasks or network queues.
 */
typedef struct struct_message {
  char eventType[12]; ///< Type of the event (e.g., "ULTRASONIC", "RFID")
  float distance;     ///< Measured distance from the ultrasonic sensor in cm
  char uid[20];       ///< RFID card unique identifier (UID) string
} struct_message;

/** @name FreeRTOS Handles
 * Handles for managing system tasks and inter-task communication queues.
 */
///@{
QueueHandle_t networkQueue;                   ///< Queue handle for network communication messages
TaskHandle_t UltrasonicTaskHandle = NULL;     ///< Task handle for the ultrasonic sensor data processing task
TaskHandle_t RfidTaskHandle = NULL;           ///< Task handle for the RFID reader and verification task
TaskHandle_t NetworkTaskHandle = NULL;        ///< Task handle for the network transmission task
///@}


uint32_t getHardwareTime() {
  *( (volatile uint32_t *) TIMG_T0UPDATE_REG(0) ) = 1;
  return *( (volatile uint32_t *) TIMG_T0LO_REG(0) );
}


/** @name Core 0: Ultrasonic Sensor Task
 * Ultrasonic sensor logic implementation
 */
///@{
void ultrasonicTask(void *pvParameters) {
  uint32_t lastLoopTime = getHardwareTime();
  uint32_t interval = 20000;

  float distance = 0.0;
  long duration = 0;
  struct_message msg;
  strcpy(msg.eventType, "ULTRASONIC"); 
  strcpy(msg.uid, "N/A");

  int heartbeatCounter = 0;

  bool isAlertActive = false;
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

      duration = pulseIn(ECHO_PIN, HIGH, 12000); ///<Measure round-trip Time-of-Flight (us)
      
      if (duration > 0) {
        distance = (duration * 0.0343) / 2.0;  ///<Calculate the distance using Speed ​​of sound in air (0.0343 cm/us) after send out the pulse
          
        if (distance < DISTANCE_THRESHOLD_CM) {
          consecutiveCloseCount++;
          consecutiveFarCount = 0; 

          if (consecutiveCloseCount >= CONSECUTIVE_CLOSE_THRESHOLD) {  ///<object has to stay long enough to trigger alarm
            if (!isAlertActive) {
              msg.distance = distance;
              xQueueSend(networkQueue, &msg, 0); ///<push into queue
              Serial.printf("[Core %d] Alarm!!! Too close -> Distance: %.2f cm\n", xPortGetCoreID(), distance);
              isAlertActive = true; ///<set the flag to true until the object is left
            }
          }
        } else {
          consecutiveFarCount++;
          consecutiveCloseCount = 0;

          if (consecutiveFarCount >= CONSECUTIVE_FAR_THRESHOLD) { ///<object has to leave long enough to deactiave alarm
            if (isAlertActive) {
              Serial.printf("[Core %d] Object left, Alarm deactivate\n", xPortGetCoreID());
              isAlertActive = false; 
            }
          }
        }
      } else { ///<sensor not receive, consider object gone
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
///@}

/** @name Core 0: RFID Task
 * RFID sensor logic implementation
 */
///@{
void rfidTask(void *pvParameters) {
  uint32_t lastLoopTime = getHardwareTime();
  uint32_t interval = 31250; ///<32 Hz -> every 31250 us

  struct_message msg;
  strcpy(msg.eventType, "RFID");
  msg.distance = -1.0;

  String lastTriggeredUid = "";  ///<record last sensing UID
  uint32_t lastTriggerTimeUs = 0; ///<record laset sensing time

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

      if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) { ///<read the card information

        String uidString = "";
        for (byte i = 0; i < mfrc522.uid.size; i++) {
          uidString += String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : "");
          uidString += String(mfrc522.uid.uidByte[i], HEX);
        }
        uidString.toUpperCase();

        if (uidString == lastTriggeredUid && (currentTime - lastTriggerTimeUs < RFID_COOLDOWN_TIMEUS_THRESHOLD)) {
          ///<no-op, this card is in cool time
        } else {
          Serial.print("[Core " + String(xPortGetCoreID()) + "] Card approaching, UID:");
          Serial.println(uidString);

          uidString.toCharArray(msg.uid, sizeof(msg.uid));
          xQueueSend(networkQueue, &msg, 0); ///<push into queue

          lastTriggeredUid = uidString;
          lastTriggerTimeUs = currentTime;
        }

        
        mfrc522.PICC_HaltA();   ///<stop sensing until next timer
        mfrc522.PCD_StopCrypto1();    ///<stop sensing until next timer
      } else { ///<unable to read card
        if (currentTime - lastTriggerTimeUs >= RFID_COOLDOWN_TIMEUS_THRESHOLD) { ///<clear tracking card after cool time
          lastTriggeredUid = "";
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(1));
  }
}
///@}


/** @name Core 1 ESP-NOW send task
 * ESP-NOW wireless transaction implementation
 */
///@{
void networkTask(void *pvParameters) {
  struct_message receivedMsg;

  while (1) {
    
    if (xQueueReceive(networkQueue, &receivedMsg, portMAX_DELAY) == pdTRUE) { ///< Queue has new event

      esp_err_t result = esp_now_send(node2Address, (uint8_t *) &receivedMsg, sizeof(receivedMsg));  ///< send to node 2
        
      if (result == ESP_OK) {
        Serial.printf("[Core 1] %s event send to Node 2\n", receivedMsg.eventType);
      } else {
        Serial.println("[Core 1] event send to Node 2 fail\n");
      }
    }
  }
}
///@}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  uint32_t config_val = 0;
  config_val |= (1UL << 31);   ///<Enable: 1
  config_val |= (1UL << 30);   ///<INCREASE: 1
  config_val |= (1UL << 29);   ///<AUTORELOAD: 1
  config_val |= (80UL << 13);  ///<DIVIDER: 80
  *( (volatile uint32_t *) TIMG_T0CONFIG_REG(0) ) = config_val;


  SPI.begin();  ///<initial RFID module
  mfrc522.PCD_Init();

  delay(4);

  
  WiFi.mode(WIFI_STA);  ///<initial ESP-NOW wireless communication
  WiFi.disconnect();

  if (esp_now_init() != ESP_OK) { 
    return; 
  }

  esp_now_peer_info_t peerInfo;  ///<initial for Node 2 connection
  memset(&peerInfo, 0, sizeof(peerInfo)); ///<clear memory
  memcpy(peerInfo.peer_addr, node2Address, 6);  ///<set Node 2 address with 6 bytes
  peerInfo.channel = 0;  ///<auto connection and align wifi channel
  peerInfo.encrypt = false; ///<no need to encrypt
  if (esp_now_add_peer(&peerInfo) != ESP_OK) { ///<set connection information into board
    return; 
  }


  networkQueue = xQueueCreate(10, sizeof(struct_message));  ///<initial queue with 10 slots
 
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
