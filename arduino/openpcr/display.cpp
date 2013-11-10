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

#include "arduinoassert.h"
#include "arduinotrace.h"
#include "display.h"
#include "pcr_includes.h"
#include "program.h"
#include "thermistors.h"
#include "thermocycler.h"

const int RESET_INTERVAL = 30000; //ms

Display::Display(
    const DisplayParameters& parameters
  )
  :
    m_contrast(100), //OpenPCR set the initial contrast to this value
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
  Trace("Display::Display");
  m_lcd.clear();
  m_lcd.begin(
    m_parameters.m_lcd_ncols,
    m_parameters.m_lcd_nrows
  );

  analogWrite(
    m_parameters.m_pin_v0,
    m_contrast
  );
}

void Display::Clear()
{
  m_prev_state = Thermocycler::EClear;
}

void Display::SetContrast(const int contrast)
{
  m_contrast = contrast;
  analogWrite(
    m_parameters.m_pin_v0,
    m_contrast
  );
  //m_lcd.begin(ms_lcd_ncols,ms_lcd_nrows);
}

void Display::Update()
{
  //Never reaches this
  //static int cnt = 0;
  //++cnt;
  //iLcd.setCursor(0,0);
  //iLcd.print("Update 1");
  
  Thermocycler::ProgramState state = GetThermocycler().GetProgramState();
  if (m_prev_state != state)
  {
    m_lcd.clear();
    m_prev_state = state;
  }
  Assert(state == m_prev_state);

  // check for reset
  if (millis() - m_prev_reset > RESET_INTERVAL)
  {
    m_lcd.begin(
      m_parameters.m_lcd_ncols,
      m_parameters.m_lcd_nrows
    );
    m_prev_reset = millis();
  }
  
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
  Assert(sz > 16);
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
  for (int i=16; i < sz-2; ++i)
  {
    text[i] = ' ';
  }
  text[sz-1] = '\0';
  m_lcd.print(text);

  delete[] text;

}
