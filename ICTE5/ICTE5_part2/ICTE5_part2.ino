//Filename: ICTE5_part2.ino
//Author: Sung-Po Chang
//Date: 05/10/2026
//Description: Implementing a Task Control Block (TCB) Based Scheduler
//Version: 1.0

//TASK STATE
#define STATE_READY    1
#define STATE_SLEEPING 0

//LED
#define LED1_PIN 1
#define LED2_PIN 2

// TCB struct
typedef struct TCBstruct {
  void (*ftpr)(void *p);
  void *arg_ptr;
  unsigned short int state;
  unsigned int interval;
  unsigned long lastRun;     
} TCB;

void blinkTask(void *p) {
  int pin = *((int *)p);
  digitalWrite(pin, !digitalRead(pin));
}

// argument for blink LED task
int argA = LED1_PIN;
int argB = LED2_PIN;

TCB TaskList[] = {
  {blinkTask, &argA, STATE_READY, 500, 0},  //1Hz (0.5s ON, 0.5s OFF)
  {blinkTask, &argB, STATE_READY, 250, 0},  //2Hz (0.25s ON, 0.25s OFF)
  {NULL,NULL,0,0,0}  //Assign a NULL function pointer to mark the end of the task list.
};

void setup() {
  Serial.begin(115200);
  Serial.println("Round-Robin Scheduler with Function Pointers Started.");
  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);
}

void loop() {
  long currentTime = millis();
  for (int i = 0; TaskList[i].ftpr != NULL; i++) {
    if (TaskList[i].state == STATE_READY) {
      if (currentTime - TaskList[i].lastRun >= TaskList[i].interval) {
        TaskList[i].ftpr(TaskList[i].arg_ptr);
        TaskList[i].lastRun = currentTime;
      }
    }
  }

  delay(10);
}
