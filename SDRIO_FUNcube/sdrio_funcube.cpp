// Copyright Scott Cutler
// This source file is licensed under the GNU Lesser General Public License (LGPL)

#include <Windows.h>
#include <stdlib.h>
#include <string.h>
#include <mmsystem.h>
#include <setupapi.h>
#include <devguid.h>

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "setupapi.lib")

#include "../sdrio/sdrio_ext.h"
#include "pthread.h"

#define CT_ASSERT(e) typedef char __CT_ASSERT__[(e)?1:-1]

//static TCHAR _szVIDPID[]=_T("Vid_04d8&Pid_fb31");

#define FCD_HID_CMD_QUERY              1 // Returns string with "FCDAPP version"

#define FCD_HID_CMD_SET_FREQUENCY    100 // Send with 3 byte unsigned little endian frequency in kHz.
#define FCD_HID_CMD_SET_FREQUENCY_HZ 101 // Send with 4 byte unsigned little endian frequency in Hz, returns with actual frequency set in Hz
#define FCD_HID_CMD_GET_FREQUENCY_HZ 102 // Returns 4 byte unsigned little endian frequency in Hz.

#define FCD_HID_CMD_SET_LNA_GAIN     110 // Send one byte, 1 on, 0 off
#define FCD_HID_CMD_SET_RF_FILTER    113 // Send one byte enum, see TUNERRFFILTERENUM
#define FCD_HID_CMD_SET_MIXER_GAIN   114 // Send one byte, 1 on, 0 off
#define FCD_HID_CMD_SET_IF_GAIN      117 // Send one byte value, valid value 0 to 59 (dB)
#define FCD_HID_CMD_SET_IF_FILTER    122 // Send one byte enum, see TUNERIFFILTERENUM
#define FCD_HID_CMD_SET_BIAS_TEE     126 // Send one byte, 1 on, 0 off

#define FCD_HID_CMD_GET_LNA_GAIN     150 // Returns one byte, 1 on, 0 off
#define FCD_HID_CMD_GET_RF_FILTER    153 // Returns one byte enum, see TUNERRFFILTERENUM
#define FCD_HID_CMD_GET_MIXER_GAIN   154 // Returns one byte, 1 on, 0 off
#define FCD_HID_CMD_GET_IF_GAIN      157 // Returns one byte value, valid value 0 to 59 (dB)
#define FCD_HID_CMD_GET_IF_FILTER    162 // Returns one byte enum, see TUNERIFFILTERENUM
#define FCD_HID_CMD_GET_BIAS_TEE     166 // Returns one byte, 1 on, 0 off

#define FCD_RESET                    255 // Reset to bootloader

#define FCD_HID_PACKET_SIZE 65
#define FCD_MIN_IF_GAIN 0
#define FCD_MAX_IF_GAIN 29

#pragma pack(push, funcube_hid_packet_defs, 1)
struct funcube_hid_response_packet
{
    funcube_hid_response_packet() { memset(this, 0, sizeof(funcube_hid_response_packet)); }

    sdrio_uint8 dummy1[2];
    sdrio_uint8 success;
    sdrio_uint8 dummy2[62];
};
CT_ASSERT(sizeof(funcube_hid_response_packet) == FCD_HID_PACKET_SIZE);

struct funcube_hid_set_freq_packet
{
    funcube_hid_set_freq_packet(sdrio_uint32 frequency)
    {
        memset(this, 0, sizeof(funcube_hid_set_freq_packet));
        reportID = 0;
        command = FCD_HID_CMD_SET_FREQUENCY_HZ;
        freq = frequency;
    }

    sdrio_uint8 reportID;
    sdrio_uint8 command;
    sdrio_uint32 freq;
    sdrio_uint8 dummy[59];
};
CT_ASSERT(sizeof(funcube_hid_set_freq_packet) == FCD_HID_PACKET_SIZE);

