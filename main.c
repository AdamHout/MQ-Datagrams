/*
 * MQDGREC - Demo program to recieve datagrams from a local queue
 * ---------------------------------------------------------------------------------------------
 * Description:
 * This console program receives datagrams consisting of host system resource information from 
 * a local IBM MQ queue (QM1 / DEV.ADAM)
 * ----------------------------------------------------------------------------------------------
 * Date       Author        Description
 * ----------------------------------------------------------------------------------------------
 * 10/01/23   A. Hout       Original source
 * ----------------------------------------------------------------------------------------------
 * 10/05/23   A. Hout       Add MQMD.MsgId to console output
 * ----------------------------------------------------------------------------------------------
 */

#include <stdio.h>
#include <string.h>
#include <cmqc.h>

#define BUF_SIZE 4096

int main(int argc, char **argv)
{
	//Connection literals
   char *pQmg = "QM1";                             //Target queue manager
   char *pQue = "DEV.ADAM";                        //Target queue
   char *pUID = "app";                             //MQ client user ID
   char *pPwd = "Vn750a16!";                       //Client password
   char msgBuf[BUF_SIZE];                          //Output message buffer
   int  ctr;                                       //Index counter
   
   //MQI structures
   MQCNO   cnxOpt = {MQCNO_DEFAULT};               //Connection options  
   MQCSP   cnxSec = {MQCSP_DEFAULT};               //Security parameters
   MQOD    objDsc = {MQOD_DEFAULT};                //Object Descriptor
   MQMD    msgDsc = {MQMD_DEFAULT};                //Message Descriptor
   MQGMO   getOpt = {MQGMO_DEFAULT};               //Get message options
   
   //MQ handles and variables
   MQHCONN  Hcnx;                                  //Connection handle
   MQHOBJ   Hobj;                                  //Object handle 
   MQLONG   opnOpt;                                //MQOPEN options  
   MQLONG   clsOpt;                                //MQCLOSE options 
   MQLONG   cmpCde;                                //MQCONNX completion code 
   MQLONG   opnCde;                                //MQOPEN completion code 
   MQLONG   msgLen;                                //Message length received
   MQLONG   resCde;                                //Reason code 
   
   //-------------------------------------------------------
   //Setup MQ connection options/security
   //-------------------------------------------------------
   cnxOpt.SecurityParmsPtr = &cnxSec;
   cnxOpt.Version = MQCNO_VERSION_5;
   cnxSec.AuthenticationType = MQCSP_AUTH_USER_ID_AND_PWD;
   cnxSec.CSPUserIdPtr = pUID;                                                //ID = "app"
   cnxSec.CSPUserIdLength = 3;
   cnxSec.CSPPasswordPtr = pPwd;
   cnxSec.CSPPasswordLength = 9;
   
   //-------------------------------------------------------
   //Connect to the queue manager; Check for errors/warnings
   //-------------------------------------------------------
   MQCONNX(pQmg,&cnxOpt,&Hcnx,&cmpCde,&resCde);                              //Queue manager = QM1

   if (cmpCde == MQCC_FAILED){
      printf("MQCONNX failed with reason code %d\n",resCde);
      return((int)resCde);
   }
   
   if (cmpCde == MQCC_WARNING){
     printf("MQCONNX generated a warning with reason code %d\n",resCde);
     printf("Continuing...\n");
   }
   
   //-------------------------------------------------------
   //Open the desired message queue for input
   //-------------------------------------------------------
   opnOpt = MQOO_INPUT_AS_Q_DEF | MQOO_FAIL_IF_QUIESCING;
   strncpy(objDsc.ObjectName,pQue,9);                                         //Queue = DEV.ADAM               
   MQOPEN(Hcnx,&objDsc,opnOpt,&Hobj,&opnCde,&resCde);
          
   if (resCde != MQRC_NONE)
      printf("MQOPEN ended with reason code %d\n",resCde);

   if (opnCde == MQCC_FAILED){
      printf("unable to open %s queue for output\n",pQue);
      printf("Disconnecting from %s and exiting\n",pQmg);
      printf("Press enter to continue\n");
      getchar();
      MQDISC(&Hcnx,&cmpCde,&resCde);
      return((int)opnCde);
   }
   
   //-------------------------------------------------------
   //Pull datagrams off of QM1 DEV.ADAM and display them 
   //on the console. Terminate after 10 seconds of inactivty
   //-------------------------------------------------------
   //getOpt.Version = MQGMO_VERSION_2;                                                          
   getOpt.Version = MQGMO_CURRENT_VERSION;                                    //Use the most recent version of MQGMO
   getOpt.MatchOptions = MQMO_NONE;                                           //FIFO retreival                                            
   getOpt.Options = MQGMO_WAIT | MQGMO_NO_SYNCPOINT;   
   getOpt.WaitInterval = 10000;                                               //Wait up to 10 seconds         
    
   do{                                                                        //Loop until no more messages
     msgDsc.Encoding = MQENC_NATIVE;                                         
     msgDsc.CodedCharSetId = MQCCSI_Q_MGR;
     MQGET(Hcnx,Hobj,&msgDsc,&getOpt,BUF_SIZE,msgBuf,&msgLen,&cmpCde,&resCde);
   
     if (resCde != MQRC_NONE){
        switch(resCde){
           case MQRC_NO_MSG_AVAILABLE:
              puts("No messages on the queue");
              break;
           case MQRC_TRUNCATED_MSG_FAILED:
              puts("Message truncated");
              break;
           default:
              printf("MQGET reason code: %d\n",resCde);
        }
     }
     else{  
        printf("Msg ID: ");                                                      //Add MQMD.MsgId to console output
        for(ctr=0; ctr<MQ_MSG_ID_LENGTH; ctr++)
           printf("%02X",msgDsc.MsgId[ctr]);
        printf("\n");
        msgBuf[msgLen] = '\0';
        printf("%s\n",msgBuf); 
     }
   }while(cmpCde != MQCC_FAILED && resCde == MQRC_NONE);
   
   //-------------------------------------------------------
   //Close the queue connection
   //-------------------------------------------------------
   clsOpt = MQCO_NONE;
   MQCLOSE(Hcnx,&Hobj,clsOpt,&cmpCde,&resCde);

   if (resCde != MQRC_NONE)
      printf("MQCLOSE ended with reason code %d\n",resCde);
     
   //Disconnect from the queue manager
   MQDISC(&Hcnx,&cmpCde,&resCde);

   if (resCde != MQRC_NONE)
      printf("MQDISC ended with reason code %d\n",resCde);
          
   return (int)resCde; 
}
