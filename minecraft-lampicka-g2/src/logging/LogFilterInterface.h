#ifndef __LOG__FILTERINTFCE__H
#define __LOG__FILTERINTFCE__H

class LogFilterInterface
{
    public:
      virtual void write( const char * text ) = 0;
};

#endif