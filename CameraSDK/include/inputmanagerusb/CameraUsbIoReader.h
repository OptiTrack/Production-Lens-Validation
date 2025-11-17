//======================================================================================================
// Copyright 2008, NaturalPoint Inc.
//======================================================================================================
#pragma once

#include "UsbIoReader.h"
#include "inputmanagerusb.h"

namespace CameraLibrary
{
    class cCameraUSBIOReader : public cUsbIoReader
    {
    public:
        cCameraUSBIOReader();
        ~cCameraUSBIOReader() = default;

        int  EndPoint() { return mEndPoint; };

        void SetListener( int EndPoint, cUSBListener* Listener );
        void SetRead( bool Enable ) { mRun = Enable; }

    protected:
        void ProcessData( cUsbIoBuf* Buf ) override;
        void ProcessBuffer( cUsbIoBuf* Buf ) override;

        void ThreadRoutine() override;

    private:
        int mEndPoint;
        cUSBListener* mListener;
        bool mRun;
    };
}
