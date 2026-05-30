
#include "SerialLogger.h"


SerialLogger::SerialLogger( Stream * out, LogFilterInterface * filter ) : LoggerInterface()
{
    this->out = out;
    this->filter = filter;
}


void SerialLogger::log( const char * format, ... )
{
    va_list args;
    va_start( args, format );
    int len = vsnprintf( this->msgBuffer, SERIALLOG_LINE_SIZE, format, args );
    va_end( args );
    this->msgBuffer[SERIALLOG_LINE_SIZE - 1] = 0;

    if( this->filter!=NULL ) {
        this->filter->write( this->msgBuffer );
    }
    
    this->printPos = 0;
    this->printed[0] = 0;

    out->println( this->msgBuffer );
}


