#include "assert.h"
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
    lcd_ncols(lcd_ncols),
    lcd_nrows(lcd_nrows),
    pin_rs(pin_rs),
    pin_e(pin_e),
    pin_d4(pin_d4),
    pin_d5(pin_d5),
    pin_d6(pin_d6),
    pin_d7(pin_d7),
    pin_v0(pin_v0)
{
  assert(m_lcd_ncols > 0 && "An LCD display has at least one column");
  assert(m_lcd_ncols >= 16 && "The LCD display used needs at least 16 columns");
  assert(m_lcd_nrows > 0 && "An LCD display has at least one row");
  assert(m_pin_rs >= 0 && "Pin RS has at least an index of zero");
  assert(m_pin_e >= 0 && "Pin E has at least an index of zero");
  assert(m_pin_d4 >= 0 && "Pin D4 has at least an index of zero");
  assert(m_pin_d5 >= 0 && "Pin D5 has at least an index of zero");
  assert(m_pin_d6 >= 0 && "Pin D6 has at least an index of zero");
  assert(m_pin_d7 >= 0 && "Pin D7 has at least an index of zero");
  assert(m_pin_v0 >= 0 && "Pin V0 has at least an index of zero");
}
