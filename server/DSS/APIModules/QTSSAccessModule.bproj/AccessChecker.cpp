/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999 Apple Computer, Inc.  All Rights Reserved.
 * The contents of this file constitute Original Code as defined in and are 
 * subject to the Apple Public Source License Version 1.1 (the "License").  
 * You may not use this file except in compliance with the License.  Please 
 * obtain a copy of the License at http://www.apple.com/publicsource and 
 * read it before using this file.
 * 
 * This Original Code and all software distributed under the License are 
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER 
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, 
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS 
 * FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the License for 
 * the specific language governing rights and limitations under the 
 * License.
 * 
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
 /*
	File:		AccessChecker.cpp

	Contains:	
					
	Created By: Chris LeCroy
	
	Copyright:	Copyright Apple Computer, Inc. 1999
				All rights reserved

*/



	#include <signal.h>
	
#ifndef __Win32__
    #ifndef __USE_XOPEN
        #define __USE_XOPEN 1
	#endif
	#include <unistd.h>
#endif

#ifdef __solaris__	
	#include <crypt.h>
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "StrPtrLen.h"
#include "StringParser.h"
#include "QTSSModuleUtils.h"
#include "OSFileSource.h"
#include "base64.h"
#include "OSMemory.h"
#include "OSHeaders.h"
#include "AccessChecker.h"


AccessChecker::AccessChecker(const char* inMovieRootDir, const char* inQTAccessFileName, const char* inUsersFilePath, const char* inGroupsFilePath) :
	fRealmHeader(NULL),
	fMovieRootDir(NULL),
	fQTAccessFileName(NULL),
	fGroupsFilePath(NULL),
	fUsersFilePath(NULL),
	fAccessFile(NULL),
	fUsersFile(NULL),
	fGroupsFile(NULL)
{
	Assert(inMovieRootDir != NULL);
	Assert(inQTAccessFileName != NULL);
	Assert(inUsersFilePath != NULL);
	Assert(inGroupsFilePath != NULL);
	
	fMovieRootDir = NEW char[strlen(inMovieRootDir)+1];
	::strcpy(fMovieRootDir, inMovieRootDir);
	
	fQTAccessFileName = NEW char[strlen(inQTAccessFileName)+1];
	::strcpy(fQTAccessFileName, inQTAccessFileName);
	
	fUsersFilePath = NEW char[strlen(inUsersFilePath)+1];
	::strcpy(fUsersFilePath, inUsersFilePath);
	
	fGroupsFilePath = NEW char[strlen(inGroupsFilePath)+1];
	::strcpy(fGroupsFilePath, inGroupsFilePath);
}

AccessChecker::~AccessChecker()
{
	delete[] fMovieRootDir;
	delete[] fQTAccessFileName;
	delete[] fGroupsFilePath;
	delete[] fUsersFilePath;
	delete[] fRealmHeader;
	
	if ( fUsersFile != NULL )
		::fclose(fUsersFile);
		
	if ( fGroupsFile != NULL )
		::fclose(fGroupsFile);
		
	if ( fAccessFile != NULL )
		::fclose(fAccessFile);
}

bool AccessChecker::CheckAccess(const char* inUsername, const char* inPassword)
{
	if ( this->CheckPassword(inUsername, inPassword) &&  // password's cool, check if this guy has dir access
		 this->CheckUserAccess(inUsername) )	
		return true;

	return false;
}


bool AccessChecker::CheckPassword(const char* inUsername, const char* inPassword)
{
	char realPasswd[255];
	
	this->GetPassword(inUsername, realPasswd);

	if ( realPasswd[0] == '\0' ) 
		return false;
	
	return ( ::strcmp(realPasswd, (char*)crypt(inPassword, realPasswd)) == 0 );
}


