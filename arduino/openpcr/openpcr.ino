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
#include <EEPROM.h>

#include "pcr_includes.h"
#include "thermocycler.h"

Thermocycler* gpThermocycler = NULL;

boolean InitialStart() {
  for (int i = 0; i < 50; i++) {
    if (EEPROM.read(i) != 0xFF)
      return false;
  }
  
  return true;
}

void setup() {
  //init factory settings
  if (InitialStart()) {
    EEPROM.write(0, 100); // set contrast to 100
  }
  
  //restart detection
  boolean restarted = !(MCUSR & 1);
  MCUSR &= 0xFE;

  const int pin_lid_thermistor = A1;
  const int pin_plate_thermistor = A4;

  const DisplayParameyers display_parameters(
    16, //lcd_ncols
    1,  //lcd_nrows ;
    6,  //lcd_pin_rs =  Arduino pin that connects to R/S pin of LCD display
    7,  //lcd_pin_e  =  Arduino pin that connects to E   pin of LCD display
    8,  //lcd_pin_d4 =  Arduino pin that connects to D4  pin of LCD display
    A5, //lcd_pin_d5 =  Arduino pin that connects to D5  pin of LCD display
    A2, //lcd_pin_d6 =  Arduino pin that connects to D6  pin of LCD display
    A3, //lcd_pin_d7 =  Arduino pin that connects to D7  pin of LCD display
    5   //lcd_pin_v0 =  Arduino pin that connects to V0  pin of LCD display
  );

  gpThermocycler = new Thermocycler(
    restarted,
    pin_lid_thermistor,
    pin_plate_thermistor,
    display_parameters
  );
}

void loop() {
  gpThermocycler->Loop();
}

