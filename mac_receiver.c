//////////////////////////////////////////////////////////////////////////////////
/// \file mac_receiver.c
/// \brief MAC receiver thread
/// \author Pascal Sartoretti (sap at hevs dot ch)
/// \version 1.0 - original
/// \date  2018-02
//////////////////////////////////////////////////////////////////////////////////
#include "stm32f7xx_hal.h"

#include <stdio.h>
#include <string.h>
#include "main.h"

//////////////////////////////////////////////////////////////////////////////////
// THREAD MAC RECEIVER
//////////////////////////////////////////////////////////////////////////////////
void MacReceiver(void *argument)
{
	struct queueMsg_t queueMsg;
	struct queueMsg_t appMsg;
	uint8_t * msg;
	uint8_t * qPtr;
	osStatus_t retCode;
	size_t	size;
	
	char * stringPtr;
	
	for(;;) {
		//wait for data, check queue
		retCode = osMessageQueueGet(queue_macR_id, &queueMsg, NULL, osWaitForever);
		CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
		qPtr = queueMsg.anyPtr;
		
		
		//read message
		if (qPtr[0] == TOKEN_TAG)    						//is it a token frame
		{
			queueMsg.anyPtr = qPtr;
			queueMsg.type = TOKEN;
			retCode = osMessageQueuePut(
				queue_macS_id,
				&queueMsg,
				osPriorityNormal,
				osWaitForever);
			CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);	
		}
		
		/*
		((msg[1]>>3) == gTokenInterface.myAddress) ||	// is destination my address
			((msg[0]>>3) == gTokenInterface.myAddress) ||	// is source my address
			((msg[1]>>3) == BROADCAST_ADDRESS))	// is a broadcast frame
		*/
		
		else
		{
			if(((qPtr[1]>>3) == gTokenInterface.myAddress) || 	//is destination my address
				((qPtr[1]>>3) == BROADCAST_ADDRESS))							//is a broadcast frame
			{
				//compute checksum
				//le checksum se fait sur les 6 bits de poids faible de la somme des bytes (SRC, DST, LEN, data bytes)				
				uint32_t check = qPtr[0]+qPtr[1]+qPtr[2];
				for(int i=0;i<qPtr[2];i++) {
					check += qPtr[3+i];
				}
				
				size = qPtr[2]+4;
				
				if((check&0x3F) == (qPtr[size]>>2)) //checksum ok
				{
					qPtr[size] = qPtr[size] | 0x3;
					
					
					//data_ind
					switch(qPtr[1]&0x3) {
						case TIME_SAPI:
							stringPtr = osMemoryPoolAlloc(memPool,osWaitForever);
							memcpy(msg,&qPtr[3],size-4);
						
							appMsg.type = DATA_IND;			
							appMsg.anyPtr = stringPtr;
							appMsg.sapi = qPtr[0]&0x7;	
							appMsg.addr = qPtr[0]>>3;
							retCode = osMessageQueuePut(
								queue_timeR_id,
								&appMsg,
								osPriorityNormal,
								osWaitForever);
							CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);				
							break;
						
						case CHAT_SAPI:
							stringPtr = osMemoryPoolAlloc(memPool,osWaitForever);
							memcpy(msg,&qPtr[3],size-4);
						
							appMsg.type = DATA_IND;			
							appMsg.anyPtr = stringPtr;
							appMsg.sapi = qPtr[0]&0x7;	
							appMsg.addr = qPtr[0]>>3;
							retCode = osMessageQueuePut(
								queue_chatR_id,
								&appMsg,
								osPriorityNormal,
								osWaitForever);
							CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
							break;
						default:
							//do nothing
							break;
					}
				}
				else //checksum error
				{
					qPtr[4+qPtr[3]] = qPtr[4+qPtr[3]] | 0x2;
				}
				
				
				
			}
			
			
			if((qPtr[0]>>3) == gTokenInterface.myAddress) //is source my address
			{
				queueMsg.anyPtr = qPtr;
				queueMsg.type = DATABACK;
				queueMsg.sapi = qPtr[0]&0x7;	
				queueMsg.addr = qPtr[0]>>3;
				retCode = osMessageQueuePut(
					queue_macS_id,
					&queueMsg,
					osPriorityNormal,
					osWaitForever);
				CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);	
			}
			else
			{
				queueMsg.anyPtr = qPtr;
				queueMsg.type = TO_PHY;
				retCode = osMessageQueuePut(
					queue_macS_id,
					&queueMsg,
					osPriorityNormal,
					osWaitForever);
				CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);	
			}		
	}	
}


