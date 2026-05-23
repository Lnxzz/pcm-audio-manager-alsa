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

#include "pcm_audio_manager.h"

// START func.c


void close_destination_device(plugin_data_t* plugin_data)
{
    /* making sure destination device handle was created; otherwise there is nothing to close */
    if (!plugin_data->dst_pcm_handle)
    {
        return;
    }

    int error = 0;

    if ((error = snd_pcm_close(plugin_data->dst_pcm_handle)) < 0)
    {
        WARNING("Error while closing destination device: %s", snd_strerror(error));
    }
    else
    {
        INFO("Destination device was closed");
    }

    if (plugin_data->dst_buffer)
    {
        free(plugin_data->dst_buffer);
        plugin_data->dst_buffer = NULL;
    }

    plugin_data->dst_pcm_handle = NULL;
}

void copy_sample(plugin_data_t* plugin_data, unsigned char* source_sample, size_t source_sample_size, unsigned char* target_sample)
{
    switch (plugin_data->src_format)
    {
        case SND_PCM_FORMAT_S8:  /* TODO: still requires testing */
        case SND_PCM_FORMAT_S16_LE:
        case SND_PCM_FORMAT_S32_LE:
            for (unsigned int s = 0; s < source_sample_size; s++) {
                target_sample[s] = source_sample[s];
            }
            break;
        case SND_PCM_FORMAT_S24_LE:
            for (unsigned int s = 0; s < 3; s++) {
                target_sample[s + 1] = source_sample[s];
            }
            break;
        default:
            break;
    }
}

void copy_frames(plugin_data_t* plugin_data, unsigned char* pcm_data, snd_pcm_uframes_t frames)
{
    size_t         sample_size        = (snd_pcm_format_physical_width(plugin_data->src_format) >> 3);
    size_t         target_sample_size = (snd_pcm_format_physical_width(plugin_data->dst_format) >> 3);
    size_t         target_frame_size  = target_sample_size * (plugin_data->alsa_data.channels + 1);
    size_t         size_difference    = target_sample_size - sample_size;
    unsigned char* target_data        = plugin_data->dst_buffer + plugin_data->dst_buffer_current * target_frame_size;

    /* resetting target buffer to make sure it is not filled with junk */
    memset(target_data, 0, frames * target_frame_size);

    /* going through frame-by-frame */
    for (unsigned int f = 0; f < frames; f++)
    {
        /* going through channel-by-channel */
        /* TODO: support is required for source channel != (target channel + 1) */
        for (unsigned int c = 0; c < plugin_data->alsa_data.channels; c++)
        {
            copy_sample(plugin_data, pcm_data, sample_size, target_data + size_difference);

            /* skipping to the next sample representing the next channel */
            pcm_data    += sample_size;
            target_data += target_sample_size;
        }

        /* target frame contains one extra channel for control data */
        target_data += target_sample_size;

        /* marking frame as containing data in the last byte of the last channel */
        *(target_data - 1) = DATA_MARKER;
    }

    /* increasing pointer of the target buffer */
    plugin_data->dst_buffer_current += frames;
}

/**
 * @brief returns log level as string
 */
const char* log_level_to_string(unsigned int log_level) {
  switch (log_level) {
    case 0:
      return "NONE";
    case 1:
      return "ERROR";
    case 2:
      return "WARNING";
    case 3:
      return "INFO";
    case 4:
      return "DEBUG";
  }
  return "UNKNOWN";
}


