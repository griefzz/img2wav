#ifndef WAV_H
#define WAV_H
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#define check_error(error, description, retval)                                 \
    do {                                                                        \
        if ((error)) {                                                          \
            fprintf(stderr, "%s [%s:%d]\n", (description), __FILE__, __LINE__); \
            return (retval);                                                    \
        }                                                                       \
    } while (0);

/** Clamp a value between [start, end] */
inline int32_t clamp(int32_t v, int32_t start, int32_t end) {
    if (v < start) v = start;
    if (v > end) v = end;
    return v;
}

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
Create a new WavHeader

@param nc Number of channels
@param ns Number of samples
@param sr Sample rate
@param bd Bit depth
*/
WavHeader *wav_header_new(uint16_t nc, uint32_t ns, uint32_t sr, uint16_t bd) {
    WavHeader *header = malloc(sizeof(*header));
    if (!header) return NULL;

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

/** Deallocate a WavHeader */
void wav_header_free(WavHeader *header) {
    if (header)
        free(header), header = NULL;
    else
        fprintf(stderr, "Failed to free WaveHeader!\n");
}

/** Configuration for wav_writer */
struct wav_write_config {
    uint16_t nc;// number of channels
    uint32_t ns;// number of samples
    uint32_t sr;// sample rate
    uint16_t bd;// bit depth
};
typedef struct wav_write_config wav_write_config;

/**
Write audio data to a wav file
 
@param cfg Configuration for the wav writer
@param path Destination path for the output file
@param data Audio data to write to our wav file
@return Number of bytes written including header
*/
int wav_write(wav_write_config cfg, const char *path, const float *data) {
    int n     = 0;
    char padd = 0;

    check_error(!data, "Data pointer must not be NULL.", n);
    check_error(cfg.nc == 0, "Number of channels must be greater than 0.", n);
    check_error(cfg.ns == 0, "Number of samples must be greater than 0.", n);
    check_error(cfg.sr == 0, "Sample rate must be greater than 0.", n);
    check_error(cfg.bd != 32 && cfg.bd != 24 && cfg.bd != 16, "Bit depth must be either 32, 24 or 16.", n);

    FILE *file = fopen(path, "wb");
    check_error(!file, "Failed to open file for writing", n);

    WavHeader *header = wav_header_new(cfg.nc, cfg.ns, cfg.sr, cfg.bd);
    check_error(!header, "Unable to allocate WavHeader", n);

    // RIFF
    n += fwrite(header->riff.title, sizeof(header->riff.title[0]), strlen(header->riff.title), file);
    n += fwrite(&header->riff.fileSize, sizeof(header->riff.fileSize), 1, file);

    // WAVE
    n += fwrite(header->wave.title, sizeof(header->wave.title[0]), strlen(header->wave.title), file);
    n += fwrite(header->wave.marker, sizeof(header->wave.marker[0]), strlen(header->wave.marker), file);
    n += fwrite(&header->wave.cksize, sizeof(header->wave.cksize), 1, file);
    n += fwrite(&header->wave.WAVE_FORMAT_EXTENSIBLE, sizeof(header->wave.WAVE_FORMAT_EXTENSIBLE), 1, file);
    n += fwrite(&header->wave.numChannels, sizeof(header->wave.numChannels), 1, file);
    n += fwrite(&header->wave.sampleRate, sizeof(header->wave.sampleRate), 1, file);
    n += fwrite(&header->wave.nAvgBytesPecSec, sizeof(header->wave.nAvgBytesPecSec), 1, file);
    n += fwrite(&header->wave.nBlockAlign, sizeof(header->wave.nBlockAlign), 1, file);
    n += fwrite(&header->wave.bitsPerSample, sizeof(header->wave.bitsPerSample), 1, file);

    // DATA
    n += fwrite(header->data.title, sizeof(header->data.title[0]), strlen(header->data.title), file);
    n += fwrite(&header->data.size, sizeof(header->data.size), 1, file);

    // append our actual audio data
    switch (cfg.bd) {
        case 32:
            fwrite(data, sizeof(*data), cfg.ns, file);
            break;
        case 24:
            for (size_t i = 0; i < cfg.ns; i++) {
                int32_t v = lround(data[i] * 0x7FFFFF) & 0xFFFFFF;
                fwrite(&v, 24 / 8, 1, file);
            }
            break;
        case 16:
            for (size_t i = 0; i < cfg.ns; i++) {
                int32_t v = (int32_t) (data[i] * 32768.0f);
                v         = clamp(v, -32768, 32767);
                fwrite(&v, sizeof(int16_t), 1, file);
            }
            break;
    }

    // padd if needed
    if (header->data.size % 2 != 0) fwrite(&padd, sizeof(padd), 1, file);

    wav_header_free(header);

    fclose(file);

    return n;
}

#undef check_error
#endif
