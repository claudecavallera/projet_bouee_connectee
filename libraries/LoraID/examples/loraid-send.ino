#include <loraid.h>

long interval = 10000;    // 10 s interval to send message
long previousMillis = 0;  // will store last time message sent
unsigned int counter = 0;     // message counter

void setup() {
  // Setup loraid access
  lora.init();

  // Set LoRaWAN Class
  lora.setDeviceClass(CLASS_A);

  // Set Data Rate
  lora.setDataRate(2);
  
  // Put Antares Key and DevAddress here
  lora.AccessKey((unsigned char *)"8878f39f897b9a50:bd6b3446f4c13871", (unsigned char *)"00000001");
}

void loop() {
  // put your main code here, to run repeatedly:
  char myStr[50];
  unsigned long currentMillis = millis();

  // Check interval overflow
  if(currentMillis - previousMillis > interval) {
    previousMillis = currentMillis; 

    sprintf(myStr, "Ini data LoRa ke-%d", counter); 
    lora.sendToAntares((unsigned char *)myStr, strlen(myStr), 0);
  }
  
  // Check Lora RX
  lora.update();
}