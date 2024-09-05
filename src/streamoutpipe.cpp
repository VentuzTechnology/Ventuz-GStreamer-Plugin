
#include "streamoutpipe.h"

#include <string.h>
#include <stdio.h>

namespace StreamOutPipe
{


    Client::Client()
    {

    }

    Client::~Client()
    {     
        Close();
        delete[] buffer;
    }

    bool Client::Open(int outputNo)
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

        audioTc = -1;
        return true;
    }

    void Client::Close()
    {
        if (pipe != INVALID_HANDLE_VALUE)
            CloseHandle(pipe);
        pipe = INVALID_HANDLE_VALUE;
    }

    bool Client::Poll()
    {
        ChunkHeader chunk;

        do
        {
            if (!ReadStruct(chunk))
                return false;
        } while (chunk.fourCC != 'fhdr');

        FrameHeader frameHeader;
        if (!ReadStruct(frameHeader))
            return false;

        if (!ReadStruct(chunk) || chunk.fourCC != 'fvid')
            return false;
        ReadBuffer(chunk.size);
        if (onVideo)
            onVideo(onVideoOpaque, buffer, chunk.size, frameHeader.frameIndex, !!(frameHeader.flags & FrameHeader::IDR_FRAME) );

        if (!ReadStruct(chunk) || chunk.fourCC != 'faud')
            return false;
        ReadBuffer(chunk.size);
        if (audioTc < 0 || frameHeader.frameIndex != lastFrameIndex + 1)
            audioTc = (int64_t)frameHeader.frameIndex * header.audioRate * header.videoFrameRateDen / header.videoFrameRateNum;
            if (onAudio)
                onAudio(onAudioOpaque, buffer, chunk.size, audioTc);

            audioTc += (int64_t)chunk.size / (2ll * header.audioChannels);
            lastFrameIndex = frameHeader.frameIndex;

            return true;
    }

    void Client::Ensure(size_t size)
    {
        if (size <= bufferSize)
            return;

        delete[] buffer;
        buffer = new uint8_t[size];
        bufferSize = size;
    }

    template<typename T> bool Client::ReadStruct(T& data)
    {
        DWORD bytesRead = 0;

        int size = sizeof(T);
        BOOL ret = ReadFile(pipe, &data, size, &bytesRead, NULL);

        return (ret && bytesRead == size);
    }

    bool Client::ReadBuffer(size_t size)
    {
        Ensure(size);

        DWORD bytesRead = 0;
        BOOL ret = ReadFile(pipe, buffer, (DWORD)size, &bytesRead, NULL);

        return ret && bytesRead == size;
    }

    Manager Manager::Instance;

    Manager::Manager()
    {
        for (int i = 0; i < MAX_OUTS; i++)
        {
            outputs[i].outputNo = i;
            g_mutex_init(&outputs[i].threadLock);
            g_mutex_init(&outputs[i].nodeLock);
            outputs[i].client.SetOnAudio(Output::OnAudioProxy, &outputs[i]);
            outputs[i].client.SetOnVideo(Output::OnVideoProxy, &outputs[i]);
        }
    }

    Manager::~Manager()
    {
        for (int i = 0; i < MAX_OUTS; i++)
        {
            g_mutex_clear(&outputs[i].nodeLock);
            g_mutex_clear(&outputs[i].threadLock);
            g_list_free(outputs[i].nodes);
        }
    }

    gpointer Manager::Output::ThreadFunc()
    {
        while (!exit)
        {

            if (!client.IsOpen())
            {
                if (client.Open(outputNo))
                {                    
                    g_mutex_lock(&nodeLock);
                
                    for (GList *n = nodes; n; n=n->next)
                    {
                        SrcDesc* desc = (SrcDesc*)n->data;
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
                        SrcDesc* desc = (SrcDesc*)n->data;
                        if (desc->onStop) desc->onStop(desc->opaque);
                    }

                    g_mutex_unlock(&nodeLock);
                }
            }
        }

        client.Close();

        return NULL;
    }

    void Manager::Output::OnVideo(const uint8_t* data, size_t size, int64_t timecode, bool isIDR)
    {
        g_mutex_lock(&nodeLock);

        for (GList* n = nodes; n; n = n->next)
        {
            SrcDesc* desc = (SrcDesc*)n->data;
            if (desc->onVideo) desc->onVideo(desc->opaque, data, size, timecode, isIDR);
        }

        g_mutex_unlock(&nodeLock);
    }

    void Manager::Output::OnAudio(const uint8_t* data, size_t size, int64_t timecode)
    {
        g_mutex_lock(&nodeLock);

        for (GList* n = nodes; n; n = n->next)
        {
            SrcDesc* desc = (SrcDesc*)n->data;
            if (desc->onAudio) desc->onAudio(desc->opaque, data, size, timecode);
        }

        g_mutex_unlock(&nodeLock);
    }

    void* Manager::Acquire(int output, const SrcDesc& desc)
    {
        g_assert(output >= 0 && output < MAX_OUTS);

        Output& out = outputs[output];

        g_mutex_lock(&out.threadLock);

        g_mutex_lock(&out.nodeLock);
        SrcDesc* node = new SrcDesc(desc);
        out.nodes = g_list_append(out.nodes, node);
        g_mutex_unlock(&out.nodeLock);

        if (!out.thread)
        {
            GError* error = nullptr;
            out.thread = g_thread_new("VentuzStreamOut", Output::ThreadProxy, &out);
        }
        g_mutex_unlock(&out.threadLock);

        return node;
    }

    void Manager::Release(int output, void** nodeptr)
    {
        g_assert(output >= 0 && output < MAX_OUTS);
        g_assert(nodeptr);

        SrcDesc* node = (SrcDesc*)*nodeptr;
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

            g_mutex_unlock(&out.threadLock);
        }
    }
}
