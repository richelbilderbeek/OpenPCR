/*
 *  thermocycler.cpp - OpenPCR control software.
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
#include "thermocycler.h"

#include "display.h"
#include "displayparameters.h"
#include "program.h"
#include "serialcontrol.h"
#include "../Wire/Wire.h"

#include <avr/io.h>
#include <avr/pgmspace.h>

//constants
  
// I2C address for MCP3422 - base address for MCP3424
#define MCP3422_ADDRESS 0X68
#define MCP342X_RES_FIELD  0X0C // resolution/rate field
#define MCP342X_18_BIT     0X0C // 18-bit 3.75 SPS
#define MCP342X_BUSY       0X80 // read: output not ready

#define CYCLE_START_TOLERANCE 0.2
#define LID_START_TOLERANCE 1.0

#define PLATE_PID_INC_NORM_P 1000
#define PLATE_PID_INC_NORM_I 250
#define PLATE_PID_INC_NORM_D 250

#define PLATE_PID_INC_LOW_THRESHOLD 40
#define PLATE_PID_INC_LOW_P 600
#define PLATE_PID_INC_LOW_I 200
#define PLATE_PID_INC_LOW_D 400

#define PLATE_PID_DEC_HIGH_THRESHOLD 70
#define PLATE_PID_DEC_HIGH_P 800
#define PLATE_PID_DEC_HIGH_I 700
#define PLATE_PID_DEC_HIGH_D 300

#define PLATE_PID_DEC_NORM_P 500
#define PLATE_PID_DEC_NORM_I 400
#define PLATE_PID_DEC_NORM_D 200

#define PLATE_PID_DEC_LOW_THRESHOLD 35
#define PLATE_PID_DEC_LOW_P 2000
#define PLATE_PID_DEC_LOW_I 100
#define PLATE_PID_DEC_LOW_D 200

#define PLATE_BANGBANG_THRESHOLD 2.0

#define MIN_PELTIER_PWM -1023
#define MAX_PELTIER_PWM 1023

#define MAX_LID_PWM 255
#define MIN_LID_PWM 0

#define STARTUP_DELAY 4000

const int Thermocycler::ms_pin_peltier_a = 2;
const int Thermocycler::ms_pin_heater_lid = 3;
const int Thermocycler::ms_pin_peltier_b = 4;
const int Thermocycler::ms_pin_block_thermistor = A4;
//pid parameters
const SPIDTuning LID_PID_GAIN_SCHEDULE[] = {
  //maxTemp, kP, kI, kD
  { 70, 40, 0.15, 60 },
  { 200, 80, 1.1, 10 }
};

Thermocycler::Thermocycler(
  boolean restarted,
  const int pin_lid_thermistor,
  const int pin_plate_thermistor,
  const DisplayParameters& display_parameters
  )
  :
    iLidThermistor(pin_lid_thermistor),
    iPlateThermistor(pin_plate_thermistor),
    iRestarted(restarted),
    ipDisplay(new Display(display_parameters)),
    ipProgram(NULL),
    ipDisplayCycle(NULL),
    ipSerialControl(NULL),
    iProgramState(EStartup),
    ipPreviousStep(NULL),
    ipCurrentStep(NULL),
    iThermalDirection(OFF),
    iPeltierPwm(0),
    iCycleStartTime(0),
    iRamping(true),
    iPlatePid(&iPlateThermistor.GetTemp(), &iPeltierPwm, &iTargetPlateTemp, PLATE_PID_INC_NORM_P, PLATE_PID_INC_NORM_I, PLATE_PID_INC_NORM_D, DIRECT),
    iLidPid(LID_PID_GAIN_SCHEDULE, MIN_LID_PWM, MAX_LID_PWM),
    iTargetLidTemp(0)
{
  ipSerialControl = new SerialControl(ipDisplay);
  
  //init pins
  //pinMode(15, INPUT); //NEVER USED
  //pinMode(2, OUTPUT); //NEVER USED
  //pinMode(3, OUTPUT); //NEVER USED
  //pinMode(4, OUTPUT); //NEVER USED
  //pinMode(5, OUTPUT); //NEVER USED
  
    // SPCR = 01010000
  //interrupt disabled,spi enabled,msb 1st,master,clk low when idle,
  //sample on leading edge of clk,system clock/4 rate (fastest)
  int clr;
  SPCR = (1<<SPE)|(1<<MSTR)|(1<<4);
  clr=SPSR;
  clr=SPDR;
  delay(10); 

  iPlatePid.SetOutputLimits(MIN_PELTIER_PWM, MAX_PELTIER_PWM);
  
  // Peltier PWM
  TCCR1A |= (1<<WGM11) | (1<<WGM10);
  TCCR1B = _BV(CS21);
  
  // Lid PWM
  TCCR2A = _BV(COM2A1) | _BV(COM2B1) | _BV(WGM21) | _BV(WGM20);
  TCCR2B = _BV(CS22);

  iszProgName[0] = '\0';

  Step * const step = new Step;
  step->SetName("StapEen");
  step->SetTemp(950.0);
  step->SetRampDurationS(10);
  step->SetStepDurationS(20);
  Cycle * const program = new Cycle;
  program->AddComponent(step);
  program->SetNumCycles(100);
  Cycle * const display_cycle = new Cycle;
  display_cycle->SetNumCycles(100);
  const char * const program_name = "BurnBurnBurn!";
  const int lid_temperature = 950;
  this->SetProgram(program,display_cycle,program_name,lid_temperature);

}

Thermocycler::~Thermocycler()
{
  delete ipSerialControl;
  delete ipDisplay;
}

// accessors
int Thermocycler::GetNumCycles()
{
  return ipDisplayCycle->GetNumCycles();
}

int Thermocycler::GetCurrentCycleNum() {
  int numCycles = GetNumCycles();
  return ipDisplayCycle->GetCurrentCycle() > numCycles ? numCycles : ipDisplayCycle->GetCurrentCycle();
}

Thermocycler::ThermalState Thermocycler::GetThermalState() {
  if (iProgramState == EStartup || iProgramState == EStopped)
    return EIdle;
  
  if (iRamping) {
    if (ipPreviousStep != NULL) {
      return ipCurrentStep->GetTemp() > ipPreviousStep->GetTemp() ? EHeating : ECooling;
    } else {
      return iThermalDirection == HEAT ? EHeating : ECooling;
    }
  } else {
    return EHolding;
  }
}
 
// control
void Thermocycler::SetProgram(Cycle* pProgram, Cycle* pDisplayCycle, const char* szProgName, int lidTemp) {
  Stop();

  ipProgram = pProgram;
  ipDisplayCycle = pDisplayCycle;

  strcpy(iszProgName, szProgName);
  iTargetLidTemp = lidTemp;
}

void Thermocycler::Stop() {
  iProgramState = EStopped;
  
  ipProgram = NULL;
  ipPreviousStep = NULL;
  ipCurrentStep = NULL;
  
  iStepPool.ResetPool();
  iCyclePool.ResetPool();
  
  ipDisplay->Clear();
}

PcrStatus Thermocycler::Start() {
  if (ipProgram == NULL)
    return ENoProgram;
  
  //advance to lid wait state
  iProgramState = ELidWait;
  
  return ESuccess;
}
    
// internal
void Thermocycler::Loop()
{
  switch (iProgramState)
  {
    case EStartup:
    if (millis() > STARTUP_DELAY) {
      iProgramState = EStopped;
      
      if (!iRestarted && !ipSerialControl->CommandReceived()) {
        //check for stored program
        SCommand command;
        if (ProgramStore::RetrieveProgram(command, (char*)ipSerialControl->GetBuffer()))
          ProcessCommand(command);
      }
    }
    break;

  case ELidWait:    
    if (GetLidTemp() >= iTargetLidTemp - LID_START_TOLERANCE) {
      //lid has warmed, begin program
      iThermalDirection = OFF;
      iPeltierPwm = 0;
      PreprocessProgram();
      iProgramState = ERunning;
      
      ipProgram->BeginIteration();
      AdvanceToNextStep();
      
      iProgramStartTimeMs = millis();
    }
    break;
  
  case ERunning:
    //update program
    if (iProgramState == ERunning) {
      if (iRamping && abs(ipCurrentStep->GetTemp() - GetPlateTemp()) <= CYCLE_START_TOLERANCE && GetRampElapsedTimeMs() > ipCurrentStep->GetRampDurationS() * 1000) {
        //begin step hold
        
        //eta updates
        if (ipCurrentStep->GetRampDurationS() == 0) {
          //fast ramp
          iElapsedFastRampDegrees += absf(GetPlateTemp() - iRampStartTemp);
          iTotalElapsedFastRampDurationMs += millis() - iRampStartTime;
        }
        
        if (iRampStartTemp > GetPlateTemp())
          iHasCooled = true;
        iRamping = false;
        iCycleStartTime = millis();
        
      } else if (!iRamping && !ipCurrentStep->IsFinal() && millis() - iCycleStartTime > (unsigned long)ipCurrentStep->GetStepDurationS() * 1000) {
        //begin next step
        AdvanceToNextStep();
          
        //check for program completion
        if (ipCurrentStep == NULL || ipCurrentStep->IsFinal())
          iProgramState = EComplete;        
      }
    }
    break;
    
  case EComplete:
    if (iRamping && ipCurrentStep != NULL && abs(ipCurrentStep->GetTemp() - GetPlateTemp()) <= CYCLE_START_TOLERANCE)
      iRamping = false;
    break;
  }
  
  //lid 
  iLidThermistor.ReadTemp();
  ControlLid();
  
  //plate  
  iPlateThermistor.ReadTemp();
  CalcPlateTarget();
  ControlPeltier();
  
  //program
  UpdateEta();
  ipDisplay->Update();
  ipSerialControl->Process();
}

//private
void Thermocycler::AdvanceToNextStep() {
  ipPreviousStep = ipCurrentStep;
  ipCurrentStep = ipProgram->GetNextStep();
  if (ipCurrentStep == NULL)
    return;
  
  //update eta calc params
  if (ipPreviousStep == NULL || ipPreviousStep->GetTemp() != ipCurrentStep->GetTemp()) {
    iRamping = true;
    iRampStartTime = millis();
    iRampStartTemp = GetPlateTemp();
  } else {
    iCycleStartTime = millis(); //next step starts immediately
  }
  
  CalcPlateTarget();
  SetPlateControlStrategy();
}

void Thermocycler::SetPlateControlStrategy() {
  if (InControlledRamp())
    return;
    
  if (absf(iTargetPlateTemp - GetPlateTemp()) >= PLATE_BANGBANG_THRESHOLD && !InControlledRamp()) {
    iPlateControlMode = EBangBang;
    iPlatePid.SetMode(MANUAL);
  } else {
    iPlateControlMode = EPIDPlate;
    iPlatePid.SetMode(AUTOMATIC);
  }
  
  if (iRamping) {
    if (iTargetPlateTemp >= GetPlateTemp()) {
      iDecreasing = false;
      if (iTargetPlateTemp < PLATE_PID_INC_LOW_THRESHOLD)
        iPlatePid.SetTunings(PLATE_PID_INC_LOW_P, PLATE_PID_INC_LOW_I, PLATE_PID_INC_LOW_D);
      else
        iPlatePid.SetTunings(PLATE_PID_INC_NORM_P, PLATE_PID_INC_NORM_I, PLATE_PID_INC_NORM_D);

    } else {
      iDecreasing = true;
      if (iTargetPlateTemp > PLATE_PID_DEC_HIGH_THRESHOLD)
        iPlatePid.SetTunings(PLATE_PID_DEC_HIGH_P, PLATE_PID_DEC_HIGH_I, PLATE_PID_DEC_HIGH_D);
      else if (iTargetPlateTemp < PLATE_PID_DEC_LOW_THRESHOLD)
        iPlatePid.SetTunings(PLATE_PID_DEC_LOW_P, PLATE_PID_DEC_LOW_I, PLATE_PID_DEC_LOW_D);
      else
        iPlatePid.SetTunings(PLATE_PID_DEC_NORM_P, PLATE_PID_DEC_NORM_I, PLATE_PID_DEC_NORM_D);
    }
  }
}

void Thermocycler::CalcPlateTarget() {
  if (ipCurrentStep == NULL)
    return;
  
  if (InControlledRamp()) {
    //controlled ramp
    double tempDelta = ipCurrentStep->GetTemp() - ipPreviousStep->GetTemp();
    double rampPoint = (double)GetRampElapsedTimeMs() / (ipCurrentStep->GetRampDurationS() * 1000);
    iTargetPlateTemp = ipPreviousStep->GetTemp() + tempDelta * rampPoint;
    
  } else {
    //fast ramp
    iTargetPlateTemp = ipCurrentStep->GetTemp();
  }
}

void Thermocycler::ControlPeltier() {
  ThermalDirection newDirection = OFF;
  
  if (iProgramState == ERunning || (iProgramState == EComplete && ipCurrentStep != NULL)) {
    // Check whether we are nearing target and should switch to PID control
    if (iPlateControlMode == EBangBang && absf(iTargetPlateTemp - GetPlateTemp()) < PLATE_BANGBANG_THRESHOLD) {
      iPlateControlMode = EPIDPlate;
      iPlatePid.SetMode(AUTOMATIC);
      iPlatePid.ResetI();
    }
 
    // Apply control mode
    if (iPlateControlMode == EBangBang)
      iPeltierPwm = iTargetPlateTemp > GetPlateTemp() ? MAX_PELTIER_PWM : MIN_PELTIER_PWM;
    iPlatePid.Compute();
    
    if (iDecreasing && iTargetPlateTemp > PLATE_PID_DEC_LOW_THRESHOLD) {
      if (iTargetPlateTemp < GetPlateTemp())
        iPlatePid.ResetI();
      else
        iDecreasing = false;
    } 
    
    if (iPeltierPwm > 0)
      newDirection = HEAT;
    else if (iPeltierPwm < 0)
      newDirection = COOL; 
    else
      newDirection = OFF;
  } else {
    iPeltierPwm = 0;
  }
  
  iThermalDirection = newDirection;
  SetPeltier(newDirection, abs(iPeltierPwm));
}

void Thermocycler::ControlLid() {
  int drive = 0;  
  if (iProgramState == ERunning || iProgramState == ELidWait)
    drive = iLidPid.Compute(iTargetLidTemp, GetLidTemp());
 
  analogWrite(ms_pin_heater_lid, drive);
}

//PreprocessProgram initializes ETA parameters and validates/modifies ramp conditions
void Thermocycler::PreprocessProgram() {
  Step* pCurrentStep;
  Step* pPreviousStep = NULL;
  
  iProgramHoldDurationS = 0;
  iEstimatedTimeRemainingS = 0;
  iHasCooled = false;
  
  iProgramControlledRampDurationS = 0;
  iProgramFastRampDegrees = 0;
  iElapsedFastRampDegrees = 0;
  iTotalElapsedFastRampDurationMs = 0;  
  
  ipProgram->BeginIteration();
  while ((pCurrentStep = ipProgram->GetNextStep()) && !pCurrentStep->IsFinal()) {
    //validate ramp
    if (pPreviousStep != NULL && pCurrentStep->GetRampDurationS() * 1000 < absf(pCurrentStep->GetTemp() - pPreviousStep->GetTemp()) * PLATE_FAST_RAMP_THRESHOLD_MS) {
      //cannot ramp that fast, ignored set ramp
      pCurrentStep->SetRampDurationS(0);
    }
    
    //update eta hold
    iProgramHoldDurationS += pCurrentStep->GetStepDurationS();
 
    //update eta ramp
    if (pCurrentStep->GetRampDurationS() > 0) {
      //controlled ramp
      iProgramControlledRampDurationS += pCurrentStep->GetRampDurationS();
    } else {
      //fast ramp
      double previousTemp = pPreviousStep ? pPreviousStep->GetTemp() : GetPlateTemp();
      iProgramFastRampDegrees += absf(previousTemp - pCurrentStep->GetTemp()) - CYCLE_START_TOLERANCE;
    }
    
    pPreviousStep = pCurrentStep;
  }
}

void Thermocycler::UpdateEta() {
  if (iProgramState == ERunning) {
    double fastSecondPerDegree;
    if (iElapsedFastRampDegrees == 0 || !iHasCooled)
      fastSecondPerDegree = 1.0;
    else
      fastSecondPerDegree = iTotalElapsedFastRampDurationMs / 1000 / iElapsedFastRampDegrees;
      
    unsigned long estimatedDurationS = iProgramHoldDurationS + iProgramControlledRampDurationS + iProgramFastRampDegrees * fastSecondPerDegree;
    unsigned long elapsedTimeS = GetElapsedTimeS();
    iEstimatedTimeRemainingS = estimatedDurationS > elapsedTimeS ? estimatedDurationS - elapsedTimeS : 0;
  }
}

void Thermocycler::SetPeltier(ThermalDirection dir, int pwm) {
  if (dir == COOL)
  {
    digitalWrite(ms_pin_peltier_a, HIGH);
    digitalWrite(ms_pin_peltier_b, LOW);
  }
  else if (dir == HEAT)
  {
    digitalWrite(ms_pin_peltier_a, LOW);
    digitalWrite(ms_pin_peltier_b, HIGH);
  }
  else
  {
    digitalWrite(ms_pin_peltier_a, LOW);
    digitalWrite(ms_pin_peltier_b, LOW);
  }
  
  analogWrite(ms_pin_block_thermistor, pwm); //was pin 9
}

void Thermocycler::ProcessCommand(SCommand& command) {
  if (command.command == SCommand::EStart) {
    //find display cycle
    Cycle* pProgram = command.pProgram;
    Cycle* pDisplayCycle = pProgram;
    int largestCycleCount = 0;
    
    for (int i = 0; i < pProgram->GetNumComponents(); i++) {
      ProgramComponent* pComp = pProgram->GetComponent(i);
      if (pComp->GetType() == ProgramComponent::ECycle) {
        Cycle* pCycle = (Cycle*)pComp;
        if (pCycle->GetNumCycles() > largestCycleCount) {
          largestCycleCount = pCycle->GetNumCycles();
          pDisplayCycle = pCycle;
        }
      }
    }
    
    GetThermocycler().SetProgram(pProgram, pDisplayCycle, command.name, command.lidTemp);
    GetThermocycler().Start();
    
  } else if (command.command == SCommand::EStop) {
    GetThermocycler().Stop(); //redundant as we already stopped during parsing
  
  } else if (command.command == SCommand::EConfig) {
    //update displayed
    ipDisplay->SetContrast(command.contrast);
    
    //update stored contrast
    ProgramStore::StoreContrast(command.contrast);
  }
}
