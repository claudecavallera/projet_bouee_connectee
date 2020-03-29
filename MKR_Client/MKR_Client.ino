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
#include <HP20x_dev.h>
#include <KalmanFilter.h>

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
enum {state_R_Temp, state_R_Baro, state_R_Occup} state_R = state_R_Temp;
const int counterMax = 10;
float data_temp = 0;
int analogOccupPin = A0; // potentiometer wiper (middle terminal) connected to analog pin 3               
unsigned char ret = 0;
enum {data_Temp_Water, data_Temp_Air, data_Pres_Air, data_Occup};
float data_buffer[4] = {0};
/* Instance */
KalmanFilter t_filter;    //temperature filter
KalmanFilter p_filter;    //pressure filter
KalmanFilter a_filter;    //altitude filter
// Connect one wire to bus 2
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

void setup() {
  /********** General Setup **********/
  Serial.begin(9600);
  //while (!Serial);


  /********** Setup LoraWan **********/
  DEBUG_PRINT("LoRa Sender non-blocking Callback");
  if (!LoRa.begin(LORA_FREQUENCY)) {
    DEBUG_PRINT("Starting LoRa failed!");
    while (1);
  }
  LoRa.setTxPower(17, PA_OUTPUT_PA_BOOST_PIN);
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125000);
  LoRa.setCodingRate4(6);
  LoRa.setSyncWord(0x12);
  // Attach interrupt to callback function
  LoRa.onTxDone(onTxDone);


  /********** Setup Sensor **********/
  // Start up the library
  sensors.begin();
  HP20x.begin();
  delay(100);
  /* Determine HP20x_dev is available or not */
  ret = HP20x.isAvailable();
  if(OK_HP20X_DEV == ret)
  {
    Serial.println("HP20x_dev is available.\n");    
  }
  else
  {
    Serial.println("HP20x_dev isn't available.\n");
  }

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
        switch (state_R)
        {
          case state_R_Temp:
            DEBUG_PRINT("State_R_Temp");
            TempSensorRead();
            state_R = state_R_Baro;
            break;
            
          case state_R_Baro:
            DEBUG_PRINT("State_R_Baro");
            BaroSensorRead();
            state_R = state_R_Occup;
            break;
            
          case state_R_Occup:
            DEBUG_PRINT("State_R_Occup");
            AnalogSensorRead();
            state_R = state_R_Temp;
            break;
            
          default:
            break;
        }
        // Transition a replacer
        counter_Read++;
        if (state_R == state_R_Temp)
        {
          if (counter_Read >= counterMax)
          {
            counter_Read = 0;
            state = state_Send;
          }
          else
          {
            state = state_Idle;
          }
        }
        else
        {
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
  LoRa.print(data_buffer[data_Temp_Water]);
  LoRa.print(data_buffer[data_Temp_Air]);
  LoRa.print(data_buffer[data_Pres_Air]);
  LoRa.print(data_buffer[data_Occup]);
  LoRa.print(counter);
  LoRa.endPacket(true); // true = async / non-blocking mode
  counter++;

}

void TempSensorRead() {
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
  data_buffer[data_Temp_Water] = sensors.getTempCByIndex(0);
  // add data temp moyenne pondérée
}

void BaroSensorRead() {
    char display[40];
    if(OK_HP20X_DEV == ret)
    { 
    Serial.println("------------------\n");
    long Temper = HP20x.ReadTemperature();
    float t = Temper/100.0;    
    Serial.println("C.\n");
    Serial.println("Filter:");
    Serial.print(t_filter.Filter(t));
    Serial.println("C.\n");
    data_buffer[data_Temp_Air] = t_filter.Filter(t);
 
    long Pressure = HP20x.ReadPressure();
    t = Pressure/100.0;
    Serial.println("hPa.\n");
    Serial.println("Filter:");
    Serial.print(p_filter.Filter(t));
    Serial.println("hPa\n");
    data_buffer[data_Pres_Air] = p_filter.Filter(t);
    
    long Altitude = HP20x.ReadAltitude();
    t = Altitude/100.0;
    Serial.print(t);
    Serial.println("m.\n");
    Serial.println("Filter:");
    Serial.print(a_filter.Filter(t));
    Serial.println("m.\n");
    Serial.println("------------------\n");
    }
}
void AnalogSensorRead() {
  float analogOccupValue = 0;  // variable to store the value read
  analogOccupValue = (float)analogRead(analogOccupPin);
  Serial.println(analogOccupValue);
  data_buffer[data_Occup] = analogOccupValue;
}

void onTxDone() {
  state = state_Idle;
}

void onRxDone() {
  state = state_Idle;
}
