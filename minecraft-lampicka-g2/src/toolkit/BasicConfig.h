#ifndef BASIC__CONFIG__H
#define BASIC__CONFIG__H

/*
Nástroj pro držení konfigurace typu
  proměnná=hodnota
  
*/

#include <Arduino.h>
#include <mutex>


#define BASICCONFIG_VERSION "2.1 2026-01-20"


class BasicConfig
{
  public:
    BasicConfig();
    ~BasicConfig();
    
    void parseFromString( char * input );
    void printTo( Print& p );

    const char * getString( const char * fieldName, const char * defaultValue );
    long getLong( const char * fieldName, long defaultValue );
    bool getBool( const char * fieldName, bool defaultValue );
    double getDouble( const char * fieldName, double defaultValue );

    /**
     * Umi zmenit hodnotu existujiciho klice ci pridat novy
     */
    void setValue( const char * fieldName, const char * value );
    void setValue( const char * fieldName, long value );

    bool isDirty();

  private:
    /** zapisuje do dat konfigu jednu naparsovanou polozku */
    void zapisVystup( char * name, int namePos, char * value, int valuePos );
    
    char ** fields;
    char ** values;
    int length;

    /**
     * Vraci klic v poli nebo -1 
     */ 
    int findKey( const char * fieldName );

    /**
     * Pokud je jiz pole zaplneno, prodlouzi ho.
     */ 
    void reserveSize();

    int size = 10;
    bool dirty;

    std::mutex serial_mtx;

};


#endif
