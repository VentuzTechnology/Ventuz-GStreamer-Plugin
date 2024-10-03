# Ventuz GStreamer Plugin

This plugin allows you to grab the video and audio from Ventuz' Encoded Stream Out outputs as source elements in a [GStreamer](https://gstreamer.freedesktop.org/) pipeline.

## Usage

1. Download and install the latest Windows MSVC 64 bit runtime from <https://gstreamer.freedesktop.org/download/#windows>
    1. In "Choose Setup Type" in the installer, choose "Complete"
1. Make sure `ventuzvideoplugin.dll`, found in the "Releases" section of this Repository, is in GStreamer's plugin path (eg. by setting `GST_PLUGIN_PATH`, using the `--gst-plugin-path` command line option or copying and pasting it to `C:\gstreamer\1.0\msvc_x86_64\lib\gstreamer-1.0\`)
1. In the Ventuz Configuration Editor, create an "Encoded Stream Output"
1. (optional) Select the appropriate codec (H264 or HVEC) if your graphics hardware supports this
1. Change "Encode Mode" to "Streaming"
1. In Ventuz, ensure "Enable All Outputs" is enabled

You now have two new video and audio sources:

-   `ventuzvideosrc` grabs the encoded video from a Stream Out output on the same machine. The video format will be exactly what's specified in the output options; either h.264 or HEVC. Please set the Encode Mode of the output to "Streaming".
-   Respectively, `ventuzaudiosrc` will give you an output's audio as stereo PCM in S16LE format.

Both sources have one option: `output-number` (int) specifies which output to grab the video and audio from.

### Examples

#### Play the video of the first output in a window

```cmd
gst-launch-1.0 ventuzvideosrc ! decodebin ! autovideosink
```

#### Play the video of a specified Ventuz output in a window

```cmd
gst-launch-1.0 ventuzvideosrc output-number=0 ! decodebin ! autovideosink
```

#### Play video and audio of the first output in a window

```cmd
gst-launch-1.0 ventuzvideosrc ! queue ! decodebin ! autovideosink ventuzaudiosrc ! audioconvert ! autoaudiosink
```

#### Stream an output to Twitch via RTMP

Note that Twitch requires h.264 video at max. 6Mbps and AAC audio. You'll need to set the output to H.264/Streaming/Constant Bitrate, and the audio will be encoded to AAC by the GStreamer pipeline.

```cmd
gst-launch-1.0 -e ventuzvideosrc ! h264parse ! queue ! mux. ventuzaudiosrc ! voaacenc bitrate=256000 ! mux. flvmux name=mux streamable=true ! rtmpsink
 location="rtmp://live.twitch.tv/app/<YOUR_STREAM_KEY>"
```

#### Stream an output to YouTube via RTMP

```cmd
gst-launch-1.0 -e ventuzvideosrc ! h264parse ! queue ! mux. ventuzaudiosrc ! voaacenc bitrate=256000 ! mux. flvmux name=mux streamable=true ! rtmpsink location="rtmp://a.rtmp.youtube.com/live2/<YOUR_STREAM_KEY>"
```

## Building

##### This is only needed if you intend to build the dll file instead of downloading the pre-built one found in the "Releases" of this Repo

-   You need to have Visual Studio 2022 (any edition will do) with Desktop C++ workload installed.
-   Install the latest Windows MSVC 64 bit runtime and development installers from <https://gstreamer.freedesktop.org/download/#windows>
-   In "Choose Setup Type" in the installer, choose "Complete"
-   Open the `GStreamerPlugin.sln` file in Visual Studio and build
-   There's no deploy pipeline yet for that single `ventuzvideoplugin.dll`, but you'll find it in x64\Debug or x64\Release depending on the build configuration.
