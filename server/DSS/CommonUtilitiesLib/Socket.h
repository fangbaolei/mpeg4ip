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
	File:		Socket.h

	Contains:	Provides a simple, object oriented socket abstraction, also
				hides the details of socket event handling. Sockets can post
				events (such as S_DATA, S_CONNECTIONCLOSED) to Tasks.
					
	
	
*/

#ifndef __SOCKET_H__
#define __SOCKET_H__

#ifndef __Win32__
#include <netinet/in.h>
#endif

#include "EventContext.h"
#include "ev.h"

class Socket : public EventContext
{
	public:
	
		enum
		{
			// Pass this in on socket constructors to specify whether the
			// socket should be non-blocking or blocking
			kNonBlockingSocketType = 1
		};

		// This class provides a global event thread.
		static void Initialize() { sEventThread = new EventThread(); }
		static void StartThread() { sEventThread->Start(); }
		static EventThread*	GetEventThread() { return sEventThread; }
		
		//Binds the socket to the following address.
		//Returns: QTSS_FileNotOpen, QTSS_NoErr, or POSIX errorcode.
		OS_Error 	Bind(UInt32 addr, UInt16 port);
		//The same. but in reverse
		void			Unbind();	
		
		//Sets the SO_REUSEADDR socket option
		void			ReuseAddr();
		void			SetSocketBufSize(UInt32 inNewSize);
		void			SetSocketRcvBufSize(UInt32 inNewSize);
		
		//Send
		//Returns: QTSS_FileNotOpen, QTSS_NoErr, or POSIX errorcode.
		OS_Error 	Send(const char* inData, const UInt32 inLength, UInt32* outLengthSent);

		//Read
		//Reads some data.
		//Returns: QTSS_FileNotOpen, QTSS_NoErr, or POSIX errorcode.
		OS_Error 	Read(void *buffer, const UInt32 length, UInt32 *rcvLen);
		
		//WriteV: same as send, but takes an iovec
		//Returns: QTSS_FileNotOpen, QTSS_NoErr, or POSIX errorcode.
		OS_Error		WriteV(const struct iovec* iov, const UInt32 numIOvecs, UInt32* outLengthSent);
		
		//You can query for the socket's state
		Bool16	IsConnected() 	{ return fState & kConnected; }
		Bool16	IsBound() 		{ return fState & kBound; }
		
		//If the socket is bound, you may find out to which addr it is bound
		UInt32	GetLocalAddr()	{ return ntohl(fLocalAddr.sin_addr.s_addr); }
		UInt16	GetLocalPort()	{ return ntohs(fLocalAddr.sin_port); }
		
		StrPtrLen*	GetLocalAddrStr();
		StrPtrLen*	GetLocalPortStr();
		StrPtrLen* GetLocalDNSStr();

		enum
		{
			kMaxNumSockets = 4096	//UInt32
		};

	protected:

		//TCPSocket takes an optional task object which will get notified when
		//certain events happen on this socket. Those events are:
		//
		//S_DATA:				Data is currently available on the socket.
		//S_CONNECTIONCLOSING:	Client is closing the connection. No longer necessary
		//						to call Close or Disconnect, Snd & Rcv will fail.
		
		Socket(Task *notifytask, UInt32 inSocketType);
		virtual ~Socket() {}

		//returns QTSS_NoErr, or appropriate posix error
		OS_Error 	Open(int theType);
		
		UInt32			fState;
		
		enum
		{
			kPortBufSizeInBytes = 8,	//UInt32
			kMaxIPAddrSizeInBytes = 20	//UInt32
		};
		
		//address information (available if bound)
		//these are always stored in network order. Conver
		struct sockaddr_in	fLocalAddr;
		struct sockaddr_in	fDestAddr;
		
		StrPtrLen* fLocalAddrStrPtr;
		StrPtrLen* fLocalDNSStrPtr;
		char fPortBuffer[kPortBufSizeInBytes];
		StrPtrLen fPortStr;
		
		//State flags. Be careful when changing these values, as subclasses add their own
		enum
		{
			kBound		= 0x0004,
			kConnected 	= 0x0008
		};
		
		static EventThread*	sEventThread;
		
};

#endif // __SOCKET_H__

