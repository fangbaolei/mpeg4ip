
#include "playlist_utils.h"
#include "OS.h"


#if Log_Enabled
	enum {gBuffSize = 532, gBuffMaxStr = 512};
	
	enum {switchEntries, switchTime, switchBytes};
	
	enum {gLogMaxBytes = 100000};
	enum {gLogMaxEntries = 1000};
	enum {gLogMaxMilliSecs = 3 * 60 * 1000};


	static int gLogSwitchSetting = switchEntries;
	static long gLogEntries  = 0;
	static SInt64 gLogTimeStart = 0;
	static long gLogNumBytes = 0;
	static gLogNumPackets = 0;
	static char gTempStr[256];
	static FILE *gLogFile_1 = 0;
	static FILE *gLogFile_2 = 0;
	static FILE *gLogFileCurrent = 0;
	static int gFileNum = 1;
	
	static bool gLogStarted = false;
	static char gLogBuff[gBuffSize];
	static int gBuffUsed = 0;



void	LogFileOpen(void)  
{
	if (!gLogFile_1 && !gLogFile_2)
	{ 	gLogFile_1 = fopen("logfile_1.txt","w");
		gLogFileCurrent = gLogFile_1;
  		*gLogBuff = 0;
   		gLogTimeStart =  PlayListUtils::Milliseconds();
   		gLogStarted = true;
		gLogEntries = 0;
		gLogTimeStart = 0;
		gLogNumBytes = 0;
		gLogNumPackets = 0;
		gFileNum = 1;
		gBuffUsed = 0;
  	}
}

void	LogFileClose(void)
{
	fclose(gLogFile_1);
	fclose(gLogFile_2);
	gLogFileCurrent = gLogFile_1 = gLogFile_2 = 0;
	
}

bool TimeToSwitch(int len)
{
	bool timeToSwitch = false;
	if (!gLogStarted) return timeToSwitch;
	  
	switch( gLogSwitchSetting ) 
	{
		case switchEntries:
			if (gLogEntries >= gLogMaxEntries)
			{	timeToSwitch = true;
				gLogEntries = 1;
			}
			else 
				gLogEntries ++;
		break;
		
		case switchTime:
		{	SInt64 timeNow = PlayListUtils::Milliseconds();
			SInt64 timeThisFile = timeNow - gLogTimeStart;
			if ( timeThisFile >  gLogMaxMilliSecs )
			{	timeToSwitch = true;
				gLogTimeStart = timeNow;
			}
		}
		break;
		
		case switchBytes:
			if (gLogNumBytes > gLogMaxBytes)
			{	timeToSwitch = true;
				gLogNumBytes = 0;
			}
			gLogNumBytes += len;
		break;
	};
	 

	return timeToSwitch;

}

void WriteToLog(void *data, int len)
{
	if (gLogFileCurrent && data && len)
	{
		bool timetoswitch =  TimeToSwitch(len);
		if ( timetoswitch )
		{	
			if (gFileNum == 1)
			{
				if (gLogFile_1) fclose(gLogFile_1);
				gLogFile_1 = 0;
				gLogFile_2 = fopen("logfile_2.txt","w");
				gFileNum = 2;

				gLogFileCurrent = gLogFile_2;
                fseek(gLogFileCurrent , 0, SEEK_SET);
			}
			else
			{	
				if (gLogFile_2) fclose(gLogFile_2);
				gLogFile_2 = 0;
				gLogFile_1 = fopen("logfile_1.txt","w");
				gFileNum = 1;
			
				gLogFileCurrent = gLogFile_1;
                fseek(gLogFileCurrent , 0, SEEK_SET);
			}
		}
		fwrite(data, sizeof(char), len, gLogFileCurrent);
		fflush(gLogFileCurrent);		
	}
}

void WritePacketToLog(void *data, int len)
{
	gLogNumPackets ++;
	LogUInt("Packet:", (UInt32) gLogNumPackets,"\n");
	LogBuffer();
	WriteToLog(data, len);
}

void WritToBuffer(void *data, int len)
{
	if (data )
	{
		if (len >= gBuffSize)
			len = gBuffSize -1;
		memcpy (gLogBuff, (char *) data, len);
		gLogBuff[gBuffSize] = 0;
	}
   gBuffUsed = len;
}

void LogBuffer(void)
{	
	WriteToLog(gLogBuff, strlen(gLogBuff) );
	*gLogBuff =0;
	gBuffUsed = strlen(gLogBuff);
}

void PrintLogBuffer(bool log)
{
	printf(gLogBuff);
	if (log) LogBuffer();
	*gLogBuff =0;
    gBuffUsed = strlen(gLogBuff);
}

void LogNum(char *str1,char *str2,char *str3)
{
	int size = strlen(str1) + strlen(str2) + strlen(str3);
	if ( size < gBuffMaxStr ) 
    	sprintf(gLogBuff, "%s%s%s%s",gLogBuff, str1, str2,str3); 
   
   gBuffUsed = strlen(gLogBuff);
}

void  LogFloat(char *str, float num, char *str2)
{
	sprintf(gTempStr,"%f",num);
	LogNum(str,gTempStr,str2);
}

void LogInt(char *str, long num, char *str2)
{
	sprintf(gTempStr,"%ld",num);
	LogNum(str,gTempStr,str2);
}

void LogUInt (char *str, unsigned long num, char *str2)
{
	sprintf(gTempStr,"%lu",num);
	LogNum(str,gTempStr,str2);
}

void LogStr(char *str)
{
   *gLogBuff = 0;
	int size = strlen(str) + gBuffUsed;
	if ( size <= gBuffMaxStr ) 
   		sprintf(gLogBuff, "%s%s",gLogBuff, str); 
   gBuffUsed = strlen(gLogBuff);
}

#endif


SInt64  PlayListUtils::sInitialMsecOffset = 0;

void PlayListUtils::Initialize()
{
	if (sInitialMsecOffset != 0) return;
	sInitialMsecOffset = OS::Milliseconds(); //Milliseconds uses sInitialMsec to make a 0 offset millisecond so this assignment is valid only once.
	//printf("sInitialMsecOffset =%qd\n",sInitialMsecOffset);
}


PlayListUtils::PlayListUtils()
{
}

UInt32 PlayListUtils::Random()
{
	UInt32 			theRandomNum ;
	theRandomNum = ::rand();
	return theRandomNum;
}
