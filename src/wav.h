/* wav.h - public domain wav file reader and writer by Spencer Stone (2022)
   
   Features:
       + Supports 32-bit float, signed 24-bit PCM, signed 16-bit PCM and signed 8-bit PCM bit depths
       + Supports multi-channel formats
       + Cross platform windows/unix/linux
    
    Limitations:
       + Only supports Little-Endian systems
       + Forced usage of standard library
       + Limited support for wav header extensions
       + Does not support partial reads

    DOCUMENTATION
    =============
    // When you want to write to a wav file to you need specify 
    // how its written using a wav_config.
    wav_config cfg;
    cfg.nc = num_channels; // >0
    cfg.ns = num_samples;  // >0
    cfg.sr = sample_rate;  // 44100, 48000, 96000 etc
    cfg.bd = bit_depth;    // 8, 16, 24, 32

    // To actually write your audio data, call wav_write() using 
    // your created wav_config. The data parameter to wav write
    // is your deinterleaved channel data of size data[num_channels][num_samples];
    wav_write(cfg, "audio.wav", data);

    // When you want to read from a wav file, you need to populate
    // the parameters of a wav_config
    wav_get_header(&cfg, "audio.wav");

    // Since wav.h doesn't automatically allocate any memory, you'll
    // need to create an input array for your data of size in[num_channels][num_samples];
    float **in = wav_malloc(sizeof(*in) * cfg.nc);
    for (size_t ch = 0; ch < cfg.nc; ch++)
        in[ch] = wav_malloc(sizeof(*in[ch]) * cfg.ns);

    // After we understand the structure of the wav file, we can begin reading
    // the channel data using wav_read()
    wav_read(cfg, "audio.wav", in);

    // Don't forget to deallocate the in buffer when you're done
    for (size_t ch = 0; ch < cfg.nc; ch++)
        free(in[ch]);
    free(in);
*/
#ifndef WAV_H
#define WAV_H
#ifdef _WIN32
    #ifndef _CRT_SECURE_NO_WARNINGS
        #define _CRT_SECURE_NO_WARNINGS
    #endif
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <limits.h>

#define WAV_KEY_SIZE    4 //!< Size of the header tags in a wav file
#define WAV_VALUE_SIZE  1 //!< Number of values to read/write each (read/write)_val call
#define WAV_HEADER_SIZE 25//!< Size of the wav header section
#define WAV_DATA_OFFSET 44//!< Offset to the channel data in a wav file

#define check_error(error, description, retval)                                 \
    do {                                                                        \
        if ((error)) {                                                          \
            fprintf(stderr, "%s [%s:%d]\n", (description), __FILE__, __LINE__); \
            return (retval);                                                    \
        }                                                                       \
    } while (0);

#define die(msg)                                                                         \
    do {                                                                                 \
        fprintf(stderr, "%s: %s [%s:%d]\n", (msg), strerror(errno), __FILE__, __LINE__); \
        exit(EXIT_FAILURE);                                                              \
    } while (0);

#define write_key(key, file) fwrite((key), sizeof(*(key)), WAV_KEY_SIZE, (file))  //!< Write a key to the wav file
#define write_val(val, file) fwrite(&(val), sizeof((val)), WAV_VALUE_SIZE, (file))//!< Write a value to the wav file
#define read_val(val, file)  fread(&(val), sizeof((val)), WAV_VALUE_SIZE, (file)) //!< Read a key from the wav file
#define read_key(key, file)  fread((key), sizeof(*(key)), WAV_KEY_SIZE, (file))   //!< Read a value from the wav file

/**
 * @brief Error checked version of malloc 
 * 
 * @param size Number of bytes to allocate
 * @return Pointer to the beginning of newly allocated memory
 */
void *wav_malloc(size_t size) {
    void *v;
    if (!(v = malloc(size)))
        die("malloc()");

    return v;
}

/**
 * @brief Error checked version of fopen
 * 
 * @param filename File name to associate the file stream to 
 * @param mode NULL-terminated character string determining file access mode
 * @return Pointer to the new file stream
 */
FILE *wav_fopen(const char *filename, const char *mode) {
    FILE *f;
    if (!(f = fopen(filename, mode)))
        die("fopen()");

    return f;
}

