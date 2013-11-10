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

#include "arduinotrace.h"
#include "arduinoassert.h"
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

//const int Thermocycler::m_pin_block_thermistor = A4;
//const int Thermocycler::m_pin_heater_lid = 3;
//const int Thermocycler::m_pin_peltier_a = 2;
//const int Thermocycler::m_pin_peltier_b = 4;
//pid parameters
const SPIDTuning LID_PID_GAIN_SCHEDULE[] = {
  //maxTemp, kP, kI, kD
  { 70, 40, 0.15, 60 },
  { 200, 80, 1.1, 10 }
};

Thermocycler::Thermocycler(
  bool is_restarted,
  const int pin_block_thermistor,
  const int pin_heater_lid,
  const int pin_lid_thermistor,
  const int pin_peltier_a,
  const int pin_peltier_b,
  const int pin_plate_thermistor,
  const DisplayParameters& display_parameters
  )
  :
    m_current_step(NULL),
    m_cycle_start_time(0),
    m_display(new Display(display_parameters)),
    m_display_cycle(NULL),
    m_is_ramping(true),
    m_is_restarted(is_restarted),
    m_lid_pid(LID_PID_GAIN_SCHEDULE, MIN_LID_PWM, MAX_LID_PWM),
    m_lid_thermistor(pin_lid_thermistor),
    m_peltier_pwm(0.0),
    m_pin_block_thermistor(pin_block_thermistor),
    m_pin_heater_lid(pin_heater_lid),
    m_pin_peltier_a(pin_peltier_a),
    m_pin_peltier_b(pin_peltier_b),
    m_plate_pid(NULL),
    m_plate_thermistor(pin_plate_thermistor),
    m_previous_step(NULL),
    m_program(NULL),
    m_program_state(EStartup),
    m_serial_control(NULL),
    m_target_lid_temp(0),
    m_thermal_direction(OFF)
{
  Trace("Thermocycler::Thermocycler");
  m_plate_pid = new PID(
    &m_plate_thermistor.GetTemp(),
    &m_peltier_pwm,
    &m_target_plate_temp,
    PLATE_PID_INC_NORM_P,
    PLATE_PID_INC_NORM_I,
    PLATE_PID_INC_NORM_D,
    DIRECT
  );
  m_serial_control = new SerialControl(m_display);
  
  // SPCR = 01010000
  //interrupt disabled,spi enabled,msb 1st,master,clk low when idle,
  //sample on leading edge of clk,system clock/4 rate (fastest)
  SPCR = (1<<SPE)|(1<<MSTR)|(1<<4);
  //int clr = SPSR;
  //clr = SPDR;
  delay(10); 

  m_plate_pid->SetOutputLimits(MIN_PELTIER_PWM, MAX_PELTIER_PWM);
  
  // Peltier PWM
  TCCR1A |= (1<<WGM11) | (1<<WGM10);
  TCCR1B = _BV(CS21);
  
  // Lid PWM
  TCCR2A = _BV(COM2A1) | _BV(COM2B1) | _BV(WGM21) | _BV(WGM20);
  TCCR2B = _BV(CS22);

  m_program_name[0] = '\0';

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
  delete m_serial_control;
  delete m_display;
}

// accessors
int Thermocycler::GetNumCycles()
{
  return m_display_cycle->GetNumCycles();
}

int Thermocycler::GetCurrentCycleNum() {
  int numCycles = GetNumCycles();
  return m_display_cycle->GetCurrentCycle() > numCycles ? numCycles : m_display_cycle->GetCurrentCycle();
}

Thermocycler::ThermalState Thermocycler::GetThermalState() {
  if (m_program_state == EStartup || m_program_state == EStopped)
    return EIdle;
  
  if (m_is_ramping) {
    if (m_previous_step != NULL) {
      return m_current_step->GetTemp() > m_previous_step->GetTemp() ? EHeating : ECooling;
    } else {
      return m_thermal_direction == HEAT ? EHeating : ECooling;
    }
  } else {
    return EHolding;
  }
}
 
