// Copyright Scott Cutler
// This source file is licensed under the GNU Lesser General Public License (LGPL)

#include <stdlib.h>
#include <string.h>

#include "sdrio_ext.h"

#define mirisdr_STATIC
#include "mirisdr.h"

#include "pthread.h"

#ifdef _WIN32
#include <Windows.h>
void usleep(unsigned long us) { Sleep(us / 1000); }
#endif

typedef struct sdrio_device_t
{
    sdrio_uint32 device_index;

    mirisdr_dev_t *mirics_device;

    sdrio_rx_async_callback callback;
    void *callback_context;
    pthread_t tid;

    sdrio_iq *samples;
    sdrio_uint32 num_samples;

    sdrio_uint64 min_freq;
    sdrio_uint64 max_freq;
};

typedef struct sdrio_iqi16_t
{
    sdrio_int16 i;
    sdrio_int16 q;
} sdrio_iqi16;


SDRIOEXPORT sdrio_int32 sdrio_init()
{
    return 1;
}

SDRIOEXPORT sdrio_int32 sdrio_get_num_devices()
{
    return mirisdr_get_device_count();
}

SDRIOEXPORT sdrio_device * sdrio_open_device(sdrio_uint32 device_index)
{
    sdrio_device *dev = (sdrio_device *)malloc(sizeof(sdrio_device));

    if (dev)
    {
        memset(dev, 0, sizeof(sdrio_device));
        dev->device_index = device_index;

        mirisdr_open (&dev->mirics_device, dev->device_index);

        if (dev->mirics_device)
        {
            mirisdr_set_sample_format(dev->mirics_device, "AUTO");
            mirisdr_set_sample_rate(dev->mirics_device, 2*1024*1000);
            mirisdr_set_center_freq(dev->mirics_device, 100000000);
            mirisdr_set_tuner_gain_mode(dev->mirics_device, 0);

            dev->min_freq = 150000;
            dev->max_freq = 1900000000;
        }
        else
        {
            free(dev);
            return 0;
        }
    }

    return dev;
}

SDRIOEXPORT sdrio_int32 sdrio_close_device(sdrio_device *dev)
{
    if (dev)
    {
        int ret = mirisdr_close(dev->mirics_device);
        dev->mirics_device = 0;
        return (ret == 0);
    }
    else
    {
        return 0;
    }
}

SDRIOEXPORT const char * sdrio_get_device_string(sdrio_device *dev)
{
    if (dev)
    {
        return mirisdr_get_device_name(dev->device_index);
    }
    else
    {
        return 0;

    }
}

SDRIOEXPORT sdrio_int32 sdrio_set_rx_samplerate(sdrio_device *dev, sdrio_uint64 sample_rate)
{
    if (dev)
    {
        return (mirisdr_set_sample_rate(dev->mirics_device, (uint32_t)sample_rate) == 0);
    }
    else
    {
        return 0;
    }
}

SDRIOEXPORT sdrio_int32 sdrio_set_rx_frequency(sdrio_device *dev, sdrio_uint64 frequency)
{
    if (dev)
    {
        return (mirisdr_set_center_freq(dev->mirics_device, (uint32_t)frequency) == 0);
    }
    else
    {
        return 0;
    }
}

SDRIOEXPORT sdrio_int32 sdrio_set_tx_samplerate(sdrio_device *dev, sdrio_uint64 sample_rate)
{
    return 0;
}

SDRIOEXPORT sdrio_int32 sdrio_set_tx_frequency(sdrio_device *dev, sdrio_uint64 frequency)
{
    return 0;
}

void mirics_read_async_cb(unsigned char *buf, uint32_t len, void *ctx)
{
    sdrio_device *dev = (sdrio_device *)ctx;

    if (dev->callback)
    {
        sdrio_uint32 num_samples = len / sizeof(sdrio_iqi16); // 4 bytes per sample

        if (dev->num_samples != num_samples)
        {
            dev->num_samples = num_samples;

            if (dev->samples)
            {
                free(dev->samples);
            }

            dev->samples = (sdrio_iq *)malloc(dev->num_samples * sizeof(sdrio_iq));
        }

        if (dev->samples)
        {
            sdrio_iqi16 *iqbuf = (sdrio_iqi16 *)buf;
            sdrio_uint32 i;
            for (i=0; i<dev->num_samples; i++)
            {
                dev->samples[i].i = (float)iqbuf[i].i * 0.000030518509476f;
                dev->samples[i].q = (float)iqbuf[i].q * 0.000030518509476f;
            }

            dev->callback(dev->callback_context, dev->samples, dev->num_samples);
        }
    }
}

