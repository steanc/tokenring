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
#define MAX_ERROR 3

//////////////////////////////////////////////////////////////////////////////////
// THREAD MAC RECEIVER
//////////////////////////////////////////////////////////////////////////////////
void MacSender(void *argument)
{
	uint8_t * saveToken;
	uint8_t * saveMsg;
	
	
	struct queueMsg_t queueMsg;
	struct queueMsg_t queueMsgLCD;
	
	osStatus_t retCode;
	uint8_t* qPtr;
	uint8_t* msg;
	
	uint8_t error_counter = 0;
	
	osMessageQueueId_t msgBuffer;
	msgBuffer = osMessageQueueNew(MESSAGE_BUFFER_SIZE, sizeof(struct queueMsg_t), NULL);
	
	//init sapi
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
				
				queueMsgLCD.type = TOKEN_LIST;	//For the LCD
				queueMsgLCD.anyPtr = NULL;
				retCode = osMessageQueuePut(
					queue_lcd_id,
					&queueMsgLCD,
					osPriorityNormal,
					osWaitForever);
				CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);

				if(osMessageQueueGetCount(msgBuffer) != 0){	//If we have msg to send
					saveToken = qPtr;				//Save the token
					retCode = osMessageQueueGet(	//Take the msg from waiting queue
						msgBuffer,
						&queueMsg,
						NULL,
						osWaitForever);
					CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
					
					saveMsg = queueMsg.anyPtr;	//Save the address of the data for DATABACK
					
					queueMsg.anyPtr = osMemoryPoolAlloc(memPool, osWaitForever); //send a copy and keep the original
					memcpy(queueMsg.anyPtr, saveMsg, saveMsg[2]+4);
					
					retCode = osMessageQueuePut(	//Send the msg to PHY_SENDER
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
			case DATABACK : //The message we sent is coming back
				uint8_t ack = qPtr[3+qPtr[2]]&0x01;
				uint8_t read = qPtr[3+qPtr[2]]&0x02;
				if(read != 0){		//Test if Read bit is 1
					if(ack != 0){		//Test if Ack bit is 1
						error_counter = 0;
						
						//Free memory msg databack
						retCode = osMemoryPoolFree(memPool, queueMsg.anyPtr);				//FREE DATABACK
						CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
						
						//Free memory msg sent
						retCode = osMemoryPoolFree(memPool, saveMsg);				//FREE DATABACK
						CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
						
						//Send token
						queueMsg.type = TO_PHY;
						queueMsg.anyPtr = saveToken;
						retCode = osMessageQueuePut(	
							queue_phyS_id,
							&queueMsg,
							osPriorityNormal,
							osWaitForever);
						CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);											
					}
					else{
						error_counter++;
						
						if(error_counter < MAX_ERROR) { //send msg again
							//qPtr[3+qPtr[2]] = (qPtr[3+qPtr[2]]&0xFC); //reset : Read=0, Ack=0
							memcpy(queueMsg.anyPtr, saveMsg, saveMsg[2]+4); // reset msg
							queueMsg.type = TO_PHY;
							retCode = osMessageQueuePut(	//Send the msg to PHY_SENDER
								queue_phyS_id,
								&queueMsg,
								osPriorityNormal,
								osWaitForever);
							CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
						}
						else { //abort msg
							error_counter = 0;
							
							struct queueMsg_t mac_error;
							mac_error.type = MAC_ERROR;
							mac_error.anyPtr = osMemoryPoolAlloc(memPool, osWaitForever);
							sprintf(mac_error.anyPtr, "Address %d : crc error.\n\r", (saveMsg[1]>>3)+1);
							mac_error.addr = queueMsg.addr;
							
							//send mac error
							retCode = osMessageQueuePut(	
								queue_lcd_id,
								&mac_error,
								osPriorityNormal,
								osWaitForever);
							CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
							
							//Free memory msg databack
							retCode = osMemoryPoolFree(memPool, queueMsg.anyPtr);
							CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
							
							//Free memory msg saved
							retCode = osMemoryPoolFree(memPool, saveMsg);
							CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
							
							//Send token
							queueMsg.type = TO_PHY;
							queueMsg.anyPtr = saveToken;
							retCode = osMessageQueuePut(	
								queue_phyS_id,
								&queueMsg,
								osPriorityNormal,
								osWaitForever);
							CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
							
						}						
					}
				}
				else {//Send MAC_ERROR
					error_counter = 0;
					
					struct queueMsg_t mac_error;
					mac_error.type = MAC_ERROR;
					mac_error.anyPtr = osMemoryPoolAlloc(memPool, osWaitForever);
					sprintf(mac_error.anyPtr, "Address %d not connected.\n\r", (saveMsg[1]>>3)+1);
					mac_error.addr = queueMsg.addr;
					
					//Free memory msg databack
					retCode = osMemoryPoolFree(memPool, queueMsg.anyPtr);
					CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
					
					//Free memory msg saved
					retCode = osMemoryPoolFree(memPool, saveMsg);
					CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
					
					//send mac error
					retCode = osMessageQueuePut(	
						queue_lcd_id,
						&mac_error,
						osPriorityNormal,
						osWaitForever);
					CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
					
					//Send token
					queueMsg.type = TO_PHY;
					queueMsg.anyPtr = saveToken;
					retCode = osMessageQueuePut(	
						queue_phyS_id,
						&queueMsg,
						osPriorityNormal,
						osWaitForever);
					CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);	
				}
				break;
				
			case NEW_TOKEN : //creat a new token
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
			
				uint32_t check = msg[0]+msg[1]+msg[2];
				for(int i=0;i<msg[2];i++) {
					check += msg[3+i];
				}
				
				msg[3+msg[2]] = (check << 2);									//Status [Checksum, Read, Ack]
				
				
				
				retCode = osMemoryPoolFree(memPool, queueMsg.anyPtr);				//FREE DATA_IND
				CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
				
				queueMsg.type = TO_PHY;
				queueMsg.anyPtr = msg;
				
				retCode = osMessageQueuePut(
					msgBuffer,
					&queueMsg,	
					osPriorityNormal,
					osWaitForever);
				CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
				
				break;
			default :
				break;
		}	//End of Switch case
	}	//End of doomsday
}	//End of file


