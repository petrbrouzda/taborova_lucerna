#include "LogShipper.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Arduino.h>

LogShipper::LogShipper( int bufferSize )
{
    this->buffer = (char*) malloc( bufferSize+2 );
    this->size = bufferSize;
    this->head = 0;
    this->tail = 0;
    this->buffer[bufferSize] = 0;
    this->buffer[bufferSize+1] = 0;
}

void LogShipper::write(const char *text)
{
    std::lock_guard<std::mutex> lck(this->serial_mtx);

    char buff[20];
    long t = millis();
    sprintf( buff, "%d.%d ", t/1000, (t%1000)/100 );
    this->writeText( buff );
    this->writeText( text );
    this->writeText( "\n" );
}

void LogShipper::writeText(const char *text)
{
    char * out = this->buffer + this->tail;
    char * in = (char*)text;
    while(true) {
        char c = *in;
        if( c==0 ) {
            *out=0;
            break;
        }
        in++;
        *out = c;
        out++;
        this->tail++;
        *out = 0;
        if( this->tail <= this->head ) {
            this->head++;
        }
        if( this->tail == this->size ) {
            this->tail = 0;
            this->head = 1;
            out = this->buffer;
        }
    }
}

char *LogShipper::getBuffer1()
{
    std::lock_guard<std::mutex> lck(this->serial_mtx);

    if( this->head == this->tail ) {
        // nic nemame 
        return NULL;
    }
    if( this->tail < this->head ) {
        return this->buffer + this->head;
    } else {
        return this->buffer;
    }
    
}

char *LogShipper::getBuffer2()
{
    std::lock_guard<std::mutex> lck(this->serial_mtx);

    if( this->tail < this->head ) {
        return this->buffer;
    } else {
        return NULL;
    }
}

void LogShipper::clear()
{
    std::lock_guard<std::mutex> lck(this->serial_mtx);

    this->head = 0;
    this->tail = 0;
    this->buffer[0] = 0;
}

int LogShipper::length()
{
    std::lock_guard<std::mutex> lck(this->serial_mtx);

    if( this->head == this->tail ) {
        // nic nemame 
        return 0;
    }
    if( this->tail < this->head ) {
        return (this->size - this->head) + this->tail;
    } else {
        return this->tail;
    }
}

int LogShipper::usage()
{
    return this->length() * 100 / this->size;
}

bool LogShipper::overwritten() 
{
    return this->tail < this->head;
}