SDRIOEXPORT void * start_rx_routine(void *ctx)
{
    sdrio_device *dev = (sdrio_device *)ctx;

    if (dev)
    {
        mirisdr_reset_buffer(dev->mirics_device);
        mirisdr_read_async(dev->mirics_device, mirics_read_async_cb, dev, 32, 262144);
        pthread_exit(0);
    }

    return 0;
}

SDRIOEXPORT sdrio_int32 sdrio_start_rx(sdrio_device *dev, sdrio_rx_async_callback callback, void *context)
{
    if (dev)
    {
        dev->callback = callback;
        dev->callback_context = context;
        return pthread_create(&dev->tid, 0, start_rx_routine, (void *)dev) == 0;
    }
    else
    {
        return 0;
    }
}

SDRIOEXPORT sdrio_int32 sdrio_stop_rx(sdrio_device *dev)
{
    if (dev)
    {
        mirisdr_cancel_async(dev->mirics_device);
        mirisdr_stop_async(dev->mirics_device);
        pthread_join(dev->tid, 0);
        return 1;
    }
    else
    {
        return 0;
    }

}

SDRIOEXPORT sdrio_int32 sdrio_start_tx(sdrio_device *dev, sdrio_tx_async_callback callback, void *context)
{
    return 0;
}

SDRIOEXPORT sdrio_int32 sdrio_stop_tx(sdrio_device *dev)
{
    return 0;
}

SDRIOEXPORT sdrio_int64 sdrio_get_rx_frequency(sdrio_device *dev)
{
    if (dev)
    {
        return mirisdr_get_center_freq(dev->mirics_device);
    }
    else
    {
        return 0;
    }
}

static const sdrio_uint32 sample_rates[] = {3*512*1024, 2*1024*1024, 4*1024*1024, 23*256*1024, 123*64*1024, 35*256*1024, 23*512*1024};

SDRIOEXPORT sdrio_int32 sdrio_get_num_samplerates(sdrio_device *dev)
{
    return sizeof(sample_rates) / sizeof(sample_rates[0]);
}

SDRIOEXPORT void sdrio_get_samplerates(sdrio_device *dev, sdrio_uint32 *sample_rates_out)
{
    memcpy(sample_rates_out, sample_rates, sizeof(sample_rates));
}

SDRIOEXPORT sdrio_int64 sdrio_get_rx_samplerate(sdrio_device *dev)
{
    if (dev)
    {
        return mirisdr_get_sample_rate(dev->mirics_device);
    }
    else
    {
        return 0;
    }
}

SDRIOEXPORT sdrio_int64 sdrio_get_tx_frequency(sdrio_device *dev)
{
    return 0;
}

SDRIOEXPORT sdrio_int64 sdrio_get_tx_samplerate(sdrio_device *dev)
{
    return 0;
}

SDRIOEXPORT sdrio_int32 sdrio_set_rx_gain_mode(sdrio_device *dev, sdrio_gain_mode gain_mode)
{
    if (dev)
    {
        switch (gain_mode)
        {
        case sdrio_gain_mode_agc:
            mirisdr_set_tuner_gain_mode(dev->mirics_device, 0);
            break;
        case sdrio_gain_mode_manual:
            mirisdr_set_tuner_gain_mode(dev->mirics_device, 1);
            break;
        default:
            return 0;
        }
    }
    return 1;
}

SDRIOEXPORT sdrio_int32 sdrio_get_rx_gain_range(sdrio_device *dev, sdrio_float32 *min, sdrio_float32 *max)
{
    if (dev)
    {
        if (min) *min = 0.0f;
        if (max) *max = 102.0f;
        return 1;
    }
    else
    {
        return 0;
    }
}

SDRIOEXPORT sdrio_int32 sdrio_set_rx_gain(sdrio_device *dev, sdrio_float32 gain)
{
    if (dev)
    {
        return (mirisdr_set_tuner_gain(dev->mirics_device, (int)gain) == 0);
    }
    else
    {
        return 0;
    }
}

SDRIOEXPORT sdrio_int32 sdrio_get_tx_gain_range(sdrio_device *dev, sdrio_float32 *min, sdrio_float32 *max)
{
    return 0;
}

SDRIOEXPORT sdrio_int32 sdrio_set_tx_gain(sdrio_device *dev, sdrio_float32 gain)
{
    return 0;
}

SDRIOEXPORT void sdrio_get_tuning_range(sdrio_device *dev, sdrio_uint64 *min, sdrio_uint64 *max)
{
    if (dev)
    {
        if (min) *min = dev->min_freq;
        if (max) *max = dev->max_freq;
    }
}

SDRIOEXPORT sdrio_int32 sdrio_get_caps(sdrio_device *dev, sdrio_caps caps)
{
    switch (caps)
    {
    case sdrio_caps_rx:  return  1;
    case sdrio_caps_tx:  return  0;
    case sdrio_caps_agc: return  1;
    default:             return -1;
    }
}