void AccessChecker::GetPassword(const char* inUsername, char* ioPassword)
{
	const int kBufLen = 1024;
	char buf[kBufLen];
	StrPtrLen bufLine;
		
	ioPassword[0] = '\0';

	if ( fUsersFile == NULL )
		return;
		
	::rewind(fUsersFile);
    while ( ::fgets(buf, kBufLen, fUsersFile) != NULL )
	{
		bufLine.Set(buf, strlen(buf));
		StringParser bufParser(&bufLine);
		
		//skip over leading whitespace
		bufParser.ConsumeUntil(NULL, StringParser::sWhitespaceMask);
			
		//skip over comments and blank lines
		if ((bufParser.GetDataRemaining() == 0) || (bufParser[0] == '#') || (bufParser[0] == '\0') )
	    	continue;
		
		StrPtrLen userName;
		bufParser.ConsumeUntil(&userName, ':');
				
		if (userName.Equal(inUsername) ) 
		{
			StrPtrLen password;
			
			if ( bufParser.Expect(':') )
			{
				bufParser.GetThruEOL(&password);
				strncpy(ioPassword, password.Ptr, password.Len+1);
				ioPassword[password.Len] = '\0';
				break;
			}
		}
	}
}


bool AccessChecker::CheckUserAccess(const char* inUsername)
{
	const int kBufLen = 2048;
    char buf[kBufLen];
	StrPtrLen bufLine;
	
	if ( fAccessFile == NULL )
		return false;
	
	::rewind(fAccessFile);
    while ( ::fgets(buf, kBufLen, fAccessFile) != NULL )
	{
		bufLine.Set(buf, strlen(buf));
		StringParser bufParser(&bufLine);
		
		//skip over leading whitespace
		bufParser.ConsumeUntil(NULL, StringParser::sWhitespaceMask);
		
		//skip over comments and blank lines...
		if ((bufParser.GetDataRemaining() == 0) || (bufParser[0] == '#') || (bufParser[0] == '\0') )
	    	continue;
		
		StrPtrLen word;
		bufParser.ConsumeUntilWhitespace(&word);
		if ( word.Equal("require") )
		{
			bufParser.ConsumeWhitespace();
			bufParser.ConsumeUntilWhitespace(&word);
			
			if ( word.Equal("user") )
			{
				while (word.Len != 0)
				{
					bufParser.ConsumeWhitespace();
					bufParser.ConsumeUntilWhitespace(&word);
				
					if (word.Equal("valid-user") || word.Equal(inUsername)) 
					{
						return true;
					}
				}
			} 
			else if ( word.Equal("group") )
			{
				while (word.Len != 0)
				{
					bufParser.ConsumeWhitespace();
					bufParser.ConsumeUntilWhitespace(&word);
					if ( this->CheckGroupMembership(inUsername, word) )
					{
						return true;
					}
				}
			}
		}
	}
		
	return false;
}

bool AccessChecker::CheckGroupMembership(const char* inUsername, const StrPtrLen& inGroupName)
{	
	const int kBufLen = 2048;
    char buf[kBufLen];
	StrPtrLen bufLine;
	
	if ( fGroupsFile == NULL )
		return false;
		
	::rewind(fGroupsFile);
    while ( ::fgets(buf, kBufLen, fGroupsFile) != NULL )
	{
		bufLine.Set(buf, strlen(buf));
		StringParser bufParser(&bufLine);
		
		//skip over leading whitespace
		bufParser.ConsumeUntil(NULL, StringParser::sWhitespaceMask);
		
		//skip over comments and blank lines...
		if ( (bufParser.GetDataRemaining() == 0) || (bufParser[0] == '#') || (bufParser[0] == '\0') )
	    	continue;
		
		//parse off the groupname
		StrPtrLen groupName;

		bufParser.ConsumeUntil(&groupName, ':');
		if (bufParser.Expect(':'))
		{
			if (groupName.Equal(inGroupName))  //found our inGroupName?
			{
				StrPtrLen userName;

				do //ToDo: this loop could be optimized with a strstr call
				{
					bufParser.ConsumeWhitespace();
					bufParser.ConsumeUntilWhitespace(&userName);
					if (userName.Equal(inUsername)) 
					{
						return true;
					}
				} while (userName.Len != 0);
			}
		}
	}

	return false;
}

