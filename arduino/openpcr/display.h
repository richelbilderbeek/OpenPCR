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
#include "displayparameters.h"

class Cycle;

///The one-line sixteen character display
///Index       |0|1|2     |3|4               |5|6|7|8|9|0|1|2|3|4|5|
///Display     |9|9|degree|C|up or down arrow| |9|8|/|9|9| |9|:|5|9|
///Description |Current     |Heating/cooling | Steps left  | Time  |
///            |temperature |                |             | left  |
struct Display
{
  Display(
    const DisplayParameters& parameters
    );
  
  void Clear();
  int GetContrast() const { return m_contrast; }
  void SetContrast(const int contrast);
  void Update();
  
private:
  void ShowAll(
    const int current_temperature,
    const bool is_heating,
    const int current_step,
    const int number_of_steps,
    const int minutes_left
  );
  
private:
  int m_contrast;
  LiquidCrystal m_lcd;
  const DisplayParameters m_parameters;
  int m_prev_reset;
  Thermocycler::ProgramState m_prev_state;
};

#endif

