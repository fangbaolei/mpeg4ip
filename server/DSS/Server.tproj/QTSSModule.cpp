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
	File:		QTSSModule.cpp

	Contains:	Implements object defined in QTSSModule.h
					
	
*/

#include <errno.h>

#include "QTSSModule.h"
#include "OSArrayObjectDeleter.h"
#include "OSMemory.h"
#include "StringParser.h"
#include "Socket.h"

Bool16	QTSSModule::sHasRTSPRequestModule = false;
Bool16	QTSSModule::sHasOpenFileModule = false;

QTSSAttrInfoDict::AttrInfo	QTSSModule::sAttributes[] =
{   /*fields:   fAttrName, fFuncPtr, fAttrDataType, fAttrPermission */
	/* 0 */ { "qtssModName",			NULL,					qtssAttrDataTypeCharArray,	qtssAttrModeRead | qtssAttrModePreempSafe },
	/* 1 */ { "qtssModDesc",			NULL,					qtssAttrDataTypeCharArray,	qtssAttrModeRead | qtssAttrModeWrite },
	/* 2 */ { "qtssModVersion",			NULL,					qtssAttrDataTypeUInt32,		qtssAttrModeRead | qtssAttrModeWrite },
	/* 3 */ { "qtssModRoles",			NULL,					qtssAttrDataTypeUInt32,		qtssAttrModeRead | qtssAttrModePreempSafe },
	/* 4 */ { "qtssModPrefs",			NULL,					qtssAttrDataTypeQTSS_Object,qtssAttrModeRead | qtssAttrModePreempSafe },
};

void QTSSModule::Initialize()
{
	//Setup all the dictionary stuff
	for (UInt32 x = 0; x < qtssModNumParams; x++)
		QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kModuleDictIndex)->
			SetAttribute(x, sAttributes[x].fAttrName, sAttributes[x].fFuncPtr,
							sAttributes[x].fAttrDataType, sAttributes[x].fAttrPermission);
}

QTSSModule::QTSSModule(char* inName, char* inPath)
: 	QTSSDictionary(QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kModuleDictIndex)),
	fQueueElem(this),
	fPath(NULL),
	fFragment(NULL),
	fDispatchFunc(NULL),
	fPrefs(NULL)
{
	if ((inPath != NULL) && (inPath[0] != '\0'))
	{
		// Create a code fragment if this module is being loaded from disk
		
		fFragment = NEW OSCodeFragment(inPath);
		fPath = NEW char[::strlen(inPath) + 2];
		::strcpy(fPath, inPath);
	}
	
	this->SetVal(qtssModPrefs, 		&fPrefs, 			sizeof(fPrefs));

	// If there is a name, copy it into the module object's internal buffer
	if (inName != NULL)
		this->SetValue(qtssModName, 0, inName, ::strlen(inName), QTSSDictionary::kDontObeyReadOnly);
				
	::memset(fRoleArray, 0, sizeof(fRoleArray));
}

QTSS_Error 	QTSSModule::SetupModule(QTSS_CallbacksPtr inCallbacks, QTSS_MainEntryPointPtr inEntrypoint)
{
	QTSS_Error theErr = QTSS_NoErr;
	
	// Load fragment from disk if necessary
	
	if ((fFragment != NULL) && (inEntrypoint == NULL))
		theErr = this->LoadFromDisk(&inEntrypoint);
	if (theErr != QTSS_NoErr)
		return theErr;
		
	// At this point, we must have an entrypoint
	if (inEntrypoint == NULL)
		return QTSS_NotAModule;
		
	// Invoke the private initialization routine
	QTSS_PrivateArgs thePrivateArgs;
	thePrivateArgs.inServerAPIVersion = QTSS_API_VERSION;
	thePrivateArgs.inCallbacks = inCallbacks;
	thePrivateArgs.outStubLibraryVersion = 0;
	thePrivateArgs.outDispatchFunction = NULL;
	
	theErr = (inEntrypoint)(&thePrivateArgs);
	if (theErr != QTSS_NoErr)
		return theErr;
		
	if (thePrivateArgs.outStubLibraryVersion != thePrivateArgs.inServerAPIVersion)
		return QTSS_WrongVersion;
	
	// Set the dispatch function so we'll be able to invoke this module later on
	
	fDispatchFunc = thePrivateArgs.outDispatchFunction;
	return QTSS_NoErr;
}

