/**
 * @file    pcm_audio_manager.h
 * @brief   pcm_audio_manager is an userspace alsa output plugin to deliver 
 *          a PCM audio stream to the audio-manager from an ALSA capapable audio application.
 *
 * Copyright (c) 2024 Eelco Heerschop (eelco@heerschop.frl)
 *
 * Distributed under the MIT License (MIT)
 * See LICENSE.md or https://opensource.org/licenses/MIT
 */

#ifndef PCM_AUDIO_MANAGER_H
#define PCM_AUDIO_MANAGER_H

#include <alsa/asoundlib.h>
#include <alsa/pcm_external.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>


extern unsigned int log_level;
extern FILE*        log_file;
extern FILE*        pcm_dump_file;
extern char*        pcm_dump_file_name;

#define DEBUG(fmt, arg...)     if (log_level >= 4) fprintf(log_file, "D, %s, " fmt "\n", __FUNCTION__ , ## arg)
#define INFO(fmt, arg...)      if (log_level >= 3) fprintf(log_file, "I, %s, " fmt "\n", __FUNCTION__ , ## arg)
#define WARNING(fmt, arg...)   if (log_level >= 2) fprintf(log_file, "W, %s, " fmt "\n", __FUNCTION__ , ## arg)
#define ERROR(fmt, arg...)     if (log_level >= 1) fprintf(log_file, "E, %s, " fmt "\n", __FUNCTION__ , ## arg)

#define ARRAY_SIZE(a)              (sizeof(a)/sizeof((a)[0]))
#define DEFAULT_TARGET_FORMAT      SND_PCM_FORMAT_S32_LE
#define PERIOD_SIZE_BYTES          16384  /* one period size = 16K bytes */
#define PERIODS                    8      /* buffer size 16K * 8 = 128K bytes */
#define BEGINNING_OF_STREAM_MARKER 1
#define END_OF_STREAM_MARKER       2
#define DATA_MARKER                3


// supported pcm device access
const unsigned int supported_accesses[] = {
    SND_PCM_ACCESS_RW_INTERLEAVED
};

// supported pcm formats
const unsigned int supported_formats[] = {
    SND_PCM_FORMAT_S8,
    SND_PCM_FORMAT_S16_LE,
    SND_PCM_FORMAT_S24_LE,
    SND_PCM_FORMAT_S32_LE,
};

// supported channels
const unsigned int supported_channels[] = { 2 };

// supported sample rates
const unsigned int supported_rates[] = {
  8000, 11025, 12000, 16000, 22500,
  24000, 32000, 44100, 48000, 88200,
  96000, 176400, 192000
};

const char *supported_devices[] = {
  "hw:1,0,1", "hw:1,0,2", "hw:1,0,3", "hw:1,0,4", "hw:1,0,5", "hw:1,0,6", "hw:1,0,7", 
  "hw:2,0,1", "hw:2,0,2", "hw:2,0,3", "hw:2,0,4", "hw:2,0,5", "hw:2,0,6"
};

typedef struct rate_device_map {
  unsigned int rate;
  char*        device;
} rate_device_map_t;


typedef struct plugin_data {
  snd_pcm_ioplug_t   alsa_data;
  unsigned int       rate_device_map_size;
  rate_device_map_t* rate_device_map;
  snd_pcm_sframes_t  pointer;
  snd_pcm_format_t   src_format;
  char*              dst_device;
  snd_pcm_t*         dst_pcm_handle;
  unsigned int       dst_channels;
  unsigned int       dst_format;
  snd_pcm_uframes_t  dst_period_size;
  unsigned int       dst_periods;
  unsigned char*     dst_buffer;
  snd_pcm_uframes_t  dst_buffer_size;
  snd_pcm_uframes_t  dst_buffer_current;
  unsigned short     transfer_started;
} plugin_data_t;

#endif  /* PCM_AUDIO_MANAGER_H */
