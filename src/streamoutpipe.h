#pragma once

#include <stdint.h>
#include <windows.h>

#include <gst/gst.h>

struct VentuzClock
{
    GstSystemClock clock;
    uint64_t refFrame;
};

namespace StreamOutPipe
{
    struct PipeHeader
    {
        enum { VERSION = 2 };

        uint32_t hdrVersion;         // should be VERSION

        uint32_t videoCodecFourCC;   // used video codec, currently 'h264' aka h.264
        uint32_t videoWidth;         // width of image
        uint32_t videoHeight;        // height of image
        uint32_t videoFrameRateNum;  // frame rate numerator
        uint32_t videoFrameRateDen;  // frame rate denominator

        uint32_t audioCodecFourCC;   // used audio codec, currently 'pc16' aka 16 bit signed little endian PCM
        uint32_t audioRate;          // audio sample rate, currently fixed at 48000
        uint32_t audioChannels;      // audio channel count, currently 2
    };

    enum class Command : uint8_t
    {
        Nop = 0x00,             // Do nothing

        RequestIDRFrame = 0x01, // Request an IDR frame. it may take a couple of frames until the IDR frame arrives due to latency reasons.

        TouchBegin = 0x10,      // Start a touch. Must be followed by a TouchPara structure
        TouchMove = 0x11,       // Move a touch. Must be followed by a TouchPara structure        
        TouchEnd = 0x12,        // Release a touch. Must be followed by a TouchPara structure        
        TouchCancel = 0x13,     // Cancal a touch if possible. Must be followed by a TouchPara structure

        Char = 0x20,            // Send a keystroke. Must be followed by a KeyPara structure
        KeyDown = 0x21,         // Send a key down event. Must be followed by a KeyPara structure
        KeyUp = 0x22,           // Send a key up event. Must be followed by a KeyPara structure

        MouseMove = 0x28,       // Mouse positon update. Must be followed by a MouseXYPara structure
        MouseButtons = 0x29,    // Mouse buttons update. Must be followed by a MouseButtonsPara structure
        MouseWheel = 0x2a,      // Mouse wheel update. Must be followed by a MouseXYPara structure

        SetEncodePara = 0x30,   // Send encoder parameters. Must be followed by an EncodePara structure
    };

    // Parameters for Touch* PipeCommands    
    struct TouchPara
    {
        uint32_t id;   // numerical id. Must be unique per touch (eg. finger # or something incremental)
        int x;     // x coordinate in pixels from the left side of the viewport
        int y;     // y coordinate in pixels from the upper side of the viewport
    };

    // Parameters for the Key PipeCommand
    struct KeyPara
    {
        uint32_t Code;   // UTF32 codepoint for Char command; Windows VK_* for KeyUp/KeyDown
    };

    // Parameters for the MouseMove and MouseWheel PipeCommands
    struct MouseXYPara
    {
        int x;     // x coordinate in pixels from the left side of the viewport, or horizontal wheel delta
        int y;     // y coordinate in pixels from the upper side of the viewport, or vertical wheel delta
    };

    // Parameters for the MouseButtons PipeCommand
    struct MouseButtonsPara
    {
        enum class MouseButtons : uint32_t
        {
            Left = 0x01,
            Right = 0x02,
            Middle = 0x04,
            X1 = 0x08,
            X2 = 0x10,
        };

        MouseButtons Buttons; // bit field of all buttons pressed at the time
    };

    // Parameters for the SetEncodePara PipeCommand
    struct EncodePara
    {
        enum RateControlMode : uint32_t
        {
            ConstQP = 0,  // BitrateOrQP is QP (0..51)
            ConstRate = 1, // BitrateOrQP is rate in kBits/s
        };

        RateControlMode Mode;
        uint32_t BitrateOrQP;
    };

    typedef void (*OnStartFunc)(void* opaque, const PipeHeader& header);
    typedef void (*OnStopFunc)(void* opaque);
    typedef void (*OnFrameFunc)(void* opaque, int64_t timecode, int frnum, int frDen);
    typedef void (*OnVideoFunc)(void* opaque, const uint8_t* data, size_t size, int64_t timecode, bool isIDR);
    typedef void (*OnAudioFunc)(void* opaque, const uint8_t* data, size_t size, int64_t timecode);

