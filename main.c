/* mqdgram - Create and put datagrams onto an IBM MQ queue
 * ----------------------------------------------------------------------------------------------
 * Description:
 * This console program builds datagrams consisting of host system resource information and
 * places it on a local IBM MQ queue (QM1 / DEV.ADAM)
 * ----------------------------------------------------------------------------------------------
 * Date       Author        Description
 * ----------------------------------------------------------------------------------------------
 * 10/01/23   A. Hout       Original source
 * ----------------------------------------------------------------------------------------------
 * 10/05/23   A. Hout       Generate unique msg and corrl ID's for each datagram
 * ----------------------------------------------------------------------------------------------
 * 10/07/23   A. Hout       Update error logic and datagram format
 * ----------------------------------------------------------------------------------------------
 */

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <time.h>
#include <sys/sysinfo.h>
#include <cmqc.h>

#define BUF_SIZE 4096

//Function to build the output datagram
int buildMsg(char *); 

//---------------------------------------------------------------
//Begin mainline processing
//---------------------------------------------------------------
int main(int argc, char **argv)
{
   //Connection literals/variables
   char *pQmg = "QM1";                             //Target queue manager
   char *pQue = "DEV.ADAM";                        //Target queue
   char uid[10];                                   //User ID
   char pwd[10];                                   //User password
   FILE *pFP;                                      //File pointer
   
   //MQI structures
   MQCNO   cnxOpt = {MQCNO_DEFAULT};               //Connection options  
   MQCSP   cnxSec = {MQCSP_DEFAULT};               //Security parameters
   MQOD    objDsc = {MQOD_DEFAULT};                //Object Descriptor
   MQMD    msgDsc = {MQMD_DEFAULT};                //Message Descriptor
   MQPMO   putOpt = {MQPMO_DEFAULT};               //Put message options    
   
   //MQ handles and variables
   MQHCONN  Hcnx;                                  //Connection handle
   MQHOBJ   Hobj;                                  //Object handle 
   MQLONG   opnOpt;                                //MQOPEN options  
   MQLONG   clsOpt;                                //MQCLOSE options 
   MQLONG   cmpCde;                                //MQCONNX completion code 
   MQLONG   opnCde;                                //MQOPEN completion code 
   MQLONG   resCde;                                //Reason code  

   //Output message variables
   char msgBuf[BUF_SIZE];                          //Output message buffer
   int  msgLen;                                    //Length of output msg
   int  msgCnt=0;                                  //Count of datagrams generated
   
   //-------------------------------------------------------
   //Setup MQ connection options/security
   //-------------------------------------------------------
   cnxOpt.SecurityParmsPtr = &cnxSec;
   cnxOpt.Version = MQCNO_VERSION_5;
   cnxSec.AuthenticationType = MQCSP_AUTH_USER_ID_AND_PWD;
   
   pFP = fopen("/home/adam/mqusers","r");
   if (pFP == NULL){
	   fprintf(stderr, "fopen() failed in file %s at line # %d", __FILE__,__LINE__);
	   return -1;
	}
   
	fscanf(pFP,"%s %s",uid,pwd);
	fclose(pFP);
   cnxSec.CSPUserIdPtr = uid;                                            
   cnxSec.CSPUserIdLength = strlen(uid);
   cnxSec.CSPPasswordPtr = pwd;
   cnxSec.CSPPasswordLength = strlen(pwd);
   
   //-------------------------------------------------------
   //Connect to the queue manager; Check for errors/warnings
   //-------------------------------------------------------
   MQCONNX(pQmg,&cnxOpt,&Hcnx,&cmpCde,&resCde);                            //Queue manager = QM1

   if (cmpCde == MQCC_FAILED){
      printf("MQCONNX failed with reason code %d\n",resCde);
      return (int)resCde;
   }
   
   if (cmpCde == MQCC_WARNING){
     printf("MQCONNX generated a warning with reason code %d\n",resCde);
     printf("Continuing...\n");
   }
   
   //-------------------------------------------------------
   //Open the desired message queue for output
   //-------------------------------------------------------
   opnOpt = MQOO_OUTPUT | MQOO_FAIL_IF_QUIESCING;
   strncpy(objDsc.ObjectName,pQue,9);                                      //Queu = DEV.ADAM               
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
   //Build datagram and place it on the queue
   //-------------------------------------------------------
   memcpy(msgDsc.Format,MQFMT_STRING,(size_t)MQ_FORMAT_LENGTH);            //Char string fmt
   putOpt.Options = MQPMO_NO_SYNCPOINT | MQPMO_FAIL_IF_QUIESCING;
   putOpt.Options |= MQPMO_NEW_MSG_ID;                                     //Unique MQMD.MsgId for each datagram
   putOpt.Options |= MQPMO_NEW_CORREL_ID;
   
   while(1){
      //memcpy(msgDsc.MsgId,MQMI_NONE,sizeof(msgDsc.MsgId));
      msgLen = buildMsg(msgBuf);
      if (msgLen < 1)
         break
         
      MQPUT(Hcnx,Hobj,&msgDsc,&putOpt,msgLen,msgBuf,&cmpCde,&resCde);
      if (resCde != MQRC_NONE){
         printf("\nMQPUT ended with reason code %d\n",resCde);
         break;
      }
      printf("\rDatagram %d placed on %s %s",++msgCnt,pQmg,pQue);
      fflush(stdout); 
   }
   
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
          
   printf("\nDisconnected from %s\n",pQue);
   return((int)resCde); 
}

