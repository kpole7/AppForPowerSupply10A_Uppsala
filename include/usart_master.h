/// @file usart_master.h

#ifndef USART_MASTER_H
#define USART_MASTER_H

#include <string>
#include <inttypes.h>
#include <chrono>


#define MAX_RESPONSE_LENGTH 300


enum class UsartLowLevelState {
    Uninitialized,
    Open,
    Closed,
    Error
};

enum class UsartLowLevelError {
    None,
    UsartDeviceNotFound,
    OpenFailed,
    SelectFailed,
    ReadFailed,
    WriteFailed,
    InternalError,
    SlaveOutOfOrder,
    CallError,
    Unknown
};

enum class UsartTransferState {
    Idle, // wait for a new outgoing data to be sent
    Sending,
    WaitingForResponse,
    Receiving,
    Completed,
    WaitingLimitExceeded, // the slave did not respond within the expected waiting limit
    IncorrectlyReceiving, // if the response is too long for a single command but not long enough to be considered a permanent error
    IncorrectlyCompleted, // ingoing transfer has ended but the response is too long
    IncorrectlyNoisy, // fatal error due to excessive noise on the line
    InternalError,
    CallError
};

class LowLevelUsart {
  private:
    UsartLowLevelState State;
    UsartTransferState TransferState;
    UsartLowLevelError ErrorCode;
    std::string* NameOfPort;
    int SerialPortHandler;
    char TotalResponse[MAX_RESPONSE_LENGTH];
    uint16_t TotalReceivedBytes;
    std::chrono::high_resolution_clock::time_point TimeWhenOutgoingDataEnded;
    std::chrono::high_resolution_clock::time_point TimeWhenResponseStarted;
    std::chrono::high_resolution_clock::time_point TimeWhenReceivedLastByte;
    std::chrono::milliseconds WaitingLimit;
    std::chrono::milliseconds ResponseDurationLimit;
  public:
    LowLevelUsart();
    ~LowLevelUsart();
    void openUsart(const std::string& Name);
    std::string getName() const;
    UsartLowLevelState getState() const;
    UsartTransferState getTransferState() const;
    UsartLowLevelError getErrorCode() const;
    void resetUsartMaster();
    void sendOutputData(const char* Buffer, int Length, std::chrono::milliseconds WaitingTimeout, 
      std::chrono::milliseconds ResponseTimeout);
    UsartTransferState collectInputDataUntilSilence();
    int getInputData(char* Buffer, int BufferSize);
};

#endif // USART_MASTER_H
