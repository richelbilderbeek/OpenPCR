/*
 *  openpcr.pde - OpenPCR control software.
 *  Copyright (C) 2010-2012 Josh Perfetto. All Rights Reserved.
 *
 *  OpenPCR control software is free software: you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as published
 *  by the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  OpenPCR control software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with
 *  the OpenPCR control software.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <LiquidCrystal.h>
//#include <EEPROM.h>

#include "arduinoassert.h"
#include "arduinotrace.h"
//#include "signal.h"
#include "displayparameters.h"
#include "pcr_includes.h"
#include "thermocycler.h"

Thermocycler* gpThermocycler = NULL;

bool InitialStart()
{
  for (int i = 0; i < 50; i++)
  {
    //if (EEPROM.read(i) != 0xFF) return false;
  }
  
  return true;
}

void setup()
{
  Serial.begin(9600);
  Trace("START");

  //restart detection
  bool restarted = !(MCUSR & 1);
  MCUSR &= 0xFE;


  const DisplayParameters display_parameters(
    16, //lcd_ncols
    2,  //lcd_nrows ;
    2,  //lcd_pin_rs =  Arduino pin that connects to R/S pin of LCD display
    3,  //lcd_pin_e  =  Arduino pin that connects to E   pin of LCD display
    4,  //lcd_pin_d4 =  Arduino pin that connects to D4  pin of LCD display
    6, //lcd_pin_d5 =  Arduino pin that connects to D5  pin of LCD display
    7, //lcd_pin_d6 =  Arduino pin that connects to D6  pin of LCD display
    7, //lcd_pin_d7 =  Arduino pin that connects to D7  pin of LCD display
    8   //lcd_pin_v0 =  Arduino pin that connects to V0  pin of LCD display
  );

  const int pin_block_thermistor = 1;
  const int pin_heater_lid = 2;
  const int pin_lid_thermistor = 3;
  const int pin_peltier_a = 4;
  const int pin_peltier_b = 5;
  const int pin_plate_thermistor = 6;

  Trace("Starting thermocycler");

  gpThermocycler = new Thermocycler(
    restarted,
    pin_block_thermistor,
    pin_heater_lid,
    pin_lid_thermistor,
    pin_peltier_a,
    pin_peltier_b,
    pin_plate_thermistor,
    display_parameters
  );
}

void loop()
{
  Trace("Looping\n");
  gpThermocycler->Loop();
}