/**
 * @brief Error checked version of fseek
 * 
 * @param stream  File stream to modify 
 * @param offset  Number of characters to shift the position relative to origin 
 * @param origin  Position to which offset is added. It can have one of the following values: SEEK_SET, SEEK_CUR, SEEK_END
 */
void wav_fseek(FILE *stream, long offset, int origin) {
    int ret;
    if (!(ret = fseek(stream, offset, origin)))
        die("fseek()");
}

/** Header for a wav file */
struct wav_header {
    /** RIFF section is the magic tag + entire file size */
    struct RIFF {
        const char *title;//!< RIFF tag
        uint32_t size;    //!< size of entire wav file in bytes
    } RIFF;

    /** WAVE section is the configuration for the wave file */
    struct WAVE {
        const char *title;              //!< WAVE tag
        const char *marker;             //!< fmt\x20 tag
        uint32_t cksize;                //!< Idk what this is
        uint16_t WAVE_FORMAT_EXTENSIBLE;//!< Idk what this is
        uint16_t num_channels;          //!< Number of channels
        uint32_t sample_rate;           //!< Sample rate
        uint32_t avg_bytes_per_sec;     //!< Average bytes per second (Sample Rate * bitDepth * Channels) / 8
        uint16_t n_block_align;         //!< Stride of the channel data (bitDepth/8) * num_channels
        uint16_t bits_per_sample;       //!< Bit depth
    } WAVE;

    /** DATA section stores the actual channel data and its size */
    struct DATA {
        const char *title;//!< data tag
        uint32_t size;    //!< Size in bytes of channel data
    } DATA;

    struct RIFF riff;
    struct WAVE wave;
    struct DATA data;
};
typedef struct wav_header wav_header;

/**
 * @brief Create a new WavHeader (internal use only)
 * 
 * @param nc Number of channels
 * @param ns Number of samples
 * @param sr Sample rate
 * @param bd Bit depth
 * @return Wav header usable for writing
 */
wav_header *wav_header_new(uint16_t nc, uint32_t ns, uint32_t sr, uint16_t bd) {
    wav_header *header = wav_malloc(sizeof(*header));

    uint16_t M = bd / 8;

    // RIFF
    header->riff.title = "RIFF";
    header->riff.size  = 28 + 8 + (M * nc * ns);
    // if our file size is odd we padd it with one byte
    if (header->riff.size % 2 != 0) header->riff.size++;

    // WAVE
    header->wave.title                  = "WAVE";
    header->wave.marker                 = "fmt\x20";
    header->wave.cksize                 = 16;
    header->wave.WAVE_FORMAT_EXTENSIBLE = (bd == 32) ? 3 : 1;
    header->wave.num_channels           = nc;
    header->wave.sample_rate            = sr;
    header->wave.avg_bytes_per_sec      = (sr * bd * nc) / 8;
    header->wave.n_block_align          = M * nc;
    header->wave.bits_per_sample        = bd;

    // DATA
    header->data.title = "data";
    header->data.size  = M * nc * ns;

    return header;
}

/**
 * @brief Deallocate a WavHeader (internal use only)
 * 
 * @param header Wav header to free
 */
void wav_header_free(wav_header *header) {
    if (header)
        free(header), header = NULL;
    else
        fprintf(stderr, "Failed to free WaveHeader!\n");
}

/** Configuration for wav_writer and wav_reader */
struct wav_config {
    uint16_t nc;//!< Number of channels
    uint32_t ns;//!< Number of samples
    uint32_t sr;//!< Sample rate
    uint16_t bd;//!< Bit depth
};
typedef struct wav_config wav_config;

/**
 * @brief Write audio data to a wav file
 * 
 * @param cfg Configuration for the wav writer
 * @param path Destination path for the output file
 * @param data Deinterleaved multi-channel Audio data to write to our wav file
 * @return Number of samples written
 */
