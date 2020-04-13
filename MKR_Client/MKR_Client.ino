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
#include "Arduino.h"
#include <SPI.h>
#include <ArduinoLowPower.h>
#include <LoRa.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <HP20x_dev.h>
#include <KalmanFilter.h>

/*--------------------------------------------*/
/*                 DEFINES                    */
/*--------------------------------------------*/
#define DEBUG
#define LORA_FREQUENCY        915E6
#define LORA_TX_POWER         17
#define LORA_SPREADING_FACTOR 7
#define LORA_SIGNAL_BANDWIDTH 125000
#define LORA_CODING_RATE      6
#define LORA_SYNC_WORD        0x12

#define ONE_WIRE_BUS          2
#define READ_DELAY            2000
#define SEND_DELAY            20000
#define RATE_SEND_READ_DELAY  SEND_DELAY/READ_DELAY

#define HEADER_SIZE           4
#define HEADER_INDEX_TO       0
#define HEADER_INDEX_FROM     1
#define HEADER_INDEX_ID       2
#define HEADER_INDEX_FLAG     3

#define DATA_SIZE             6
#define DATA_INDEX_TEMP_WATER 0
#define DATA_INDEX_TEMP_AIR   1
#define DATA_INDEX_PRES_AIR   2
#define DATA_INDEX_OCCUP      3

#define PAYLOAD_BUFFER_SIZE   128
#define CHAR_SEPARATOR        0x23

#ifdef  DEBUG
#define DEBUG_PRINT(x)  Serial.println (x)
#else
#define DEBUG_PRINT(x)
#endif
/*--------------------------------------------*/
/*                 GLOBAL DATA                */
/*--------------------------------------------*/
enum {state_Read, state_Send, state_Compute, state_Idle}
state = state_Read;
enum {state_R_Temp, state_R_Baro, state_R_Occup}
state_R = state_R_Temp;
const int counterMax = RATE_SEND_READ_DELAY;
int analogOccupPin = A0; // potentiometer wiper (middle terminal) connected to analog pin 3
unsigned char ret = 0;
char tmp[10];

char header_to = 0xff;
char header_from = 0x01;
char header_id = 00;
char header_flags = 00;

struct loRaFrame {
  char header_buffer[HEADER_SIZE];
  char waterTemp[10];
  char airTemp[10];
  char airPressure[10];
  char occup[10];
  //float data_buffer[DATA_SIZE];
  char  separator;
};

//char  payload_buffer[PAYLOAD_BUFFER_SIZE] = {0};

/*--------------------------------------------*/
/*                 INSTANCES                  */
/*--------------------------------------------*/
/* OneWire */
OneWire oneWire(ONE_WIRE_BUS);
/* DallasTemperature */
DallasTemperature sensors(&oneWire);
/* Kalman Filter */
KalmanFilter t_filter;    //temperature filter
KalmanFilter p_filter;    //pressure filter
KalmanFilter a_filter;    //altitude filter
/* loRa Frame */
struct loRaFrame LoraFrame1 = {"0", "0", "0", "0", "0", CHAR_SEPARATOR};

/*--------------------------------------------*/
/*              SETUP FUNCTION                */
/*--------------------------------------------*/
void setup() {
  /********** General Setup **********/
  Serial.begin(9600);
  //while (!Serial);
  LowPower.attachInterruptWakeup(RTC_ALARM_WAKEUP, onWakeUp, CHANGE);


  /********** Setup LoraWan **********/
  DEBUG_PRINT("LoRa Sender non-blocking Callback");
  if (!LoRa.begin(LORA_FREQUENCY)) {
    DEBUG_PRINT("Starting LoRa failed!");
    while (1);
  }
  LoRa.setTxPower         (LORA_TX_POWER, PA_OUTPUT_PA_BOOST_PIN);
  LoRa.setSpreadingFactor (LORA_SPREADING_FACTOR);
  LoRa.setSignalBandwidth (LORA_SIGNAL_BANDWIDTH);
  LoRa.setCodingRate4     (LORA_CODING_RATE);
  LoRa.setSyncWord        (LORA_SYNC_WORD);
  // Attach interrupt to callback function
  LoRa.onTxDone(onTxDone);
  LoraFrame1.header_buffer[HEADER_INDEX_TO]   = header_to;
  LoraFrame1.header_buffer[HEADER_INDEX_FROM] = header_from;
  LoraFrame1.header_buffer[HEADER_INDEX_ID]   = header_id;
  LoraFrame1.header_buffer[HEADER_INDEX_FLAG] = header_flags;


  /********** Setup Sensor **********/
  // Start up the library
  sensors.begin();
  HP20x.begin();
  delay(100);
  /* Determine HP20x_dev is available or not */
  ret = HP20x.isAvailable();
  if (OK_HP20X_DEV == ret)
  {
    Serial.println("HP20x_dev is available.\n");
  }
  else
  {
    Serial.println("HP20x_dev isn't available.\n");
  }

}

