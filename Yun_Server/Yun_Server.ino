/*
   Copyright (c) 2020 by Antoine Cavallera <antoine.cavallera@gmail.com>
   Copyright (c) 2020 by Claude Cavallera <claude.cavallera@gmail.com>
   LoraWan project for MKR 1300

   This file is free software; you can redistribute it and/or modify
   it under the terms of either the GNU General Public License version 2
   or the GNU Lesser General Public License version 2.1, both as
   published by the Free Software Foundation.
*/

/*--------------------------------------------*/
/*                 INCLUDES                   */
/*--------------------------------------------*/
#include <Console.h>
#include <SPI.h>
#include <RH_RF95.h>
#include <Process.h>
#include <RHReliableDatagram.h>

/*--------------------------------------------*/
/*                 DEFINES                    */
/*--------------------------------------------*/
//If you use Dragino IoT Mesh Firmware, uncomment below lines.
//For product: LG01.
#define BAUDRATE              115200
#define SERVER_ADDRESS        2
#define CHAR_SEPARATOR        0x23
//If you use Dragino Yun Mesh Firmware , uncomment below lines.
//#define BAUDRATE 250000

/*--------------------------------------------*/
/*                 GLOBAL DATA                */
/*--------------------------------------------*/
int led = A2;
float frequency = 915.0;
void uploadData(); // Upload Data to ThingSpeak.
String dataString = "";
String myWriteAPIString = "37P1XWK8VAF508NY";
uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];

enum {state_Receive, state_Decode, state_Upload, state_Idle}
state = state_Receive;

enum {state_D_waterTemp, state_D_airTemp, state_D_airPressure, state_D_occup, state_D_done}
state_D = state_D_waterTemp;

struct loRaFrame {
  char waterTemp[10];
  char airTemp[10];
  char airPressure[10];
  char occup[10];
  //float data_buffer[DATA_SIZE];
  char  separator;
};

/*--------------------------------------------*/
/*                 INSTANCES                  */
/*--------------------------------------------*/
// Singleton instance of the radio driver
RH_RF95 rf95;
// Class to manage message delivery and receipt, using the driver declared above
RHReliableDatagram manager(rf95, SERVER_ADDRESS);
// instance of the LoraFrame
struct loRaFrame LoraFrame1 = {"0", "0", "0", "0", CHAR_SEPARATOR};

/*--------------------------------------------*/
/*              SETUP FUNCTION                */
/*--------------------------------------------*/
void setup()
{
  pinMode(led, OUTPUT);
  Bridge.begin(BAUDRATE);
  Console.begin();
  while (!Console) ; // Wait for console port to be available
  Console.println("Start Sketch");
  if (!rf95.init())
    Console.println("init failed");
  // Setup ISM frequency
  rf95.setFrequency(frequency);
  // Setup Power,dBm
  rf95.setTxPower(20);
  //rf95.setPromiscuous(true);

  // Setup Spreading Factor (6 ~ 12)
  rf95.setSpreadingFactor(7);

  // Setup BandWidth, option: 7800,10400,15600,20800,31200,41700,62500,125000,250000,500000
  rf95.setSignalBandwidth(125000);

  // Setup Coding Rate:5(4/5),6(4/6),7(4/7),8(4/8)
  rf95.setCodingRate4(6);

  Console.print("Listening on frequency: ");
  Console.println(frequency);
}

/*--------------------------------------------*/
/*              MAIN FUNCTION                 */
/*--------------------------------------------*/
void loop()
{
  while (1)
  {
    switch (state)
    {
      case state_Receive:
        Console.println("state_Receive");
        receiveData();
        state = state_Decode;
        break;

      case state_Decode:
        Console.println("state_Decode");
        decodeData();
        state = state_Upload;
        break;

      case state_Upload:
        Console.println("state_Upload");
        uploadData();
        digitalWrite(led, LOW);
        state = state_Idle;
        break;

      case state_Idle:
        Console.println("state_Idle");
        delay(50);
        state = state_Receive;
        break;

      default:
        state = state_Idle;
        break;
    }
  }
}

