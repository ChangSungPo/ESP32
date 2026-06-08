/**
* @file FinalProject_controller.ino 
* @author Amith Chivukula
* @date June 5, 2026
* @version 1.0 
* * @brief Central Controller Node (Node 2) for the Distributed Secure Entry System 
* * This file implements the primary application host logic on the second ESP32-S3. 
* It establishes an ESP-NOW wireless receiver to consume security events from Node 1. 
* Incoming packets are pushed into a FreeRTOS queue where a state machine manages access 
* control windows, intrusion states, and updates the UI elements (LCD + Buzzer).
*/

// ===================== Includes =====================
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <WiFi.h>
#include <esp_now.h>


// ===================== Macros & Constants =====================
#define BUTTON_PIN 4  /**<GPIO PIN connected to system arm/disarm button */ 
#define BUZZER_PIN 7 /**<GPIO PIN connected to Piezo alarm buzzer */ 
#define AUTHORIZED_CARD_COUNT 2 /**<Total count of keys stored locally*/
#define I2C_SDA 8 
#define I2C_SCL 9 


// ===================== Global Variables =====================

LiquidCrystal_I2C lcd(0x27, 16, 2); // Initialize the LCD

// TODO: Update Node 2 MAC address
uint8_t node2Address[] = {0x80, 0xB5, 0x4E, 0xE3, 0x0F, 0x04};
/**
* @struct struct_message
* @brief Network packet matching node 1's data serialization 
*/
typedef struct struct_message {
  char eventType[12]; /**<Flag mapping the source sensor RFID or ULTRASONIC */
  float distance; /**<Raw proximity distance value in cm or -1*/    
  char uid[20];  /**< RFID token string*/     
} struct_message;

struct_message incomingData; 

QueueHandle_t securityQueue; 

TaskHandle_t StateMachineTaskHandle = NULL; 

/**
* @enum SystemState 
* @brief States repersenting the FSM tracking secura access 
*/
enum SystemState{
  SYSTEM_ARMED, 
  ACCESS_GRANTED, 
  BREACH_ALARM
};

SystemState currentState = SYSTEM_ARMED;

const String authorizedCards[AUTHORIZED_CARD_COUNT] = {
  "A1B2C3D4", 
  "ECE12345"
}; 

// ===================== Function Prototypes =====================
void onDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len); 
void stateMachineTask(void* pvParameters); 

// ===================== Function Implementations =====================

/**
* @brief ESP-NOW Packet Reception Callback (ISR Context)
* * Automatically executed at hardware link whenever a peer node broadcasts
* an RF packet matching this device's MAC address. Copies the mem struct and 
* hands it off to the async FSM
* *  @param info Pointer to the esp_now_recv_info struct metadata
* @param incomingData Pointer to buffer received 
* @param len Total size of incoming buffer in bytes
* @return void
*/
void onDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len){
  struct_message packet; 
  
  memcpy(&packet, incomingData, sizeof(packet)); 
  xQueueSendFromISR(securityQueue, &packet, NULL); // non-blocking thread-safe from ISR
}

