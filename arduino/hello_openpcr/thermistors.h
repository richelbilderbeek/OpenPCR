/*
 *  thermistors.h - OpenPCR control software.
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

#ifndef _LID_THERMISTOR_H_
#define _LID_THERMISTOR_H_

class CLidThermistor {
public:
  CLidThermistor();
  double& GetTemp() { return iTemp; }
  void ReadTemp();
  
private:
  double iTemp;
  const int m_pin; //#1
};

class CPlateThermistor {
public:
  CPlateThermistor();
  double& GetTemp() { return iTemp; }
  void ReadTemp();
private:
  #ifdef REALLY_USE_ADC_AS_ORIGINAL_AUTHORS_DID
  char SPITransfer(volatile char data);
  #endif

private:
  //The temperature in ???
  double iTemp;

  const int m_pin; //#4
};

#endif
