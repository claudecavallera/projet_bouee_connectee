/*
   Copyright (c) 2020 by Antoine Cavallera <antoine.cavallera@gmail.com>
   Copyright (c) 2020 by Claude Cavallera <claude.cavallera@gmail.com>
   LoraWan project for MKR 1300

   This file is free software; you can redistribute it and/or modify
   it under the terms of either the GNU General Public License version 2
   or the GNU Lesser General Public License version 2.1, both as
   published by the Free Software Foundation.
*/

#include "Arduino.h"
#include <SPI.h>
#include <ArduinoLowPower.h>
#include <LoRa.h>
#include <OneWire.h>
#include <DallasTemperature.h>

/* defines */
#define DEBUG
#define LORA_FREQUENCY 433E6
#define TRUE 1
#define FALSE 0
#define ONE_WIRE_BUS 2
/* preprocessor define for debug purpose */
#ifdef DEBUG
#define DEBUG_PRINT(x)  Serial.println (x)
#else
#define DEBUG_PRINT(x)
#endif
/* Global data */
enum {state_Read, state_Send, state_Compute, state_Idle} state = state_Read;
const int counterMax = 10;
float data_temp = 0;
// Connect one wire to bus 2
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

void setup() {
  /********** General Setup **********/
  Serial.begin(9600);
  while (!Serial);


  /********** Setup LoraWan **********/
  DEBUG_PRINT("LoRa Sender non-blocking Callback");
  if (!LoRa.begin(LORA_FREQUENCY)) {
    DEBUG_PRINT("Starting LoRa failed!");
    while (1);
  }
  LoRa.setTxPower(20, PA_OUTPUT_PA_BOOST_PIN);
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125000);
  LoRa.setCodingRate4(6);
  LoRa.setSyncWord(0x12);
  // Attach interrupt to callback function
  LoRa.onTxDone(onTxDone);


  /********** Setup Sensor **********/
  // Start up the library
  sensors.begin();

}

void loop() {
  int counter_Read = 0;
  unsigned long start;
  unsigned long stop;

  while (1)
  {
    switch (state)
    {
      case state_Read:
        start = millis(); 
        DEBUG_PRINT("State Read");
        // Read information on sensor
        SensorRead();
        // Transition
        counter_Read++;
        if ( counter_Read >= counterMax) {
          counter_Read = 0;
          state = state_Send;
        }
        else {
          state = state_Idle;
          
        }
        break;

      case state_Send:
        DEBUG_PRINT("State Send");
        // Send Read data with Lora
        LoRaSend();

        //Transistion
        state = state_Compute;
        break;

      case state_Compute:
        DEBUG_PRINT("State Compute");
        break;

      case state_Idle:
        DEBUG_PRINT("State Idle");
        //LowPower.idle();
        //LowPower.sleep(500);
        delay(500);
        // Transition
        state = state_Read;
        stop = millis();
        DEBUG_PRINT("Time used for 1 cycle: ");
        DEBUG_PRINT(stop - start);
        break;

      default:
        break;
    }
  }
}

void LoRaSend() {
  int counter = 0;
  char header_to = 0xff;
  char header_from = 0x01;
  char header_id = 00;
  char header_flags = 00;
  char header[] = {header_to, header_from, header_id, header_flags};

  DEBUG_PRINT("Sending packet non-blocking: ");
  DEBUG_PRINT(counter);

  // send in async / non-blocking mode
  LoRa.beginPacket();
  LoRa.print(header[0]);
  LoRa.print(header[1]);
  LoRa.print(header[2]);
  LoRa.print(header[3]);
  LoRa.print(data_temp);
  LoRa.print(counter);
  LoRa.endPacket(true); // true = async / non-blocking mode
  counter++;

}

void SensorRead() {
  char data_receive[] = {};
  DEBUG_PRINT("Sensor Read");
  DEBUG_PRINT("Before blocking requestForConversion");
  unsigned long start = millis();    

  sensors.requestTemperatures();

  unsigned long stop = millis();
  DEBUG_PRINT("After blocking requestForConversion");
  DEBUG_PRINT("Time used: ");
  DEBUG_PRINT(stop - start);
  
  // get temperature
  DEBUG_PRINT("Temperature: ");
  DEBUG_PRINT(sensors.getTempCByIndex(0));  
  DEBUG_PRINT("\n");
  data_temp = sensors.getTempCByIndex(0);
  // add data temp moyenne pondérée 
}

void onTxDone() {
  state = state_Idle;
}

void onRxDone() {
  state = state_Idle;
}
