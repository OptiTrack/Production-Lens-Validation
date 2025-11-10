//======================================================================================================
// Copyright 2008, NaturalPoint Inc.
//======================================================================================================
#pragma once

/************************************************************************
*  Module:       UsbIoReader.h
*  Long name:    CUsbIoReader class
*  Description:  Reads a data stream from a pipe using a thread
*
*  Runtime Env.: Win32, Part of UsbioLib
*  Author(s):    Guenter Hildebrandt, Udo Eberhardt, Mario Guenther
*  Company:      Thesycon GmbH, Ilmenau
************************************************************************/

#include "UsbIoThread.h"

namespace CameraLibrary
{
    // implements a worker-thread that continuously 
    // reads a data stream from a pipe
    class cUsbIoReader : public cUsbIoThread
    {
    public:
        cUsbIoReader();
        virtual ~cUsbIoReader();

    protected:
        // ThreadRoutine, overloaded
        void ThreadRoutine() override;

        // TerminateThread, overloaded 
        void TerminateThread() override;
    };
}
