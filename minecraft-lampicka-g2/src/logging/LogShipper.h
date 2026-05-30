#ifndef __LOG__SHIPPER__H
#define __LOG__SHIPPER__H

/**
 * Filtr do logu, který logové záznamy ukládá do cyklického bufferu.
 * To umožňuje odesílat log na server k dalšímu zpracování.
  */

#include <mutex>

#include "LogFilterInterface.h"

/** velikost  bufferu */
#define LS_BUFFER_SIZE 10000


class LogShipper : public LogFilterInterface
{
    public:
      LogShipper( int bufferSize = LS_BUFFER_SIZE );
      void write( const char * text );
      char * getBuffer1();
      char * getBuffer2();
      void clear();

      /** délka textu v bufferu */
      int length();

      /** procentuální naplněnost */
      int usage();

      /** true, pokud byl log přeplněný */
      bool overwritten();


    private:
      char * buffer;
      int size;

      /** zacatek obsahu */
      int head;
      /** konec obsahu */
      int tail;

      void writeText( const char * text );

      std::mutex serial_mtx;

};

#endif