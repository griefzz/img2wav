#ifdef _WIN32
    #ifndef _CRT_SECURE_NO_WARNINGS
        #define _CRT_SECURE_NO_WARNINGS
    #endif
#endif
#include <stdlib.h>
#include <stdio.h>

#define _USE_MATH_DEFINES
#include <math.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "wav.h"

#define check_error(error, description, retval)                                 \
    do {                                                                        \
        if ((error)) {                                                          \
            fprintf(stderr, "%s [%s:%d]\n", (description), __FILE__, __LINE__); \
            return (retval);                                                    \
        }                                                                       \
    } while (0);

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
    float max = find_max(src, n);
    if (max > 1.0)
        for (size_t i = 0; i < n; i++)
            src[i] /= max;
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
    unsigned char *data = stbi_load(path, x, y, &n, 3);
    check_error(!data, "stbi_load(): Failed to open file", NULL);
    check_error(n != 3, "stbi_load(): Image stride must be 3 (RGB)", NULL);

    int *pixels = malloc(*x * *y * sizeof(*pixels));
    check_error(!pixels, "malloc(): Failed to allocate pixels", NULL);

    for (int i = 0; i < *y; i++) {
        int col = 0;
        for (int j = 0; j < *x * n; j += n) {
            const int r = data[i * (*x * n) + j];
            const int g = data[i * (*x * n) + j + 1];
            const int b = data[i * (*x * n) + j + 2];

            // convert the image to gray scale using Luma BT.601
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
    check_error(!result, "calloc(): Failed to allocate result", NULL);

    float *rp = result;
    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            if (pixels[y * width + x] < 10) continue;// skip nonexistant pixels

            const float heat = pixels[y * width + x];
            const float A    = map(heat, 0.0f, 255.0f, 0.001f, 1.0f);
            int t            = 0;
            while (t < target) {
                // do (height - y) * scale to flip the image upside down
                const float fc = y * scale;
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

        return EXIT_FAILURE;
    }

    const float sample_rate = atof(argv[1]);
    check_error(sample_rate == 0.0, "Sample rate must be greater than 0", EXIT_FAILURE);

    const float time_s = atof(argv[2]);
    check_error(time_s == 0.0, "Transmission time must be greater than 0", EXIT_FAILURE);

    int x, y;
    int *pixels = get_pixels(argv[3], &x, &y);
    check_error(!pixels, "get_pixels()", EXIT_FAILURE);

    int n;
    float *freqs = get_freqs(pixels, sample_rate, time_s, x, y, &n);
    check_error(!freqs, "get_freqs()", EXIT_FAILURE);

    /* Wav files expect amplitudes between [-1.0, 1.0] */
    normalize(freqs, n);

    wav_config cfg = {1, n, sample_rate, 24};
    check_error(wav_write(cfg, argv[4], &freqs) != n * cfg.nc, "wav_write()", EXIT_FAILURE);

    free(pixels);
    free(freqs);

    return EXIT_SUCCESS;
}
