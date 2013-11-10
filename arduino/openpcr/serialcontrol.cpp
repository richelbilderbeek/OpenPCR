/*
 *  serialcontrol.cpp - OpenPCR control software.
 *  Copyright (C) 2010-2012 Josh Perfetto and Xia Hong. All Rights Reserved.
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
 
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
#include "pcr_includes.h"
#include "serialcontrol.h"

#include "thermocycler.h"
#include "program.h"
#include "display.h"
#include "thermistors.h"

#pragma GCC diagnostic pop

//const int BAUD_RATE = 4800;

SerialControl::SerialControl(Display* pDisplay)
  :
    packetState(STATE_START),
    lastPacketSeq(0xff),
    m_command_id(0),
    m_packet_len(0),
    m_packet_real_len(0),
    bEscapeCodeFound(false),
    iReceivedStatusRequest(false),
    m_display(pDisplay)
{  
}

SerialControl::~SerialControl() {
}

void SerialControl::Process() {
  while (ReadPacket()) {}
}

/////////////////////////////////////////////////////////////////
// Private
boolean SerialControl::ReadPacket()
{
  return true;
}

void SerialControl::ProcessPacket(byte* data, int datasize)
{
  PCPPacket* packet = (PCPPacket*)data;
  uint8_t packetType = packet->eType & 0xf0;
  uint8_t packetSeq = packet->eType & 0x0f;
  //uint8_t result = false;
  char* pCommandBuf;
  
  switch(packetType){
  case SEND_CMD:
    data[datasize] = '\0';
    SCommand command;
    pCommandBuf = (char*)(data + sizeof(PCPPacket));
    
    //store start commands for restart
    //ProgramStore::StoreProgram(pCommandBuf);
    
    CommandParser::ParseCommand(command, pCommandBuf);
    GetThermocycler().ProcessCommand(command);
    m_command_id = command.commandId;
    break;
    
  case STATUS_REQ:
    iReceivedStatusRequest = true;
    SendStatus();
    break;
  default:
    break;
 }

  lastPacketSeq = packetSeq;
}

#define STATUS_FILE_LEN 100

void SerialControl::SendStatus() {
  Thermocycler::ProgramState state = GetThermocycler().GetProgramState();
  const char* const szStatus = GetProgramStateString_P(state);
  const char* const szThermState = GetThermalStateString_P(GetThermocycler().GetThermalState());
      
  char statusBuf[STATUS_FILE_LEN];
  char* statusPtr = statusBuf;
  Thermocycler& tc = GetThermocycler();
    
  statusPtr = AddParam(statusPtr, 'd', (unsigned long)m_command_id, true);
  statusPtr = AddParam_P(statusPtr, 's', szStatus);
  statusPtr = AddParam(statusPtr, 'l', (int)tc.GetLidTemp());
  statusPtr = AddParam(statusPtr, 'b', tc.GetPlateTemp(), 1, false);
  statusPtr = AddParam_P(statusPtr, 't', szThermState);
  statusPtr = AddParam(statusPtr, 'o', GetThermocycler().GetDisplay()->GetContrast());

  if (state == Thermocycler::ERunning || state == Thermocycler::EComplete)
  {
    statusPtr = AddParam(statusPtr, 'e', tc.GetElapsedTimeS());
    statusPtr = AddParam(statusPtr, 'r', tc.GetTimeRemainingS());
    statusPtr = AddParam(statusPtr, 'u', tc.GetNumCycles());
    statusPtr = AddParam(statusPtr, 'c', tc.GetCurrentCycleNum());
    //statusPtr = AddParam(statusPtr, 'n', tc.GetProgName());
    if (tc.GetCurrentStep() != NULL)
    {
      statusPtr = AddParam(statusPtr, 'p', tc.GetCurrentStep()->GetName());
    }
  }
  else if (state == static_cast<int>(Thermocycler::EIdle)) //RJCB: ???
  {
    statusPtr = AddParam(statusPtr, 'v', OPENPCR_FIRMWARE_VERSION_STRING);
  }
  statusPtr++; //to include null terminator

  //send packet
}

char* SerialControl::AddParam(char* pBuffer, char key, int val, boolean init) {
  if (!init)
    *pBuffer++ = '&';
  *pBuffer++ = key;
  *pBuffer++ = '=';
  itoa(val, pBuffer, 10);
  while (*pBuffer != '\0')
    pBuffer++;
    
  return pBuffer;
}

char* SerialControl::AddParam(char* pBuffer, char key, unsigned long val, boolean init) {
  if (!init)
    *pBuffer++ = '&';
  *pBuffer++ = key;
  *pBuffer++ = '=';
  ultoa(val, pBuffer, 10);
  while (*pBuffer != '\0')
    pBuffer++;
    
  return pBuffer;
}

char* SerialControl::AddParam(char* pBuffer, char key, float val, int decimalDigits, boolean pad, boolean init) {
  if (!init)
    *pBuffer++ = '&';
  *pBuffer++ = key;
  *pBuffer++ = '=';
  sprintFloat(pBuffer, val, decimalDigits, pad);
  while (*pBuffer != '\0')
    pBuffer++;
    
  return pBuffer;
}

char* SerialControl::AddParam(char* pBuffer, char key, const char* szVal, boolean init) {
  if (!init)
    *pBuffer++ = '&';
  *pBuffer++ = key;
  *pBuffer++ = '=';
  strcpy(pBuffer, szVal);
  while (*pBuffer != '\0')
    pBuffer++;
    
  return pBuffer;
}

char* SerialControl::AddParam_P(char* pBuffer, char key, const char* szVal, boolean init) {
  if (!init)
    *pBuffer++ = '&';
  *pBuffer++ = key;
  *pBuffer++ = '=';
  strcpy_P(pBuffer, szVal);
  while (*pBuffer != '\0')
    pBuffer++;
    
  return pBuffer;
}

const char STOPPED_STR[] PROGMEM = "stopped";
const char LIDWAIT_STR[] PROGMEM = "lidwait";
const char RUNNING_STR[] PROGMEM = "running";
const char COMPLETE_STR[] PROGMEM = "complete";
const char STARTUP_STR[] PROGMEM = "startup";
const char ERROR_STR[] PROGMEM = "error";
const char* SerialControl::GetProgramStateString_P(Thermocycler::ProgramState state) {
  switch (state) {
  case Thermocycler::EStopped:
    return STOPPED_STR;
  case Thermocycler::ELidWait:
    return LIDWAIT_STR;
  case Thermocycler::ERunning:
    return RUNNING_STR;
  case Thermocycler::EComplete:
    return COMPLETE_STR;
  case Thermocycler::EStartup:
    return STARTUP_STR;
  case Thermocycler::EError:
  default:
    return ERROR_STR;
  }
}

const char HEATING_STR[] PROGMEM = "heating";
const char COOLING_STR[] PROGMEM = "cooling";
const char HOLDING_STR[] PROGMEM = "holding";
const char IDLE_STR[] PROGMEM = "idle";
const char* SerialControl::GetThermalStateString_P(Thermocycler::ThermalState state) {
  switch (state) {
  case Thermocycler::EHeating:
    return HEATING_STR;
  case Thermocycler::ECooling:
    return COOLING_STR;
  case Thermocycler::EHolding:
    return HOLDING_STR;
  case Thermocycler::EIdle:
    return IDLE_STR;
  default:
    return ERROR_STR;
  }
}

