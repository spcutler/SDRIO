// Copyright Scott Cutler
// This source file is licensed under the GNU Lesser General Public License (LGPL)

#include <stdlib.h>
#include <string.h>

#include "sdrio_ext.h"

#define rtlsdr_STATIC
#include "rtl-sdr.h"

#include "pthread.h"

typedef struct sdrio_device_t
{
    sdrio_uint32 device_index;
    rtlsdr_dev_t *rtl_device;

    enum rtlsdr_tuner tuner;

    sdrio_int32 num_gains;
    sdrio_int32 *gains;

    sdrio_rx_async_callback callback;
    void *callback_context;
    pthread_t tid;

    sdrio_iq *samples;
    sdrio_uint32 num_samples;

    sdrio_uint64 min_freq;
    sdrio_uint64 max_freq;
};

typedef struct sdrio_iqu8_t
{
    sdrio_uint8 i;
    sdrio_uint8 q;
} sdrio_iqu8;


SDRIOEXPORT sdrio_int32 sdrio_init()
{
    return 1;
}

SDRIOEXPORT sdrio_int32 sdrio_get_num_devices()
{
    return rtlsdr_get_device_count();
}

SDRIOEXPORT sdrio_device * sdrio_open_device(sdrio_uint32 device_index)
{
    sdrio_device *dev = (sdrio_device *)malloc(sizeof(sdrio_device));

    if (dev)
    {
        memset(dev, 0, sizeof(sdrio_device));
        dev->device_index = device_index;

        rtlsdr_open(&dev->rtl_device, dev->device_index);

        if (dev->rtl_device)
        {
            dev->tuner = rtlsdr_get_tuner_type(dev->rtl_device);

            dev->num_gains = (sdrio_int32)rtlsdr_get_tuner_gains(dev->rtl_device, 0);
            if (dev->num_gains > 0)
            {
                dev->gains = (sdrio_int32 *)malloc(dev->num_gains * sizeof(sdrio_int32));
                rtlsdr_get_tuner_gains(dev->rtl_device, (int *)dev->gains);
            }
            else
            {
                free(dev);
                return 0;
            }

            sdrio_set_rx_samplerate(dev, 2048000);

            switch (rtlsdr_get_tuner_type(dev->rtl_device))
            {
            case RTLSDR_TUNER_E4000:  dev->min_freq = 52000000;  dev->max_freq = 2200000000; break;
            case RTLSDR_TUNER_FC0012: dev->min_freq = 22000000;  dev->max_freq = 948600000; break;
            case RTLSDR_TUNER_FC0013: dev->min_freq = 22000000;  dev->max_freq = 1100000000; break;
            case RTLSDR_TUNER_FC2580: dev->min_freq = 146000000; dev->max_freq = 924000000; break;
            case RTLSDR_TUNER_R820T:  dev->min_freq = 24000000;  dev->max_freq = 1766000000; break;
            case RTLSDR_TUNER_R828D:  dev->min_freq = 24000000;  dev->max_freq = 1766000000; break;
            default: dev->min_freq = 0; dev->max_freq = 0; break;
            }
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
    if (dev && dev->rtl_device)
    {
        int r = rtlsdr_close(dev->rtl_device);
        dev->rtl_device = 0;
        return r;
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
        return rtlsdr_get_device_name(dev->device_index);
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
        return rtlsdr_set_sample_rate(dev->rtl_device, (uint32_t)sample_rate);
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
        return rtlsdr_set_center_freq(dev->rtl_device, (uint32_t)frequency);
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

void rtlsdr_read_async_cb(unsigned char *buf, uint32_t len, void *ctx)
{
    sdrio_device *dev = (sdrio_device *)ctx;

    if (dev->callback)
    {
        sdrio_uint32 num_samples = len / 2; // 2 bytes per sample

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
            sdrio_iqu8 *iqbuf = (sdrio_iqu8 *)buf;
            sdrio_uint32 i;
            for (i=0; i<dev->num_samples; i++)
            {
                dev->samples[i].i = ((float)iqbuf[i].i - 127.5f) * 0.0078431373f;
                dev->samples[i].q = ((float)iqbuf[i].q - 127.5f) * 0.0078431373f;
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
        rtlsdr_reset_buffer(dev->rtl_device);
        rtlsdr_read_async(dev->rtl_device, rtlsdr_read_async_cb, (void *)dev, 0, 65536);
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
        rtlsdr_cancel_async(dev->rtl_device);
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
        return rtlsdr_get_center_freq(dev->rtl_device);
    }
    else
    {
        return 0;
    }
}

static const sdrio_uint32 sample_rates[] = {1024000, 1800000, 1920000, 2048000, 2400000, 2600000, 2800000, 3000000, 3200000};

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
        return rtlsdr_get_sample_rate(dev->rtl_device);
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
            rtlsdr_set_agc_mode(dev->rtl_device, 0);
            rtlsdr_set_tuner_gain_mode(dev->rtl_device, 0);
            break;
        case sdrio_gain_mode_manual:
            rtlsdr_set_agc_mode(dev->rtl_device, 0);
            rtlsdr_set_tuner_gain_mode(dev->rtl_device, 1);
            break;
        default:
            return 0;
        }
    }
    return 1;
}

SDRIOEXPORT sdrio_int32 sdrio_get_rx_gain_range(sdrio_device *dev, sdrio_float32 *min, sdrio_float32 *max)
{
    if (dev && dev->gains)
    {
        if (min)
        {
            *min = 0.1f * (float)dev->gains[0];
        }

        if (max)
        {
            *max = 0.1f * (float)dev->gains[dev->num_gains-1];
        }

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
        sdrio_int32 i;
        for (i=dev->num_gains-1; i>=0; i--)
        {
            if ((sdrio_int32)(gain * 10.0f) >= dev->gains[i])
            {
                rtlsdr_set_tuner_gain(dev->rtl_device, dev->gains[i]);
                return 1;
            }
        }

        if (dev->num_gains)
        {
            rtlsdr_set_tuner_gain(dev->rtl_device, dev->gains[0]);
            return 1;
        }
        else
        {
            return 0;
        }
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