int open_destination_device(plugin_data_t* plugin_data)
{
    int                  error     = 0;
    snd_pcm_hw_params_t* hw_params = NULL;

    /* opening the target device */
    if (!error)
    {
        if ((error = snd_pcm_open(&plugin_data->dst_pcm_handle, plugin_data->dst_device, SND_PCM_STREAM_PLAYBACK, 0)) < 0)
        {
            ERROR("Could not open destination device: %s", snd_strerror(error));
        }
    }

    /* allocating hardware parameters object and fill it with default values */
    if (!error)
    {
        if ((error = snd_pcm_hw_params_malloc(&hw_params)) < 0)
        {
            ERROR("Could not allocate HW parameters: %s", snd_strerror(error));
        }
    }
    if (!error)
    {
        if ((error = snd_pcm_hw_params_any(plugin_data->dst_pcm_handle, hw_params)) < 0)
        {
            ERROR("Could not fill HW parameters with defaults: %s", snd_strerror(error));
        }
    }

    /* setting target device parameters */
    if (!error)
    {
        if ((error = snd_pcm_hw_params_set_access(plugin_data->dst_pcm_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
        {
            ERROR("Could not set destination device access mode: %s", snd_strerror(error));
        }
    }
    if (!error)
    {
        if ((error = snd_pcm_hw_params_set_format(plugin_data->dst_pcm_handle, hw_params, plugin_data->dst_format)) < 0)
        {
            ERROR("Could not set destination device format: %s %d", snd_strerror(error), plugin_data->dst_format);
        }
    }
    if (!error)
    {
        if ((error = snd_pcm_hw_params_set_channels(plugin_data->dst_pcm_handle, hw_params, plugin_data->alsa_data.channels + 1)) < 0)
        {
            ERROR("Could not set amount of channels for destination device: %s", snd_strerror(error));
        }
    }
    if (!error)
    {
        if ((error = snd_pcm_hw_params_set_rate(plugin_data->dst_pcm_handle, hw_params, plugin_data->alsa_data.rate, 0)) < 0)
        {
            ERROR("Could not set sample rate for destination device: %s", snd_strerror(error));
        }
    }
    if (!error)
    {
        if ((error = snd_pcm_hw_params_set_period_size(plugin_data->dst_pcm_handle, hw_params, plugin_data->dst_period_size, 0)) < 0)
        {
            ERROR("Could not set period size for destination device: %s", snd_strerror(error));
        }
    }
    if (!error)
    {
        if ((error = snd_pcm_hw_params_set_periods(plugin_data->dst_pcm_handle, hw_params, plugin_data->dst_periods, 0)) < 0)
        {
            ERROR("Could not set amount of periods for destination device: %s", snd_strerror(error));
        }
    }

#if SND_LIB_VERSION >= 0x010009
    /* disabling ALSA resampling */
    if (!error)
    {
        if ((error = snd_pcm_hw_params_set_rate_resample(plugin_data->dst_pcm_handle, hw_params, 0)) < 0)
        {
            ERROR("Could not disable ALSA resampling: %s", snd_strerror(error));
        }
    }
#endif

    /* saving hardware parameters for target device */
    if (!error) {
        if ((error = snd_pcm_hw_params(plugin_data->dst_pcm_handle, hw_params)) < 0) {
            ERROR("Could not set hardware parameters: %s", snd_strerror(error));
        }
    }
    if (!hw_params) {
        snd_pcm_hw_params_free(hw_params);
    }

    /* allocating buffer required to transfer data to target device */
    if (!error) {
        /* it will allow multiple calls to set ALSA HW parameters */
        if (plugin_data->dst_buffer) {
            free(plugin_data->dst_buffer);
            plugin_data->dst_buffer = NULL;
        }

        /* target buffer must not be equal to the source buffer size; otherwise pointer callback will always return 0 */
        plugin_data->dst_buffer_size    = plugin_data->dst_period_size;
        plugin_data->dst_buffer_current = 0;

        /* adding extra space for one extra channel */
        size_t size_in_bytes = plugin_data->dst_buffer_size * (snd_pcm_format_physical_width(plugin_data->dst_format) >> 3) * (plugin_data->alsa_data.channels + 1);

        /* calloc sets content to zero */
        plugin_data->dst_buffer = (unsigned char*) calloc(1, size_in_bytes);
        if (!plugin_data->dst_buffer)
        {
            error = -ENOMEM;
            ERROR("Could not allocate memory for transfer buffer (requested %lu bytes)", size_in_bytes);
        }
        else
        {
            DEBUG("Transfer buffer was allocated (%ld bytes)", size_in_bytes);
        }
    }

    return error;
}


int set_dst_hw_params(plugin_data_t* plugin_data, snd_pcm_hw_params_t *params)
{
    int error = 0;

    /* looking up for the target device name based on sample rate */
    plugin_data->dst_device = NULL;
    for (unsigned int i = 0; i < plugin_data->rate_device_map_size && !plugin_data->dst_device; i++)
    {
        if (plugin_data->rate_device_map[i].rate == plugin_data->alsa_data.rate)
        {
            plugin_data->dst_device = plugin_data->rate_device_map[i].device;
        }
    }
    if (!plugin_data->dst_device)
    {
        error = -ENODEV;
        ERROR("Could not find target device for sample rate %u", plugin_data->alsa_data.rate);
    }
    else
    {
        INFO("destination device=%s", plugin_data->dst_device);
    }

    /* collecting details about the PCM stream */
    if (!error)
    {
        if ((error = snd_pcm_hw_params_get_period_size(params, &plugin_data->dst_period_size, 0)) < 0)
        {
            ERROR("Could not get period size value: %s", snd_strerror(error));
        }
        else
        {
            INFO("destination period size=%ld", plugin_data->dst_period_size);
        }
    }
    if (!error)
    {
        if ((error = snd_pcm_hw_params_get_periods(params, &plugin_data->dst_periods, 0)) < 0)
        {
            ERROR("Could not get buffer size value: %s", snd_strerror(error));
        }
        else
        {
            INFO("destination periods=%d", plugin_data->dst_periods);
        }
    }
    if (!error)
    {
        if ((error = snd_pcm_hw_params_get_format(params, &plugin_data->src_format)) < 0)
        {
            ERROR("Could not get format value: %s", snd_strerror(error));
        }
        else
        {
            INFO("source format=%d", plugin_data->src_format);
        }
    }

    if (!error)
    {
        error = open_destination_device(plugin_data);
    }

    return error;
}


int set_dst_sw_params(plugin_data_t* plugin_data, snd_pcm_sw_params_t *params)
{
    int                  error     = 0;
    snd_pcm_sw_params_t* sw_params = NULL;

    /* allocating software parameters object and fill it with default values */
    if (!error)
    {
        if ((error = snd_pcm_sw_params_malloc(&sw_params)) < 0)
        {
            ERROR("Could not allocate SW parameters: %s", snd_strerror(error));
        }
    }
    if (!error)
    {
        if ((error = snd_pcm_sw_params_current(plugin_data->dst_pcm_handle, sw_params)) < 0)
        {
            ERROR("Could not fill SW parameters with defaults: %s", snd_strerror(error));
        }
    }
    if (!error)
    {
        if ((error = snd_pcm_sw_params_set_start_threshold(plugin_data->dst_pcm_handle, sw_params, plugin_data->dst_buffer_size)) < 0)
        {
            ERROR("Could not set threshold for destination device: %s", snd_strerror(error));
        }
    }
    if (!error)
    {
        if ((error = snd_pcm_sw_params_set_avail_min(plugin_data->dst_pcm_handle, sw_params, plugin_data->dst_period_size)) < 0)
        {
            ERROR("Could not set min available amount for destination device: %s", snd_strerror(error));
        }
    }

    /* saving software parameters for target device */
    if (!error)
    {
        if ((error = snd_pcm_sw_params(plugin_data->dst_pcm_handle, sw_params)) < 0)
        {
            ERROR("Could set software parameters: %s", snd_strerror(error));
        }
    }
    if (sw_params)
    {
        snd_pcm_sw_params_free(sw_params);
    }

    return error;
}


int set_src_hw_params(snd_pcm_ioplug_t *io)
{
    int error = 0;

    /* supported access type */
    if (!error)
    {
        if ((error = snd_pcm_ioplug_set_param_list(io, SND_PCM_IOPLUG_HW_ACCESS, ARRAY_SIZE(supported_accesses), supported_accesses)) < 0)
        {
            ERROR("Could not set required access mode: %s", snd_strerror(error));
        }
    }

    /* supported formats */
    if (!error)
    {
        if ((error = snd_pcm_ioplug_set_param_list(io, SND_PCM_IOPLUG_HW_FORMAT, ARRAY_SIZE(supported_formats), supported_formats)) < 0)
        {
            ERROR("Could not set required format: %s", snd_strerror(error));
        }
    }

    /* supported amount of channels */
    if (!error)
    {
        if ((error = snd_pcm_ioplug_set_param_list(io, SND_PCM_IOPLUG_HW_CHANNELS, ARRAY_SIZE(supported_channels), supported_channels)) < 0)
        {
            ERROR("Could not set required amount of channels: %s", snd_strerror(error));
        }
    }

    /* supported rates */
    if (!error)
    {
        if ((error = snd_pcm_ioplug_set_param_list(io, SND_PCM_IOPLUG_HW_RATE, ARRAY_SIZE(supported_rates), supported_rates)) < 0)
        {
            ERROR("Could not set required sample rate: %s", snd_strerror(error));
        }
    }

    /* defining buffer size: buffer = period size * number of periods */
    if (!error)
    {
        if ((error = snd_pcm_ioplug_set_param_minmax(io, SND_PCM_IOPLUG_HW_PERIOD_BYTES, PERIOD_SIZE_BYTES, PERIOD_SIZE_BYTES)) < 0)
        {
            ERROR("Could not set required period size: %s", snd_strerror(error));
        }
    }
    if (!error)
    {
        if ((error = snd_pcm_ioplug_set_param_minmax(io, SND_PCM_IOPLUG_HW_PERIODS, PERIODS, PERIODS)) < 0)
        {
            ERROR("Could not set required amount of periods: %s", snd_strerror(error));
        }
    }

    return error;
}

snd_pcm_sframes_t write_to_dst(plugin_data_t* plugin_data) {
    snd_pcm_sframes_t result = 0;

    /* if there is anything to be written to the target device */
    if (plugin_data->dst_buffer_current > 0)
    {
        /* writing to the target device */
        result = snd_pcm_writei(plugin_data->dst_pcm_handle, plugin_data->dst_buffer, plugin_data->dst_buffer_current);

        /* no need to restore from an error in case of -EAGAIN */
        if (result < 0 && result != -EAGAIN)
        {
            result = snd_pcm_prepare(plugin_data->dst_pcm_handle);
            if (result < 0)
            {
                ERROR("Target device restore error: %s", snd_strerror(result));
            }
        }
        else if (result == -EAGAIN)
        {
            /* it will make ALSA call transfer callback again with the same data */
            result = 0;
        }
        else if (result > 0)
        {
            /* dumping PCM content if configured */
            if (pcm_dump_file)
            {
                size_t size_in_bytes = (plugin_data->dst_buffer_current * 3) << 2;
                size_t bytes_written = fwrite(plugin_data->dst_buffer, 1, size_in_bytes, pcm_dump_file);
                if (bytes_written != size_in_bytes)
                {
                    ERROR("Error while writting PCM data to file (error=%s)", strerror(ferror(pcm_dump_file)));
                }
            }

            /* if not all data was written then moving reminder of the target buffer to the beginning */
            if (result < plugin_data->dst_buffer_current)
            {
                size_t            target_sample_size = (snd_pcm_format_physical_width(plugin_data->dst_format) >> 3);
                size_t            target_frame_size  = target_sample_size * (plugin_data->alsa_data.channels + 1);
                size_t            offset             = result * target_frame_size;
                snd_pcm_uframes_t frames             = plugin_data->dst_buffer_current - result;

                memcpy(plugin_data->dst_buffer, plugin_data->dst_buffer + offset, frames * target_frame_size);
            }

            /* updating target and ALSA buffers' pointers */
            plugin_data->dst_buffer_current -= result;
            plugin_data->pointer            += result;
        }
    }

    return result;
}

void write_stream_marker(plugin_data_t* plugin_data, unsigned char marker) {
    snd_pcm_sframes_t result = 0;

    /* writting whatever is left in the target buffer */
    while (plugin_data->dst_buffer_current > 0 && result >= 0)
    {
        result = write_to_dst(plugin_data);
        if (result < 0)
        {
            ERROR("Error while writting to target device: %s", snd_strerror(result));
        }
    }

    /* opening a dump file if configured and streaming starts */
    if (pcm_dump_file_name && marker == BEGINNING_OF_STREAM_MARKER)
    {
        pcm_dump_file = fopen(pcm_dump_file_name, "a");
        if (!pcm_dump_file)
        {
            ERROR("Could not open PCM dump file, PCM data will not be save in the file (error=%s)", strerror(errno));
        }
    }

    /* reseting target buffer */
    size_t target_sample_size = (snd_pcm_format_physical_width(plugin_data->dst_format) >> 3);
    size_t target_frame_size  = target_sample_size * (plugin_data->alsa_data.channels + 1);
    memset(plugin_data->dst_buffer, 0, plugin_data->dst_buffer_size * target_frame_size);

    /* marking stream as closed; useful to detect ALSA junk at the end */
    for (snd_pcm_uframes_t i = 0; i < plugin_data->dst_buffer_size; i++)
    {
        plugin_data->dst_buffer[(i + 1) * target_frame_size - 1] = marker;
    }

    /* making sure a single period is written */
    for (plugin_data->dst_buffer_current = plugin_data->dst_buffer_size; plugin_data->dst_buffer_current > 0 && result >= 0;)
    {
        result = write_to_dst(plugin_data);
        if (result < 0)
        {
            ERROR("Error while writting to target device: %s", snd_strerror(result));
        }
    }

    /* closing a dump file if it is opened */
    if (pcm_dump_file && marker == END_OF_STREAM_MARKER)
    {
        fclose(pcm_dump_file);
    }
}


// END func.c


/* define the default logging level (0 - NONE, 1 - ERROR, 2 - WARNING, 3 - INFO, 4 - DEBUG) */
unsigned int log_level          = 4;
FILE*        log_file           = NULL;
FILE*        pcm_dump_file      = NULL;
char*        pcm_dump_file_name = NULL;


/**
 * @brief Close stream callback
 */
static int callback_close(snd_pcm_ioplug_t *io) {
    DEBUG("Close stream callback was invoked");

    plugin_data_t* plugin_data = (plugin_data_t*)io->private_data;

    if (plugin_data->dst_pcm_handle)
    {
        /* if there PCM data transfer was actually started then marking the end of stream and draining buffer */
        if (plugin_data->transfer_started)
        {
            write_stream_marker(plugin_data, END_OF_STREAM_MARKER);

            int tmp;
            if ((tmp = snd_pcm_drain(plugin_data->dst_pcm_handle)) < 0)
            {
                WARNING("Error while draining target device: %s", snd_strerror(tmp));
            }
            plugin_data->transfer_started = 0;
        }

        /* closing destination device which will release relevant resources */
        close_destination_device(plugin_data);
    }

    /* log file is flushed instead of closing it to be able to log in case of multiple calls to ALSA close routine */
    fsync(fileno(log_file));

    /* close routine may not fail */
    return 0;
}

/**
 * @brief Set hardware parameters callback
 */
static int callback_hw_params(snd_pcm_ioplug_t *io, snd_pcm_hw_params_t *params) {
  DEBUG("HW parameters setup callback was invoked");

  plugin_data_t* plugin_data = (plugin_data_t*)io->private_data;

  // closing the target device first to allow multiple calls to snd_pcm_hw_params / snd_pcm_prepare
  close_destination_device(plugin_data);

  // setting up target device hardware parameters
  return set_dst_hw_params(plugin_data, params);
}

/**
 * @brief Pointer change callback
 */
static snd_pcm_sframes_t callback_pointer(snd_pcm_ioplug_t *io) {
  plugin_data_t* plugin_data = (plugin_data_t*)io->private_data;

  DEBUG("Pointer change callback was called (pointer=%ld, buffer size=%ld)", plugin_data->pointer, io->buffer_size);

  plugin_data->pointer %= io->buffer_size;

  return plugin_data->pointer;
}

/**
 * @brief callback to prepare for pcm processing
 */
static int callback_prepare(snd_pcm_ioplug_t *io) {
  DEBUG("Prepare processing callback was invoked");

  int error = 0;
  plugin_data_t* plugin_data = (plugin_data_t*)io->private_data;

  // reset the hw buffer pointer
  plugin_data->pointer = 0;

  if (snd_pcm_state(plugin_data->dst_pcm_handle) > SND_PCM_STATE_PREPARED) {
    return error;
  }

  // prepare the destination device
  if ((error = snd_pcm_prepare(plugin_data->dst_pcm_handle)) < 0) {
    ERROR("Error while preparing destination device: %s", snd_strerror(error));
  }

  return error;
}

/**
 * @brief callback to start pcm processing
 */
static int callback_start(snd_pcm_ioplug_t *io) {
  DEBUG("Start processing callback was invoked");
  return 0;
}

/**
 * @brief callback to stop pcm processing
 */
static int callback_stop(snd_pcm_ioplug_t *io) {
  DEBUG("Stop processing callback was invoked");
  return 0;
}

/**
 * @brief callback to set software parameters
 */
static int callback_sw_params(snd_pcm_ioplug_t *io, snd_pcm_sw_params_t *params) {
  DEBUG("SW parameters setup callback was invoked");
  plugin_data_t* plugin_data = (plugin_data_t*)io->private_data;
  return set_dst_sw_params(plugin_data, params);
}

/**
 * @brief callback to transfer pcm data
 */
static snd_pcm_sframes_t callback_transfer(snd_pcm_ioplug_t *io, const snd_pcm_channel_area_t *areas, snd_pcm_uframes_t offset, snd_pcm_uframes_t frames_provided) {

    plugin_data_t* plugin_data = (plugin_data_t*)io->private_data;
    unsigned char* pcm_data    = (unsigned char*)areas->addr + (areas->first >> 3) + ((areas->step * offset) >> 3);

    DEBUG("Data transfer callback was invoked (offset=%lu, frames provided=%lu, frames already present=%ld)", offset, frames_provided, plugin_data->dst_buffer_current);

    // mark the start of the pcm stream if this is the first time this method is called
    if (!plugin_data->transfer_started)
    {
        write_stream_marker(plugin_data, BEGINNING_OF_STREAM_MARKER);
        plugin_data->transfer_started = 1;
    }

    // it's ok to process less frames than provided as ALSA will call this callback with the rest of data
    // adjusting amount of frames to be processed, which is max(available,provided)
    snd_pcm_uframes_t available_size     = plugin_data->dst_buffer_size - plugin_data->dst_buffer_current;
    snd_pcm_uframes_t frames_processable = frames_provided;
    if (available_size < frames_provided)
    {
        DEBUG("More frames provided than buffer available (frames provided=%lu, available buffer size=%ld)", frames_provided, available_size);
        frames_processable = available_size;
    }

    // copying frames from the source buffer to the target buffer
    copy_frames(plugin_data, pcm_data, frames_processable);

    // writting to the target device
    snd_pcm_sframes_t result = write_to_dst(plugin_data);
    if (result < 0)
    {
        ERROR("Error while writting to target device: %s", snd_strerror(result));
    }
    else if (result < frames_processable)
    {
        WARNING("Less frames were written to the target device than expected (written frames=%ld, expected to write frames=%ld)", result, frames_processable);
    }

    /* frames are 'consumed' as long as they were coppied to the transfer buffer, even though some are still pending for delivery */
    return frames_processable;
}

/**
 * @brief Plugin callbacks
 */
const snd_pcm_ioplug_callback_t callbacks = {
    .start     = callback_start,
    .stop      = callback_stop,
    .pointer   = callback_pointer,
    .close     = callback_close,
    .hw_params = callback_hw_params,
    .sw_params = callback_sw_params,
    .prepare   = callback_prepare,
    .transfer  = callback_transfer,
};

/**
 * @brief Plugin entry point
 *        This macro defines the function with a proper name to be referred from alsa-lib
 * 
 */
SND_PCM_PLUGIN_DEFINE_FUNC(audio_manager) {

    int                   error          = 0;
    plugin_data_t*        plugin_data    = NULL;
    unsigned short        plugin_created = 0;
    snd_config_iterator_t i;
    snd_config_iterator_t next;
    const char*           log_level_name;
    const char*           log_file_name;
    int                   log_file_open_error = 0;

    // iterate through the configuration items
    snd_config_for_each(i, next, conf) {

      snd_config_t* n = snd_config_iterator_entry(i);
      const char*   id;

      if (snd_config_get_id(n, &id) < 0) {
        continue;
      }

      if (strcasecmp(id, "comment") == 0 || strcasecmp(id, "type") == 0) {
        continue;
      }

      // set log level
      if (strcasecmp(id, "log_level") == 0) {
        if (snd_config_get_string(n, &log_level_name) < 0) {
          continue;
        }

        if (strcasecmp(log_level_name, "none") == 0) {
          log_level = 0;
        } else if (strcasecmp(log_level_name, "error") == 0) {
          log_level = 1;
        } else if (strcasecmp(log_level_name, "warning") == 0) {
          log_level = 2;
        } else if (strcasecmp(log_level_name, "info") == 0) {
          log_level = 3;
        } else if (strcasecmp(log_level_name, "debug") == 0) {
          log_level = 4;
        }

        continue;
      }

      // set log file
      if (strcasecmp(id, "log_file") == 0) {

        if (snd_config_get_string(n, &log_file_name) < 0) {
          continue;
        }

        if (strcasecmp(log_file_name, "stdout") == 0) {
          log_file = stdout;
        } else if (strcasecmp(log_file_name, "stderr") == 0) {
          log_file = stderr;
        } else {
          // log file is never closed, which allows support of multiple calls to ALSA close routine
          log_file = fopen(log_file_name, "a");
          if (!log_file) {
            log_file_open_error = errno;
          }
        }
      }

      // setting PCM dump file if provided
      if (strcasecmp(id, "pcm_dump_file") == 0) {
        const char* str;
        if (snd_config_get_string(n, &str) < 0) {
          continue;
        }

        // allocated memory here will be released when plugin is unloaded
        pcm_dump_file_name = calloc(1, strlen(str) + 1);
        if (!pcm_dump_file_name) {
          error = -ENOMEM;
          ERROR("Could not allocate memory for dump file name (requested %lu bytes)", strlen(str) + 1);
          break;
        }
        strcpy(pcm_dump_file_name, str);
      }
    }

    // making sure log_file is always initialized
    if (!log_file) {
      log_file = stdout;
    }

    if (!error) {
      INFO("Loading PCM audio_manager ALSA plugin...");
      if (log_level) {
        INFO("Log level: %s", log_level_to_string(log_level));
        if (!log_file_open_error) {
          INFO("Log file: %s", log_file_name);
        } else {
          ERROR("Could not open log file defined in configuration, using stdout instead (error=%s, provided file name=%s)", strerror(errno), log_file_name);
        }
      }

      if (pcm_dump_file_name) {
        INFO("PCM dump file name is %s", pcm_dump_file_name);
      } else {
        INFO("PCM dump file is not used");
      }
    }

    // allocating memory for plugin data structure
    if (!error) {
      plugin_data = calloc(1, sizeof(plugin_data_t));
      if (!plugin_data) {
        error = -ENOMEM;
        ERROR("Could not allocate memory for plugin data (requested %lu bytes)", sizeof(plugin_data_t));
      }
    }

    // initializing plugin data structure */
    if (!error) {
      plugin_data->alsa_data.version      = SND_PCM_IOPLUG_VERSION;
      plugin_data->alsa_data.name         = "audio_manager -  Audio Manager PCM ALSA plugin";
      plugin_data->alsa_data.callback     = &callbacks;
      plugin_data->alsa_data.private_data = plugin_data;
      plugin_data->dst_format = DEFAULT_TARGET_FORMAT;

      // allocate memory for rate->device data structure
      plugin_data->rate_device_map_size = ARRAY_SIZE(supported_rates);
      plugin_data->rate_device_map = calloc(plugin_data->rate_device_map_size, sizeof(rate_device_map_t));
      if (!plugin_data->rate_device_map) {
        error = -ENOMEM;
        ERROR("Could not allocate memory for sampling rate mapping (requested %lu bytes)", sizeof(rate_device_map_t));
      }
    }

    // initialize rate->device map data structure
    if (!error) {
      for (size_t i = 0; i < plugin_data->rate_device_map_size && i < ARRAY_SIZE(supported_devices); i++) {
          plugin_data->rate_device_map[i].rate = supported_rates[i];
          plugin_data->rate_device_map[i].device = strdup(supported_devices[i]);
          if (!plugin_data->rate_device_map[i].device) {
              error = -ENOMEM;
              ERROR("Could not allocate memory for device string");
              break;
          }
      }
    }

    // required to avoid ALSA mutex deadlocks; ALSA does not expose this functionality via its API
    if (!error) {
      if (setenv("LIBASOUND_THREAD_SAFE", "0", 1) < 0) {
        error = -EPERM;
        ERROR("Could not disable thread-safety for ALSA library");
      }
    }

    // create an io plugin instance
    if (!error) {
      if ((error = snd_pcm_ioplug_create(&plugin_data->alsa_data, name, stream, mode)) < 0) {
        ERROR("Could not register plugin within ALSA: %s", snd_strerror(error));
      } else {
        plugin_created = 1;
      }
    }

    // set hw parameters
    if (!error) {
      error = set_src_hw_params(&plugin_data->alsa_data);
    }

    if (!error) {
      // plugin created
      *pcmp = plugin_data->alsa_data.pcm;
      INFO("Plugin was loaded");
    } else {
      // plugin was not created properly
      if (!plugin_data && plugin_created) {
        snd_pcm_ioplug_delete(&plugin_data->alsa_data);
      }
      // free allocated resources in case of error
      if (plugin_data->rate_device_map) {
        for (size_t i = 0; i < plugin_data->rate_device_map_size; i++) {
          if (plugin_data->rate_device_map[i].device) {
            free(plugin_data->rate_device_map[i].device);
          }
        }
        free(plugin_data->rate_device_map);
        plugin_data->rate_device_map = NULL;
      }
    }

    return error;
}
SND_PCM_PLUGIN_SYMBOL(audio_manager)
