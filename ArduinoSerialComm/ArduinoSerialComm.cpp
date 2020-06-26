#include "ArduinoSerialComm.h"


ArduinoSerialComm::ArduinoSerialComm(unsigned int bufferSize, unsigned int packetSize, unsigned int readWriteTimeout) 
{
    readRemaining = 0;
    readPos = 0;
    mainBufferSize = 0;
    bufferLimit = bufferSize;
    chunkSize = packetSize;
    timeout = readWriteTimeout;
    alive = timeout + 1;
    mainBuffer = new char[bufferSize];
}

ArduinoSerialComm::~ArduinoSerialComm()
{
    delete[] mainBuffer;
}

bool ArduinoSerialComm::sendData(char * chData, int dataSize)
{
  char * pointer = chData;
  elapsedMillis e;
  bool ok = false;
  byte reply = 0;
  if (dataSize) {
    char sz[2];
    sz[0] = ((char*)&dataSize)[0];
    sz[1] = ((char*)&dataSize)[1];
    Serial.write(sz, 2);
    while (e < timeout) {
      reply = 0;
      if (Serial.available()) {
        reply = Serial.read();
        if (reply == 1) {
          ok = true;
          alive = 0;
          break;
        }
      }
    }
    if (!ok)
      return false;
  }
  int remainingSize = dataSize;
  int writeSize = min(chunkSize, remainingSize);
  while (remainingSize > 0) {
    Serial.write(pointer, writeSize);
    pointer += writeSize;
    remainingSize -= writeSize;
    writeSize = min(chunkSize, remainingSize);

    ok = false;
    e = 0;
    while (e < timeout) {
      reply = 0;
      if (Serial.available()) {
        reply = Serial.read();
        if (reply == 1) {
          ok = true;
          alive = 0;
          break;
        }
      }
    }

    if (!ok)
      return false;
  }
  return true;
}

bool ArduinoSerialComm::newInput()
{
    if (!Serial)
        return false;
    if (!Serial.available()) {
        return false;
    }
    if (!mainBufferSize && Serial.available() == 1) {
        byte cmd = Serial.read();
        if (cmd == 0) {
            Serial.write(((byte)chunkSize));
            Serial.flush();
        }
        else if (cmd == 1) {
            unsigned int bl = bufferLimit;
            Serial.write(((char*)&bl), sizeof(int));
            Serial.flush();
        }
        else if (cmd == 2) {
            unsigned int tio = timeout;
            Serial.write(((char*)&tio), sizeof(int));
            Serial.flush();
        }
        else if (cmd == 3)
            alive = 0;
        return false;
    }
    if (!readRemaining  && Serial.available() == 1) {
        byte cmd = Serial.read();
        if (cmd == 3) {
            alive = 0;
        }
        return false;
    }
    if (!mainBufferSize) {
        if (Serial.available() < sizeof(int)) {
            return false;
        }
        if (Serial.readBytes(((char*)&mainBufferSize), sizeof(int)) != sizeof(int)) {
            mainBufferSize = 0;
            return false;
        }
        alive = 0;
        Serial.write(1);
        if (!mainBufferSize) {
            return false;
        }
        if (mainBufferSize > bufferLimit) {
            mainBufferSize = 0;
            return false;
        }
        readRemaining = mainBufferSize;
        elapsed = 0;
        readPos = 0;
    }

    if (elapsed > timeout) {
        readRemaining = 0;
        readPos = 0;
        mainBufferSize = 0;
        return false;
    }

  
    alive = 0;
    int readSize = min(readRemaining, chunkSize);

    if (Serial.available() < readSize)
        return false;
  
    char * buf = mainBuffer;
    buf += readPos;
  
    if (Serial.readBytes(buf, readSize) != readSize) {
        readRemaining = 0;
        readPos = 0;
        mainBufferSize = 0;
        return false;
    }

    readPos += readSize;
    readRemaining -= readSize;
  
    Serial.write(1);
  
    if (readRemaining)
        return newInput();
    mainBuffer[mainBufferSize] = '\0';
    delay(200);
    return true;
}

bool ArduinoSerialComm::stillAlive()
{
    newInput();
    return (alive < timeout);
}

const char * ArduinoSerialComm::data()
{
    const char * sndData = (const char*)mainBuffer;
    return sndData;
}
