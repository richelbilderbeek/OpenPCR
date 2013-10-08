QT += core
QT -= gui
TEMPLATE = app

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

INCLUDEPATH += \
  /home/richel/ProjectRichelBilderbeek/Libraries/Arduino/libraries/LiquidCrystal \
  /home/richel/ProjectRichelBilderbeek/Libraries/Arduino/hardware/arduino/cores/arduino \
  /home/richel/ProjectRichelBilderbeek/Libraries/Arduino/hardware/arduino/variants/standard \
  /home/richel/ProjectRichelBilderbeek/Libraries/avr-libc-1.8.0/include

SOURCES += \
    openpcr/util.cpp \
    openpcr/thermocycler.cpp \
    openpcr/thermistors.cpp \
    openpcr/serialcontrol.cpp \
    openpcr/program.cpp \
    openpcr/PID_v1.cpp \
    openpcr/pid.cpp \
    openpcr/display.cpp

HEADERS += \
    openpcr/thermocycler.h \
    openpcr/thermistors.h \
    openpcr/serialcontrol.h \
    openpcr/program.h \
    openpcr/PID_v1.h \
    openpcr/pid.h \
    openpcr/pcr_includes.h \
    openpcr/display.h

OTHER_FILES += \
    ../air/js/openpcr.js \
    ../air/js/first_run.js