/*--------------------------------------------*/
/*              MAIN FUNCTION                 */
/*--------------------------------------------*/
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
            if (counter_Read >= counterMax) {
              counter_Read = 0;
              state = state_Send;
            }
            else {
              counter_Read++;
              state = state_Idle;
            }
            break;

          default:
            break;
        }
        break;

      case state_Send:
        DEBUG_PRINT("State Send");
        // Send Read data with Lora
        LoRaSend();
        state = state_Compute;
        break;

      case state_Compute:
        DEBUG_PRINT("State Compute");
        break;

      case state_Idle:
        DEBUG_PRINT("State Idle");
        stop = millis();
        DEBUG_PRINT("------------------\n");
        DEBUG_PRINT("Time used for 1 cycle: ");
        DEBUG_PRINT(stop - start);
        //LowPower.sleep(READ_DELAY);
        delay((stop - start)- READ_DELAY);
        state = state_Read;
        break;

      default:
        break;
    }
  }
}

/*--------------------------------------------*/
/*              LoRaSend FUNCTION             */
/*--------------------------------------------*/
void LoRaSend() {
  int counter = 0;
  DEBUG_PRINT("Sending packet non-blocking: ");
  DEBUG_PRINT(counter);
  // reset buffer

  /*send in async / non-blocking mode*/
  LoRa.beginPacket();
  DEBUG_PRINT("------------------\n");
  DEBUG_PRINT("trame:");
  
  /* header */
  LoRa.print(LoraFrame1.header_buffer);
  DEBUG_PRINT(LoraFrame1.header_buffer);
  
  /* payload*/
  LoRa.print(LoraFrame1.waterTemp);
  DEBUG_PRINT(LoraFrame1.waterTemp);
  LoRa.print(LoraFrame1.separator);
  DEBUG_PRINT(LoraFrame1.separator);

  LoRa.print(LoraFrame1.airTemp);
  DEBUG_PRINT(LoraFrame1.airTemp);
  LoRa.print(LoraFrame1.separator);
  DEBUG_PRINT(LoraFrame1.separator);

  LoRa.print(LoraFrame1.airPressure);
  DEBUG_PRINT(LoraFrame1.airPressure);
  LoRa.print(LoraFrame1.separator);
  DEBUG_PRINT(LoraFrame1.separator);

  LoRa.print(LoraFrame1.occup);
  DEBUG_PRINT(LoraFrame1.occup);
  LoRa.print(LoraFrame1.separator);
  DEBUG_PRINT(LoraFrame1.separator);

  LoRa.print(counter);
  LoRa.endPacket(true); // true = async / non-blocking mode

  //memset(LoraFrame1.data_buffer, 0, DATA_SIZE);
  counter++;
}

/*--------------------------------------------*/
/*        TempSensorRead FUNCTION             */
/*--------------------------------------------*/
void TempSensorRead() {
  // get temperature
  sensors.requestTemperatures();
  dtostrf(sensors.getTempCByIndex(0), 6, 2, LoraFrame1.waterTemp);

  DEBUG_PRINT("------------------\n");
  DEBUG_PRINT("WaterTemperature:");
  DEBUG_PRINT(sensors.getTempCByIndex(0));
  DEBUG_PRINT("C.\n");
  // add data temp moyenne pondérée
}

/*--------------------------------------------*/
/*        BaroSensorRead FUNCTION             */
/*--------------------------------------------*/
void BaroSensorRead() {
  char display[40];
  if (OK_HP20X_DEV == ret)
  {
    long Temper = HP20x.ReadTemperature();
    float t = Temper / 100.0;
    dtostrf(t_filter.Filter(t), 6, 2, LoraFrame1.airTemp);

    DEBUG_PRINT("------------------\n");
    DEBUG_PRINT("AirTemperature:");
    DEBUG_PRINT(t_filter.Filter(t));
    DEBUG_PRINT("C.\n");


    long Pressure = HP20x.ReadPressure();
    t = Pressure / 100.0;
    dtostrf(p_filter.Filter(t), 6, 2, LoraFrame1.airPressure);

    DEBUG_PRINT("------------------\n");
    DEBUG_PRINT("AirPressure:");
    DEBUG_PRINT(p_filter.Filter(t));
    DEBUG_PRINT("hPa\n");


    long Altitude = HP20x.ReadAltitude();
    t = Altitude / 100.0;
    DEBUG_PRINT("------------------\n");
    DEBUG_PRINT("ReadAltitude:");
    DEBUG_PRINT(a_filter.Filter(t));
    DEBUG_PRINT("m\n");
    DEBUG_PRINT("------------------\n");
  }
  else
  {
    DEBUG_PRINT("HP20x not available:");
  }
}

/*--------------------------------------------*/
/*        AnalogSensorRead FUNCTION           */
/*--------------------------------------------*/
void AnalogSensorRead() {
  float analogOccupValue = 0;  // variable to store the value read
  analogOccupValue = (float)analogRead(analogOccupPin);
  Serial.println(analogOccupValue);
  dtostrf(analogOccupValue, 6, 2, LoraFrame1.occup);
}

/*--------------------------------------------*/
/*                onTxDone ISR                */
/*--------------------------------------------*/
void onTxDone() {
  state = state_Idle;
}

/*--------------------------------------------*/
/*                onRxDone ISR                */
/*--------------------------------------------*/
void onRxDone() {
  state = state_Idle;
}

/*--------------------------------------------*/
/*                onWakeUp ISR                */
/*--------------------------------------------*/
void onWakeUp() {
  state = state_Read;
}

/*--------------------------------------------*/
/*       float to char FUNCTION               */
/*--------------------------------------------*/
char *dtostrf (float val, signed char width, unsigned char prec, char *sout) {
  char fmt[20];
  sprintf(fmt, "%%%d.%df", width, prec);
  sprintf(sout, fmt, val);
  return sout;
}
