
#ifndef __NULLLOGGER_LOGGER_H
#define __NULLLOGGER_LOGGER_H

/**
 * Prázdný logger - implementuje interface ale nic nikam nevypisuje.
 */


#include "LoggerInterface.h"


/*
    Simple logging interface.
    Implements printf()-like functionality.
    
    If you want to log non-trivial objects (for example String or IPaddress) you can't pass them asi object:
        error: cannot pass objects of non-trivially-copyable type 'class IPAddress' through '...'
    raLogger implements Print interface for this.
    Printed data are stored into public char* logger->printed.
    Buffer is deleted after call to ->log, so this is the correct usage and you don't need to think of buffer used:

        IPAddress ip = WiFi.localIP();
        logger->print( ip );
        logger->log( "IP address = %s", logger->printed );
*/


#define SERIALLOG_LINE_SIZE 500


class NullLogger : public LoggerInterface
{
  public:
    NullLogger();
    void log ( const char * format, ... );
};

#endif  
