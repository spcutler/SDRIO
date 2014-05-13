// Copyright Scott Cutler
// This source file is licensed under the GNU Lesser General Public License (LGPL)

#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <stdio.h>
#include <iostream>
#include <fstream>

#include "sdrio_ext.h"

#include "pthread.h"

#include "libbladeRF.h"

extern "C" {

struct sdrio_device_t
{
    sdrio_uint32 device_index;

    bladerf *bladerf_device;

    struct
    {
        sdrio_rx_async_callback callback;
        void *callback_context;
        pthread_t tid;
        volatile bool done;

        sdrio_iq *samples;
        sdrio_uint32 num_samples;

        void **buffers;
        sdrio_uint32 num_buffers;
        sdrio_uint32 buffer_index;

        sdrio_uint64 frequency;
        sdrio_uint64 sample_rate;
    } rx, tx;
};

typedef struct sdrio_iqu8_t
{
    sdrio_uint8 i;
    sdrio_uint8 q;
} sdrio_iqu8;

bladerf_devinfo *g_devinfo = 0;
int g_num_devices = 0;

SDRIOEXPORT sdrio_int32 sdrio_init()
{
    HMODULE hLib;
    if ((hLib = LoadLibrary("bladeRF.dll")) == NULL)
    {
        return 0;
    }
    else
    {
        FreeLibrary(hLib);
        g_num_devices = bladerf_get_device_list(&g_devinfo);
        return (g_num_devices > 0);
    }
}

SDRIOEXPORT sdrio_int32 sdrio_get_num_devices()
{
    return g_num_devices;
}

SDRIOEXPORT sdrio_device * sdrio_open_device(sdrio_uint32 device_index)
{
    if (g_num_devices > 0)
    {
        sdrio_device *dev = (sdrio_device *)malloc(sizeof(sdrio_device));
        memset(dev, 0, sizeof(sdrio_device));

        if (dev)
        {
            dev->bladerf_device = 0;
            int ret = bladerf_open_with_devinfo(&dev->bladerf_device, &g_devinfo[0]);

            bladerf_fpga_size size = BLADERF_FPGA_UNKNOWN;
            if (!ret)
            {
                ret = bladerf_get_fpga_size(dev->bladerf_device, &size);
                if (!ret && (size == BLADERF_FPGA_UNKNOWN))
                {
                    size = BLADERF_FPGA_40KLE;
                }
            }

            if (!ret)
            {
                std::string fpgaFile;

                switch (size)
                {
                case BLADERF_FPGA_40KLE:  fpgaFile = "hostedx40.rbf"; break;
                case BLADERF_FPGA_115KLE: fpgaFile = "hostedx115.rbf"; break;
                default:
                case BLADERF_FPGA_UNKNOWN: return 0;
                }

                //struct stat fileStat;
                std::string fpgaPath;
                std::string programFilesx86 = getenv("ProgramFiles(x86)");
                std::string programFiles    = getenv("ProgramFiles");

                std::string searchPaths[] = {
                    "",
                    programFilesx86 + "\\bladeRF\\",
                    programFiles    + "\\bladeRF\\"};

                int numPathsToSearch = sizeof(searchPaths) / sizeof(searchPaths[0]);
                int pathIndex = 0;
                for (pathIndex=0; pathIndex<numPathsToSearch; pathIndex++)
                {
                    fpgaPath = std::string(searchPaths[pathIndex]) + fpgaFile;
                    std::ifstream ifs(fpgaPath.c_str());
                    if (ifs.good())
                    {
                        break;
                    }
                }

                if (pathIndex < numPathsToSearch)
                {
                    ret = bladerf_load_fpga(dev->bladerf_device, fpgaPath.c_str());
                }
            }

            // get rid of signal mirroring.  why?  see:
            // https://www.nuand.com/forums/viewtopic.php?f=4&t=2917&sid=3301bfb47f19cbf797054e055639f361&start=10#p3619
            //ret = bladerf_lms_write(dev->bladerf_device, 0x5a, 0xa0);

            sdrio_set_rx_samplerate(dev, 2*1024*1024);

            ret = ret;
        }

        return dev;
    }

    return 0;
}

SDRIOEXPORT sdrio_int32 sdrio_close_device(sdrio_device *dev)
{
    if (dev)
    {
        bladerf_close(dev->bladerf_device);
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
        return "bladeRF";
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
        unsigned int actual_rate = 0;
        int ret = bladerf_set_sample_rate(dev->bladerf_device, BLADERF_MODULE_RX, (unsigned int)sample_rate, &actual_rate);
        dev->rx.sample_rate = (sdrio_uint64)actual_rate;

        unsigned int actual_bandwidth = 0;
        ret = bladerf_set_bandwidth(dev->bladerf_device, BLADERF_MODULE_RX, (unsigned int)sample_rate, &actual_bandwidth);

        return (ret >= 0);
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
        dev->rx.frequency = frequency;
        int ret = bladerf_set_frequency(dev->bladerf_device, BLADERF_MODULE_RX, (unsigned int)dev->rx.frequency);
        return (ret >= 0);
    }
    else
    {
        return 0;
    }
}

SDRIOEXPORT sdrio_int32 sdrio_set_tx_samplerate(sdrio_device *dev, sdrio_uint64 sample_rate)
{
    if (dev)
    {
        unsigned int actual_rate = 0;
        int ret = bladerf_set_sample_rate(dev->bladerf_device, BLADERF_MODULE_TX, (unsigned int)sample_rate, &actual_rate);
        dev->tx.sample_rate = (sdrio_uint64)actual_rate;

        unsigned int actual_bandwidth = 0;
        ret = bladerf_set_bandwidth(dev->bladerf_device, BLADERF_MODULE_TX, (unsigned int)sample_rate, &actual_bandwidth);

        return (ret >= 0);
    }
    else
    {
        return 0;
    }
}

SDRIOEXPORT sdrio_int32 sdrio_set_tx_frequency(sdrio_device *dev, sdrio_uint64 frequency)
{
    if (dev)
    {
        dev->tx.frequency = frequency;
        int ret = bladerf_set_frequency(dev->bladerf_device, BLADERF_MODULE_TX, (unsigned int)dev->tx.frequency);
        return (ret >= 0);
    }
    else
    {
        return 0;
    }
}

typedef struct sdrio_iqi16_t
{
    int16_t i;
    int16_t q;
} sdrio_iqi16;

void * bladerf_stream_rx_callback(struct bladerf *bladerf_device, struct bladerf_stream *stream, struct bladerf_metadata *meta, void *samples, size_t num_samples, void *user_data)
{
    sdrio_device *dev = (sdrio_device *)user_data;

    if (dev)
    {
        if (dev->rx.num_samples != num_samples)
        {
            dev->rx.num_samples = num_samples;

            if (dev->rx.samples)
            {
                free(dev->rx.samples);
            }

            dev->rx.samples = (sdrio_iq *)malloc(dev->rx.num_samples * sizeof(sdrio_iq));
        }

        if (dev->rx.samples)
        {
            sdrio_iqi16 *iqbuf = (sdrio_iqi16 *)samples;
            sdrio_uint32 i;

            for (i=0; i<dev->rx.num_samples; i++)
            {
                dev->rx.samples[i].i = (float)(int16_t)(iqbuf[i].i << 4) * (0.000030517578125f);
                dev->rx.samples[i].q = (float)(int16_t)(iqbuf[i].q << 4) * (0.000030517578125f);
                //dev->rx.samples[i].i = (float)(int16_t)(iqbuf[i].q << 4) * (0.000030517578125f);
                //dev->rx.samples[i].q = (float)(int16_t)(iqbuf[i].i << 4) * (0.000030517578125f);
            }

            dev->rx.callback(dev->rx.callback_context, dev->rx.samples, dev->rx.num_samples);
        }

        if (!dev->rx.done)
        {
            void *rv = dev->rx.buffers[dev->rx.buffer_index];
            dev->rx.buffer_index = (dev->rx.buffer_index + 1) % dev->rx.num_buffers;
            return rv;
        }
    }

    return 0;
}

SDRIOEXPORT void * start_rx_routine(void *ctx)
{
    sdrio_device *dev = (sdrio_device *)ctx;

    if (dev)
    {
        struct bladerf_stream *stream = 0;
        dev->rx.buffers = 0;
        dev->rx.num_buffers = 32;
        bladerf_format format = BLADERF_FORMAT_SC16_Q12;
        size_t num_samples = 32768;
        size_t num_transfers = 16;
        int ret = bladerf_init_stream(
            &stream,
            dev->bladerf_device,
            bladerf_stream_rx_callback,
            &dev->rx.buffers,
            dev->rx.num_buffers,
            format,
            num_samples,
            num_transfers,
            ctx);

        bladerf_stream(stream, BLADERF_MODULE_RX);
    }

    return 0;
}

SDRIOEXPORT sdrio_int32 sdrio_start_rx(sdrio_device *dev, sdrio_rx_async_callback callback, void *context)
{
    if (dev)
    {
        dev->rx.callback = callback;
        dev->rx.callback_context = context;
        bladerf_enable_module(dev->bladerf_device, BLADERF_MODULE_RX, true);
        return pthread_create(&dev->rx.tid, 0, start_rx_routine, (void *)dev) == 0;
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
        dev->rx.done = 1;
        pthread_join(dev->rx.tid, 0);
        bladerf_enable_module(dev->bladerf_device, BLADERF_MODULE_RX, false);
        return 1;
    }
    else
    {
        return 0;
    }
}

void * bladerf_stream_tx_callback(struct bladerf *bladerf_device, struct bladerf_stream *stream, struct bladerf_metadata *meta, void *samples, size_t num_samples, void *user_data)
{
    sdrio_device *dev = (sdrio_device *)user_data;

    if (dev)
    {
        if (dev->tx.num_samples != num_samples)
        {
            dev->tx.num_samples = num_samples;

            if (dev->tx.samples)
            {
                free(dev->tx.samples);
            }

            dev->tx.samples = (sdrio_iq *)malloc(dev->tx.num_samples * sizeof(sdrio_iq));
        }

        if (dev->tx.samples && samples)
        {
            dev->tx.callback(dev->tx.callback_context, dev->tx.samples, dev->tx.num_samples);

            sdrio_iqi16 *iqbuf = (sdrio_iqi16 *)samples;
            sdrio_uint32 i;

            for (i=0; i<dev->tx.num_samples; i++)
            {
                iqbuf[i].i = (sdrio_uint16)(sdrio_int16)(dev->tx.samples[i].i * 32767.0f) >> 4;
                iqbuf[i].q = (sdrio_uint16)(sdrio_int16)(dev->tx.samples[i].q * 32767.0f) >> 4;
            }
        }

        if (!dev->tx.done)
        {
            void *rv = dev->tx.buffers[dev->tx.buffer_index];
            dev->tx.buffer_index = (dev->tx.buffer_index + 1) % dev->tx.num_buffers;
            return rv;
        }
    }

    return 0;
}

SDRIOEXPORT void * start_tx_routine(void *ctx)
{
    sdrio_device *dev = (sdrio_device *)ctx;

    if (dev)
    {
        struct bladerf_stream *stream = 0;
        dev->tx.buffers = 0;
        dev->tx.num_buffers = 32;
        bladerf_format format = BLADERF_FORMAT_SC16_Q12;
        size_t num_samples = 65536;
        size_t num_transfers = 16;
        int ret = bladerf_init_stream(
            &stream,
            dev->bladerf_device,
            bladerf_stream_tx_callback,
            &dev->tx.buffers,
            dev->tx.num_buffers,
            format,
            num_samples,
            num_transfers,
            ctx);

        bladerf_stream(stream, BLADERF_MODULE_TX);
    }

    return 0;
}

SDRIOEXPORT sdrio_int32 sdrio_start_tx(sdrio_device *dev, sdrio_tx_async_callback callback, void *context)
{
    if (dev)
    {
        dev->tx.callback = callback;
        dev->tx.callback_context = context;
        bladerf_enable_module(dev->bladerf_device, BLADERF_MODULE_TX, true);
        return pthread_create(&dev->tx.tid, 0, start_tx_routine, (void *)dev) == 0;
    }
    else
    {
        return 0;
    }
}

SDRIOEXPORT sdrio_int32 sdrio_stop_tx(sdrio_device *dev)
{
    if (dev)
    {
        dev->tx.done = 1;
        pthread_join(dev->tx.tid, 0);
        bladerf_enable_module(dev->bladerf_device, BLADERF_MODULE_TX, false);
        return 1;
    }
    else
    {
        return 0;
    }
}

SDRIOEXPORT sdrio_int64 sdrio_get_rx_frequency(sdrio_device *dev)
{
    if (dev)
    {
        return dev->rx.frequency;
    }
    else
    {
        return 0;
    }
}

static const sdrio_uint32 sample_rates[] = {1024*1024, 1536*1024, 2*1024*1024, 3*1024*1024, 4*1024*1024, 6*1024*1024, 8*1024*1024, 12*1024*1024, 16*1024*1024};

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
        return dev->rx.sample_rate;
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
            break;
        case sdrio_gain_mode_manual:
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
        if (min)
        {
            *min = 0;
        }

        if (max)
        {
            *max = 67;
        }

        return 1;
    }
    else
    {
        return 0;
    }
}