/**
* @brief Core 1 Security FSM and UI Management. 
* * Operates at ~50Hz. Evals SW debounced reset switch, parses telem payloads from data queue,
* manages FSM state transitions, toggles alarm, and updates LCD display. 
* * @param pvParameters FreeRTOS param pointer config (unused). 
* @return void
*/
void stateMachineTask(void *pvParameters){
  struct_message receivedMsg; 

  // Initial Layout 
  lcd.setCursor(0, 0); 
  lcd.print("STATUS: ARMED  "); 
  lcd.setCursor(0, 1); 
  lcd.print("Scanning...    "); 

  while(1){
    // UI Elements 
    if (digitalRead(BUTTON_PIN) == LOW){
      vTaskDelay(pdMS_TO_TICKS(50)); 
      if (digitalRead(BUTTON_PIN)  == LOW){
        Serial.println("[BUTTON DEBUG] Physical button press detected!");
        if (currentState == BREACH_ALARM){
          Serial.println("[Core 1] Manual Reset Button Pressed. Re-arming system."); 
          ledcWriteTone(BUZZER_PIN, 0);
          currentState = SYSTEM_ARMED; 
          lcd.clear(); 
          lcd.setCursor(0, 0); 
          lcd.print("STATUS: ARMED  "); 
          lcd.setCursor(0, 1); 
          lcd.print("System Reset!    "); 
        }
        // Avoid multi-triggering while user holds buttond own 
        while(digitalRead(BUTTON_PIN) == LOW){
          vTaskDelay(pdMS_TO_TICKS(10)); 
        }
      }
    }

    // Network data processing + FSM
    if (xQueueReceive(securityQueue, &receivedMsg, pdMS_TO_TICKS(20)) == pdTRUE){
      String event = String(receivedMsg.eventType); 

      // Evaluate incoming RFID creds
      if (event == "RFID"){
        String scannedUid = String(receivedMsg.uid); 
        Serial.printf("[Core 1] Processing RFID Event for UID: %s\n", scannedUid.c_str()); 

        bool matched = false; 
        for (int i = 0; i < AUTHORIZED_CARD_COUNT; i++){
          if (scannedUid == authorizedCards[i]){
            matched = true; 
            break; 
          }
        }
        if (matched){
          currentState = ACCESS_GRANTED; 
          lcd.clear(); 
          lcd.setCursor(0, 0); 
          lcd.print("ACCESS GRANTED  "); 
          lcd.setCursor(0, 1); 
          lcd.print("UID: ");
          lcd.print(scannedUid); 

          // keep open for 4000ms before re-arming 
          vTaskDelay(pdMS_TO_TICKS(4000)); 

          currentState = SYSTEM_ARMED; 
          lcd.clear(); 
          lcd.setCursor(0, 0); 
          lcd.print("STATUS: ARMED  "); 
          lcd.setCursor(0, 1); 
          lcd.print("Scanning...    "); 
        }
        else{
          // Log unauthorized access attempt
          Serial.printf("[Core 1] WARNING: Unauthorized Card Read: %s\n", scannedUid.c_str());
          lcd.setCursor(0, 1); 
          lcd.print("INVALID CARD!  "); 
          vTaskDelay(pdMS_TO_TICKS(1500)); 

          if (currentState == SYSTEM_ARMED){
            lcd.setCursor(0, 1); 
            lcd.print("Scanning...   "); 
          }
        }
      }

      // Eval ultrasonic telem
      else if (event == "ULTRASONIC"){
        Serial.printf("[Core 1] Processing potential breach. Distance: %.2f cm\n", receivedMsg.distance); 
        if (currentState == SYSTEM_ARMED){
          currentState = BREACH_ALARM; 
          lcd.clear(); 
          lcd.setCursor(0, 0); 
          lcd.print("** INTRUSION **"); 
          lcd.setCursor(0, 1); 
          lcd.print("Dist: "); 
          lcd.print(receivedMsg.distance); 
          lcd.print(" cm"); 

          // The buzzer will stay on until the reset button is hit as an ack. 
          ledcWriteTone(BUZZER_PIN, 2000); 
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(20)); // Target 50Hz scheduling
  }
}

/**
 * @brief Temporary Simulation Task to mimic Node 1 wireless payloads for offline testing.
 * Pushes mock RFID and Ultrasonic events into the queue every few seconds.
 * * @param pvParameters FreeRTOS param pointer config (unused). 
 */
void offlineSimulationTask(void *pvParameters) {
  struct_message mockPacket;
  
  while(1) {
    // ---- Test Case A: Simulate a Valid RFID Swipe ----
    Serial.println("[Sim] Sending valid RFID card event to queue...");
    strcpy(mockPacket.eventType, "RFID");
    strcpy(mockPacket.uid, "A1B2C3D4"); // Matches what's in the list
    mockPacket.distance = -1.0;
    
    xQueueSend(securityQueue, &mockPacket, 0); 
    vTaskDelay(pdMS_TO_TICKS(10000)); // Wait 10 seconds

    // ---- Test Case B: Simulate an Unauthorized Intrusion ----
    Serial.println("[Sim] Sending Ultrasonic breach event to queue...");
    strcpy(mockPacket.eventType, "ULTRASONIC");
    strcpy(mockPacket.uid, "N/A");
    mockPacket.distance = 23.4; // Simulates an object sitting at 23.4 cm
    
    xQueueSend(securityQueue, &mockPacket, 0);
    vTaskDelay(pdMS_TO_TICKS(12000)); // Wait 12 seconds before repeating
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(BUTTON_PIN, INPUT_PULLUP); 
  pinMode(BUZZER_PIN, OUTPUT); 
  digitalWrite(BUZZER_PIN, LOW); 

  ledcAttach(BUZZER_PIN, 2000, 8);

  Wire.begin(I2C_SDA, I2C_SCL); 
  lcd.init(); 
  lcd.backlight(); 
  lcd.setCursor(0, 0); 
  lcd.print("Booting...  "); 

  // WiFi stack
  WiFi.mode(WIFI_STA); 
  WiFi.STA.begin();
  WiFi.disconnect(); 

  Serial.print("Controller Board MAC Address: "); 
  Serial.println(WiFi.macAddress()); 

  if (esp_now_init() != ESP_OK){
    Serial.println("Fatal: ESP-NOW Fwk Init Failed"); 
    lcd.setCursor(0, 1); 
    lcd.print("ESP-NOW ERROR  "); 
    return; 
  }

  // Register background consumer 
  esp_now_register_recv_cb(onDataRecv); 

  securityQueue = xQueueCreate(10, sizeof(struct_message)); 

  xTaskCreatePinnedToCore(
    stateMachineTask,
    "StateMachineTask",
    4096, 
    NULL, 
    2, 
    &StateMachineTaskHandle, 
    1
  ); 


  Serial.println("Node 2 Init Done"); 

  // Spawn the offline simulation task on Core 0 to act as our "virtual" partner node
  // xTaskCreatePinnedToCore(offlineSimulationTask, "SimTask", 2048, NULL, 1, NULL, 0);
}

void loop() {
  // put your main code here, to run repeatedly:
  vTaskDelay(pdMS_TO_TICKS(1000)); // yield peremanently 
}
