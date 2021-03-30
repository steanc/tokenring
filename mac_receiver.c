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

/*
void MAC_DATA_REQUEST(station, SAPI, data)
{
	
}
*/

//////////////////////////////////////////////////////////////////////////////////
// THREAD MAC RECEIVER
//////////////////////////////////////////////////////////////////////////////////
void MacReceiver(void *argument)
{
	struct queueMsg_t queueMsg;
	uint8_t * msg;
	uint8_t * qPtr;
	osStatus_t retCode;
	size_t	size;
	
	for(;;) {
		//wait for data, check queue
		retCode = osMessageQueueGet(queue_macR_id, &queueMsg, NULL, osWaitForever);
		CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
		qPtr = queueMsg.anyPtr;
		
		
		//read message
		if (qPtr[0] == TOKEN_TAG)    						// is it a token frame ?
		{
		  //size = TOKENSIZE;											// yes -> token frame size
			
			//todo after token OK, move after the "end if"
			//msg = osMemoryPoolAlloc(memPool,osWaitForever);
			//memcpy(msg,qPtr,size);
			queueMsg.anyPtr = qPtr;
			queueMsg.type = TOKEN;
			retCode = osMessageQueuePut(
				queue_macS_id,
				&queueMsg,
				osPriorityNormal,
				osWaitForever);
			CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);	
		}
		else
		{
			//do nothing yet
			//size = qPtr[3]+6;    									// size of string + 6 (ETX,...)
		}
				
	}	
}


