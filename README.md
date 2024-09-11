# Ventuz GStreamer Plugin

This plugin allows you to grab the video and audio from Ventuz' Encoded Stream Out outputs as source elements in a GStreamer pipeline.

## Usage

Make sure `ventuzvideoplugin.dll` is in GStreamer's plugin path (eg. by setting `GST_PLUGIN_PATH` or using the `--gst-plugin-path` command line option)

You now have two new video and audio sources:
* `ventuzvideosrc` grabs the encoded video from a Stream Out output on the same machine. The video format will be exactly what's specified in the output options; either h.264 or HEVC. Please set the Encode Mode of the output to "Streaming".
* Respectively, `ventuzaudiosrc` will give you an output's audio as 16 bit stereo S16LE.

Both sources have one option: `output-number` (int) specifies which output to grab the video and audio from.

### Examples 

#### Play the video of the first output in a window
```cmd
gst-launch-1.0 ventuzvideosrc ! decodebin ! autovideosink
```

#### Play video and audio of the first output in a window
```cmd
gst-launch-1.0 ventuzvideosrc ! queue ! decodebin ! autovideosink ventuzaudiosrc ! audioconvert ! autoaudiosink
```

#### Stream an output via RTMP to Twitch 

Note that Twitch requires h.264 video at max. 6Mbps and AAC audio. You'll need to set the output to H.264/Streaming/Constant Bitrate, and the audio will be encoded to AAC by the GStreamer pipeline.

```cmd
gst-launch-1.0 -e ventuzvideosrc ! h264parse ! queue ! mux. ventuzaudiosrc ! voaacenc bitrate=256000 ! mux. flvmux name=mux streamable=true ! rtmpsink
 location="rtmp://live.twitch.tv/app/<YOUR_STREAM_KEY>"
```

## Building

* You need to have Visual Studio 2022 (any edition will do) with Desktop C++ workload installed.
* Install the latest Windows MSVC 64 bit runtime and development installers from https://gstreamer.freedesktop.org/download/#windows
* Open the `GStreamerPlugin.sln` file in Visual Studio and build