/**********************FUNCTIONS*************************************/

//Build an output datagram comprised of host system resources
int buildMsg(char *buffer){
   FILE *pFP;                                       //For /proc/stat
   int  len = 0;                                    //Output msg length
   char hostName[HOST_NAME_MAX+1];
   long double loadBeg[4], loadEnd[4], loadAvg;     //CPU core arrays
   time_t raw_time;                                 //Time structure
   struct tm *ptr_ts;                               //Tmime structure pointer
   struct sysinfo info;                             //For total / available RAM
   sysinfo(&info);                                  
    
   //Put timestamp into the output buffer
   time(&raw_time);
   ptr_ts = gmtime(&raw_time);
   len = sprintf (buffer,"UTC Time: %02d:%02d:%02d\n",
                    ptr_ts->tm_hour, ptr_ts->tm_min,ptr_ts->tm_sec);
      
   //Put host name into the output buffer
   gethostname(hostName,HOST_NAME_MAX+1);
   len += sprintf(buffer + len,"Host ID : %s\n",hostName);
   
   //Derive CPU utilization 
   pFP = fopen("/proc/stat","r");
   if (pFP == NULL){
	   fprintf(stderr, "fopen() failed in file %s at line # %d", __FILE__,__LINE__);
	   return -1; 
	}
   
   fscanf(pFP,"%*s %Lf %Lf %Lf %Lf",&loadBeg[0],&loadBeg[1],&loadBeg[2],&loadBeg[3]);
   fclose(pFP);
   sleep(1);
   pFP = fopen("/proc/stat","r");
   if (pFP == NULL){
	   fprintf(stderr, "fopen() failed in file %s at line # %d", __FILE__,__LINE__);
	   return -1;
	}
   
   fscanf(pFP,"%*s %Lf %Lf %Lf %Lf",&loadEnd[0],&loadEnd[1],&loadEnd[2],&loadEnd[3]);
   fclose(pFP);
   loadAvg = ((loadEnd[0]+loadEnd[1]+loadEnd[2]) - (loadBeg[0]+loadBeg[1]+loadBeg[2])) 
             / ((loadEnd[0]+loadEnd[1]+loadEnd[2]+loadEnd[3]) - (loadBeg[0]+loadBeg[1]+loadBeg[2]+loadBeg[3]));
   loadAvg *= 100;
   len += sprintf(buffer + len,"CPU Use : %05.2LF%%\n",loadAvg);
   
   //Add avaialble memory to the output message
   len += sprintf(buffer + len,"Free Mem: %ldKB\n",info.freeram/1024);
   len += sprintf(buffer + len,"%% Free  : %.2LF%%\n",
                      (long double)info.freeram / (long double)info.totalram * 100);
   
   return len;
} 
