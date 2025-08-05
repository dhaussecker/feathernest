#include "LSM6DSOXSensor.h"
#include "lsm6dsoxtest.h"

#define INT_1 D3

extern volatile bool motionDetected = false;
extern volatile int state = -1;

//Interrupts.
volatile int mems_event = 0;

// Components
LSM6DSOXSensor AccGyr(&Wire, LSM6DSOX_I2C_ADD_L);

// MLC
ucf_line_t *ProgramPointer;
int32_t LineCounter;
int32_t TotalNumberOfLine;

void INT1Event_cb();
void printMLCStatus(uint8_t status);

void setupLSM6DSOX() 
{

  AccGyr.begin();

    /* Feed the program to Machine Learning Core */
  /* Motion Intensity program from lsm6dsoxtest.h */
  ProgramPointer = (ucf_line_t *)lsm6dsoxtest;
  TotalNumberOfLine = sizeof(lsm6dsoxtest) / sizeof(ucf_line_t);
  Serial.println("Motion Intensity for LSM6DSOX MLC");
  Serial.print("UCF Number Line=");
  Serial.println(TotalNumberOfLine);

  for (LineCounter = 0; LineCounter < TotalNumberOfLine; LineCounter++) {
    if (AccGyr.Write_Reg(ProgramPointer[LineCounter].address, ProgramPointer[LineCounter].data)) {
      Serial.print("Error loading the Program to LSM6DSOX at line: ");
      Serial.println(LineCounter);
      while (1) {
        delay(1000);
      }
    }
  }

  Serial.println("Program loaded inside the LSM6DSOX MLC");
  Serial.println("AccX,AccY,AccZ"); // Header for Serial Plotter

  AccGyr.Enable_X();
  AccGyr.Set_X_ODR(26.0f);  // Your 26 Hz ODR
  AccGyr.Set_X_FS(2);

  //Interrupts.
  pinMode(INT_1, INPUT);
  attachInterrupt(INT_1, INT1Event_cb, RISING);

}

int checkForStateChange()
{
    if (motionDetected) {
    LSM6DSOX_MLC_Status_t status;
    AccGyr.Get_MLC_Status(&status);
    if (status.is_mlc1) {
      uint8_t mlc_out[8];
      AccGyr.Get_MLC_Output(mlc_out);
      return mlc_out[0];
    }
  }
  return -1;
}

void INT1Event_cb() {
  motionDetected = true;
  state = checkForStateChange();
}