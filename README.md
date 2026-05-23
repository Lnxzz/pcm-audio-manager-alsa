# pcm_audio_manager_alsa
pcm_audio_manager_alsa is a userspace alsa output plugin to deliver a PCM audio stream to the [audio-manager](https://github.com/Lnxzz/audio-manager) from an ALSA capable audio application such as the Music Player Daemon (MPD).

## installing prerequisites
To build this project from source, a standard C/C++ build environment with cmake needs to be installed next to the ALSA development library.

```
sudo get update
sudo apt-get install build-essential cmake
sudo apt-get install libasound2-dev
```

## building the plugin
``
mkdir build
cd build
cmake ../
make
``

The output should be something like this:
```
[ 50%] Building C object CMakeFiles/asound_module_pcm_audio_manager.dir/src/pcm_audio_manager.c.o
[100%] Linking C shared library libasound_module_pcm_audio_manager.so
[100%] Built target asound_module_pcm_audio_manager
```

## installing the plugin
After building the plugin, the plugin can be installed with:
```
sudo make install
```

## alsa configuration
Edit the ALSA Configuration
 
/etc/asound.conf
```
# Define am_alsa using the pcm_audio_manager_alsa plugin
pcm.am_alsa {
  type pcm_audio_manager
}

# Configure the 'plug' plugin to be used as fallback
pcm.plug {
  type plug
  slave {
    pcm "hw:0,0"
  }
}

# The following lines define the pcm audio manager as the default device
pcm.!default {
  type am_alsa
}
```

## mpd configuration
When using the Music Player Daemon (MPD) for playback, you have to edit the configuration of MPD as well.

/etc/mpd.conf

```
audio_output {
       type                "alsa"
       name                "Default ALSA Device"
       #device: not specified, use default alsa audio device
       dop                 "no"
       auto_resample       "no"
       auto_format         "no"
       auto_channels       "no"
       replay_gain_handler "none"
       mixer_type          "none"     
}

replaygain			"off"
```

<br>

# COPYING
 Copyright (c) 2024-2026 Eelco Heerschop (eelco@heerschop.frl)
 
 This software is released under the MIT License (MIT)

 See [LICENSE](LICENSE) or https://opensource.org/licenses/MIT
