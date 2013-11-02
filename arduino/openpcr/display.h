/*
 *  display.h - OpenPCR control software.
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

#ifndef _DISPLAY_H_
#define _DISPLAY_H_

#include <LiquidCrystal.h>
#include "thermocycler.h"

class Cycle;

///The one-line sixteen character display
struct Display
{
  Display();
  
  //accessotrs
  uint8_t GetContrast() { return iContrast; }
  
  void SetContrast(uint8_t contrast);
  void Clear();
  void SetDebugMsg(char* szDebugMsg);
  void Update();
  
private:
  void DisplayEta();
  void DisplayLidTemp();
  void DisplayBlockTemp();
  void DisplayCycle();
  void DisplayState();
  
private:
  LiquidCrystal iLcd;
#ifdef DEBUG_DISPLAY
  char iszDebugMsg[21];
#endif
  Thermocycler::ProgramState iLastState;
  unsigned long iLastReset;
  uint8_t iContrast;

  static const int ms_lcd_ncols; //Number of columns of LCD display
  static const int ms_lcd_nrows; //Number of rows of LCD display

  static const int ms_pin_rs; //R/S pin, must be connected to pin  4 of LCD
  static const int ms_pin_e ; //E   pin, must be connected to pin  6 of LCD
  static const int ms_pin_d4; //D4  pin, must be connected to pin 11 of LCD
  static const int ms_pin_d5; //D5  pin, must be connected to pin 12 of LCD
  static const int ms_pin_d6; //D6  pin, must be connected to pin 13 of LCD
  static const int ms_pin_d7; //D7  pin, must be connected to pin 14 of LCD
  static const int ms_pin_v0; //V0  pin, must be connected to pin 3 of LCD
};

#endif