SDRIOEXPORT sdrio_int32 sdrio_set_rx_gain(sdrio_device *dev, float gain)
{
    if (dev)
    {
        sdrio_int32 int_gain = (sdrio_int32)gain;
        if (int_gain >= 6)
        {
            bladerf_set_lna_gain(dev->bladerf_device, BLADERF_LNA_GAIN_MAX);
            int_gain -= 6;
        }
        else
        {
            bladerf_set_lna_gain(dev->bladerf_device, BLADERF_LNA_GAIN_BYPASS);
        }

        if (int_gain <= 25)
        {
            bladerf_set_rxvga2(dev->bladerf_device, 0);
            bladerf_set_rxvga1(dev->bladerf_device, int_gain * 4);
            bladerf_set_lpf_mode(dev->bladerf_device, BLADERF_MODULE_RX, BLADERF_LPF_BYPASSED);
        }
        else if (int_gain <= 31)
        {
            bladerf_set_rxvga2(dev->bladerf_device, 0);
            bladerf_set_rxvga1(dev->bladerf_device, (int_gain - 6) * 4);
            bladerf_set_lpf_mode(dev->bladerf_device, BLADERF_MODULE_RX, BLADERF_LPF_NORMAL);
        }
        else
        {
            sdrio_int32 excess = int_gain - 31;
            sdrio_int32 excess_rounded = (excess + 2) / 3 * 3;
            bladerf_set_rxvga2(dev->bladerf_device, excess_rounded);
            int_gain -= excess_rounded;
            bladerf_set_rxvga1(dev->bladerf_device, (int_gain - 6) * 4);
            bladerf_set_lpf_mode(dev->bladerf_device, BLADERF_MODULE_RX, BLADERF_LPF_NORMAL);
        }

        return 1;
    }
    else
    {
        return 0;
    }
}

