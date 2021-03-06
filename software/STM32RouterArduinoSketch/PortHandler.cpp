#include "PortHandler.h"

const long DATA_TIMEOUT = 500;

cPortHandler::cPortHandler(){
  m_SendBufferSize = 0;
  m_ReceiveBufferSize = 0;
  m_ReceiverState = WAIT_START;

  m_Overflow = false;
  m_DataError = false;
  setAddressEnable(BROADCAST_ADDRESS);
}

void cPortHandler::setStream(SerialUART* serial)
{
  m_Stream = serial;
}


void cPortHandler::queuePackage(const uint8_t* data){
  if (m_ListenersList.isAddressEnable(data[ADDRESS_POS])){    
    if (m_SendBufferSize<HALF_SEND_BUFFER_SIZE || data[START_CHAR_POS]==START_CHAR_HIGH){
      uint8_t packageLength = PACKAGE_SERVICE_DATA_SIZE+data[LENGTH_POS];
      if (m_SendBufferSize+packageLength<SEND_BUFFER_SIZE){
        memcpy(&m_SendBuffer[m_SendBufferSize],data,packageLength);
        m_SendBufferSize+=packageLength;
      }
      else{
        //BUFFER OVERFLOW, EVENT HIGH PRIORITY DATA CANT BE SEND
        m_Overflow = true;
      }
    }
    else{
      //BUFFER OVERFLOW, ONLY HIGH PRIORITY DATA SENDING AVAILABLE
      m_Overflow = true;
    }    
  }
}

void cPortHandler::receive(uint8_t data, bool clear)
{
  if (clear){
    m_ReceiveBufferSize = 0;
    m_Checksum = 0;
  }
  m_ReceiveBuffer[m_ReceiveBufferSize] = data;
  m_Checksum += data;
  if (m_ReceiveBufferSize<PACKAGE_BUFFER_SIZE-1)
    m_ReceiveBufferSize++;
}

bool cPortHandler::processData(long dt){
  m_DataError = false;
  m_Overflow = false;
  if (m_TimeFromLastData<DATA_TIMEOUT){
    m_TimeFromLastData += dt;
    if (m_TimeFromLastData>=DATA_TIMEOUT && m_ReceiverState!=WAIT_START){
      //timeout            
      m_DataError = true;
      m_ReceiverState=WAIT_START;
    }
  }
  
  while (m_Stream->available())
  {
    m_TimeFromLastData = 0;
    int c = m_Stream->read();    
    
    switch(m_ReceiverState){
      case WAIT_START:{
        if (c==START_CHAR_LOW || c==START_CHAR_HIGH){
          receive(c,true);          
          m_ReceiverState = WAIT_ADDRESS;
        }
      };break;
      case WAIT_ADDRESS:{
        receive(c,false);        
        m_ReceiverState = WAIT_LENGTH;
      };break;
      case WAIT_LENGTH:{
        if (c<=MAX_PACKAGE_DATA_SIZE){
          receive(c,false);          
          m_ReceiverState = WAIT_CHECKSUM;
        }
        else{
          //incorrect data, wait next package
          m_DataError = true;
          m_ReceiverState = WAIT_START;
        }
      };break;
      case WAIT_CHECKSUM:{
        uint8_t currentChecksum = m_Checksum;
        receive(c,false);
        if (m_ReceiveBuffer[LENGTH_POS]==m_ReceiveBufferSize-LENGTH_POS-2){        
          m_ReceiverState = WAIT_START;
          if (c==currentChecksum){            
            m_DataError = false; 
            //DATA READY
            return true;  
          }
          else{
            //error - incorrect checksum
            m_DataError = true;
          }          
        }
      };break;
    }
  }
  return false;
}

bool cPortHandler::startSending(){
  m_Overflow = false;
  if (m_SendBufferSize==0)
    return false;

  long maxWriteCount = m_Stream->availableForWrite();
  if (maxWriteCount==0)
    return false;
    
  int dataToSend = m_SendBufferSize;
  if (dataToSend>maxWriteCount){
    dataToSend = maxWriteCount;
  }

  m_Stream->write(m_SendBuffer,dataToSend);
  if (dataToSend<m_SendBufferSize){
    memmove(m_SendBuffer,&m_SendBuffer[dataToSend],m_SendBufferSize-dataToSend);
    m_SendBufferSize -= dataToSend;
  }
  else{
    m_SendBufferSize = 0;
  }
  return true;
}


