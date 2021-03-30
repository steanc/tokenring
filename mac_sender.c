//////////////////////////////////////////////////////////////////////////////////
/// \file mac_sender.c
/// \brief MAC sender thread
/// \author Pascal Sartoretti (pascal dot sartoretti at hevs dot ch)
/// \version 1.0 - original
/// \date  2018-02
//////////////////////////////////////////////////////////////////////////////////
#include "stm32f7xx_hal.h"

#include <stdio.h>
#include <string.h>
#include "main.h"
#include "ext_led.h"


//////////////////////////////////////////////////////////////////////////////////
// THREAD MAC RECEIVER
//////////////////////////////////////////////////////////////////////////////////
void MacSender(void *argument)
{
	struct queueMsg_t queueMsg;
	struct queueMsg_t queueMsg2;
	osStatus_t retCode;
	uint8_t* qPtr;
	uint8_t* msg;
	size_t size;
	/*
	* #From MAC_Receiver
	* TOKEN : 0xFF... (TOKEN
	* DATAPACK : SRC|DST|...CS (DATA Frame pointer, Src add, Src SAPI)
	* #From Touch
	* NEW_TOKEN : ... 
	* START : ...
	* STOP : ...
	* #From CHAT_SENDER
	* DATA_IND : ...\0 (STRING frame pointer, Dest add, Src SAPI)
	* #From TIME_SENDER
	* DATA_IND : ...\0 (STRING frame pointer, Broadcast (0x0F), Src API)
	* 
	* #To LCD
	* TOKEN_LIST : ...
	* MAC_ERROR : ...\0 (STRING frame pointer, Src add)
	* #To PHY_SENDER
	* TO_PHY : SRC|DST|...CS (DATA frame pointer)
	*/
	
	//init time on
	gTokenInterface.station_list[MYADDRESS] = ((1 << TIME_SAPI) + (1 << CHAT_SAPI));
	
	for(;;){	//Loop until doomsday
		retCode = osMessageQueueGet(
			queue_macS_id,
			&queueMsg,
			NULL,
			osWaitForever);
		CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);	
		qPtr = queueMsg.anyPtr;
		
		switch(queueMsg.type){
			case TOKEN :
				//TODO first
				for(int i = 0; i < 15; i++){
					if(i == MYADDRESS) {
						qPtr[i+1] = gTokenInterface.station_list[i];
					}
					else {
						gTokenInterface.station_list[i] = qPtr[i+1];	//Update Ready list
					}
					
				}
				queueMsg2.type = TOKEN_LIST;
				queueMsg2.anyPtr = NULL;
				retCode = osMessageQueuePut(
					queue_lcd_id,
					&queueMsg2,
					osPriorityNormal,
					osWaitForever);
				CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
/*				
				if(!gTokenInterface.station_list[MYADDRESS]){
					//qPtr[MYADDRESS+1] = ((1 << TIME_SAPI) + (1 << CHAT_SAPI));
					qPtr[MYADDRESS] = ((1 << TIME_SAPI) + (1 << CHAT_SAPI));
				}
				*/
				//msg = osMemoryPoolAlloc(memPool,osWaitForever);	
				//size = TOKENSIZE;
				//memcpy(msg,qPtr,size-2);
				queueMsg.anyPtr = qPtr;
				queueMsg.type = TO_PHY;
				retCode = osMessageQueuePut(
					queue_phyS_id,
					&queueMsg,
					osPriorityNormal,
					osWaitForever);
				CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);				
				break;
			case DATABACK :
				break;
			case NEW_TOKEN :
				//TODO first
				msg = osMemoryPoolAlloc(memPool,osWaitForever);	
				size = TOKENSIZE;
				msg[0] = 0xFF;
				for(int i = 0; i < 16; i++){
					msg[i+1] = 0x00;
				}
				queueMsg.anyPtr = msg;
				queueMsg.type = TO_PHY;
				retCode = osMessageQueuePut(
					queue_phyS_id,
					&queueMsg,
					osPriorityNormal,
					osWaitForever);
				CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);			
				break;
			case START :
				gTokenInterface.station_list[MYADDRESS] = ((1 << TIME_SAPI) + (1 << CHAT_SAPI));
				break;
			case STOP :
				gTokenInterface.station_list[MYADDRESS] = (1 << TIME_SAPI);
				break;
			case DATA_IND : //From Chat_Sender or Time_Sender ?
				break;
			default :
				break;
		}	//End of Switch case
	}	//End of doomsday
}	//End of file


