// Copyright Scott Cutler
// This source file is licensed under the GNU Lesser General Public License (LGPL)

typedef unsigned char sdrio_uint8;
typedef char sdrio_int8;

typedef unsigned short sdrio_uint16;
typedef short sdrio_int16;

typedef unsigned long sdrio_uint32;
typedef long sdrio_int32;

typedef unsigned __int64 sdrio_uint64;
typedef __int64 sdrio_int64;

typedef float sdrio_float32;
typedef double sdrio_float64;

struct sdrio_device_t;
typedef struct sdrio_device_t sdrio_device;

typedef enum
{
    sdrio_gain_mode_agc,
    sdrio_gain_mode_manual
} sdrio_gain_mode;

typedef enum
{
    sdrio_caps_rx,
    sdrio_caps_tx,
    sdrio_caps_agc
} sdrio_caps;

typedef struct sdrio_iq_t
{
    sdrio_float32 i;
    sdrio_float32 q;
} sdrio_iq;

typedef sdrio_int32 (*sdrio_rx_async_callback)(void *context, sdrio_iq *samples, sdrio_uint32 length);
typedef sdrio_int32 (*sdrio_tx_async_callback)(void *context, sdrio_iq *samples, sdrio_uint32 length);

#define SDRIOEXPORT __declspec(dllexport)

typedef sdrio_int32 (*sdrio_init_t)();

typedef sdrio_int32 (*sdrio_get_num_devices_t)();

typedef sdrio_device * (*sdrio_open_device_t)(sdrio_uint32 device_index);
typedef sdrio_int32 (*sdrio_close_device_t)(sdrio_device *dev);

typedef const char * (*sdrio_get_device_string_t)(sdrio_device *dev);

typedef sdrio_int32 (*sdrio_set_rx_samplerate_t)(sdrio_device *dev, sdrio_uint64 sample_rate);
typedef sdrio_int32 (*sdrio_set_rx_frequency_t)(sdrio_device *dev, sdrio_uint64 frequency);

typedef sdrio_int32 (*sdrio_set_tx_samplerate_t)(sdrio_device *dev, sdrio_uint64 sample_rate);
typedef sdrio_int32 (*sdrio_set_tx_frequency_t)(sdrio_device *dev, sdrio_uint64 frequency);

typedef sdrio_int32 (*sdrio_start_rx_t)(sdrio_device *dev, sdrio_rx_async_callback callback, void *context);
typedef sdrio_int32 (*sdrio_stop_rx_t)(sdrio_device *dev);

typedef sdrio_int32 (*sdrio_start_tx_t)(sdrio_device *dev, sdrio_tx_async_callback callback, void *context);
typedef sdrio_int32 (*sdrio_stop_tx_t)(sdrio_device *dev);

typedef sdrio_int32 (*sdrio_get_num_samplerates_t)(sdrio_device *dev);
typedef void        (*sdrio_get_samplerates_t)(sdrio_device *dev, sdrio_uint32 *sample_rates_out);

typedef sdrio_int64 (*sdrio_get_rx_frequency_t)(sdrio_device *dev);
typedef sdrio_int64 (*sdrio_get_rx_samplerate_t)(sdrio_device *dev);

typedef sdrio_int64 (*sdrio_get_tx_frequency_t)(sdrio_device *dev);
typedef sdrio_int64 (*sdrio_get_tx_samplerate_t)(sdrio_device *dev);

typedef sdrio_int32 (*sdrio_set_rx_gain_mode_t)(sdrio_device *dev, sdrio_gain_mode gain_mode);
typedef sdrio_int32 (*sdrio_get_rx_gain_range_t)(sdrio_device *dev, sdrio_float32 *min, sdrio_float32 *max);
typedef sdrio_int32 (*sdrio_set_rx_gain_t)(sdrio_device *dev, sdrio_float32 gain);