struct funcube_hid_set_if_gain_packet
{
    funcube_hid_set_if_gain_packet(sdrio_uint32 gain)
    {
        memset(this, 0, sizeof(funcube_hid_set_if_gain_packet));
        reportID = 0;
        command = FCD_HID_CMD_SET_IF_GAIN;
        if_gain = (sdrio_uint8)min(FCD_MAX_IF_GAIN, max(FCD_MIN_IF_GAIN, gain));
    }

    sdrio_uint8 reportID;
    sdrio_uint8 command;
    sdrio_uint8 if_gain;
    sdrio_uint8 dummy[62];
};
CT_ASSERT(sizeof(funcube_hid_set_if_gain_packet) == FCD_HID_PACKET_SIZE);

#pragma pack(pop, funcube_hid_packet_defs)

extern "C" {

#define NUM_BUFFERS 2
#define SAMPLES_PER_BUFFER 4096

struct sdrio_device_t
{
    sdrio_uint32 device_id;
    HWAVEIN      hWaveIn;
    WAVEHDR      waveInHdr[NUM_BUFFERS];
    WAVEFORMATEX format;

    pthread_t     rx_tid;
    volatile bool rx_done;
    sdrio_uint64  rx_freq;

    sdrio_uint32 rx_current_buffer;

    sdrio_rx_async_callback rx_callback;
    void                   *rx_context;

    HANDLE hidRead;
    HANDLE hidWrite;

};

bool funcube_hidopen(sdrio_device_t *dev)
{
    static char funCubeVIDPID[] = "vid_04d8&pid_fb31";
    _strlwr_s(funCubeVIDPID);

	GUID guid = {0x4d1e55b2, 0xf16f, 0x11cf, 0x88, 0xcb, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30}; 

    HDEVINFO hDevInfo = SetupDiGetClassDevs(&guid, 0, 0, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

    if (hDevInfo == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    sdrio_uint32 devIndex = 0;
	SP_DEVINFO_DATA devInfoData;
    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    while (SetupDiEnumDeviceInfo(hDevInfo, devIndex, &devInfoData))
    {
		DWORD DataT;
        char temp[1] = {0};
		DWORD buffersize = 0;
	    BOOL ret = SetupDiGetDeviceRegistryProperty(hDevInfo, &devInfoData, SPDRP_HARDWAREID, &DataT, (PBYTE)temp, sizeof(temp), &buffersize);

        char *devRegProp = new char[buffersize];
        DWORD len = 0;
	    SetupDiGetDeviceRegistryProperty(hDevInfo, &devInfoData, SPDRP_HARDWAREID, &DataT, (PBYTE)devRegProp, buffersize, &len);

        _strlwr_s(devRegProp, buffersize);
       
		if (strstr(devRegProp, funCubeVIDPID))
        {
			SP_DEVICE_INTERFACE_DATA devIntData;

			devIntData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
			if (SetupDiEnumDeviceInterfaces(hDevInfo, NULL, &guid, devIndex, &devIntData))
			{
				DWORD diddSize = 0;
				SetupDiGetDeviceInterfaceDetail(hDevInfo, &devIntData, NULL, 0, &diddSize, NULL);	

                SP_DEVICE_INTERFACE_DETAIL_DATA *pDevIntDetData = (SP_DEVICE_INTERFACE_DETAIL_DATA *)(new char[diddSize]);

				pDevIntDetData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

				SetupDiGetDeviceInterfaceDetail(hDevInfo, &devIntData, pDevIntDetData, diddSize, NULL, NULL);

                dev->hidWrite = CreateFile(pDevIntDetData->DevicePath, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);
				if (GetLastError() != ERROR_SUCCESS)
				{
                    delete [] pDevIntDetData;
					goto next_device;
				}

                dev->hidRead = CreateFile(pDevIntDetData->DevicePath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);
				if (GetLastError() != ERROR_SUCCESS)
				{
                    delete [] pDevIntDetData;
					goto next_device;
				}

                delete [] pDevIntDetData;
                delete [] devRegProp;
       			SetupDiDestroyDeviceInfoList(hDevInfo);

                return true;
			}
        }

next_device:
        delete [] devRegProp;
        devIndex++;
    }

    return false;
}

sdrio_uint32 g_device_id = ~0;
sdrio_uint32 g_num_devices = 0;

typedef struct sdrio_iqu8_t
{
	sdrio_int16 i;
	sdrio_int16 q;
} sdrio_iqi16;

SDRIOEXPORT sdrio_int32 sdrio_init()
{
    sdrio_uint32 num_wave_devices = waveInGetNumDevs();

    for (sdrio_uint32 i=0; i<num_wave_devices; i++)
    {
        WAVEINCAPS waveInCaps;   
        waveInGetDevCaps(i, &waveInCaps, sizeof(WAVEINCAPS));
        char name[sizeof(waveInCaps.szPname)];
        strcpy_s(name, waveInCaps.szPname);
        _strlwr_s(name);
        if (strstr(name, "funcube"))
        {
            g_device_id   = i;
            g_num_devices = 1;
            return g_num_devices;
        }
    }

	return g_num_devices;
}

SDRIOEXPORT sdrio_int32 sdrio_get_num_devices()
{
	return g_num_devices;
}

SDRIOEXPORT sdrio_device * sdrio_open_device(sdrio_uint32 device_index)
{
    sdrio_device_t *dev = 0;

    if (g_num_devices > 0)
    {
        dev = new sdrio_device_t;
        memset(dev, 0, sizeof(sdrio_device_t));

        if (!funcube_hidopen(dev))
        {
            return 0;
        }

        sdrio_set_rx_frequency(dev, 100000000);

        dev->format.wFormatTag      = WAVE_FORMAT_PCM;
        dev->format.nChannels       = 2;
        dev->format.nSamplesPerSec  = 192000;
        dev->format.nAvgBytesPerSec = dev->format.nSamplesPerSec * dev->format.nChannels * sizeof(sdrio_int16);
        dev->format.nBlockAlign     = dev->format.nChannels * sizeof(sdrio_int16);
        dev->format.wBitsPerSample  = 16;
        dev->format.cbSize          = 0;

        MMRESULT result = waveInOpen(&dev->hWaveIn, g_device_id, &dev->format, 0L, 0L, WAVE_FORMAT_DIRECT);

        if (result) goto open_device_error;

        for (sdrio_uint32 i=0; i<NUM_BUFFERS; i++)
        {
            dev->waveInHdr[i].dwBufferLength = SAMPLES_PER_BUFFER * 2 * sizeof(sdrio_int16);
            dev->waveInHdr[i].lpData = new char[dev->waveInHdr[i].dwBufferLength];
            dev->waveInHdr[i].dwBytesRecorded = 0;
            dev->waveInHdr[i].dwUser = i;
            dev->waveInHdr[i].dwFlags = 0;
            dev->waveInHdr[i].dwLoops = 0;

            result = waveInPrepareHeader(dev->hWaveIn, &dev->waveInHdr[i], sizeof(WAVEHDR));
            if (result) goto open_device_error;

            result = waveInAddBuffer(dev->hWaveIn, &dev->waveInHdr[i], sizeof(WAVEHDR));
            if (result) goto open_device_error;

            //result = waveInStart(dev->hWaveIn);
        }

        dev->rx_current_buffer = 0;

        return dev;
    }

open_device_error:
    delete dev;
	return 0;
}

SDRIOEXPORT sdrio_int32 sdrio_close_device(sdrio_device *dev)
{
	if (dev)
	{
    	CloseHandle(dev->hidWrite);
	    CloseHandle(dev->hidRead);
        delete dev;
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
		return "FUNcube Dongle";
	}
	else
	{
		return 0;

	}
}

static const sdrio_uint32 sample_rates[] = {192000};

SDRIOEXPORT sdrio_int32 sdrio_set_rx_samplerate(sdrio_device *dev, sdrio_uint64 sample_rate)
{
	if (dev)
	{
        sdrio_uint32 num_sample_rates = sdrio_get_num_samplerates(dev);
        sdrio_uint32 i = 0;
        for (i=0; i<num_sample_rates; i++)
        {
            if (sample_rates[i] == sample_rate)
            {
                break;
            }
        }

		return (i < num_sample_rates);
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
        dev->rx_freq = frequency;

        funcube_hid_set_freq_packet setFreq((sdrio_uint32)frequency);
        sdrio_uint32 bytesWritten = 0;
	    WriteFile(dev->hidWrite, &setFreq, sizeof(funcube_hid_set_freq_packet), &bytesWritten, 0);

        funcube_hid_response_packet response;
        sdrio_uint32 bytesRead = 0;
	    ReadFile(dev->hidRead, &response, sizeof(response), &bytesRead, 0);

        return response.success;
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

SDRIOEXPORT void * start_rx_routine(void *ctx)
{
	sdrio_device *dev = (sdrio_device *)ctx;

	if (dev)
	{
        dev->rx_done = 0;

        MMRESULT result = waveInStart(dev->hWaveIn);

        while (!dev->rx_done)
        {
            if (waveInUnprepareHeader(dev->hWaveIn, &dev->waveInHdr[dev->rx_current_buffer], sizeof(WAVEHDR)) != WAVERR_STILLPLAYING)
            {
                static sdrio_iq samples[SAMPLES_PER_BUFFER];

                sdrio_iqi16 *iq = (sdrio_iqi16 *)dev->waveInHdr[dev->rx_current_buffer].lpData;
                for (sdrio_uint32 i=0; i<SAMPLES_PER_BUFFER; i++)
                {
                    samples[i].i = float(iq[i].i) * (1.0f / 32767.0f);
                    samples[i].q = float(iq[i].q) * (1.0f / 32767.0f);
                }
                dev->rx_callback(dev->rx_context, samples, SAMPLES_PER_BUFFER);

                waveInPrepareHeader(dev->hWaveIn, &dev->waveInHdr[dev->rx_current_buffer], sizeof(WAVEHDR));
                result = waveInAddBuffer(dev->hWaveIn, &dev->waveInHdr[dev->rx_current_buffer], sizeof(WAVEHDR));

                dev->rx_current_buffer = (dev->rx_current_buffer + 1) % NUM_BUFFERS;
            }
            else
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
        dev->rx_callback = callback;
        dev->rx_context = context;
        return pthread_create(&dev->rx_tid, 0, start_rx_routine, (void *)dev) == 0;
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
        dev->rx_done = 1;
		pthread_join(dev->rx_tid, 0);
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
        return 192000;
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
			*min = FCD_MIN_IF_GAIN;
		}

		if (max)
		{
			*max = FCD_MAX_IF_GAIN;
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
        funcube_hid_set_if_gain_packet setGain((sdrio_uint32)gain);
        sdrio_uint32 bytesWritten = 0;
	    WriteFile(dev->hidWrite, &setGain, sizeof(funcube_hid_set_if_gain_packet), &bytesWritten, 0);

        funcube_hid_response_packet response;
        sdrio_uint32 bytesRead = 0;
	    ReadFile(dev->hidRead, &response, sizeof(response), &bytesRead, 0);

        return response.success;
	}
	else
	{
		return 0;
	}
}

SDRIOEXPORT sdrio_int32 sdrio_get_tx_gain_range(sdrio_device *dev, float *min, float *max)
{
	return 0;
}

SDRIOEXPORT sdrio_int32 sdrio_set_tx_gain(sdrio_device *dev, float gain)
{
	return 0;
}

SDRIOEXPORT void sdrio_get_tuning_range(sdrio_device *dev, sdrio_float64 *min, sdrio_float64 *max)
{
    if (dev)
    {
        if (min) *min = 150000;
        if (max) *max = 2050000000;
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

}