#ifndef __INTERFACE_LOGGER_H
#define __INTERFACE_LOGGER_H

/**
 * Základní interface pro logování, který je implementován v AsyncLogger nebo SerialLogger (nebo prázdném NullLogger)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <Arduino.h>

#include "LogFilterInterface.h"


#define LOG_PRINT_BUFFER_SIZE 150

class LoggerInterface : public Print
{
  public:
    LoggerInterface();

    /** zapíše do logu záznam */
    virtual void log ( const char * format, ... )  = 0;
    
    /** needed for Print interface  */ 
    size_t write(uint8_t);
    /** needed for Print interface  */ 
    size_t write(const uint8_t* buffer, size_t size);

    /** content written by Print interface - erased after next log() call */ 
    char printed[LOG_PRINT_BUFFER_SIZE];

    /** nastaví filtr; filtr dostane do ruky každý zaznamenávaný záznam */
    void setFilter( LogFilterInterface * filter );

  protected:
    int printPos;
    
    LogFilterInterface * filter = NULL;
};

#endif