bool AccessChecker::GetAccessFile(const char* dirPath)
{
	char* currentDir= NULL;
	char* lastSlash = NULL;
	int movieRootDirLen = ::strlen(fMovieRootDir);
	int maxLen = strlen(dirPath)+strlen(fQTAccessFileName) + strlen(kPathDelimiterString) + 1;
	currentDir = NEW char[maxLen];

	::strcpy(currentDir, dirPath);

	//strip off filename
	lastSlash = ::strrchr(currentDir, kPathDelimiterChar);
	if (lastSlash != NULL)
		lastSlash[0] = '\0';
	
	//check qtaccess files
	
	while ( true )	//walk backward up the dir tree?
	{
		int curLen = strlen(currentDir) + strlen(fQTAccessFileName) + strlen(kPathDelimiterString);

		if ( curLen >= maxLen )
			break;
	
		::strcat(currentDir, kPathDelimiterString);
		::strcat(currentDir, fQTAccessFileName);
	
		fAccessFile = ::fopen(currentDir, "r");
		
		//strip off the "/qtaccess"
		lastSlash = ::strrchr(currentDir, kPathDelimiterChar);
		lastSlash[0] = '\0';
			
		
		if ( fAccessFile != NULL )
		{
			this->GetAccessFileInfo(currentDir);	
			delete[] currentDir;
			return true;
		}
		else
		{	
			//strip of the tailing directory
			lastSlash = ::strrchr(currentDir, kPathDelimiterChar);
			if (lastSlash == NULL)
				break;
			else
				lastSlash[0] = '\0';
		}
		
		if ( (lastSlash-currentDir) < movieRootDirLen )	//bail if we start eating our way out of fMovieRootDir
			break;
	}
	
	delete[] currentDir;
	return false;
}

void AccessChecker::GetAccessFileInfo(const  char* inQTAccessDir)
{
	Assert( fAccessFile != NULL);

	const int kBufLen = 2048;
	char buf[kBufLen];
	StrPtrLen bufLine;
		
	while ( ::fgets(buf, kBufLen, fAccessFile) != NULL )
	{
		bufLine.Set(buf, strlen(buf));
		StringParser bufParser(&bufLine);
		
		//skip over leading whitespace
		bufParser.ConsumeUntil(NULL, StringParser::sWhitespaceMask);
		
		//skip over comments and blank lines...
		
		if ( (bufParser.GetDataRemaining() == 0) || (bufParser[0] == '#') || (bufParser[0] == '\0') )
	    	continue;
			
		StrPtrLen word;
		bufParser.ConsumeUntilWhitespace(&word);
		bufParser.ConsumeWhitespace();

		if ( word.Equal("AuthName") ) //realm name
		{
			bufParser.GetThruEOL(&word);
			
			delete[] fRealmHeader;
			fRealmHeader = NEW char[word.Len+1];
			::memcpy(fRealmHeader, word.Ptr, word.Len);
			fRealmHeader[word.Len] = '\0';
		}
		else if ( word.Equal("AuthUserFile" ) )	//users name
		{
			char filePath[kBufLen];
			bufParser.GetThruEOL(&word); 
			if (word.Ptr[0] == kPathDelimiterChar)	//absolute path
			{
				::memcpy(filePath, word.Ptr, word.Len);
				filePath[word.Len] = '\0';
			}
			else
			{
				::strcpy(filePath, inQTAccessDir);
				::strcat(filePath, kPathDelimiterString);
				::strncat(filePath, word.Ptr, word.Len);
			}
			fUsersFile = ::fopen(filePath, "r");
		}
		else if ( word.Equal("AuthGroupFile") )	//groups name
		{
			char filePath[kBufLen];
			bufParser.GetThruEOL(&word); 
			if (word.Ptr[0] == kPathDelimiterChar)	//absolute path
			{
				::memcpy(filePath, word.Ptr, word.Len);
				filePath[word.Len] = '\0';
			}
			else
			{
				::strcpy(filePath, inQTAccessDir);
				::strcat(filePath, kPathDelimiterString);
				::strncat(filePath, word.Ptr, word.Len);
			}
			fGroupsFile = ::fopen(filePath, "r");
		}
	}
	
			
	if (fUsersFile == NULL)
	{
		fUsersFile = ::fopen(fUsersFilePath, "r");
	}
			
	if (fGroupsFile == NULL)
	{
		fGroupsFile = ::fopen(fGroupsFilePath, "r");
	}
}

