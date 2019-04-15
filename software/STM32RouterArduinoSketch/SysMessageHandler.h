#ifndef SYS_MESSAGE_HANDLER_H
#define SYS_MESSAGE_HANDLER_H
#include <Arduino.h>
#include "ProtocolListener.h"
#include "PortHandler.h"

const uint8_t ERROR_1_TRACE_INCORRECT_STEP_SOURCE   = 1;
const uint8_t ERROR_2_TRACE_INCORRECT_PORT          = 2;
const uint8_t ERROR_3_TRACE_MASTER_PORT_NOT_ALLOWED = 3;
const uint8_t ERROR_4_TRACE_INCORRECT_PORTS         = 4;
const uint8_t ERROR_5_TRACE_INCORRECT_STATE_SOURCE  = 5;
const uint8_t ERROR_6_TRACE_INCORRECT_CHILD_PORTS   = 6;
const uint8_t ERROR_7_TRACE_WRONG_LENGTH            = 7;
const uint8_t ERROR_8_TRACE_CHILD_GONE              = 8;

const uint8_t SYSMSG_TRACE_START  = 0x10;
const uint8_t SYSMSG_TRACE_STEP   = 0x11;
const uint8_t SYSMSG_TRACE_STATE  = 0x12;
const uint8_t SYSMSG_TRACE_ERROR  = 0x13;

const uint8_t SYSMSG_PONG = 0x01;

const long TRACE_TIMEOUT = 1000;

struct sSysMessageInfo{
  bool needSysHandling;
  bool needDefaultHandling;
};

class cSysMessageHandler{
private:
  cPortHandler*     m_Ports;
  int               m_PortsCount;

  int               m_CurrentTracePortPortsCount;
  int               m_CurrentTracePort;  
  int               m_MasterTracePort;
  int               m_TraceTimeoutTimer;
  bool              m_WaitTraceResponse;
  bool              m_ChildDetected;
  
  bool              m_DataError;
  void processTraceStep(uint8_t* package, int portIndex);
  void processTraceState(uint8_t* package, int portIndex);
  void toNextTracePort();
  void sendTraceError(uint8_t errorCode);
public:
  cSysMessageHandler(cPortHandler* ports, int portsCount);

  sSysMessageInfo getMessageInfo(uint8_t* package);

  void processSysMessage(uint8_t* package, int portIndex);

  void update(long dt);

  bool isDataError(){return m_DataError;}

  
};

#endif

