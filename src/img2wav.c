#define _CRT_SECURE_NO_WARNINGS
#include <stdlib.h>
#include <stdio.h>

#define _USE_MATH_DEFINES
#include <math.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "wav.h"

/** Remaps a value from a source range to a target range. */
float map(float src, float src_min, float src_max, float target_min, float target_max) {
    return target_min + ((target_max - target_min) * (src - src_min)) / (src_max - src_min);
}

/** Find absolute maximum value in array */
float find_max(float *src, size_t n) {
    float max = 0.0f;
    for (size_t i = 0; i < n; i++)
        if (fabs(src[i]) > max)
            max = src[i];

    return fabs(max);
}

/** Normalize values in array to be [-1.0, 1.0] */
void normalize(float *src, size_t n) {
    float max  = find_max(src, n);
    float norm = 1.0f / max;
    for (size_t i = 0; i < n; i++)
        src[i] *= norm;
}

/**
Convert an image into a Width x Height sized array of greyscale values between [0, 255].

@param path Path to the image to convert
@param x Pointer to a variable that stores the width of an image
@param y Pointer to a variable that stores the height of an image
@return An array of pixels x by y in size containing values [0, 255]
*/
int *get_pixels(const char *path, int *x, int *y) {
    int n;
    int comp            = 3;
    unsigned char *data = stbi_load(path, x, y, &n, comp);
    if (!data) {
        fprintf(stderr, "Failed to open file: %s!\n", path);
        return NULL;
    }

    if (n != comp) {
        fprintf(stderr, "Image stride must be 3 (RGB) got %d!\n", n);
        return NULL;
    }

    int *pixels = malloc(*x * *y * sizeof(*pixels));
    if (!pixels) {
        fprintf(stderr, "Failed to allocate: %zu bytes in get_pixels()!\n", *x * *y * sizeof(*pixels));
        return NULL;
    }

    for (int i = 0; i < *y; i++) {
        int col = 0;
        for (int j = 0; j < *x * n; j += n) {
            const int r = data[i * (*x * n) + j];
            const int g = data[i * (*x * n) + j + 1];
            const int b = data[i * (*x * n) + j + 2];

            // convert the image to gray scale using BT.601
            pixels[i * *x + col] = (r * 0.299 + g * 0.587 + b * 0.114);
            col++;
        }
    }

    stbi_image_free(data);

    return pixels;
}

/**
Convert pixel data to frequency data for use in generating audio files

@param pixels Single channel pixel data of values between [0, 255]
@param sample_rate Desired sample rate of the frequencies
@param time_s Length in seconds of the transmission time
@param width Width of the pixel data
@param height Height of the pixel data
@param size Pointer to the number of generated amplitudes
@return Array of amplitudes corresponding to the frequencies generated
*/
float *get_freqs(const int *pixels, float sample_rate, float time_s, int width, int height, int *size) {
    const float two_pi   = M_PI * 2.0;
    const float fs       = sample_rate;
    const int target     = (fs * time_s) / width;
    const float max_freq = 48000.0;// maximum displayed frequency in the spectrogram
    const float scale    = max_freq / height;
    *size                = time_s * fs;

    float *result = calloc(*size, sizeof(*result));
    if (!result) {
        fprintf(stderr, "Failed to allocate: %d bytes in img2freq()!\n", *size);
        return NULL;
    }

    float *rp = result;
    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            if (pixels[y * width + x] < 10) continue;// skip nonexistant pixels

            const float heat = pixels[y * width + x];
            const float A    = map(heat, 0.0f, 255.0f, 0.001f, 1.0f);
            int t            = 0;
            while (t < target) {
                // we do (height - y) to flip the image upside down
                const float fc = (height - y) * scale;
                rp[t] += A * sin(two_pi * (fc / fs) * t);
                t++;
            }
        }
        rp += target;
    }

    return result;
}

int main(int argc, char **argv) {
    if (argc < 5) {
        printf("img2wav - Convert an image to the frequency spectrum of an audio file\n"
               "Usage: img2wav [sample_rate] [time_s] in.jpg out.wav\n");

        return EXIT_SUCCESS;
    }

    const float sample_rate = atof(argv[1]);
    if (sample_rate == 0.0) {
        printf("Invalid sample rate: %f!", sample_rate);

        return EXIT_FAILURE;
    }

    const float time_s = atof(argv[2]);
    if (time_s == 0.0) {
        printf("Invalid transmission time: %f!", time_s);

        return EXIT_FAILURE;
    }

    int x, y;
    int *pixels = get_pixels(argv[3], &x, &y);
    if (!pixels) return EXIT_FAILURE;

    int n;
    float *freqs = get_freqs(pixels, sample_rate, time_s, x, y, &n);
    free(pixels);
    if (!freqs) return EXIT_FAILURE;

    /* Wav files expect amplitudes between [-1.0, 1.0] */
    normalize(freqs, n);

    wav_write_config cfg = {1, n, sample_rate};
    wav_write(cfg, argv[4], freqs);

    free(freqs);

    return EXIT_SUCCESS;
}