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
	//struct queueMsg_t queueMsg;
	
	struct queueMsg_t qMsgIn;
	struct queueMsg_t qMsgOut;
	uint8_t * qPtrIn;
	
	
	struct queueMsg_t qMsgToApp;	
	
	char * stringPtr;
	
	
	osStatus_t retCode;
	size_t	size;
	
	for(;;) {
		//wait for data, check queue
		retCode = osMessageQueueGet(queue_macR_id, &qMsgIn, NULL, osWaitForever);
		CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
		qPtrIn = qMsgIn.anyPtr;
		
		
		//read message
		if (qPtrIn[0] == TOKEN_TAG) //is it a token frame ?
		{
			//send away
			qMsgOut.anyPtr = qPtrIn;
			qMsgOut.type = TOKEN;
			retCode = osMessageQueuePut(
				queue_macS_id,
				&qMsgOut,
				osPriorityNormal,
				osWaitForever);
			CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);	
		}
		
		else //not a token
		{
			if(((qPtrIn[1]>>3) == gTokenInterface.myAddress) || 	//is destination my address ?
				((qPtrIn[1]>>3) == BROADCAST_ADDRESS))							//is it a broadcast frame ?
			{
				size = qPtrIn[2]+4;
				//compute checksum
				//le checksum se fait sur les 6 bits de poids faible de la somme des bytes (SRC, DST, LEN, data bytes)				
				uint32_t check = qPtrIn[0]+qPtrIn[1]+qPtrIn[2];
				for(int i=0;i<size-4;i++) {
					check += qPtrIn[3+i];
				}
				
				if((check&0x3F) == (qPtrIn[size-1]>>2)) //checksum ok
				{
					qPtrIn[size-1] |= 0x3; //update READ / ACK
					
					
					//data_ind
					switch(qPtrIn[1]&0x7) {
						case TIME_SAPI:
							stringPtr = osMemoryPoolAlloc(memPool,osWaitForever);
							memcpy(stringPtr,&qPtrIn[3],size-4);
							stringPtr[qPtrIn[2]] = '\0';
							qMsgToApp.type = DATA_IND;			
							qMsgToApp.anyPtr = stringPtr;
							qMsgToApp.sapi = qPtrIn[0]&0x7;	
							qMsgToApp.addr = qPtrIn[0]>>3;
							retCode = osMessageQueuePut(
								queue_timeR_id,
								&qMsgToApp,
								osPriorityNormal,
								osWaitForever);
							CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);				
							break;
						
						case CHAT_SAPI:
							stringPtr = osMemoryPoolAlloc(memPool,osWaitForever);
							memcpy(stringPtr,&qPtrIn[3],size-4);
							stringPtr[qPtrIn[2]] = '\0';
							qMsgToApp.type = DATA_IND;			
							qMsgToApp.anyPtr = stringPtr;
							qMsgToApp.sapi = qPtrIn[0]&0x7;	
							qMsgToApp.addr = qPtrIn[0]>>3;
							retCode = osMessageQueuePut(
								queue_chatR_id,
								&qMsgToApp,
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
					qPtrIn[3+qPtrIn[3]] |=  0x2; //update READ and not ACK
					
					//do nothing else
				}
				
				
				
			}
			
			
			if((qPtrIn[0]>>3) == gTokenInterface.myAddress) //is source my address
			{
				qMsgOut.anyPtr = qPtrIn;
				qMsgOut.type = DATABACK;
				qMsgOut.sapi = qPtrIn[0]&0x7;	
				qMsgOut.addr = qPtrIn[0]>>3;
				retCode = osMessageQueuePut(
					queue_macS_id,
					&qMsgOut,
					osPriorityNormal,
					osWaitForever);
				CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);	
			}
			else
			{
				qMsgOut.anyPtr = qPtrIn;
				qMsgOut.type = TO_PHY;
				retCode = osMessageQueuePut(
					queue_macS_id,
					&qMsgOut,
					osPriorityNormal,
					osWaitForever);
				CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);	
			}		
		}
	}
}


