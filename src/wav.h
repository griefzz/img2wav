/* wav.h - public domain wav file reader and writer by Spencer Stone (2022)
   
   Features:
       + Supports 32-bit signed 24-bit PCM and signed 16-bit PCM bit depths
       + Supports multi-channel formats
       + Cross platform win32/unix/linux
    
    Limitations:
       + Only supports Little-Endian systems
       + Forced usage of standard library
       + Limited support for wav header extensions

    DOCUMENTATION
    =============
    // When you want to write to a wav file to you need specify 
    // how its written using a wav_config.
    wav_config cfg;
    cfg.nc = num_channels; // >0
    cfg.ns = num_samples;  // >0
    cfg.sr = sample_rate;  // 44100, 48000, 96000 etc
    cfg.bd = bit_depth;    // 16, 24, 32

    // To actually write your audio data, call wav_write() using 
    // your created wav_config. The data parameter to wav write
    // is your deinterleaved channel data of size data[num_channels][num_samples];
    wav_write(cfg, "audio.wav", data);

    // When you want to read from a wav file, you need to populate
    // the parameters of a wav_config
    wav_get_header(&cfg, "audio.wav");

    // Since wav.h doesn't automatically allocate any memory, you'll
    // need to create an input array for your data of size in[num_channels][num_samples];
    float **in = malloc(sizeof(*in) * cfg.nc);
    for (size_t ch = 0; ch < cfg.nc; ch++)
        in[ch] = malloc(sizeof(*in[ch]) * cfg.ns);

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

#define WAV_HEADER_SIZE 25

#define check_error(error, description, retval)                                 \
    do {                                                                        \
        if ((error)) {                                                          \
            fprintf(stderr, "%s [%s:%d]\n", (description), __FILE__, __LINE__); \
            return (retval);                                                    \
        }                                                                       \
    } while (0);

#define write_key(key, file) fwrite((key), sizeof(*(key)), strlen((key)), (file))
#define write_val(val, file) fwrite(&(val), sizeof((val)), 1, (file))
#define read_val(val, file)  fread(&(val), sizeof((val)), 1, (file))
#define read_key(key, file)  fread((key), sizeof(*(key)), 4, (file));

/** Header for a wav file */
struct WavHeader {
    struct RIFF {
        const char *title;// RIFF
        uint32_t fileSize;
    } RIFF;

    struct WAVE {
        const char *title; // WAVE
        const char *marker;// fmt\x20
        uint32_t cksize;
        uint16_t WAVE_FORMAT_EXTENSIBLE;// idk what this is
        uint16_t numChannels;
        uint32_t sampleRate;
        uint32_t nAvgBytesPecSec;// (Sample Rate * bitDepth * Channels) / 8
        uint16_t nBlockAlign;    // (bitDepth/8) * numChannels
        uint16_t bitsPerSample;
    } WAVE;

    struct DATA {
        const char *title;// data
        uint32_t size;
    } DATA;

    struct RIFF riff;
    struct WAVE wave;
    struct DATA data;
};
typedef struct WavHeader WavHeader;

/**
 * @brief Create a new WavHeader (internal use only)
 * 
 * @param nc Number of channels
 * @param ns Number of samples
 * @param sr Sample rate
 * @param bd Bit depth
 * @return WavHeader usable for writing
 */
WavHeader *wav_header_new(uint16_t nc, uint32_t ns, uint32_t sr, uint16_t bd) {
    WavHeader *header = malloc(sizeof(*header));
    check_error(!header, "malloc(): Failed to allocate WavHeader", NULL);

    uint16_t M = bd / 8;

    // RIFF
    header->riff.title    = "RIFF";
    header->riff.fileSize = 28 + 8 + (M * nc * ns);
    // if our filesize is odd we padd it with one byte
    if (header->riff.fileSize % 2 != 0) header->riff.fileSize++;

    // WAVE
    header->wave.title                  = "WAVE";
    header->wave.marker                 = "fmt\x20";
    header->wave.cksize                 = 16;
    header->wave.WAVE_FORMAT_EXTENSIBLE = (bd == 32) ? 3 : 1;
    header->wave.numChannels            = nc;
    header->wave.sampleRate             = sr;
    header->wave.nAvgBytesPecSec        = (sr * bd * nc) / 8;
    header->wave.nBlockAlign            = M * nc;
    header->wave.bitsPerSample          = bd;

    // DATA
    header->data.title = "data";
    header->data.size  = M * nc * ns;

    return header;
}

/**
 * @brief Deallocate a WavHeader (internal use only)
 * 
 * @param header WavHeader to free
 */
void wav_header_free(WavHeader *header) {
    if (header)
        free(header), header = NULL;
    else
        fprintf(stderr, "Failed to free WaveHeader!\n");
}

/** Configuration for wav_writer and wav_reader */
struct wav_config {
    uint16_t nc;//!< number of channels
    uint32_t ns;//!< number of samples
    uint32_t sr;//!< sample rate
    uint16_t bd;//!< bit depth
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

