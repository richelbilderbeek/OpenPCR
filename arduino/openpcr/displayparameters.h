#ifndef DISPLAYPARAMETERS_H
#define DISPLAYPARAMETERS_H

struct DisplayParameters
{
  DisplayParameters(
    const int lcd_ncols,
    const int lcd_nrows,
    const int pin_rs,
    const int pin_e ,
    const int pin_d4,
    const int pin_d5,
    const int pin_d6,
    const int pin_d7,
    const int pin_v0
  );

  const int m_lcd_ncols; //Number of columns of LCD display
  const int m_lcd_nrows; //Number of rows of LCD display
  const int m_pin_rs; //R/S pin, must be connected to pin  4 of LCD
  const int m_pin_e ; //E   pin, must be connected to pin  6 of LCD
  const int m_pin_d4; //D4  pin, must be connected to pin 11 of LCD
  const int m_pin_d5; //D5  pin, must be connected to pin 12 of LCD
  const int m_pin_d6; //D6  pin, must be connected to pin 13 of LCD
  const int m_pin_d7; //D7  pin, must be connected to pin 14 of LCD
  const int m_pin_v0; //V0  pin, must be connected to pin 3 of LCD

};

#endif // DISPLAYPARAMETERS_H