void Thermocycler::SetProgram(
  Cycle* pProgram,
  Cycle* pDisplayCycle,
  const char* szProgName,
  int lidTemp)
{
  Stop();

  m_program = pProgram;
  m_display_cycle = pDisplayCycle;

  strcpy(m_program_name, szProgName);
  m_target_lid_temp = lidTemp;
}

void Thermocycler::Stop() {
  m_program_state = EStopped;
  
  m_program = NULL;
  m_previous_step = NULL;
  m_current_step = NULL;
  
  m_step_pool.ResetPool();
  m_cycle_pool.ResetPool();
  
  m_display->Clear();
}

PcrStatus Thermocycler::Start() {
  if (m_program == NULL)
    return ENoProgram;
  
  //advance to lid wait state
  m_program_state = ELidWait;
  
  return ESuccess;
}
    
void Thermocycler::Loop()
{
  switch (m_program_state)
  {
    case EStartup:
    if (millis() > STARTUP_DELAY) {
      m_program_state = EStopped;
      
      //if (!m_is_restarted && !m_serial_control->CommandReceived())
      {
        //check for stored program
        //SCommand command;
        //if (ProgramStore::RetrieveProgram(command, (char*)m_serial_control->GetBuffer()))
        //  ProcessCommand(command);
      }
    }
    break;

  case ELidWait:    
    if (GetLidTemp() >= m_target_lid_temp - LID_START_TOLERANCE) {
      //lid has warmed, begin program
      m_thermal_direction = OFF;
      m_peltier_pwm = 0;
      PreprocessProgram();
      m_program_state = ERunning;
      
      m_program->BeginIteration();
      AdvanceToNextStep();
      
      m_program_start_time_ms = millis();
    }
    break;
  
  case ERunning:
    //update program
    if (m_program_state == ERunning) {
      if (m_is_ramping && abs(m_current_step->GetTemp() - GetPlateTemp()) <= CYCLE_START_TOLERANCE && GetRampElapsedTimeMs() > m_current_step->GetRampDurationS() * 1000) {
        //begin step hold
        
        //eta updates
        if (m_current_step->GetRampDurationS() == 0) {
          //fast ramp
          m_elapsed_fast_ramp_degrees += absf(GetPlateTemp() - m_ramp_start_temp);
          m_total_elapsed_fast_ramp_duration_ms += millis() - m_ramp_start_time;
        }
        
        if (m_ramp_start_temp > GetPlateTemp())
          m_has_cooled = true;
        m_is_ramping = false;
        m_cycle_start_time = millis();
        
      } else if (!m_is_ramping && !m_current_step->IsFinal() && millis() - m_cycle_start_time > (unsigned long)m_current_step->GetStepDurationS() * 1000) {
        //begin next step
        AdvanceToNextStep();
          
        //check for program completion
        if (m_current_step == NULL || m_current_step->IsFinal())
          m_program_state = EComplete;
      }
    }
    break;
    
  case EComplete:
    if (m_is_ramping && m_current_step != NULL && abs(m_current_step->GetTemp() - GetPlateTemp()) <= CYCLE_START_TOLERANCE)
      m_is_ramping = false;
    break;
  case EStopped: //Nothing
  case EError: //Nothing
  case EClear: //Nothing
    break;
  }
  
  //lid 
  m_lid_thermistor.ReadTemp();
  ControlLid();
  
  //plate  
  m_plate_thermistor.ReadTemp();
  CalcPlateTarget();
  ControlPeltier();
  
  //program
  UpdateEta();
  m_display->Update();
  m_serial_control->Process();
}