    check_error(!data, "Data pointer must not be NULL.", n);
    check_error(!path, "Path pointer must not be NULL.", n);
    check_error(cfg.nc == 0, "Number of channels must be greater than 0.", n);
    check_error(cfg.ns == 0, "Number of samples must be greater than 0.", n);
    check_error(cfg.sr == 0, "Sample rate must be greater than 0.", n);
    check_error(cfg.bd != 32 && cfg.bd != 24 && cfg.bd != 16, "Bit depth must be either 32, 24 or 16.", n);

    FILE *file = fopen(path, "wb");
    check_error(!file, "fopen(): Failed to open file for writing", n);

    WavHeader *header = wav_header_new(cfg.nc, cfg.ns, cfg.sr, cfg.bd);
    check_error(!header, "wav_header_new(): Failed to allocate WavHeader", n);

    // RIFF
    n += write_key(header->riff.title, file);
    n += write_val(header->riff.fileSize, file);

    // WAVE
    n += write_key(header->wave.title, file);
    n += write_key(header->wave.marker, file);
    n += write_val(header->wave.cksize, file);
    n += write_val(header->wave.WAVE_FORMAT_EXTENSIBLE, file);
    n += write_val(header->wave.numChannels, file);
    n += write_val(header->wave.sampleRate, file);
    n += write_val(header->wave.nAvgBytesPecSec, file);
    n += write_val(header->wave.nBlockAlign, file);
    n += write_val(header->wave.bitsPerSample, file);

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
    WavHeader header;
    char title[4];
    int n = 0;

    check_error(!cfg, "Wav config must not be NULL", n);
    check_error(!path, "Path pointer must not be NULL.", n);

    FILE *file = fopen(path, "rb");
    check_error(!file, "fopen(): Failed to open file for reading.", n);

    // RIFF
    n += read_key(title, file);
    check_error(strncmp(title, "RIFF", 4) != 0, "Invalid RIFF section.", n);
    n += read_val(header.riff.fileSize, file);

    // WAVE
    n += read_key(title, file);
    check_error(strncmp(title, "WAVE", 4) != 0, "Invalid WAVE section.", n);
    n += read_key(title, file);
    check_error(strncmp(title, "fmt\x20", 4) != 0, "Invalid WAVE section.", n);
    n += read_val(header.wave.cksize, file);
    n += read_val(header.wave.WAVE_FORMAT_EXTENSIBLE, file);
    n += read_val(header.wave.numChannels, file);
    n += read_val(header.wave.sampleRate, file);
    n += read_val(header.wave.nAvgBytesPecSec, file);
    n += read_val(header.wave.nBlockAlign, file);
    n += read_val(header.wave.bitsPerSample, file);

    // DATA
    n += read_key(title, file);
    check_error(strncmp(title, "data", 4) != 0, "Invalid data section.", n);
    n += read_val(header.data.size, file);

    check_error(n != WAV_HEADER_SIZE, "Invalid wave header size", n);

    cfg->nc = header.wave.numChannels;
    cfg->bd = header.wave.bitsPerSample;
    cfg->sr = header.wave.sampleRate;
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
    int n = 0;
    check_error(cfg.nc == 0, "Number of channels must be greater than 0.", n);
    check_error(cfg.ns == 0, "Number of samples must be greater than 0.", n);
    check_error(cfg.sr == 0, "Sample rate must be greater than 0.", n);
    check_error(cfg.bd != 32 && cfg.bd != 24 && cfg.bd != 16, "Bit depth must be either 32, 24 or 16.", n);
    check_error(!path, "Path pointer must not be NULL.", n);
    check_error(!data, "Data pointer must not be NULL!", n);
    for (size_t ch = 0; ch < cfg.nc; ch++)
        check_error(!data[ch], "Data channel pointers must not be NULL!", n);

    FILE *file = fopen(path, "rb");
    check_error(!file, "fopen(): Failed to open file for reading.", n);

    // Move to start of our float data
    check_error(fseek(file, 44, SEEK_SET) != 0, "fseek(): Failed to seek file for reading", n);

    switch (cfg.bd) {
        case 32:
            for (size_t i = 0; i < cfg.ns; i++) {
                for (size_t ch = 0; ch < cfg.nc; ch++) {
                    n += read_val(data[ch][i], file);
                }
            }
            break;
        case 24:
            for (size_t i = 0; i < cfg.ns; i++) {
                for (size_t ch = 0; ch < cfg.nc; ch++) {
                    int32_t v;
                    n += fread(&v, 24 / 8 /* 3 bytes */, 1, file);
                    long l1 = 0;
                    memcpy(((unsigned char *) &l1) + 1, &v, 3);
                    data[ch][i] = (float) l1 / 2147483648.f;
                }
            }
            break;
        case 16:
            for (size_t i = 0; i < cfg.ns; i++) {
                for (size_t ch = 0; ch < cfg.nc; ch++) {
                    int16_t v;
                    n += fread(&v, sizeof(v), 1, file);
                    data[ch][i] = v * 0x1p-15f;
                }
            }
            break;
    }

    return n / cfg.nc;
}

#undef check_error
#undef write_key
#undef write_val
#undef read_key
#undef read_val
#endif
