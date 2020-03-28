/*
  LoRa Simple Yun Server :
  Support Devices: LG01.

  Example sketch showing how to create a simple messageing server,
  with the RH_RF95 class. RH_RF95 class does not provide for addressing or
  reliability, so you should only use RH_RF95 if you do not need the higher
  level messaging abilities.

  It is designed to work with the other example LoRa Simple Client

  User need to use the modified RadioHead library from:
  https://github.com/dragino/RadioHead

  modified 16 11 2016
  by Edwin Chen <support@dragino.com>
  Dragino Technology Co., Limited
*/
//If you use Dragino IoT Mesh Firmware, uncomment below lines.
//For product: LG01.
#define BAUDRATE 115200

//If you use Dragino Yun Mesh Firmware , uncomment below lines.
//#define BAUDRATE 250000

#include <Console.h>
#include <SPI.h>
#include <RH_RF95.h>
#include <Process.h>
// Singleton instance of the radio driver
RH_RF95 rf95;

int led = A2;
float frequency = 433.0;
void uploadData(); // Upload Data to ThingSpeak.
String dataStringTempWater = "";
String dataStringTempAir = "";
String dataStringPresAir = "";
String dataStringOccup = "";
String dataString = "";
String myWriteAPIString = "37P1XWK8VAF508NY";
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

void loop()
{
  if (rf95.available())
  {
    // Should be a message for us now
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);
    int i = 0;
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
      String dataStringTempWater = "";
      String dataStringTempAir = "";
      String dataStringPresAir = "";
      String dataStringOccup = "";

      i=0;
      dataStringTempWater = (char)buf[i];
      for (i = 1; i < 4; i++)
      {
        dataStringTempWater += (char)buf[i];
      }
      dataStringTempWater += "0";

      dataStringTempAir = (char)buf[i];
      for (i = 6; i < 9; i++)
      {
        dataStringTempAir += (char)buf[i];
      }

      dataStringPresAir = (char)buf[i];
      for (i = 11; i < 14; i++)
      {
        dataStringPresAir += (char)buf[i];
      }

      dataStringOccup = (char)buf[i];
      for (i = 16; i < 19; i++)
      {
        dataStringOccup += (char)buf[i];
      }


      Console.print("temp :");
      Console.print(dataStringTempWater);
//      Console.print(dataStringTempAir);
//      Console.print(dataStringPresAir);
//      Console.print(dataStringOccup);

   
       int newData[6] = {0, 0, 0, 0, 0,0}; //Store Sensor Data here
                    for (int i = 0; i < 6; i++)
                    {
                        newData[i] = buf[i];
                    }

                    char td = newData[0];
                    char tu = newData[1];
                    char pt = newData[2];
                    char diz = newData[3];
                    char cent = newData[4];
                    char mill = newData[5];
                    Console.print("Get Temperature:");
                    Console.print(td);
                    Console.print (tu);
                    Console.println(pt);
                    Console.print(diz);
                    Console.print(cent);
                    Console.print(mill);


                    dataString ="";
                    dataString += td;
                    dataString += tu;
                    dataString += pt;
                    dataString += diz;
                    dataString += cent ;
                    dataString += mill;
                    uploadData(); //








      digitalWrite(led, LOW);
    }
    else
    {
      Console.println("recv failed");
    }
  }
}
void uploadData() {//Upload Data to ThingSpeak
  // form the string for the API header parameter:


  // form the string for the URL parameter, be careful about the required "
  String upload_url = "https://api.thingspeak.com/update?api_key=";
  upload_url += myWriteAPIString;
  upload_url += "&field1=";
  upload_url += dataString;
  upload_url += "&field2=";
  upload_url += dataString;
  upload_url += "&field3=";
  upload_url += dataString;
  upload_url += "&field4=";
  upload_url += dataString;

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