typedef sdrio_int32 (*sdrio_get_tx_gain_range_t)(sdrio_device *dev, sdrio_float32 *min, sdrio_float32 *max);
typedef sdrio_int32 (*sdrio_set_tx_gain_t)(sdrio_device *dev, sdrio_float32 gain);

typedef void        (*sdrio_get_tuning_range_t)(sdrio_device *dev, sdrio_float64 *min, sdrio_float64 *max);
typedef sdrio_int32 (*sdrio_get_caps_t)(sdrio_device *dev, sdrio_caps caps);

#ifdef __cplusplus
extern "C" {
#endif

    SDRIOEXPORT sdrio_int32 sdrio_init();

    SDRIOEXPORT sdrio_int32 sdrio_get_num_devices();

    SDRIOEXPORT sdrio_device * sdrio_open_device(sdrio_uint32 device_index);
    SDRIOEXPORT sdrio_int32 sdrio_close_device(sdrio_device *dev);

    SDRIOEXPORT const char * sdrio_get_device_string(sdrio_device *dev);

    SDRIOEXPORT sdrio_int32 sdrio_set_rx_samplerate(sdrio_device *dev, sdrio_uint64 sample_rate);
    SDRIOEXPORT sdrio_int32 sdrio_set_rx_frequency(sdrio_device *dev, sdrio_uint64 frequency);

    SDRIOEXPORT sdrio_int32 sdrio_set_tx_samplerate(sdrio_device *dev, sdrio_uint64 sample_rate);
    SDRIOEXPORT sdrio_int32 sdrio_set_tx_frequency(sdrio_device *dev, sdrio_uint64 frequency);

    SDRIOEXPORT sdrio_int32 sdrio_start_rx(sdrio_device *dev, sdrio_rx_async_callback callback, void *context);
    SDRIOEXPORT sdrio_int32 sdrio_stop_rx(sdrio_device *dev);

    SDRIOEXPORT sdrio_int32 sdrio_start_tx(sdrio_device *dev, sdrio_tx_async_callback callback, void *context);
    SDRIOEXPORT sdrio_int32 sdrio_stop_tx(sdrio_device *dev);

    SDRIOEXPORT sdrio_int32 sdrio_get_num_samplerates(sdrio_device *dev);
    SDRIOEXPORT void        sdrio_get_samplerates(sdrio_device *dev, sdrio_uint32 *sample_rates_out);

    SDRIOEXPORT sdrio_int64 sdrio_get_rx_frequency(sdrio_device *dev);
    SDRIOEXPORT sdrio_int64 sdrio_get_rx_samplerate(sdrio_device *dev);

    SDRIOEXPORT sdrio_int64 sdrio_get_tx_frequency(sdrio_device *dev);
    SDRIOEXPORT sdrio_int64 sdrio_get_tx_samplerate(sdrio_device *dev);

    SDRIOEXPORT sdrio_int32 sdrio_set_rx_gain_mode(sdrio_device *dev, sdrio_gain_mode gain_mode);
    SDRIOEXPORT sdrio_int32 sdrio_get_rx_gain_range(sdrio_device *dev, sdrio_float32 *min, sdrio_float32 *max);
    SDRIOEXPORT sdrio_int32 sdrio_set_rx_gain(sdrio_device *dev, sdrio_float32 gain);

    SDRIOEXPORT sdrio_int32 sdrio_get_tx_gain_range(sdrio_device *dev, sdrio_float32 *min, sdrio_float32 *max);
    SDRIOEXPORT sdrio_int32 sdrio_set_tx_gain(sdrio_device *dev, sdrio_float32 gain);

    SDRIOEXPORT void        sdrio_get_tuning_range(sdrio_device *dev, sdrio_float64 *min, sdrio_float64 *max);
    SDRIOEXPORT sdrio_int32 sdrio_get_caps(sdrio_device *dev, sdrio_caps caps);

#ifdef __cplusplus
}
#endif
