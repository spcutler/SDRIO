// Copyright Scott Cutler
// This source file is licensed under the GNU Lesser General Public License (LGPL)

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#include "sdrio_ext.h"

#include "hackrf.h"

#include "pthread.h"

#define MIN_FREQ 30000000
#define MAX_FREQ 4000000000

#define MIN_GAIN 0.0f
#define MAX_GAIN 102.0f

typedef struct sdrio_device_t
{
    hackrf_device *hackrf_device;
    sdrio_uint64 rx_freq;
    sdrio_uint32 sample_rate;

    sdrio_rx_async_callback callback;
    void *callback_context;
    pthread_t tid;

    sdrio_iq *samples;
    sdrio_uint32 num_samples;
};

typedef struct sdrio_iqu8_t
{
    sdrio_int8 i;
    sdrio_int8 q;
} sdrio_iqi8;

SDRIOEXPORT sdrio_int32 sdrio_init()
{
    return (hackrf_init() == HACKRF_SUCCESS);
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
        dev->sample_rate = 8000000;

        if (hackrf_open(&dev->hackrf_device) != HACKRF_SUCCESS)
        {
            free(dev);
            return 0;
        }

        hackrf_set_sample_rate(dev->hackrf_device, dev->sample_rate);
    }

    return dev;
}

SDRIOEXPORT sdrio_int32 sdrio_close_device(sdrio_device *dev)
{
    if (dev)
    {
        hackrf_close(dev->hackrf_device);
        hackrf_exit();
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
    static char device_string[256];
    if (dev)
    {
        uint8_t board_id = BOARD_ID_INVALID;
        char hackrf_version[255];

        hackrf_board_id_read(dev->hackrf_device, &board_id);
        hackrf_version_string_read(dev->hackrf_device, hackrf_version, sizeof(hackrf_version));

        sprintf_s(device_string, sizeof(device_string), "hackRF: board=%s, version=%s", hackrf_board_id_name((enum hackrf_board_id)board_id), hackrf_version);
        return device_string;
    }
    else
    {
        return 0;
    }
}

static const sdrio_uint32 sample_rates[] = {8000000, 10000000, 12500000, 16000000, 20000000};

SDRIOEXPORT sdrio_int32 sdrio_set_rx_samplerate(sdrio_device *dev, sdrio_uint64 sample_rate)
{
    if (dev)
    {
        dev->sample_rate = (sdrio_uint32)sample_rate;
        return (hackrf_set_sample_rate(dev->hackrf_device, (double)sample_rate) == HACKRF_SUCCESS);
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
        return (hackrf_set_freq(dev->hackrf_device, frequency) == HACKRF_SUCCESS);
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

int hackrf_sample_block_callback(hackrf_transfer* transfer)
{
    sdrio_device *dev = (sdrio_device *)transfer->rx_ctx;

    if (dev)
    {
        sdrio_uint32 num_samples = transfer->buffer_length / 2;

        if (!dev->samples)
        {
            dev->samples = (sdrio_iq *)malloc(num_samples * sizeof(sdrio_iq));
        }
        else if (dev->num_samples != num_samples)
        {
            free(dev->samples);
            dev->samples = (sdrio_iq *)malloc(num_samples * sizeof(sdrio_iq));
        }

        if (dev->samples)
        {
            sdrio_uint32 i;
            sdrio_iqi8 *samples = (sdrio_iqi8 *)transfer->buffer;
            for (i=0; i<num_samples; i++)
            {
                dev->samples[i].i = (sdrio_float32)samples[i].i * (1.0f / 127.0f);
                dev->samples[i].q = (sdrio_float32)samples[i].q * (1.0f / 127.0f);
            }

            dev->callback(dev->callback_context, dev->samples, dev->num_samples);
        }
    }

    return HACKRF_SUCCESS;
}


SDRIOEXPORT sdrio_int32 sdrio_start_rx(sdrio_device *dev, sdrio_rx_async_callback callback, void *context)
{
    if (dev)
    {
        dev->callback = callback;
        dev->callback_context = context;

        return (hackrf_start_rx(dev->hackrf_device, hackrf_sample_block_callback, context) == HACKRF_SUCCESS);
    }

    return 0;
}

SDRIOEXPORT sdrio_int32 sdrio_stop_rx(sdrio_device *dev)
{
    if (dev)
    {
        return (hackrf_stop_rx(dev->hackrf_device) == HACKRF_SUCCESS);
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
        if (min) *min = MIN_GAIN;
        if (max) *max = MAX_GAIN;

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
        sdrio_uint32 remaining_gain = (sdrio_uint32)gain;
        sdrio_uint32 lna_gain = 0;
        sdrio_uint32 vga_gain = 0;

        while ((remaining_gain >= 8) && (lna_gain < 40))
        {
            remaining_gain -= 8;
            lna_gain += 8;
        }

        while ((remaining_gain >= 2) && (vga_gain < 62))
        {
            remaining_gain -= 2;
            vga_gain += 2;
        }

        hackrf_set_lna_gain(dev->hackrf_device, lna_gain);
        hackrf_set_vga_gain(dev->hackrf_device, vga_gain);

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
