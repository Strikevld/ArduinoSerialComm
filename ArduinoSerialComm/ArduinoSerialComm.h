#ifndef ARDUINOSERIALCOMM_H
#define ARDUINOSERIALCOMM_H
 
#include "WProgram.h"
#include <elapsedMillis.h>
 
class ArduinoSerialComm
{
  public:
    ArduinoSerialComm(unsigned int bufferSize = 50, unsigned int packetSize = 10, unsigned int readWriteTimeout = 10000);
    ~ArduinoSerialComm();
    bool sendData(char * chData, int dataSize);
    bool newInput();
    bool stillAlive();
    const char * data();
    int size() { return mainBufferSize; }

  private:
    char *mainBuffer;
    unsigned int mainBufferSize;
    unsigned int readRemaining;
    unsigned int readPos;
    unsigned int chunkSize;
    unsigned int bufferLimit;
    unsigned int timeout;
    elapsedMillis elapsed;
    elapsedMillis alive;
};
 
#endif // ARDUINOSERIALCOMM_H
