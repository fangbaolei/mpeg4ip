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
	File:		TimeoutTask.cpp

	Contains:	Implementation of TimeoutTask
					
	
*/

#include "TimeoutTask.h"
#include "OSMemory.h"

TimeoutTaskThread*	TimeoutTask::sThread = NULL;

void TimeoutTask::Initialize()
{
	if (sThread == NULL)
	{
		sThread = NEW TimeoutTaskThread();
		sThread->Signal(Task::kStartEvent);
	}
}

TimeoutTask::TimeoutTask(Task* inTask, SInt64 inTimeoutInMilSecs)
: fTask(inTask), fQueueElem(this)
{
	this->SetTimeout(inTimeoutInMilSecs);
	Assert(fTask != NULL);
	OSMutexLocker locker(&sThread->fMutex);
	sThread->fQueue.EnQueue(&fQueueElem);
}

TimeoutTask::~TimeoutTask()
{
	OSMutexLocker locker(&sThread->fMutex);
	sThread->fQueue.Remove(&fQueueElem);
}

void TimeoutTask::SetTimeout(SInt64 inTimeoutInMilSecs)
{
	fTimeoutInMilSecs = inTimeoutInMilSecs;
	if (inTimeoutInMilSecs == 0)
		fTimeoutAtThisTime = 0;
	else
		fTimeoutAtThisTime = OS::Milliseconds() + fTimeoutInMilSecs;
	Assert(fTimeoutAtThisTime > 0);
}

SInt64 TimeoutTaskThread::Run()
{
	//ok, check for timeouts now. Go through the whole queue
	OSMutexLocker locker(&fMutex);
	SInt64 curTime = OS::Milliseconds();
	
	for (OSQueueIter iter(&fQueue); !iter.IsDone(); iter.Next())
	{
		TimeoutTask* theTimeoutTask = (TimeoutTask*)iter.GetCurrent()->GetEnclosingObject();
		
		//if it's time to time this task out, signal it
		if ((theTimeoutTask->fTimeoutAtThisTime > 0) && (curTime >= theTimeoutTask->fTimeoutAtThisTime))
		{
#if TIMEOUT_DEBUGGING
			printf("TimeoutTask %ld timed out. Curtime = %"_64BITARG_"d, timeout time = %"_64BITARG_"d\n",(SInt32)theTimeoutTask, curTime, theTimeoutTask->fTimeoutAtThisTime);
#endif
			theTimeoutTask->fTask->Signal(Task::kTimeoutEvent);
		}
#if TIMEOUT_DEBUGGING
		else
			printf("TimeoutTask %ld not being timed out. Curtime = %"_64BITARG_"d. timeout time = %"_64BITARG_"d\n", (SInt32)theTimeoutTask, curTime, theTimeoutTask->fTimeoutAtThisTime);
#endif
	}
	(void)this->GetEvents();//we must clear the event mask!
	
	OSThread::ThreadYield();
	
	return kIntervalSeconds * 1000;//don't delete me!
}
