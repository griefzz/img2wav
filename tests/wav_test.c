#define _USE_MATH_DEFINES
#include <math.h>
#include <float.h>
#include <assert.h>
#include <stdio.h>

#include "../src/wav.h"

int compare(float *const *a, float *const *b, size_t nc, size_t ns, size_t bd) {
    float epsilon = FLT_EPSILON;
    if (bd == 24) epsilon = 0.000001f;
    if (bd == 16) epsilon = 0.0001f;

    for (size_t ch = 0; ch < nc; ch++) {
        for (size_t i = 0; i < ns; i++) {
            const float x = a[ch][i];
            const float y = b[ch][i];
            if (!(fabs(x - y) <= epsilon)) {
                return 0;
            }
        }
    }

    return 1;
}

int main() {
    const int nc       = 3;
    const int sr       = 44100;
    const int duration = 2.0;
    const int ns       = sr * duration;
    const int bd       = 32;
    // Allocate audio buffers
    float *c[3];
    float *b[3];
    for (size_t ch = 0; ch < nc; ch++) {
        c[ch] = malloc(ns * sizeof(*c[ch]));
        assert(c[ch] != NULL);
        b[ch] = malloc(ns * sizeof(*b[ch]));
        assert(b[ch] != NULL);
        for (size_t i = 0; i < ns; i++) {
            const double t = i / (double) sr;
            c[ch][i]       = 0.8 * sin(2.0 * M_PI * 440.0 * t);
        }
    }

    //// Test valid file writing
    // Mono test
    wav_config mono_hdr = {1, ns, sr, bd};
    assert(wav_write(mono_hdr, "mono.wav", c) == ns);

    // Stereo test
    wav_config stereo_hdr = {2, ns, sr, bd};
    assert(wav_write(stereo_hdr, "stereo.wav", c) == ns);

    // Multi-channel test
    wav_config multi_hdr = {nc, ns, sr, bd};
    assert(wav_write(multi_hdr, "multi.wav", c) == ns);

    // 24-bit test
    wav_config multi_24_hdr = {nc, ns, sr, 24};
    assert(wav_write(multi_24_hdr, "multi_24.wav", c) == ns);

    // 16-bit test
    wav_config multi_16_hdr = {nc, ns, sr, 16};
    assert(wav_write(multi_16_hdr, "multi_16.wav", c) == ns);

    //// Test invalid headers
    // Invalid number of channels test
    wav_config ch_hdr = {0, ns, sr, bd};
    assert(wav_write(ch_hdr, "channels.wav", c) == 0);

    // Invalid number of samples test
    wav_config ns_hdr = {nc, 0, sr, bd};
    assert(wav_write(ns_hdr, "samples.wav", c) == 0);

    // Invalid sample rate test
    wav_config sr_hdr = {nc, ns, 0, bd};
    assert(wav_write(sr_hdr, "sample_rate.wav", c) == 0);

    // Invalid bit depth test
    wav_config bd_hdr = {nc, ns, sr, 0};
    assert(wav_write(bd_hdr, "bit_depth.wav", c) == 0);

    //// Test invalid path
    wav_config path_hdr = {nc, ns, sr, bd};
    assert(wav_write(path_hdr, "", c) == 0);

    //// Test valid file header reading
    // Test 32-bit multi-channel header reading
    wav_config read_hdr;
    assert(wav_get_header(&read_hdr, "multi.wav") == WAV_HEADER_SIZE);
    assert(read_hdr.nc = nc);
    assert(read_hdr.ns = ns);
    assert(read_hdr.sr = sr);
    assert(read_hdr.bd = bd);

    // Test 24-bit multi-channel header reading
    assert(wav_get_header(&read_hdr, "multi_24.wav") == WAV_HEADER_SIZE);
    assert(read_hdr.nc = nc);
    assert(read_hdr.ns = ns);
    assert(read_hdr.sr = sr);
    assert(read_hdr.bd = 24);

    // Test 16-bit multi-channel header reading
    assert(wav_get_header(&read_hdr, "multi_16.wav") == WAV_HEADER_SIZE);
    assert(read_hdr.nc = nc);
    assert(read_hdr.ns = ns);
    assert(read_hdr.sr = sr);
    assert(read_hdr.bd = 16);

    //// Test valid file data reading
    // Test 32-bit multi-channel header reading
    read_hdr.bd = 32;
    assert(wav_read(read_hdr, "multi.wav", b) == ns);
    assert(compare(c, b, read_hdr.nc, read_hdr.ns, read_hdr.bd));

    // Test 24-bit multi-channel header reading
    read_hdr.bd = 24;
    assert(wav_read(read_hdr, "multi_24.wav", b) == ns);
    assert(compare(c, b, read_hdr.nc, read_hdr.ns, read_hdr.bd));

    // Test 16-bit multi-channel header reading
    read_hdr.bd = 16;
    assert(wav_read(read_hdr, "multi_16.wav", b) == ns);
    assert(compare(c, b, read_hdr.nc, read_hdr.ns, read_hdr.bd));

    // Cleanup
    for (size_t ch = 0; ch < nc; ch++) {
        free(c[ch]);
        free(b[ch]);
    }
}