// Copyright Scott Cutler
// This source file is licensed under the GNU Lesser General Public License (LGPL)

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "sdrio_ext.h"

#include "pthread.h"

#define MIN_FREQ 5000000
#define MAX_FREQ 4000000000

#define MIN_GAIN 0.0f
#define MAX_GAIN 60.0f

#define NUM_SAMPLES 16384

#include <Windows.h>
#include <time.h>

sdrio_float64 get_time()
{
    LARGE_INTEGER f, t;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&t);
    return (sdrio_float64)t.QuadPart / f.QuadPart;
}

typedef struct sdrio_device_t
{
    volatile sdrio_uint8 running;
    sdrio_uint64 rx_freq;

    sdrio_rx_async_callback callback;
    void *callback_context;
    pthread_t tid;

    sdrio_iq *samples;
    sdrio_uint64 sample_rate;

    sdrio_float32 gain;

    sdrio_uint64 samples_since_last_rate_change;
    sdrio_float64 timestamp_at_last_rate_change;
};

SDRIOEXPORT sdrio_int32 sdrio_init()
{
    return 1;
}

SDRIOEXPORT sdrio_int32 sdrio_get_num_devices()
{
    return 1;
}

SDRIOEXPORT sdrio_device * sdrio_open_device(sdrio_uint32 device_index)
{
    sdrio_device *dev = (sdrio_device *)malloc(sizeof(sdrio_device));

    if (dev)
    {
        memset(dev, 0, sizeof(sdrio_device));
        dev->rx_freq = 100000000;
        dev->sample_rate = 1024*1024;

        dev->samples_since_last_rate_change = 0;
        dev->timestamp_at_last_rate_change = get_time();
    }

    return dev;
}

SDRIOEXPORT sdrio_int32 sdrio_close_device(sdrio_device *dev)
{
    if (dev)
    {
        free(dev);
        return 1;
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
        return "Null device (WARNING: The null device is only used when no hardware is available.  If you have SDR hardware, ensure the drivers are correctly installed.)";
    }
    else
    {
        return 0;

    }
}

static const sdrio_uint32 sample_rates[] = {192000, 1024*1024, 2*1024*1024, 4*1024*1024};

SDRIOEXPORT sdrio_int32 sdrio_set_rx_samplerate(sdrio_device *dev, sdrio_uint64 sample_rate)
{
    if (dev)
    {
        dev->sample_rate = sample_rate;
        dev->samples_since_last_rate_change = 0;
        dev->timestamp_at_last_rate_change = get_time();
        return 1;
    }
    else
    {
        return 0;
    }
}

SDRIOEXPORT sdrio_int32 sdrio_set_rx_frequency(sdrio_device *dev, sdrio_uint64 frequency)
{
    if (dev && (frequency >= MIN_FREQ) && (frequency <= MAX_FREQ))
    {
        dev->rx_freq = frequency;
        return 1;
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

sdrio_float32 rand_minus_one_to_one()
{
    sdrio_float32 zero_to_one = rand() * (1.0f / RAND_MAX);
    return (zero_to_one * 2.0f) - 1.0f;
}

SDRIOEXPORT void * start_rx_routine(void *ctx)
{
    sdrio_device *dev = (sdrio_device *)ctx;

    while (dev->running)
    {
        if (dev->callback)
        {
            if (!dev->samples)
            {
                dev->samples = (sdrio_iq *)malloc(NUM_SAMPLES * sizeof(sdrio_iq));
            }

            if (dev->samples)
            {
                sdrio_uint32 i;
                for (i=0; i<NUM_SAMPLES; i++)
                {
                    dev->samples[i].i = rand_minus_one_to_one() * dev->gain;
                    dev->samples[i].q = rand_minus_one_to_one() * dev->gain;
                }

                dev->callback(dev->callback_context, dev->samples, NUM_SAMPLES);
                dev->samples_since_last_rate_change += NUM_SAMPLES;
            }

            while ((dev->samples_since_last_rate_change / (get_time() - dev->timestamp_at_last_rate_change)) > dev->sample_rate)
            {
                Sleep(1);
            }
        }
    }

    return 0;
}

SDRIOEXPORT sdrio_int32 sdrio_start_rx(sdrio_device *dev, sdrio_rx_async_callback callback, void *context)
{
    if (dev)
    {
        dev->running = 1;
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
        dev->running = 0;
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
        return dev->rx_freq;
    }
    else
    {
        return 0;
    }
}

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
        return dev->sample_rate;
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
    return 1;
}

SDRIOEXPORT sdrio_int32 sdrio_get_rx_gain_range(sdrio_device *dev, sdrio_float32 *min, sdrio_float32 *max)
{
    if (dev)
    {
        if (min)
        {
            *min = MIN_GAIN;
        }

        if (max)
        {
            *max = MAX_GAIN;
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
    if (dev && (gain >= MIN_GAIN) && (gain <= MAX_GAIN))
    {
        dev->gain = (sdrio_float32)pow(10, (gain - MAX_GAIN) * 0.05f);
        return 1;
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
        if (min) *min = MIN_FREQ;
        if (max) *max = MAX_FREQ;
    }
}

SDRIOEXPORT sdrio_int32 sdrio_get_caps(sdrio_device *dev, sdrio_caps caps)
{
    switch (caps)
    {
    case sdrio_caps_rx:  return  1;
    case sdrio_caps_tx:  return  0;
    case sdrio_caps_agc: return  0;
    default:             return -1;
    }
}
