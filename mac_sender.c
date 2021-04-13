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

#define MESSAGE_BUFFER_SIZE	10

//////////////////////////////////////////////////////////////////////////////////
// THREAD MAC RECEIVER
//////////////////////////////////////////////////////////////////////////////////
void MacSender(void *argument)
{
	struct queueMsg_t* saveToken;
	struct queueMsg_t* saveMsg;
	struct queueMsg_t queueMsg;
	struct queueMsg_t queueMsgLCD;
	osStatus_t retCode;
	uint8_t* qPtr;
	uint8_t* msg;
	
	osMessageQueueId_t msgBuffer;
	msgBuffer = osMessageQueueNew(MESSAGE_BUFFER_SIZE, sizeof(struct queueMsg_t), NULL);
	
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
				queueMsg.type = TO_PHY;	//Update token destination
				queueMsg.anyPtr = qPtr;	//Update token data
				
				queueMsgLCD.type = TOKEN_LIST;	//Pour le LCD
				queueMsgLCD.anyPtr = NULL;
				retCode = osMessageQueuePut(
					queue_lcd_id,
					&queueMsgLCD,
					osPriorityNormal,
					osWaitForever);
				CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);

				if(osMessageQueueGetCount(msgBuffer) != 0){	//Si on a des messages à envoyer
					saveToken = &queueMsg;				//Save le token
					retCode = osMessageQueueGet(	//Prend le message à envoyer
						msgBuffer,
						&queueMsg,
						NULL,
						osWaitForever);
					CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
					
					saveMsg = &queueMsg;	//Save the address of the message we send for DATABACK
					
					retCode = osMessageQueuePut(	//Envoie le message au PHY_SENDER
						queue_phyS_id,
						&queueMsg,
						osPriorityNormal,
						osWaitForever);
					CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);	
				}
				else{	//Si la queue des messages est vide
					retCode = osMessageQueuePut(	//Send le token
						queue_phyS_id,
						&queueMsg,
						osPriorityNormal,
						osWaitForever);
					CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);	
				}
				
				break;
			case DATABACK : //Quand on a envoyé un message, on va le recevoir en retour
				uint8_t ack = qPtr[3+qPtr[3]]&0x01;
				uint8_t read = qPtr[3+qPtr[3]]&0x02;
				if(read != 0){		//Tester si le Read bit est à 1
					if(ack != 0){		//Tester si le Ack bit est à 1
						//Send token
						retCode = osMessageQueuePut(	
							queue_phyS_id,
							saveToken,
							osPriorityNormal,
							osWaitForever);
						CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);

						//Free memory msg databack
						retCode = osMemoryPoolFree(memPool, &queueMsg);				//FREE DATABACK
						CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
						
						//Free memory msg sent
						retCode = osMemoryPoolFree(memPool, saveMsg);				//FREE DATABACK
						CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);					
					}
					else{
						qPtr[3+qPtr[2]] = (qPtr[3+qPtr[2]]&0xFC); //Read=0, Ack=0
						//TODO : Send data
					}
				}
				else {
					//TODO : Send MAC_ERROR 
					
					//Send token
					retCode = osMessageQueuePut(	
						queue_phyS_id,
						saveToken,
						osPriorityNormal,
						osWaitForever);
					CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);	
				}
				break;
			case NEW_TOKEN :
				//TODO first
				msg = osMemoryPoolAlloc(memPool,osWaitForever);		//MALLOC for the new token
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
			case DATA_IND : 
				//queueMessage contient :
				//type : DATA_ind
				//qPtr : pointeur sur le string à envoyer
				//addr : adresse de destination du message
				//src	 : Src SAPI
				msg = osMemoryPoolAlloc(memPool, osWaitForever);	//MALLOC msg to send
				msg[0] = (MYADDRESS << 3) + queueMsg.sapi;			//Control 1/2
				msg[1] = (queueMsg.addr << 3) + queueMsg.sapi;	//Control 2/2
				msg[2] = strlen(qPtr);		//Message's length		//Length
				memcpy(&(msg[3]), qPtr, msg[2]);								//User Data
			
				uint32_t check = qPtr[0]+qPtr[1]+qPtr[2];
				for(int i=0;i<qPtr[2];i++) {
					check += qPtr[3+i];
				}		
				msg[3+qPtr[2]] = (check << 2);									//Status [Checksum, Read, Ack]
				
				retCode = osMessageQueuePut(
					msgBuffer,
					msg,	
					osPriorityNormal,
					osWaitForever);
				CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
				
				retCode = osMemoryPoolFree(memPool, &queueMsg);				//FREE DATA_IND
				CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
				break;
			default :
				break;
		}	//End of Switch case
	}	//End of doomsday
}	//End of file


