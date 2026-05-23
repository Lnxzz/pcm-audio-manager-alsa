#pcm_audio_manager_alsa
pcm_audio_manager_alsa is an userspace alsa output plugin to deliver a PCM audio stream to the audio-manager from an ALSA capapable audio application such as the Music Player Daemon (MPD).

## installing prerequisites
pcm_audio_manager_alsa
```
sudo get update
sudo apt-get install build-essential libasound2-dev
```

## building the plugin
``
mkdir build
cd build
cmake ../
make
``

``
[ 50%] Building C object CMakeFiles/asound_module_pcm_audio_manager.dir/src/pcm_audio_manager.c.o
[100%] Linking C shared library libasound_module_pcm_audio_manager.so
[100%] Built target asound_module_pcm_audio_manager
``

## installing the plugin
After building the plugin, the plugin can be installed with:
``
sudo make install
``

## alsa configuration

/etc/asound.conf
``
# Define am_alsa using the pcm_audio_manager_alsa plugin
pcm.am_alsa {
  type pcm_audio_manager_alsa
}

# Configure the 'plug' plugin to be used as fallback
pcm.plug {
  type plug
  slave {
    pcm "hw:0,0"
  }
}

# Set the default device to am_alsa
pcm.!default {
  type am_alsa
}
``

## mpd configuration

/etc/mpd.conf

``
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
``



