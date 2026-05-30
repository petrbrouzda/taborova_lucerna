#ifndef ___APP_STATE_H_
#define ___APP_STATE_H_

/*
Sdílený stav celé aplikace.
Používá se, aby jednotlivé komponenty mohly vracet chyby až na UI úroveň.
*/

#define APPSTATE_VERSION "2.1 2026-01-31"

#include "../logging/LoggerInterface.h"

#define STATUS_TEXT_MAX_LEN 255

enum ProblemLevel {
    NONE,
    WARNING,
    ERROR
};

class AppState
{
    public:

        AppState( LoggerInterface * logger = NULL );

        ProblemLevel globalState;
        char problemDesc[STATUS_TEXT_MAX_LEN + 5];
        /** millis time */
        long problemTime;

        void setProblem( ProblemLevel state, const char * format, ... );
        void clearProblem();

        bool isProblem();
        bool newProblem;

    private:
        LoggerInterface * logger; 
};

#endif