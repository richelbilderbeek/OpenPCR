#include <stdlib.h>

#include "arduinoassert.h"
#include "arduinotrace.h"
#include "displayparameters.h"

DisplayParameters::DisplayParameters(
  const int lcd_ncols,
  const int lcd_nrows,
  const int pin_rs,
  const int pin_e ,
  const int pin_d4,
  const int pin_d5,
  const int pin_d6,
  const int pin_d7,
  const int pin_v0
  )
  :
    m_lcd_ncols(lcd_ncols),
    m_lcd_nrows(lcd_nrows),
    m_pin_rs(pin_rs),
    m_pin_e(pin_e),
    m_pin_d4(pin_d4),
    m_pin_d5(pin_d5),
    m_pin_d6(pin_d6),
    m_pin_d7(pin_d7),
    m_pin_v0(pin_v0)
{
  Trace("Display::Display")
  Assert(m_lcd_ncols > 0 && "An LCD display has at least one column");
  Assert(m_lcd_ncols >= 16 && "The LCD display used needs at least 16 columns");
  Assert(m_lcd_nrows > 0 && "An LCD display has at least one row");
  Assert(m_pin_rs >= 0 && "Pin RS has at least an index of zero");
  Assert(m_pin_e >= 0 && "Pin E has at least an index of zero");
  Assert(m_pin_d4 >= 0 && "Pin D4 has at least an index of zero");
  Assert(m_pin_d5 >= 0 && "Pin D5 has at least an index of zero");
  Assert(m_pin_d6 >= 0 && "Pin D6 has at least an index of zero");
  Assert(m_pin_d7 >= 0 && "Pin D7 has at least an index of zero");
  Assert(m_pin_v0 >= 0 && "Pin V0 has at least an index of zero");
  Trace("End of Display::Display")
}

bool operator==(const DisplayParameters& lhs, const DisplayParameters& rhs)
{
  return
       lhs.m_lcd_ncols == rhs.m_lcd_ncols
    && lhs.m_lcd_nrows == rhs.m_lcd_nrows
    && lhs.m_pin_rs    == rhs.m_pin_rs
    && lhs.m_pin_e     == rhs.m_pin_e
    && lhs.m_pin_d4    == rhs.m_pin_d4
    && lhs.m_pin_d5    == rhs.m_pin_d5
    && lhs.m_pin_d6    == rhs.m_pin_d6
    && lhs.m_pin_d7    == rhs.m_pin_d7
    && lhs.m_pin_v0    == lhs.m_pin_v0;
}
