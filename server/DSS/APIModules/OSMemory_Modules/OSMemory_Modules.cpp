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
	File:		OSMemory_Modules.cpp

	Contains:	
					

*/

#include "OSMemory.h"
#include "QTSS.h"

#if MEMORY_DEBUGGING

void* operator new(size_t s, char* /*inFile*/, int /*inLine*/)
{
	return QTSS_New (FOUR_CHARS_TO_INT('0', '0', '0', '0'), s);
}

void* operator new[](size_t s, char* /*inFile*/, int /*inLine*/)
{
	return QTSS_New (FOUR_CHARS_TO_INT('0', '0', '0', '0'), s);
}

#else

void* operator new (size_t s)
{
	return QTSS_New (FOUR_CHARS_TO_INT('0', '0', '0', '0'), s);
}

void* operator new[](size_t s)
{
	return QTSS_New (FOUR_CHARS_TO_INT('0', '0', '0', '0'), s);
}

void operator delete(void* mem)
{
	QTSS_Delete(mem);
}

void operator delete[](void* mem)
{
	QTSS_Delete(mem);
}


#endif //__OS_MEMORY_H__

