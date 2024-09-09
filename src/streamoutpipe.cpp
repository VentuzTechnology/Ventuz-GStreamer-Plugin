
#include "streamoutpipe.h"

#include <string.h>
#include <stdio.h>

#define GST_TYPE_VENTUZ_CLOCK (ventuz_clock_get_type())
#define GST_VENTUZ_CLOCK(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_VENTUZ_CLOCK,VentuzClock))

struct VentuzClockClass
{
    GstSystemClockClass parent_class;
};

G_DEFINE_TYPE(VentuzClock, ventuz_clock, GST_TYPE_SYSTEM_CLOCK);

static GstClockTime ventuz_clock_get_internal_time(GstClock* clock);

static void ventuz_clock_class_init(VentuzClockClass* klass)
{
    GstClockClass* clock_class = (GstClockClass*)klass;

    clock_class->get_internal_time = ventuz_clock_get_internal_time;
}

static void ventuz_clock_init(VentuzClock* clock)
{
    GST_OBJECT_FLAG_SET(clock, GST_CLOCK_FLAG_CAN_SET_MASTER);
}

VentuzClock* ventuz_clock_new(const gchar* name)
{
    VentuzClock* self =
        GST_VENTUZ_CLOCK(g_object_new(GST_TYPE_VENTUZ_CLOCK, "name", name,
            "clock-type", GST_CLOCK_TYPE_OTHER, NULL));

    gst_object_ref_sink(self);

    return self;
}

static GstClockTime ventuz_clock_get_internal_time(GstClock* clock)
{
    return StreamOutPipe::OutputManager::Instance.GetVentuzTime();
}

namespace StreamOutPipe
{
    PipeClient::PipeClient()
    {

    }

    PipeClient::~PipeClient()
    {
        Close();
        delete[] buffer;
    }