QTSS_Error QTSSModule::LoadFromDisk(QTSS_MainEntryPointPtr* outEntrypoint)
{
	static StrPtrLen sMainEntrypointName("_Main");
	
	Assert(outEntrypoint != NULL);
	
	// Modules only need to be initialized if they reside on disk. 
	if (fFragment == NULL)
		return QTSS_NoErr;
	
	if (!fFragment->IsValid())
		return QTSS_NotAModule;
		
	// fPath is actually a path. Extract the file name.
	
	StrPtrLen theFileName(fPath);
	StringParser thePathParser(&theFileName);
	
	while (thePathParser.GetThru(&theFileName, kPathDelimiterChar))
		;
	Assert(theFileName.Len > 0);
	Assert(theFileName.Ptr != NULL);

#ifdef __Win32__
	StringParser theDLLTruncator(&theFileName);
	theDLLTruncator.ConsumeUntil(&theFileName, '.'); // strip off the ".DLL"
#endif
	
	// At this point, theFileName points to the file name. Make this the module name.
	this->SetValue(qtssModName, 0, theFileName.Ptr, theFileName.Len, QTSSDictionary::kDontObeyReadOnly);
	
	// 
	// The main entrypoint symbol name is the file name plus that _Main__ string up there.
	OSCharArrayDeleter theSymbolName(NEW char[theFileName.Len + sMainEntrypointName.Len + 2]);
	::memcpy(theSymbolName, theFileName.Ptr, theFileName.Len);
	theSymbolName[theFileName.Len] = '\0';
	
	::strcat(theSymbolName, sMainEntrypointName.Ptr);
	*outEntrypoint = (QTSS_MainEntryPointPtr)fFragment->GetSymbol(theSymbolName.GetObject());
	return QTSS_NoErr;
}

QTSS_Error	QTSSModule::AddRole(QTSS_Role inRole)
{
	// There can only be one QTSS_RTSPRequest processing module
	if ((inRole == QTSS_RTSPRequest_Role) && (sHasRTSPRequestModule))
		return QTSS_RequestFailed;
	if ((inRole == QTSS_OpenFilePreProcess_Role) && (sHasOpenFileModule))
		return QTSS_RequestFailed;
	
	switch (inRole)
	{
		// Map actual QTSS Role names to our private enum values. Turn on the proper one
		// in the role array
		case QTSS_Initialize_Role:			fRoleArray[kInitializeRole] = true;			break;
		case QTSS_Shutdown_Role:			fRoleArray[kShutdownRole] = true;			break;
		case QTSS_RTSPFilter_Role:			fRoleArray[kRTSPFilterRole] = true;			break;
		case QTSS_RTSPRoute_Role:			fRoleArray[kRTSPRouteRole] = true;			break;
		case QTSS_RTSPAuthorize_Role:		fRoleArray[kRTSPAuthRole] = true;			break;
		case QTSS_RTSPPreProcessor_Role:	fRoleArray[kRTSPPreProcessorRole] = true;	break;
		case QTSS_RTSPRequest_Role:			fRoleArray[kRTSPRequestRole] = true;		break;
		case QTSS_RTSPPostProcessor_Role:	fRoleArray[kRTSPPostProcessorRole] = true;	break;
		case QTSS_RTSPSessionClosing_Role:	fRoleArray[kRTSPSessionClosingRole] = true;	break;
		case QTSS_RTPSendPackets_Role:		fRoleArray[kRTPSendPacketsRole] = true;	 	break;
		case QTSS_ClientSessionClosing_Role:fRoleArray[kClientSessionClosingRole] = true;break;
		case QTSS_RTCPProcess_Role:			fRoleArray[kRTCPProcessRole] = true;	 	break;
		case QTSS_ErrorLog_Role:			fRoleArray[kErrorLogRole] = true;			break;
		case QTSS_RereadPrefs_Role:			fRoleArray[kRereadPrefsRole] = true;		break;
		case QTSS_OpenFile_Role:			fRoleArray[kOpenFileRole] = true;			break;
		case QTSS_OpenFilePreProcess_Role:	fRoleArray[kOpenFilePreProcessRole] = true;	break;
		case QTSS_AdviseFile_Role:			fRoleArray[kAdviseFileRole] = true;			break;
		case QTSS_ReadFile_Role:			fRoleArray[kReadFileRole] = true;			break;
		case QTSS_CloseFile_Role:			fRoleArray[kCloseFileRole] = true;			break;
		case QTSS_RequestEventFile_Role:	fRoleArray[kRequestEventFileRole] = true;	break;
		case QTSS_RTSPIncomingData_Role:	fRoleArray[kRTSPIncomingDataRole] = true;	break;
		default:
			return QTSS_BadArgument;
	}
	
	if (inRole == QTSS_RTSPRequest_Role)
		sHasRTSPRequestModule = true;
	if (inRole == QTSS_OpenFile_Role)
		sHasOpenFileModule = true;
		
	//
	// Add this role to the array of roles attribute
	QTSS_Error theErr = this->SetValue(qtssModRoles, this->GetNumValues(qtssModRoles), &inRole, sizeof(inRole), QTSSDictionary::kDontObeyReadOnly);
	Assert(theErr == QTSS_NoErr);
	
	return QTSS_NoErr;
}