/*--------------------------------------------*/
/*              receiveData FUNCTION          */
/*--------------------------------------------*/
void receiveData() {
  if (rf95.available())
  {
    int i = 0;
    // Should be a message for us now

    uint8_t len = sizeof(buf);
    if (rf95.recv(buf, &len))
    {
      digitalWrite(led, HIGH);
      RH_RF95::printBuffer("request: ", buf, len);
      Console.print("Get LoRa Packet: ");
      for (int i = 0; i < len; i++)
      {
        Console.print(buf[i], HEX);
        Console.print(" ");
      }
      Console.println();

      Console.print("got request: ");
      Console.print((char*)buf);
      Console.print("RSSI: ");
      Console.println(rf95.lastRssi(), DEC);
    }
    else
    {
      Console.println("recv failed");
    }

  }
}

/*--------------------------------------------*/
/*              decodeData FUNCTION           */
/*--------------------------------------------*/
void decodeData() {
  int j = 0;
  for (int i = 0; i < 30; i++ )
  {
    switch (state_D)
    {
      case state_D_waterTemp:
        j = 0;
        if (buf[i] != 0x23) {
          LoraFrame1.waterTemp[j] = buf[i];
        }
        else {
          i++;
          state_D = state_D_airTemp;
        }
        break;

      case state_D_airTemp:
        j = 0;
        if (buf[i] != 0x23) {
          LoraFrame1.airTemp[j] = buf[i];
        }
        else {
          i++;
          state_D = state_D_airPressure;
        }
        break;

      case state_D_airPressure:
        j = 0;
        if (buf[i] != 0x23) {
          LoraFrame1.airPressure[j] = buf[i];
        }
        else {
          i++;
          state_D = state_D_occup;
        }
        break;

      case state_D_occup:
        j = 0;
        if (buf[i] != 0x23) {
          LoraFrame1.airPressure[j] = buf[i];
        }
        else {
          i++;
          state_D = state_D_done;
        }
        break;

      case state_D_done:
        i = 200;
        break;

      default:
        state_D = state_D_done;
        break;
    }
  }
  Console.print("waterTemp");
  Console.println(LoraFrame1.waterTemp);
  Console.print("airTemp");
  Console.println(LoraFrame1.airTemp);
  Console.print("airPressure");
  Console.println(LoraFrame1.airPressure);
  Console.print("occup");
  Console.println(LoraFrame1.occup);
}
/*--------------------------------------------*/
/*              uploadData FUNCTION           */
/*--------------------------------------------*/
void uploadData() {//Upload Data to ThingSpeak
  // form the string for the API header parameter:


  // form the string for the URL parameter, be careful about the required "
  String upload_url = "https://api.thingspeak.com/update?api_key=";
  upload_url += myWriteAPIString;
  upload_url += "&field1=";
  upload_url += LoraFrame1.waterTemp;
  upload_url += "&field2=";
  upload_url += LoraFrame1.airTemp;
  upload_url += "&field3=";
  upload_url += LoraFrame1.airPressure;
  upload_url += "&field4=";
  upload_url += LoraFrame1.occup;

  Console.println("URL :");
  Console.println(upload_url);
  Console.println("\n");

  Console.println("Call Linux Command to Send Data");
  Process p;    // Create a process and call it "p", this process will execute a Linux curl command
  p.begin("curl");
  p.addParameter("-k");
  p.addParameter(upload_url);
  p.run();    // Run the process and wait for its termination

  Console.print("Feedback from Linux: ");
  // If there's output from Linux,
  // send it out the Console:
  while (p.available() > 0)
  {
    char c = p.read();
    Console.write(c);
  }
  Console.println("");
  Console.println("Call Finished");
  Console.println("####################################");
  Console.println("");
}