int wav_write(wav_config cfg, const char *path, float *const *data) {
    int n = 0;

    check_error(!path, "Path pointer must not be NULL!", n);
    check_error(!data, "Data pointer must not be NULL!", n);
    check_error(cfg.nc == 0, "Number of channels must be greater than 0.", n);
    check_error(cfg.ns == 0, "Number of samples must be greater than 0.", n);
    check_error(cfg.sr == 0, "Sample rate must be greater than 0.", n);
    check_error(cfg.bd != 32 && cfg.bd != 24 && cfg.bd != 16 && cfg.bd != 8, "Bit depth must be either 32, 24, 16 or 8.", n);

    FILE *file = wav_fopen(path, "wb");

    wav_header *header = wav_header_new(cfg.nc, cfg.ns, cfg.sr, cfg.bd);

    // RIFF
    n += write_key(header->riff.title, file);
    n += write_val(header->riff.size, file);

    // WAVE
    n += write_key(header->wave.title, file);
    n += write_key(header->wave.marker, file);
    n += write_val(header->wave.cksize, file);
    n += write_val(header->wave.WAVE_FORMAT_EXTENSIBLE, file);
    n += write_val(header->wave.num_channels, file);
    n += write_val(header->wave.sample_rate, file);
    n += write_val(header->wave.avg_bytes_per_sec, file);
    n += write_val(header->wave.n_block_align, file);
    n += write_val(header->wave.bits_per_sample, file);

    // DATA
    n += write_key(header->data.title, file);
    n += write_val(header->data.size, file);

    check_error(n != 25, "Header must equal 25 bytes.", n);

    // append our actual audio data
    n = 0;
    switch (cfg.bd) {
        case 32:
            for (size_t i = 0; i < cfg.ns; i++) {
                for (size_t ch = 0; ch < cfg.nc; ch++) {
                    n += write_val(data[ch][i], file);
                }
            }
            break;
        case 24:
            for (size_t i = 0; i < cfg.ns; i++) {
                for (size_t ch = 0; ch < cfg.nc; ch++) {
                    const int32_t v = lround(data[ch][i] * 0x7FFFFF) & 0xFFFFFF;
                    n += fwrite(&v, 24 / 8 /* 3 bytes */, 1, file);
                }
            }
            break;
        case 16:
            for (size_t i = 0; i < cfg.ns; i++) {
                for (size_t ch = 0; ch < cfg.nc; ch++) {
                    int32_t v = (int32_t) (data[ch][i] * 32768.0f);
                    // clamp v between [-32768, 32767]
                    if (v < -32768) v = -32768;
                    if (v > 32767) v = 32767;
                    n += fwrite(&v, sizeof(int16_t), 1, file);
                }
            }
            break;
        case 8:
            for (size_t i = 0; i < cfg.ns; i++) {
                for (size_t ch = 0; ch < cfg.nc; ch++) {
                    uint8_t v = (uint8_t) (128 + ((uint8_t) (data[ch][i] * (127.0f))));
                    n += fwrite(&v, sizeof(uint8_t), 1, file);
                }
            }
            break;
    }

    // pad an extra byte if needed
    char padding = 0;
    if (header->data.size % 2 != 0) write_val(padding, file);

    wav_header_free(header);

    fclose(file);

    return n / cfg.nc;
}

/**
 * @brief Read a wav file configuration
 * 
 * @see wav_read()
 * @param cfg Pointer to store wav file configuration
 * @param path Path to the wav file to read
 * @return int Number of data entries read;
 */
