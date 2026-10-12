/// @file usart_master.h

#ifndef USART_MASTER_H
#define USART_MASTER_H

#include <string>
#include <inttypes.h>
#include <chrono>


#define MAX_RESPONSE_LENGTH 100


enum class UsartState {
    Uninitialized,
    Open,
    Closed,
    Error
};

enum class UsartError {
    None,
    UsartDeviceNotFound,
    OpenFailed,
    SelectFailed,
    ReadFailed,
    WriteFailed,
    InternalError,
    Unknown
};

class LowLevelUsart {
  private:
    std::string* NameOfPort;
    UsartState State;
    int SerialPortHandler;
    UsartError ErrorCode;
    char TotalResponse[MAX_RESPONSE_LENGTH];
    uint16_t TotalReceivedBytes;
    std::chrono::high_resolution_clock::time_point TimeWhenSilenceStarted;
  public:
    LowLevelUsart();
    ~LowLevelUsart();
    void openUsart(const std::string& Name);
    std::string getName() const;
    UsartState getState() const;
    UsartError getErrorCode() const;
    bool collectInputDataUntilSilence();
    int getInputData(char* Buffer, int BufferSize);
    int sendOutputData(const char* Buffer, int Length);
    void clearInputBuffer();
    std::chrono::milliseconds getWaitTime() const;
};

#endif // USART_MASTER_H
