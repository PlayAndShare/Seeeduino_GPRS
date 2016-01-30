/*
2015-01-30 V1.0
*/

#include "gprs.h"
#include "TimerOne.h"
#include <SoftwareSerial.h>

/* Define */
#define DEBUG  0
#if (DEBUG == 1)
  #define UART_LOG(String) Serial.println(String)
#else 
  #define UART_LOG(String) NULL
#endif
#define ON             1
#define OFF            0
#define GPRSBUFFERSIZE 256
#define SHORTWAIT      2

/* Global Structure */
enum GSMState{
  get_nothing = 0, 
  get_ring,
  get_sms
};

enum SMSCmd{
  SWITCH_ON = 0,
  SWITCH_OFF,
  SET_MASTER_PHONE,
  CHANGE_CRYPTOGRAM
};

/* Global parameters */
String masterNumber1 = "135********";  /* Change Number1 to test */
String masterNumber2 = "136********";
String masterNumber3 = "137********";
char default_SmsOnCmd[12] = {  /* SN0000SETON */
  'S', 'N', '0', '0', '0', '0', 
  'S', 'E', 'T', 'O', 'N', '\0'
};
char default_SmsOffCmd[13] = {  /* SN0000SETOFF */
  'S', 'N', '0', '0', '0', '0', 
  'S', 'E', 'T', 'O', 'F', 'F', '\0'
};

char smsOnCmd[11];
char smsOffCmd[12];
unsigned long timeCountStart;
bool switchState = OFF;
char *gprsBuffer = (char *)malloc(GPRSBUFFERSIZE * sizeof(char));

/* Global Object */
GPRS gprs(9600);//BaudRate

/* Declare of functions */
int getGSMState(void);
int compareMSM(char *msm);

/* Setup function */
void setup(){
  Serial.begin(9600);
  while(!Serial);
  Serial.println("SMS massage read example...");
  gprs.preInit();//Power on SIM800
  while(0 != gprs.sendATTest())
  {
      Serial.println("sim800 init error!");
  }
  Serial.println("sim800 init O.K!");    
  while( 0 != gprs.deleteSMS(1))  /* Delete the first SMS for new one to store */
  {
    delay(1000);
  }
  Serial.println("Deleted the first SMS in storage!");
  timeCountStart = millis();    
}

/* Loop function */
void loop(){ 
  int state;  
  
  if(millis() - timeCountStart > 1000)
  {
    Serial.println("Heart Beat");
    timeCountStart = millis();
  }
  
  state = getGSMState();
  if( state == get_sms )
  {
    char *s = NULL;
    /* Print phone number */
    Serial.print("Get a SMS message from - ");
    if(NULL != ( s = strstr(gprsBuffer,"+CMGL:")))
    {      
      int i = 0;
      s = s + 23;
      while(*s != '\"')
      {       
        gprs.SMS_PhoneNumber[i++] = *s;
        s++;
      }
      gprs.SMS_PhoneNumber[i] = '\0';
      Serial.println(gprs.SMS_PhoneNumber);
    }                       
    /* Print message content */
    Serial.print("SMS message content: ");
    if(NULL != ( s = strstr(gprsBuffer,"+32")))
    {           
      int i = 0;
      s = s + 6;  /* Get message */                 
      
      while((*s != '\n'))
      {
        gprs.SMS_message[i++] = *s;
        s++;
      }
      gprs.SMS_message[i] = '\0';
      Serial.println(gprs.SMS_message);

      /* Deal with SMS */
      int msmCmd = compareMSM(gprs.SMS_message);
      switch(msmCmd)
      {
        case 0:  /* Turn on switch */
          Serial.println("Turn on switch");
          gprs.sendSMS(gprs.SMS_PhoneNumber, "switch on");
          break;
        case 1:  /* Turn off switch */
          Serial.println("Turn off switch");
          gprs.sendSMS(gprs.SMS_PhoneNumber, "switch off");
          break;
        default: break;
      }
      Serial.print("Switch ");
      switchState ? Serial.println("ON") : Serial.println("OFF");      
    }
    gprs.cleanBuffer(gprs.SMS_PhoneNumber,16);
    gprs.cleanBuffer(gprs.SMS_message,256);       
  }
  else if( state == get_ring )
  {
    boolean ret = true;
    for(int i = 0; i < strlen(gprs.RING_PhoneNumber); i++)
    {
      if(gprs.RING_PhoneNumber[i] != masterNumber1[i])        
      {
        ret = false;          
        break;
      }
    }
    if(ret)
    {
      switchState = !switchState;
      Serial.println("This is the master's phone number!");
      gprs.hangUpCall();  /* Rename hangeUpCall as terminateIncomeCall */
      Serial.println("Terminate the incoming phone!");
      Serial.print("Switch ");
      switchState ? Serial.println("ON") : Serial.println("OFF");
    }
    else
    {
      Serial.println("This is not the master's phone number");
    }      
  }  
}

