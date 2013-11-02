/*
 *  display.cpp - OpenPCR control software.
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

#include "pcr_includes.h"
#include "display.h"
#include "assert.h"
#include "thermocycler.h"
#include "thermistors.h"
#include "program.h"

#define RESET_INTERVAL 30000 //ms

//progmem strings
const char HEATING_STR[] PROGMEM = "Heating";
const char COOLING_STR[] PROGMEM = "Cooling";
const char LIDWAIT_STR[] PROGMEM = "Heating Lid";
const char STOPPED_STR[] PROGMEM = "Ready";
const char RUN_COMPLETE_STR[] PROGMEM = "*** Run Complete ***";
const char OPENPCR_STR[] PROGMEM = "OpenPCR";
const char POWERED_OFF_STR[] PROGMEM = "Powered Off";
const char ETA_OVER_1000H_STR[] PROGMEM = "ETA: >1000h";

const char LID_FORM_STR[] PROGMEM = "Lid: %3d C";
const char CYCLE_FORM_STR[] PROGMEM = "%d of %d";
const char ETA_HOURMIN_FORM_STR[] PROGMEM = "ETA: %d:%02d";
const char ETA_SEC_FORM_STR[] PROGMEM = "ETA: %2ds";
const char BLOCK_TEMP_FORM_STR[] PROGMEM = "%s C";
const char STATE_FORM_STR[] PROGMEM = "%-13s";
const char VERSION_FORM_STR[] PROGMEM = "Firmware v%s";

//OpenPCR defaults. Note: pin 16 and 17 don't exist!
//iLcd(6, 7, 8, A5, 16, 17),
const int Display::ms_pin_rs =  6 ; //Arduino pin that connects to R/S pin of LCD display
const int Display::ms_pin_e  =  7; //Arduino pin that connects to E   pin of LCD display
const int Display::ms_pin_d4 =  8; //Arduino pin that connects to D4  pin of LCD display
const int Display::ms_pin_d5 =  A5; //Arduino pin that connects to D5  pin of LCD display
const int Display::ms_pin_d6 =  A2; //Arduino pin that connects to D6  pin of LCD display
const int Display::ms_pin_d7 =  A3; //Arduino pin that connects to D7  pin of LCD display
const int Display::ms_pin_v0 =  5; //Arduino pin that connects to V0  pin of LCD display

Display::Display(
    const DisplayParameters& parameters
  )
  :
    m_contrast(ProgramStore::RetrieveContrast()),
    m_lcd(
      parameters.m_pin_rs,
      parameters.m_pin_e,
      parameters.m_pin_d4,
      parameters.m_pin_d5,
      parameters.m_pin_d6,
      parameters.m_pin_d7
    ),
    m_parameters(parameters),
    m_prev_reset(millis()),
    m_prev_state(Thermocycler::EStartup)
{
  m_lcd.clear();
  m_lcd.begin(ms_lcd_ncols, ms_lcd_nrows);
  analogWrite(ms_pin_v0, m_contrast);
}

void Display::Clear()
{
  m_prev_state = Thermocycler::EClear;
}

void Display::SetContrast(const int contrast)
{
  m_contrast = contrast;
  analogWrite(ms_pin_v0, m_contrast);
  //m_lcd.begin(ms_lcd_ncols,ms_lcd_nrows);
}
  
void Display::SetDebugMsg(char* szDebugMsg)
{
  #ifdef DEBUG_DISPLAY
  strcpy(iszDebugMsg, szDebugMsg);
  #endif
  m_lcd.clear();
  Update();
}

void Display::Update()
{
  //Never reaches this
  //static int cnt = 0;
  //++cnt;
  //iLcd.setCursor(0,0);
  //iLcd.print("Update 1");

  char buf[16];
  
  Thermocycler::ProgramState state = GetThermocycler().GetProgramState();
  if (m_prev_state != state)
  {
    m_lcd.clear();
    m_prev_state = state;
  }
  assert(state == m_prev_state);

  //iLcd.setCursor(0,1);
  //iLcd.print("Update 2");

  // check for reset
  if (millis() - m_prev_reset > RESET_INTERVAL)
  {
    m_lcd.begin(ms_lcd_ncols,ms_lcd_nrows);
    m_prev_reset = millis();
  }
  
  switch (state)
  {
  case Thermocycler::ERunning:
  case Thermocycler::EComplete:
  case Thermocycler::ELidWait:
  case Thermocycler::EStopped:
  {
    m_lcd.setCursor(0, 1 % ms_lcd_nrows);
    #ifdef DEBUG_DISPLAY
    iLcd.print(iszDebugMsg);
    #else
    m_lcd.print(GetThermocycler().GetProgName());
    #endif
           
    DisplayLidTemp();
    DisplayBlockTemp();
    DisplayState();

    if (state == Thermocycler::ERunning && !GetThermocycler().GetCurrentStep()->IsFinal())
    {
      DisplayCycle();
      DisplayEta();
    }
    else if (state == Thermocycler::EComplete)
    {
      m_lcd.setCursor(0, 3 % ms_lcd_nrows);
      m_lcd.print(rps(RUN_COMPLETE_STR));
    }
  }
  break;
  case Thermocycler::EStartup:
    m_lcd.setCursor(6, 1);
    m_lcd.print(rps(OPENPCR_STR));

    m_lcd.setCursor(2, 2 % ms_lcd_nrows);
    sprintf_P(buf, VERSION_FORM_STR, OPENPCR_FIRMWARE_VERSION_STRING);
    m_lcd.print(buf);
  break;
  }
}

void Display::DisplayEta()
{
  char timeString[16];
  unsigned long timeRemaining = GetThermocycler().GetTimeRemainingS();
  int hours = timeRemaining / 3600;
  int mins = (timeRemaining % 3600) / 60;
  int secs = timeRemaining % 60;
  
  if (hours >= 1000)
    strcpy_P(timeString, ETA_OVER_1000H_STR);
  else if (mins >= 1 || hours >= 1)
    sprintf_P(timeString, ETA_HOURMIN_FORM_STR, hours, mins);
  else
    sprintf_P(timeString, ETA_SEC_FORM_STR, secs);
  
  m_lcd.setCursor(20 - strlen(timeString), 3 % ms_lcd_nrows);
  m_lcd.print(timeString);
}

void Display::DisplayLidTemp()
{
  char buf[16];
  sprintf_P(buf, LID_FORM_STR, (int)(GetThermocycler().GetLidTemp() + 0.5));
  //iLcd.setCursor(10, 2 % ms_lcd_nrows);
  m_lcd.setCursor(10, 2 % ms_lcd_nrows);
  m_lcd.print(buf);
}

void Display::DisplayBlockTemp() {
  char buf[16];
  char floatStr[16];
  
  sprintFloat(floatStr, GetThermocycler().GetPlateTemp(), 1, true);
  sprintf_P(buf, BLOCK_TEMP_FORM_STR, floatStr);
  //iLcd.setCursor(13, 0 % ms_lcd_nrows);
  m_lcd.setCursor(13, 0 % ms_lcd_nrows);
  m_lcd.print(buf);
}

void Display::DisplayCycle()
{
  char buf[16];
  
  m_lcd.setCursor(0, 3 % ms_lcd_nrows);
  sprintf_P(buf, CYCLE_FORM_STR, GetThermocycler().GetCurrentCycleNum(), GetThermocycler().GetNumCycles());
  m_lcd.print(buf);
}

void Display::DisplayState()
{
  char buf[32];
  char* stateStr;
  
  switch (GetThermocycler().GetProgramState()) {
  case Thermocycler::ELidWait:
    stateStr = rps(LIDWAIT_STR);
    break;
    
  case Thermocycler::ERunning:
  case Thermocycler::EComplete:
    switch (GetThermocycler().GetThermalState()) {
    case Thermocycler::EHeating:
      stateStr = rps(HEATING_STR);
      break;
    case Thermocycler::ECooling:
      stateStr = rps(COOLING_STR);
      break;
    case Thermocycler::EHolding:
      stateStr = GetThermocycler().GetCurrentStep()->GetName();
      break;
    case Thermocycler::EIdle:
      stateStr = rps(STOPPED_STR);
      break;
    }
    break;
    
  case Thermocycler::EStopped:
    stateStr = rps(STOPPED_STR);
    break;
  }
  
  m_lcd.setCursor(0, 0 % ms_lcd_nrows);
  sprintf_P(buf, STATE_FORM_STR, stateStr);
  m_lcd.print(buf);
}

void Display::ShowAll(
  const int current_temperature,
  const bool is_heating,
  const int current_step,
  const int number_of_steps,
  const int minutes_left
)
{
  const int sz = m_parameters.m_lcd_ncols + 1; //+1 for null terminator
  assert(sz > 16);
  char * const text = new char[sz];
  text[ 0] = '0' + (current_temperature / 10);
  text[ 1] = '0' + (current_temperature % 10);
  text[ 2] = 'o';
  text[ 3] = 'C';
  text[ 4] = is_heating ? '^' : 'v';
  text[ 5] = ' ';
  text[ 6] = '0' + (current_step / 10);
  text[ 7] = '0' + (current_step % 10);
  text[ 8] = '/';
  text[ 9] = '0' + (number_of_steps / 10);
  text[10] = '0' + (number_of_steps % 10);
  text[11] = ' ';
  text[12] = '0' + (minutes_left / 60);
  text[13] = ':';
  text[14] = '0' + ((minutes_left % 60) / 10);
  text[15] = '0' + ((minutes_left % 60) % 10);
  //Add spaces
  for (int i=16; i < z-2; ++i)
  {
    text[i] = ' ';
  }
  text[sz-1] = '\0';
  m_lcd.print(text);

  delete[] text;

}
