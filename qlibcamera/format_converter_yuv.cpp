#include "format_converter_yuv.h"

// Helper macros for RGB565 extraction
#define RGB565_R(rgb) (((rgb) >> 11) & 0x1F) // Extract 5-bit red
#define RGB565_G(rgb) (((rgb) >> 5) & 0x3F)  // Extract 6-bit green
#define RGB565_B(rgb) ((rgb) & 0x1F)         // Extract 5-bit blue

// Scale RGB565 components to 8-bit range
#define SCALE_R(r) (((r) * 255) / 31)
#define SCALE_G(g) (((g) * 255) / 63)
#define SCALE_B(b) (((b) * 255) / 31)

// RGB to YUV conversion constants
#define CLIP(val) ((val) < 0 ? 0 : (val) > 255 ? 255 : (val))

// RGB to YUV conversion macros
#define RGB_TO_Y(r, g, b) (CLIP((0.299 * (r)) + (0.587 * (g)) + (0.114 * (b))))
#define RGB_TO_U(r, g, b) (CLIP((-0.169 * (r)) - (0.331 * (g)) + (0.500 * (b)) + 128))
#define RGB_TO_V(r, g, b) (CLIP((0.500 * (r)) - (0.419 * (g)) - (0.081 * (b)) + 128))

// Function to convert RGB24 to YUV420
void rgb24_to_yuv420(quint8* rgb24, quint8* y_plane, quint8* u_plane, quint8* v_plane, int width, int height) {
    int uv_index = 0;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int pixel_index = (y * width + x) * 3;
            int r = rgb24[pixel_index];
            int g = rgb24[pixel_index + 1];
            int b = rgb24[pixel_index + 2];

            // Calculate YUV values
            quint8 y_value = RGB_TO_Y(r, g, b);
            quint8 u_value = RGB_TO_U(r, g, b);
            quint8 v_value = RGB_TO_V(r, g, b);

            // Write Y plane
            y_plane[y * width + x] = y_value;

            // Subsample U and V planes (4:2:0 format)
            if (y % 2 == 0 && x % 2 == 0) {
                u_plane[uv_index] = u_value;
                v_plane[uv_index] = v_value;
                uv_index++;
            }
        }
    }
}

// Function to convert BGR24 to YUV420
void bgr24_to_yuv420(quint8* rgb24, quint8* y_plane, quint8* u_plane, quint8* v_plane, int width, int height) {
    int uv_index = 0;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int pixel_index = (y * width + x) * 3;
            int r = rgb24[pixel_index + 2];
            int g = rgb24[pixel_index + 1];
            int b = rgb24[pixel_index];

            // Calculate YUV values
            quint8 y_value = RGB_TO_Y(r, g, b);
            quint8 u_value = RGB_TO_U(r, g, b);
            quint8 v_value = RGB_TO_V(r, g, b);

            // Write Y plane
            y_plane[y * width + x] = y_value;

            // Subsample U and V planes (4:2:0 format)
            if (y % 2 == 0 && x % 2 == 0) {
                u_plane[uv_index] = u_value;
                v_plane[uv_index] = v_value;
                uv_index++;
            }
        }
    }
}

// Function to convert RGB565 to YUV420
void rgb565_to_yuv420(quint16* rgb565, quint8* y_plane, quint8* u_plane, quint8* v_plane, int width, int height) {
    int uv_index = 0;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            quint16 rgb = rgb565[y * width + x];
            int r = SCALE_R(RGB565_R(rgb));
            int g = SCALE_G(RGB565_G(rgb));
            int b = SCALE_B(RGB565_B(rgb));

            // Convert RGB to YUV
            quint8 y_value = CLIP((0.299 * r) + (0.587 * g) + (0.114 * b));
            quint8 u_value = CLIP((-0.169 * r) - (0.331 * g) + (0.500 * b) + 128);
            quint8 v_value = CLIP((0.500 * r) - (0.419 * g) - (0.081 * b) + 128);

            // Write Y plane
            y_plane[y * width + x] = y_value;

            // Subsample U and V planes (4:2:0 format)
            if (y % 2 == 0 && x % 2 == 0) {
                u_plane[uv_index] = u_value;
                v_plane[uv_index] = v_value;
                uv_index++;
            }
        }
    }
}
