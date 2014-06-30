/*
Sketch: FM Test

Function: This sketch is used to test SIM800'FM function.
*********************************************************************************
note: the following pins has been used and should not be used for other purposes.
  pin 8   // tx pin
  pin 7   // rx pin
  pin 9   // power key pin
  pin 12  // power status pin
*********************************************************************************
create on 2013/12/5, version: 0.1
by lawliet.zou(lawliet.zou@gmail.com)
*/

#include <fm.h>
#include <SoftwareSerial.h>

int channelButton = 5; //used for changing channel
FM fm;

void setup() {
  pinMode(channelButton,INPUT);
  Serial.begin(9600);
  Serial.println("FM Test...");
  fm.preInit();
  while(0 != fm.powerOn()){
    Serial.println("FM power on failed, try again...");
    delay(2000);
  }
  fm.setVolume(6); //0,1,2,3,4,5,6
  fm.scanChannel();
  Serial.println("FM init success");
}

void loop() {
  while(HIGH == digitalRead(channelButton)){
    delay(50);
  }
  Serial.print("change Channel\r\n");
  fm.channelNext();
  while(LOW == digitalRead(channelButton)){
    delay(50);
  }
}