SDRIOEXPORT sdrio_int32 sdrio_get_tx_gain_range(sdrio_device *dev, float *min, float *max)
{
    if (dev)
    {
        if (min)
        {
            *min = -35.0f;
        }

        if (max)
        {
            *max = 21.0f;
        }

        return 1;
    }

    return 0;
}

SDRIOEXPORT sdrio_int32 sdrio_set_tx_gain(sdrio_device *dev, float gain)
{
    if (dev)
    {
        if (gain > -4.0f)
        {
            bladerf_set_txvga1(dev->bladerf_device, -4);
            bladerf_set_txvga2(dev->bladerf_device, (int)gain + 4);
        }
        else
        {
            bladerf_set_txvga1(dev->bladerf_device, (int)gain);
            bladerf_set_txvga2(dev->bladerf_device, 0);
        }
        return 1;
    }

    return 0;
}

SDRIOEXPORT void sdrio_get_tuning_range(sdrio_device *dev, sdrio_uint64 *min, sdrio_uint64 *max)
{
    if (dev)
    {
        if (min) *min =  300000000;
        if (max) *max = 3800000000;
    }
}

SDRIOEXPORT sdrio_int32 sdrio_get_caps(sdrio_device *dev, sdrio_caps caps)
{
    switch (caps)
    {
    case sdrio_caps_rx:  return  1;
    case sdrio_caps_tx:  return  1;
    case sdrio_caps_agc: return  0;
    default:             return -1;
    }
}


}