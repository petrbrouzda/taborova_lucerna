#include <string.h>
#include <Arduino.h>

#include "AppState.h"
#include "../logging/NullLogger.h"

/*
viz popis v AppState.h
*/

AppState::AppState( LoggerInterface *logger  )
{
    this->globalState = NONE;
    this->problemDesc[0] = 0;
    this->problemTime = 0;
    this->newProblem = false;

    if( logger == NULL ) {
        this->logger = (LoggerInterface*) new NullLogger();
    } else {
        this->logger = logger;
    }
}

void AppState::setProblem( ProblemLevel state, const char * format, ... ) {
    va_list args;
    va_start( args, format );
    int len = vsnprintf( this->problemDesc, STATUS_TEXT_MAX_LEN, format, args );
    va_end( args );
    this->problemDesc[STATUS_TEXT_MAX_LEN] = 0;

    this->globalState = state;
    this->problemTime = millis();

    this->logger->log( "!!!appState: %s: %s", 
        state==ERROR ? "ERROR" : "WARN",
        this->problemDesc
    );

    this->newProblem = true;
}

void AppState::clearProblem()
{
    this->globalState = NONE;
    this->problemDesc[0] = 0;
    this->problemTime = 0;

    this->logger->log( "!!!appState: problem cleared" );
}

bool AppState::isProblem()
{
    return this->globalState!=NONE;
}
