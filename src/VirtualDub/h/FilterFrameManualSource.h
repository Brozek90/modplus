//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2009 Avery Lee
//
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with this program; if not, write to the Free Software
//	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#ifndef f_VD2_FILTERFRAMEMANUALSOURCE_H
#define f_VD2_FILTERFRAMEMANUALSOURCE_H

#include <vd2/system/refcount.h>
#include <vd2/Kasumi/pixmap.h>
#include "FilterInstance.h"
#include "FilterFrameAllocatorProxy.h"
#include "FilterFrameCache.h"
#include "FilterFrameQueue.h"

class VDFilterFrameRequest;

class VDFilterFrameManualSource : public vdrefcounted<IVDFilterFrameSource> {
	VDFilterFrameManualSource(const VDFilterFrameManualSource&);
	VDFilterFrameManualSource& operator=(const VDFilterFrameManualSource&);

public:
	VDFilterFrameManualSource();
	~VDFilterFrameManualSource();

	void *AsInterface(uint32 id);

	VDFilterFrameAllocatorProxy *GetOutputAllocatorProxy();

	void RegisterAllocatorProxies(VDFilterFrameAllocatorManager *mgr, VDFilterFrameAllocatorProxy *prev);
	void SetOutputLayout(const VDPixmapLayout& layout);

	bool CreateRequest(sint64 outputFrame, bool writable, IVDFilterFrameClientRequest **req);
	bool GetDirectMapping(sint64 outputFrame, sint64& sourceFrame, int& sourceIndex);
	sint64 GetSourceFrame(sint64 outputFrame);
	sint64 GetSymbolicFrame(sint64 outputFrame, IVDFilterFrameSource *source);
	sint64 GetNearestUniqueFrame(sint64 outputFrame);
	const VDPixmapLayout& GetOutputLayout() { return mLayout; }
	void InvalidateAllCachedFrames();
	void Stop() {}
	RunResult RunRequests() { return kRunResult_Idle; }

	bool PeekNextRequestFrame(VDPosition& pos);
	bool GetNextRequest(VDFilterFrameRequest **ppReq);
	bool AllocateRequestBuffer(VDFilterFrameRequest *req);
	void CompleteRequest(VDFilterFrameRequest *req, bool cache);

protected:
	virtual bool InitNewRequest(VDFilterFrameRequest *req, sint64 outputFrame, bool writable);

	VDFilterFrameQueue mFrameQueueWaiting;
	VDFilterFrameQueue mFrameQueueInProgress;
	VDFilterFrameCache mFrameCache;
	VDFilterFrameAllocatorProxy mAllocator;

	VDPixmapLayout mLayout;
};

#endif	// f_VD2_FILTERFRAMEMANUALSOURCE_H