//private
void Thermocycler::AdvanceToNextStep() {
  m_previous_step = m_current_step;
  m_current_step = m_program->GetNextStep();
  if (m_current_step == NULL)
    return;
  
  //update eta calc params
  if (m_previous_step == NULL || m_previous_step->GetTemp() != m_current_step->GetTemp()) {
    m_is_ramping = true;
    m_ramp_start_time = millis();
    m_ramp_start_temp = GetPlateTemp();
  } else {
    m_cycle_start_time = millis(); //next step starts immediately
  }
  
  CalcPlateTarget();
  SetPlateControlStrategy();
}

void Thermocycler::SetPlateControlStrategy() {
  if (InControlledRamp())
    return;
    
  if (absf(m_target_plate_temp - GetPlateTemp()) >= PLATE_BANGBANG_THRESHOLD && !InControlledRamp()) {
    m_plate_control_mode = EBangBang;
    m_plate_pid->SetMode(MANUAL);
  } else {
    m_plate_control_mode = EPIDPlate;
    m_plate_pid->SetMode(AUTOMATIC);
  }
  
  if (m_is_ramping) {
    if (m_target_plate_temp >= GetPlateTemp()) {
      m_is_decreasing = false;
      if (m_target_plate_temp < PLATE_PID_INC_LOW_THRESHOLD)
        m_plate_pid->SetTunings(PLATE_PID_INC_LOW_P, PLATE_PID_INC_LOW_I, PLATE_PID_INC_LOW_D);
      else
        m_plate_pid->SetTunings(PLATE_PID_INC_NORM_P, PLATE_PID_INC_NORM_I, PLATE_PID_INC_NORM_D);

    } else {
      m_is_decreasing = true;
      if (m_target_plate_temp > PLATE_PID_DEC_HIGH_THRESHOLD)
        m_plate_pid->SetTunings(PLATE_PID_DEC_HIGH_P, PLATE_PID_DEC_HIGH_I, PLATE_PID_DEC_HIGH_D);
      else if (m_target_plate_temp < PLATE_PID_DEC_LOW_THRESHOLD)
        m_plate_pid->SetTunings(PLATE_PID_DEC_LOW_P, PLATE_PID_DEC_LOW_I, PLATE_PID_DEC_LOW_D);
      else
        m_plate_pid->SetTunings(PLATE_PID_DEC_NORM_P, PLATE_PID_DEC_NORM_I, PLATE_PID_DEC_NORM_D);
    }
  }
}

void Thermocycler::CalcPlateTarget() {
  if (m_current_step == NULL)
    return;
  
  if (InControlledRamp()) {
    //controlled ramp
    double tempDelta = m_current_step->GetTemp() - m_previous_step->GetTemp();
    double rampPoint = (double)GetRampElapsedTimeMs() / (m_current_step->GetRampDurationS() * 1000);
    m_target_plate_temp = m_previous_step->GetTemp() + tempDelta * rampPoint;
    
  } else {
    //fast ramp
    m_target_plate_temp = m_current_step->GetTemp();
  }
}

void Thermocycler::ControlPeltier() {
  ThermalDirection newDirection = OFF;
  
  if (m_program_state == ERunning || (m_program_state == EComplete && m_current_step != NULL)) {
    // Check whether we are nearing target and should switch to PID control
    if (m_plate_control_mode == EBangBang && absf(m_target_plate_temp - GetPlateTemp()) < PLATE_BANGBANG_THRESHOLD) {
      m_plate_control_mode = EPIDPlate;
      m_plate_pid->SetMode(AUTOMATIC);
      m_plate_pid->ResetI();
    }
 
    // Apply control mode
    if (m_plate_control_mode == EBangBang)
      m_peltier_pwm = m_target_plate_temp > GetPlateTemp() ? MAX_PELTIER_PWM : MIN_PELTIER_PWM;
    m_plate_pid->Compute();
    
    if (m_is_decreasing && m_target_plate_temp > PLATE_PID_DEC_LOW_THRESHOLD) {
      if (m_target_plate_temp < GetPlateTemp())
        m_plate_pid->ResetI();
      else
        m_is_decreasing = false;
    } 
    
    if (m_peltier_pwm > 0)
      newDirection = HEAT;
    else if (m_peltier_pwm < 0)
      newDirection = COOL; 
    else
      newDirection = OFF;
  } else {
    m_peltier_pwm = 0;
  }
  
  m_thermal_direction = newDirection;
  SetPeltier(newDirection, abs(m_peltier_pwm));
}

