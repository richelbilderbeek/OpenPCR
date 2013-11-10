#ifndef ARDUINOTRACE_H
#define ARDUINOTRACE_H

//From http://www.richelbilderbeek.nl/CppAssert.htm
#ifdef NTRACE
  #define Trace(x) ((void)0)
#else
  #include "string.h"

  #define Trace(x)      \
  {                     \
    Serial.print(#x);   \
    Serial.print("\n"); \
    delay(100);         \
  }

#endif


#endif //~ARDUINOTRACE_H
