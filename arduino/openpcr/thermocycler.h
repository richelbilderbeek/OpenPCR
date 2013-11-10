/*
 *  thermocycler.h - OpenPCR control software.
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

#ifndef _THERMOCYCLER_H_
#define _THERMOCYCLER_H_

#include "PID_v1.h"
#include "pid.h"
#include "program.h"
#include "thermistors.h"

class Display;
class DisplayParameters;
class SerialControl;

class Thermocycler {
public:
  enum ProgramState {
    EStartup = 0,
    EStopped,
    ELidWait,
    ERunning,
    EComplete,
    EError,
    EClear //for Display clearing only
  };
  
  enum ThermalState {
    EHolding = 0,
    EHeating,
    ECooling,
    EIdle
  };
  
  enum ThermalDirection {
    OFF,
    HEAT,
    COOL
  };
  
  enum ControlMode {
    EBangBang,
    EPIDLid,
    EPIDPlate
  };
  
  Thermocycler(
    bool is_restarted,
    const int pin_block_thermistor,
    const int pin_heater_lid,
    const int pin_lid_thermistor,
    const int pin_peltier_a,
    const int pin_peltier_b,
    const int pin_plate_thermistor,
    const DisplayParameters& display_parameters
  );
  ~Thermocycler();
  
  // accessors
  ProgramState GetProgramState() const { return m_program_state; }
  ThermalState GetThermalState();
  Step* GetCurrentStep() { return m_current_step; }
  Cycle* GetDisplayCycle() { return m_display_cycle; }
  int GetNumCycles();
  int GetCurrentCycleNum();
  const char* GetProgName() { return m_program_name; }
  Display* GetDisplay() const { return m_display; }
  ProgramComponentPool<Cycle, 4>& GetCyclePool() { return m_cycle_pool; }
  ProgramComponentPool<Step, 20>& GetStepPool() { return m_step_pool; }
  
  boolean Ramping() { return m_is_ramping; }
  int GetPeltierPwm() { return m_peltier_pwm; }
  double GetLidTemp() { return m_lid_thermistor.GetTemp(); }
  double GetPlateTemp() { return m_plate_thermistor.GetTemp(); }
  unsigned long GetTimeRemainingS() { return m_estimated_time_remaining_sec; }
  unsigned long GetElapsedTimeS() { return (millis() - m_program_start_time_ms) / 1000; }
  unsigned long GetRampElapsedTimeMs() { return millis() - m_ramp_start_time; }
  boolean InControlledRamp() { return m_is_ramping && m_current_step->GetRampDurationS() > 0 && m_previous_step != NULL; }
  
  // control
  void SetProgram(Cycle* pProgram, Cycle* pDisplayCycle, const char* szProgName, int lidTemp); //takes ownership of cycles
  void Stop();
  PcrStatus Start();
  void ProcessCommand(SCommand& command);
  
  // internal
  void Loop();
  
private:
  void CheckPower();
  void ReadLidTemp();
  void ReadPlateTemp();
  void CalcPlateTarget();
  void ControlPeltier();
  void ControlLid();
  void PreprocessProgram();
  void UpdateEta();
 
  //util functions
  void AdvanceToNextStep();
  void SetPlateControlStrategy();
  void SetPeltier(ThermalDirection dir, int pwm);
  
private:

  Step* m_current_step;
  ProgramComponentPool<Cycle, 4> m_cycle_pool;
  unsigned long m_cycle_start_time;
  Display* const m_display;
  Cycle* m_display_cycle;
  double m_elapsed_fast_ramp_degrees;
  unsigned long m_estimated_time_remaining_sec;
  bool m_has_cooled;
  bool m_is_decreasing;
  bool m_is_ramping;
  bool m_is_restarted;
  CPIDController m_lid_pid;
  CLidThermistor m_lid_thermistor;
  double m_peltier_pwm;
  const int m_pin_block_thermistor;
  const int m_pin_heater_lid;
  const int m_pin_peltier_a;
  const int m_pin_peltier_b;
  PID * m_plate_pid;
  ControlMode m_plate_control_mode;
  CPlateThermistor m_plate_thermistor;
  Step* m_previous_step;
  Cycle* m_program;
  unsigned long m_program_controlled_ramp_duration_sec;
  double m_program_fast_ramp_degrees;
  unsigned long m_program_hold_duration_sec;
  char m_program_name[21];
  unsigned long m_program_start_time_ms;
  ProgramState m_program_state;
  double m_ramp_start_temp;
  unsigned long m_ramp_start_time;
  SerialControl* m_serial_control;
  ProgramComponentPool<Step, 20> m_step_pool;
  double m_target_lid_temp;
  double m_target_plate_temp;
  ThermalDirection m_thermal_direction; //holds actual real-time state
  unsigned long m_total_elapsed_fast_ramp_duration_ms;




};

#endif