void Thermocycler::ControlLid() {
  int drive = 0;  
  if (m_program_state == ERunning || m_program_state == ELidWait)
    drive = m_lid_pid.Compute(m_target_lid_temp, GetLidTemp());
 
  analogWrite(m_pin_heater_lid, drive);
}

//PreprocessProgram initializes ETA parameters and validates/modifies ramp conditions
void Thermocycler::PreprocessProgram() {
  Step* pCurrentStep;
  Step* pPreviousStep = NULL;
  
  m_program_hold_duration_sec = 0;
  m_estimated_time_remaining_sec = 0;
  m_has_cooled = false;
  
  m_program_controlled_ramp_duration_sec = 0;
  m_program_fast_ramp_degrees = 0;
  m_elapsed_fast_ramp_degrees = 0;
  m_total_elapsed_fast_ramp_duration_ms = 0;
  
  m_program->BeginIteration();
  while ((pCurrentStep = m_program->GetNextStep()) && !pCurrentStep->IsFinal()) {
    //validate ramp
    if (pPreviousStep != NULL && pCurrentStep->GetRampDurationS() * 1000 < absf(pCurrentStep->GetTemp() - pPreviousStep->GetTemp()) * PLATE_FAST_RAMP_THRESHOLD_MS) {
      //cannot ramp that fast, ignored set ramp
      pCurrentStep->SetRampDurationS(0);
    }
    
    //update eta hold
    m_program_hold_duration_sec += pCurrentStep->GetStepDurationS();
 
    //update eta ramp
    if (pCurrentStep->GetRampDurationS() > 0) {
      //controlled ramp
      m_program_controlled_ramp_duration_sec += pCurrentStep->GetRampDurationS();
    } else {
      //fast ramp
      double previousTemp = pPreviousStep ? pPreviousStep->GetTemp() : GetPlateTemp();
      m_program_fast_ramp_degrees += absf(previousTemp - pCurrentStep->GetTemp()) - CYCLE_START_TOLERANCE;
    }
    
    pPreviousStep = pCurrentStep;
  }
}

void Thermocycler::UpdateEta() {
  if (m_program_state == ERunning) {
    double fastSecondPerDegree;
    if (m_elapsed_fast_ramp_degrees == 0 || !m_has_cooled)
      fastSecondPerDegree = 1.0;
    else
      fastSecondPerDegree = m_total_elapsed_fast_ramp_duration_ms / 1000 / m_elapsed_fast_ramp_degrees;
      
    unsigned long estimatedDurationS = m_program_hold_duration_sec + m_program_controlled_ramp_duration_sec + m_program_fast_ramp_degrees * fastSecondPerDegree;
    unsigned long elapsedTimeS = GetElapsedTimeS();
    m_estimated_time_remaining_sec = estimatedDurationS > elapsedTimeS ? estimatedDurationS - elapsedTimeS : 0;
  }
}

void Thermocycler::SetPeltier(ThermalDirection dir, int pwm) {
  if (dir == COOL)
  {
    digitalWrite(m_pin_peltier_a, HIGH);
    digitalWrite(m_pin_peltier_b, LOW);
  }
  else if (dir == HEAT)
  {
    digitalWrite(m_pin_peltier_a, LOW);
    digitalWrite(m_pin_peltier_b, HIGH);
  }
  else
  {
    digitalWrite(m_pin_peltier_a, LOW);
    digitalWrite(m_pin_peltier_b, LOW);
  }
  
  analogWrite(m_pin_block_thermistor, pwm); //was pin 9
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
    m_display->SetContrast(command.contrast);
    
    //update stored contrast
    //ProgramStore::StoreContrast(command.contrast);
  }
}