int getGSMState(void)
{  
  unsigned long timeoutStart;
  int i = 0;
  int ret = get_nothing;
  char cmd[16];
  char *s;    
      
  memset(gprsBuffer, '\0', GPRSBUFFERSIZE * sizeof(char));  
  gprs.cleanBuffer(gprs.RING_PhoneNumber,16);     
  
  if(gprs.serialSIM800.available()) 
  {
    gprs.readBuffer(gprsBuffer,GPRSBUFFERSIZE,SHORTWAIT);
    UART_LOG(gprsBuffer);
    
    if(NULL != strstr(gprsBuffer,"RING"))  // Comes a call
    {      
      UART_LOG("Get a phone call! - ");
      if(NULL != ( s = strstr(gprsBuffer,"+CLIP:")))
      {        
        s = s + 8;
        while(*s != '\"')
        {
          gprs.RING_PhoneNumber[i++] = *s;
          s++;
        }
        gprs.RING_PhoneNumber[i] = '\0';
        UART_LOG(gprs.RING_PhoneNumber);        
      }
      
      ret = get_ring;
    }
    else if(NULL != strstr(gprsBuffer,"+CMTI: \"SM\""))  // Comes a SMS message
    {            
      char i = 0;
      
      //gprs.cleanBuffer(gprsBuffer,GPRSBUFFERSIZE);
      memset(gprsBuffer, '\0', GPRSBUFFERSIZE * sizeof(char));
      gprs.sendCmdAndWaitForResp("AT+CMGF=1\r\n","OK",2);
      sprintf(cmd,"AT+CMGL=\"REC UNREAD\"\r\n");
      gprs.serialSIM800.write(cmd);
      delay(200);
      //gprs.serialDebug();
      while(!gprs.serialSIM800.available());
      timeoutStart = millis();
      while( (millis() - timeoutStart < 2000))
      {
        if(gprs.serialSIM800.available())
        {
          gprsBuffer[i++] = gprs.serialSIM800.read();
        }        
      }
      gprsBuffer[i] = '\0';
      
      while(1)  /* Delete the first SMS for new one to store */
      {
        if(0 == gprs.deleteSMS(1))
        {
          UART_LOG("Deleted new MSM message O.K!");
          break;
        }
        else delay(1000);
      }
      ret = get_sms;
    }
    else
    {
      UART_LOG("Neither RING nor SMS!");
    }    
    UART_LOG("Deal with one Serial event over!");
  }
  
  return ret;
}

int compareMSM(char *msm)
{
  int ret = -1;

  if(!strncmp(default_SmsOnCmd, msm, sizeof(default_SmsOnCmd) - 1)) {
    ret = SWITCH_ON;
  } else if(!strncmp(default_SmsOffCmd, msm, sizeof(default_SmsOnCmd) - 1)) {
    ret = SWITCH_OFF;
  }
  
  return ret;
}