    bool PipeClient::Open(int outputNo)
    {
        char pipeName[64];
        sprintf_s(pipeName, "\\\\.\\pipe\\VentuzOut%c", 'A' + outputNo);

        Ensure(1048576);

        for (;;)
        {
            pipe = CreateFileA(pipeName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

            if (pipe != INVALID_HANDLE_VALUE)
                break;

            DWORD err = GetLastError();

            if (err != ERROR_PIPE_BUSY)
            {
                return false;
            }
            if (!WaitNamedPipeA(pipeName, INFINITE))
                return false;
        }

        ChunkHeader chunk;

        if (!ReadStruct(chunk))
            return false;

        if (chunk.fourCC != 'VVSP')
            return false;

        if (!ReadStruct(header))
            return false;

        idrRequested = 0;
        return true;
    }

    void PipeClient::Close()
    {
        if (pipe != INVALID_HANDLE_VALUE)
            CloseHandle(pipe);
        pipe = INVALID_HANDLE_VALUE;
    }

    bool PipeClient::Poll()
    {
        ChunkHeader chunk;

        if (InterlockedExchange(&idrRequested, 0))
        {
            uint8_t cmd = (uint8_t)Command::RequestIDRFrame;

            DWORD written;
            WriteFile(pipe, &cmd, 1, &written, NULL);
        }

        do
        {
            if (!ReadStruct(chunk))
                return false;
        } while (chunk.fourCC != 'fhdr');

        FrameHeader frameHeader;
        if (!ReadStruct(frameHeader))
            return false;

        if (onFrame)
            onFrame(onFrameOpaque, frameHeader.frameIndex, header.videoFrameRateNum, header.videoFrameRateDen);

        if (!ReadStruct(chunk) || chunk.fourCC != 'fvid')
            return false;
        ReadBuffer(chunk.size);
        if (onVideo)
            onVideo(onVideoOpaque, buffer, chunk.size, frameHeader.frameIndex, !!(frameHeader.flags & FrameHeader::IDR_FRAME));

        if (!ReadStruct(chunk) || chunk.fourCC != 'faud')
            return false;
        ReadBuffer(chunk.size);
        if (onAudio)
            onAudio(onAudioOpaque, buffer, chunk.size, frameHeader.frameIndex);

        return true;
    }

    void PipeClient::Ensure(size_t size)
    {
        if (size <= bufferSize)
            return;

        delete[] buffer;
        buffer = new uint8_t[size];
        bufferSize = size;
    }

    template<typename T> bool PipeClient::ReadStruct(T& data)
    {
        DWORD bytesRead = 0;

        int size = sizeof(T);
        BOOL ret = ReadFile(pipe, &data, size, &bytesRead, NULL);

        return (ret && bytesRead == size);
    }

    bool PipeClient::ReadBuffer(size_t size)
    {
        Ensure(size);

        DWORD bytesRead = 0;
        BOOL ret = ReadFile(pipe, buffer, (DWORD)size, &bytesRead, NULL);

        return ret && bytesRead == size;
    }

    OutputManager OutputManager::Instance;

    OutputManager::OutputManager()
    {
        for (int i = 0; i < MAX_OUTS; i++)
        {
            outputs[i].outputNo = i;
            g_mutex_init(&outputs[i].threadLock);
            g_mutex_init(&outputs[i].nodeLock);
            outputs[i].client.SetOnAudio(Output::OnAudioProxy, &outputs[i]);
            outputs[i].client.SetOnVideo(Output::OnVideoProxy, &outputs[i]);
            outputs[i].client.SetOnFrame(OnFrameProxy, this);
        }

        clk = ventuz_clock_new("VentuzOutputClock");
        g_mutex_init(&timeLock);
    }

    OutputManager::~OutputManager()
    {
        for (int i = 0; i < MAX_OUTS; i++)
        {
            g_mutex_clear(&outputs[i].nodeLock);
            g_mutex_clear(&outputs[i].threadLock);
            for (GList* n = outputs[i].nodes; n; n = n->next)
                delete (Callbacks*)n->data;
            g_list_free(outputs[i].nodes);
        }
        g_mutex_clear(&timeLock);
    }

    gpointer OutputManager::Output::ThreadFunc()
    {
        while (!exit)
        {

            if (!client.IsOpen())
            {
                if (client.Open(outputNo))
                {
                    g_mutex_lock(&nodeLock);

                    for (GList* n = nodes; n; n = n->next)
                    {
                        Callbacks* desc = (Callbacks*)n->data;
                        if (desc->onStart) desc->onStart(desc->opaque, client.GetHeader());
                    }

                    g_mutex_unlock(&nodeLock);
                }
                else
                    Sleep(10);
            }
            else
            {
                if (!client.Poll())
                {
                    client.Close();

                    g_mutex_lock(&nodeLock);

                    for (GList* n = nodes; n; n = n->next)
                    {
                        Callbacks* desc = (Callbacks*)n->data;
                        if (desc->onStop) desc->onStop(desc->opaque);
                    }

                    g_mutex_unlock(&nodeLock);
                }
            }
        }

        client.Close();

        return NULL;
    }

    void OutputManager::Output::OnVideo(const uint8_t* data, size_t size, int64_t timecode, bool isIDR)
    {
        g_mutex_lock(&nodeLock);

        for (GList* n = nodes; n; n = n->next)
        {
            Callbacks* desc = (Callbacks*)n->data;
            if (desc->onVideo) desc->onVideo(desc->opaque, data, size, timecode, isIDR);
        }

        g_mutex_unlock(&nodeLock);
    }

    void OutputManager::Output::OnAudio(const uint8_t* data, size_t size, int64_t timecode)
    {
        g_mutex_lock(&nodeLock);

        for (GList* n = nodes; n; n = n->next)
        {
            Callbacks* desc = (Callbacks*)n->data;
            if (desc->onAudio) desc->onAudio(desc->opaque, data, size, timecode);
        }

        g_mutex_unlock(&nodeLock);
    }

    void* OutputManager::Acquire(int output, const Callbacks& desc)
    {
        g_assert(output >= 0 && output < MAX_OUTS);

        Output& out = outputs[output];

        g_mutex_lock(&out.threadLock);

        g_mutex_lock(&out.nodeLock);
        Callbacks* node = new Callbacks(desc);
        out.nodes = g_list_append(out.nodes, node);

        if (out.client.IsOpen() && node->onStart)
            node->onStart(node->opaque, out.client.GetHeader());
            
        g_mutex_unlock(&out.nodeLock);

        if (!out.thread)
        {
            GError* error = nullptr;
            out.thread = g_thread_new("VentuzStreamOut", Output::ThreadProxy, &out);
        }
        g_mutex_unlock(&out.threadLock);

        out.client.RequestIDR();

        return node;
    }

    void OutputManager::Release(int output, void** nodeptr)
    {
        g_assert(output >= 0 && output < MAX_OUTS);
        g_assert(nodeptr);

        Callbacks* node = (Callbacks*)*nodeptr;
        if (!node) return;

        *nodeptr = nullptr;
        Output& out = outputs[output];

        g_mutex_lock(&out.threadLock);
        g_mutex_lock(&out.nodeLock);

        out.nodes = g_list_remove(out.nodes, node);
        delete node;

        bool join = !out.nodes;
        g_mutex_unlock(&out.nodeLock);

        if (join)
        {
            out.exit = true;
            g_thread_join(out.thread);
            out.thread = nullptr;
            out.exit = false;
        }

        g_mutex_unlock(&out.threadLock);
    }

    void OutputManager::OnFrame(int64_t timeCode, int frNum, int frDen)
    {
        g_mutex_lock(&timeLock);

        int64_t delta = timeCode - lastTimeCode;
        if (delta<-100 || delta>0)
        {
            dur = GST_SECOND * frDen / frNum;
            time += dur;
            lastTimeCode = timeCode;
        }

        g_mutex_unlock(&timeLock);
    }

    GstClockTimeDiff OutputManager::GetTimeDiff(int64_t timeCode)
    {
        g_mutex_lock(&timeLock);
        GstClockTimeDiff diff = dur * (timeCode - lastTimeCode);
        g_mutex_unlock(&timeLock);
        return diff;
    }
}

