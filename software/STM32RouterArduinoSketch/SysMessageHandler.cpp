#include "SysMessageHandler.h"

cSysMessageHandler::cSysMessageHandler(cPortHandler* ports, int portsCount){
  m_Ports = ports;
  m_PortsCount = portsCount;
  m_CurrentTracePort = -1;
  m_MasterTracePort = 0;
  m_WaitTraceResponse = false;
  m_DataError = false;
}

sSysMessageInfo cSysMessageHandler::getMessageInfo(uint8_t* package){
  sSysMessageInfo state;
  state.needSysHandling = false;
  state.needDefaultHandling = true;
  if (package[START_CHAR_POS]==START_CHAR_HIGH && package[ADDRESS_POS]==BROADCAST_ADDRESS && package[LENGTH_POS]>=2){
    if (package[DATA_START_POS + 0] == SYS_MSG){
      state.needSysHandling = true;
      if (package[DATA_START_POS + 1]==SYSMSG_TRACE_STEP || (package[DATA_START_POS + 1]==SYSMSG_TRACE_STATE && package[LENGTH_POS]>2))
        state.needDefaultHandling = false;
    }
  }
  return state;
}

void cSysMessageHandler::processSysMessage(uint8_t* package, int portIndex){  
  if (package[ADDRESS_POS]==ADDRESS_BROADCAST){
    int dataLength = package[LENGTH_POS];
    if (dataLength>=2){
      if (package[DATA_START_POS+0]==SYS_MSG){
        switch (package[DATA_START_POS+1]){
          case SYSMSG_PONG:{
            int addressesCount = package[DATA_START_POS+2];
            if (dataLength==1+1+1+addressesCount){
              for (uint8_t i = 0; i<addressesCount; i++)
                m_Ports[portIndex].setAddressEnable(package[DATA_START_POS+1+1+1+i]);
            }
            else{
              //INCORRECT SIZE
              m_DataError = true;
            }
          }; break;
          case SYSMSG_TRACE_START:{
            m_DataError = false;
            m_WaitTraceResponse= false;
            m_MasterTracePort = portIndex;
            m_CurrentTracePort = -1;            
            m_CurrentTracePortPortsCount = 0;
          };break;
          case SYSMSG_TRACE_STEP:{
            processTraceStep(package, portIndex);
          };break;
          case SYSMSG_TRACE_STATE:{
            processTraceState(package, portIndex);
          };break;
          
        }
        
      }
    }       
  }
}

void cSysMessageHandler::toNextTracePort(){  
  m_ChildDetected = false;
  m_CurrentTracePort = m_CurrentTracePort + 1;
  m_CurrentTracePortPortsCount = 0;
  
  if (m_CurrentTracePort==m_MasterTracePort){
    toNextTracePort();
    return;
  }  
}

void cSysMessageHandler::processTraceStep(uint8_t* package, int portIndex){
  if (portIndex!=m_MasterTracePort){
    sendTraceError(ERROR_1_TRACE_INCORRECT_STEP_SOURCE);
    return;
  }

  if (m_CurrentTracePort==-1){
    toNextTracePort();

    //send TRACE_STATE
    uint8_t buf[9];
    buf[0] = START_CHAR_HIGH;
    buf[1] = BROADCAST_ADDRESS;
    buf[2] = 5;
    
    buf[3] = SYS_MSG;
    buf[4] = SYSMSG_TRACE_STATE;
    
    buf[5] = 0x00; //router has not own address
    buf[6] = m_PortsCount;
    buf[7] = m_MasterTracePort;

    buf[8] = 0;
    for (int i = 0; i<8; ++i)
      buf[8] = buf[8] + buf[i];
    m_Ports[m_MasterTracePort].queuePackage(buf);  
    return;  
  }
  
  if (m_CurrentTracePort<0){
    sendTraceError(ERROR_2_TRACE_INCORRECT_PORT);
    return;
  }
  if (m_CurrentTracePort==m_MasterTracePort){
    sendTraceError(ERROR_3_TRACE_MASTER_PORT_NOT_ALLOWED);
    return;
  }
  if (m_CurrentTracePort>=m_PortsCount){
    sendTraceError(ERROR_4_TRACE_INCORRECT_PORTS);
    return;
  }

  m_TraceTimeoutTimer = 0;
  m_WaitTraceResponse = true;
  m_Ports[m_CurrentTracePort].queuePackage(package);
}

void cSysMessageHandler::processTraceState(uint8_t* package, int portIndex){  
  if (portIndex!=m_CurrentTracePort){
    sendTraceError(ERROR_5_TRACE_INCORRECT_STATE_SOURCE);
    return;
  }

  m_ChildDetected = true;
  m_CurrentTracePortPortsCount--; //RESPONSE RECEIVED - remove one pended port
  int dataLength = package[LENGTH_POS];
  if (dataLength==5){
    int portsCount = package[DATA_START_POS+3];
    int masterPort = package[DATA_START_POS+4];
    if (portsCount>masterPort){      
      m_Ports[m_MasterTracePort].queuePackage(package);
      m_CurrentTracePortPortsCount+=portsCount-1;
      if (m_CurrentTracePortPortsCount<0)
        toNextTracePort();
    }
    else{
      sendTraceError(ERROR_6_TRACE_INCORRECT_CHILD_PORTS);
    }
  }
  else{
    sendTraceError(ERROR_7_TRACE_WRONG_LENGTH);
  }
}

void cSysMessageHandler::sendTraceError(uint8_t errorCode){
  uint8_t buf[7];
  buf[0] = START_CHAR_HIGH;
  buf[1] = BROADCAST_ADDRESS;
  buf[2] = 0x03;
  
  buf[3] = 0x00;
  buf[4] = SYSMSG_TRACE_ERROR;
  
  buf[5] = errorCode;

  buf[6] = 0;
  for (int i = 0; i<6; ++i)
    buf[6] = buf[6] + buf[i];
  m_DataError = true;
  m_Ports[m_MasterTracePort].queuePackage(buf);  
}

void cSysMessageHandler::update(long dt){
  m_DataError = false;

  if (!m_WaitTraceResponse)
    return;
    
  m_TraceTimeoutTimer += dt;
  if (m_TraceTimeoutTimer>m_ChildDetected?TRACE_TIMEOUT*2:TRACE_TIMEOUT){
    m_WaitTraceResponse = false;
    if(m_ChildDetected){
      sendTraceError(ERROR_8_TRACE_CHILD_GONE);
    }    
    m_CurrentTracePortPortsCount --;
    if (m_CurrentTracePortPortsCount <0){
      toNextTracePort();
    }
  }
}


