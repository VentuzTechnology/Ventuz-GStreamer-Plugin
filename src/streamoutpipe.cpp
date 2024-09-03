
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
        if (pipe != INVALID_HANDLE_VALUE)
            CloseHandle(pipe);

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

            if (err == ENOENT)
            {
                Sleep(1000);
                continue;
            }

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

    bool Client::Poll()
    {
        ChunkHeader chunk;
        if (!ReadStruct(chunk))
            return false;

        if (chunk.fourCC != 'fhdr')
        {
            // skip unknown chunks
            return ReadBuffer(chunk.size);
        }

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

        return false;
    }

    void Client::Ensure(size_t size)
    {
        if (size <= bufferSize)
            return;

        delete[] buffer;
        buffer = new uint8_t[size];
        bufferSize = size;
    }

    template<typename T> bool Client::ReadStruct(T &data)
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

}