int wav_get_header(wav_config *cfg, const char *path) {
    wav_header header;
    char key[4];
    int n = 0;

    check_error(!cfg, "Wav config must not be NULL!", n);
    check_error(!path, "Path pointer must not be NULL!", n);

    FILE *file = wav_fopen(path, "rb");

    // RIFF
    n += read_key(key, file);
    check_error(strncmp(key, "RIFF", 4) != 0, "Invalid RIFF section.", n);
    n += read_val(header.riff.size, file);

    // WAVE
    n += read_key(key, file);
    check_error(strncmp(key, "WAVE", 4) != 0, "Invalid WAVE section.", n);
    n += read_key(key, file);
    check_error(strncmp(key, "fmt\x20", 4) != 0, "Invalid WAVE section.", n);
    n += read_val(header.wave.cksize, file);
    n += read_val(header.wave.WAVE_FORMAT_EXTENSIBLE, file);
    n += read_val(header.wave.num_channels, file);
    n += read_val(header.wave.sample_rate, file);
    n += read_val(header.wave.avg_bytes_per_sec, file);
    n += read_val(header.wave.n_block_align, file);
    n += read_val(header.wave.bits_per_sample, file);

    // DATA
    n += read_key(key, file);
    check_error(strncmp(key, "data", 4) != 0, "Invalid data section.", n);
    n += read_val(header.data.size, file);

    check_error(n != WAV_HEADER_SIZE, "Invalid wave header size", n);

    cfg->nc = header.wave.num_channels;
    cfg->bd = header.wave.bits_per_sample;
    cfg->sr = header.wave.sample_rate;
    cfg->ns = header.data.size / (cfg->nc * cfg->bd / 8);// ns = size / (nc * M)

    fclose(file);

    return n;
}

/**
 * @brief Read audio data from a wav file
 * 
 * @see wav_get_header()
 * @param cfg Configuration for the wav reader
 * @param path Path for the input file
 * @param data Array of channels to write data to
 * @return Number of samples read
 */
int wav_read(wav_config cfg, const char *path, float **data) {
    uint16_t M = cfg.bd / 8;
    int n      = 0;
    check_error(cfg.nc == 0, "Number of channels must be greater than 0.", n);
    check_error(cfg.ns == 0, "Number of samples must be greater than 0.", n);
    check_error(cfg.sr == 0, "Sample rate must be greater than 0.", n);
    check_error(cfg.bd != 32 && cfg.bd != 24 && cfg.bd != 16 && cfg.bd != 8, "Bit depth must be either 32, 24, 16 or 8.", n);
    check_error(!path, "Path pointer must not be NULL!", n);
    check_error(!data, "Data pointer must not be NULL!", n);
    for (size_t ch = 0; ch < cfg.nc; ch++)
        check_error(!data[ch], "Data channel pointers must not be NULL!", n);

    FILE *file = wav_fopen(path, "rb");

    // Move to start of our float data
    wav_fseek(file, WAV_DATA_OFFSET, SEEK_SET);

    // Memory map the channel data for fast processing
    uint8_t *map = wav_malloc(sizeof(*map) * cfg.ns * cfg.nc * M);
    uint8_t *mp  = map;

    // Read channel data into memory
    n = fread(map, sizeof(*map), cfg.ns * cfg.nc * M, file);
    check_error(n != cfg.ns * cfg.nc * M, "Failed to read into map.", n);
    n /= M * cfg.nc;

    switch (cfg.bd) {
        case 32:
            for (size_t i = 0; i < cfg.ns; i++) {
                for (size_t ch = 0; ch < cfg.nc; ch++, mp += M) {
                    memcpy(&data[ch][i], mp, M);
                }
            }
            break;
        case 24:
            for (size_t i = 0; i < cfg.ns; i++) {
                for (size_t ch = 0; ch < cfg.nc; ch++, mp += M) {
                    int32_t v = 0;
                    long l1   = 0;
                    memcpy(&v, mp, M);
                    memcpy(((unsigned char *) &l1) + 1, &v, M);
                    data[ch][i] = (float) l1 * 0x1p-31f;
                }
            }
            break;
        case 16:
            for (size_t i = 0; i < cfg.ns; i++) {
                for (size_t ch = 0; ch < cfg.nc; ch++, mp += M) {
                    int16_t v = 0;
                    memcpy(&v, mp, M);
                    data[ch][i] = v * 0x1p-15f;
                }
            }
            break;
        case 8:
            for (size_t i = 0; i < cfg.ns; i++) {
                for (size_t ch = 0; ch < cfg.nc; ch++, mp += M) {
                    uint8_t v = 0;
                    memcpy(&v, mp, M);
                    data[ch][i] = (v - 128) * 0x1p-7f;
                }
            }
            break;
    }

    free(map);

    return n;
}

#undef check_error
#undef die
#undef write_key
#undef write_val
#undef read_key
#undef read_val
#undef WAV_KEY_SIZE
#undef WAV_VALUE_SIZE
#undef WAV_DATA_OFFSET
#endif
