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

/** clamp a range of values to be between [-1.0, 1.0] */
void clamp(float *src, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (src[i] > 1.0f) src[i] = 1.0f;
        if (src[i] < -1.0f) src[i] = -1.0f;
    }
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
    unsigned char *data = stbi_load(path, x, y, &n, 0);
    if (!data) {
        fprintf(stderr, "Failed to open file: %s!\n", path);
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
            /* 
            take the average of our pixel data based on the stride n
            we do it this way since we cant know the stride at compile time
            so just iterate over the stride using an offset 
            */
            int off = 0;
            int avg = 0;
            while (off < n) {
                avg += data[i * (*x * n) + j + off];
                off++;
            }
            avg /= n;
            avg = 255 - avg;

            pixels[i * *x + col] = avg;
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
@param duration_s Length in seconds of the transmission time
@param width Width of the pixel data
@param height Height of the pixel data
@param size Pointer to the number of generated amplitudes
@return Array of amplitudes corresponding to the frequencies generated
*/
float *get_freqs(const int *pixels, float sample_rate, float duration_s, int width, int height, int *size) {
    const float two_pi = M_PI * 2.0;
    const float fs     = sample_rate;
    const float time   = duration_s;// length in seconds
    const int target   = (fs * time) / width;

    *size         = time * fs;
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
            const float A    = map(heat, 0.0f, 255.0f, 0.01f, 0.1f);
            int t            = 0;
            while (t < target) {
                rp[t] += A * sin(two_pi * (((height - y) * 500.0) / fs) * t);
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
               "Usage: img2wav [sampleRate] [duration_s] in.jpg out.wav\n");

        return EXIT_SUCCESS;
    }

    const float sample_rate = atof(argv[1]);
    if (sample_rate == 0.0) {
        printf("Invalid sample rate: %f!", sample_rate);

        return EXIT_FAILURE;
    }

    const float duration_s = atof(argv[2]);
    if (duration_s == 0.0) {
        printf("Invalid duration: %f!", duration_s);

        return EXIT_FAILURE;
    }

    int x, y;
    int *pixels = get_pixels(argv[3], &x, &y);
    if (!pixels) return EXIT_FAILURE;

    int n;
    float *freqs = get_freqs(pixels, sample_rate, duration_s, x, y, &n);
    free(pixels);
    if (!freqs) return EXIT_FAILURE;

    /* Wav files expect amplitudes between [-1.0, 1.0] */
    clamp(freqs, n);

    wav_write_config cfg = {1, n, sample_rate};
    wav_write(cfg, argv[4], freqs);

    free(freqs);

    return EXIT_SUCCESS;
}