//Filename: ICTE5_part1.ino
//Author: Sung-Po Chang
//Date: 05/10/2026
//Description: Implementing a Round-Robin Scheduler Using Function Pointers
//Version: 1.0

#define LED1_PIN 1
#define LED2_PIN 2

typedef void (*funcPtr)();  //function pointer
long taskATimer = 0;
long taskAinterval = 500;
long taskBTimer = 0;
long taskBinterval = 200;

//taskA for LED1
void taskA() {
  long currentTime = millis();
  if (currentTime - taskATimer >= taskAinterval) {
    digitalWrite(LED1_PIN, !digitalRead(LED1_PIN));
    taskATimer = currentTime;
  }
}

//taskB for LED2
void taskB() {
  long currentTime = millis();
  if (currentTime - taskBTimer >= taskBinterval) {
    digitalWrite(LED2_PIN, !digitalRead(LED2_PIN));
    taskBTimer = currentTime;
  }
}

void executeTask(void (*functionPTR)()) {
  if (functionPTR != NULL) {
    functionPTR();
  }
}

// taskList and size
funcPtr taskList[] = {taskA, taskB};
int numTasks = sizeof(taskList) / sizeof(taskList[0]);

void setup() {
  Serial.begin(115200);
  Serial.println("Round-Robin Scheduler with Function Pointers Started.");
  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);
}

void loop() {
  for (int i = 0; i < numTasks; i++) {
    executeTask(taskList[i]);
  }

  delay(10);
}