    class PipeClient
    {
    public:

        PipeClient();
        ~PipeClient();

        void SetOnFrame(OnFrameFunc func, void* opaque) { onFrame = func; onFrameOpaque = opaque; }
        void SetOnVideo(OnVideoFunc func, void* opaque) { onVideo = func; onVideoOpaque = opaque; }
        void SetOnAudio(OnAudioFunc func, void* opaque) { onAudio = func; onAudioOpaque = opaque; }

        bool Open(int outputNo);
        void Close();
        bool Poll();

        bool IsOpen() const { return pipe != INVALID_HANDLE_VALUE; }

        const PipeHeader& GetHeader() const { return header; }

        void RequestIDR() { idrRequested = 1; }

    private:

        struct ChunkHeader
        {
            uint32_t fourCC;            // chunk type (four character code)
            int size;               // size of chunk data
        };

        struct FrameHeader
        {
            enum FrameFlags : uint32_t
            {
                IDR_FRAME = 0x01,           // frame is IDR frame (aka key frame / stream restart/sync point)
            };

            uint32_t frameIndex;         // frame index. If this isn't perfectly contiguous there was a frame drop in Ventuz
            FrameFlags flags;        // flags, see above
        };

        OnVideoFunc onVideo = nullptr;
        void* onVideoOpaque = nullptr;
        OnAudioFunc onAudio = nullptr;
        void* onAudioOpaque = nullptr;
        OnFrameFunc onFrame = nullptr;
        void* onFrameOpaque = nullptr;

        PipeHeader header = {};       

        HANDLE pipe = INVALID_HANDLE_VALUE;

        uint8_t* buffer = nullptr;
        size_t bufferSize = 0;

        volatile uint32_t idrRequested = 0;

        void Ensure(size_t size);

        template<typename T> bool ReadStruct(T &data);
        bool ReadBuffer(size_t size);
    };


    class OutputManager
    {
    public:
        static OutputManager Instance;

        struct Callbacks
        {
            void* opaque = nullptr;
            OnStartFunc onStart = nullptr;
            OnStopFunc onStop = nullptr;
            OnVideoFunc onVideo = nullptr;
            OnAudioFunc onAudio = nullptr;
        };

        void* Acquire(int output, const Callbacks& desc);
        void Release(int output, void**);

        GstClock* GetClock() const { return (GstClock*)clk; }

        GstClockTime GetVentuzTime() const { return time; }

        GstClockTimeDiff GetTimeDiff(int64_t timeCode);

    private:

        static const int MAX_OUTS = 8;

        OutputManager();
        ~OutputManager();

        struct Output
        {
            int outputNo;
            PipeClient client;
            GList* nodes = nullptr;
            GThread* thread = nullptr;
            GMutex threadLock;
            GMutex nodeLock;
            bool exit = false;

            static gpointer ThreadProxy(gpointer data) { return ((Output*)data)->ThreadFunc(); }
            gpointer ThreadFunc();
            void OnVideo(const uint8_t* data, size_t size, int64_t timecode, bool isIDR);
            void OnAudio(const uint8_t* data, size_t size, int64_t timecode);

            static void OnVideoProxy(void* opaque, const uint8_t* data, size_t size, int64_t timecode, bool isIDR)
            {
                ((Output*)opaque)->OnVideo(data, size, timecode, isIDR);
            }

            static void OnAudioProxy(void* opaque, const uint8_t* data, size_t size, int64_t timecode)
            {
                ((Output*)opaque)->OnAudio(data, size, timecode);
            }
        };

        Output outputs[MAX_OUTS] = {};
        VentuzClock* clk = nullptr;

        static void OnFrameProxy(void* opaque, int64_t timecode, int frNum, int frDen) { ((OutputManager*)opaque)->OnFrame(timecode, frNum, frDen); }

        void OnFrame(int64_t timecode, int frNum, int frDen);

        GMutex timeLock;
        uint64_t lastTimeCode = 0;
        GstClockTime time = 0;
        GstClockTimeDiff dur = 0;
    };

}