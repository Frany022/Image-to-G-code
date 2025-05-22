#include <stdlib.h>
#include <stdio.h>
#include "raylib.h"
#include <math.h>

#define IMG_SIZE 300
#define INDEX(x, y) ((y) * IMG_SIZE + (x))

int sobelKernelX[3][3] = {
    {-1, 0, 1},
    {-2, 0, 2},
    {-1, 0, 1}
};

int sobelKernelY[3][3] = {
    {-1, -2, -1},
    { 0,  0,  0},
    { 1,  2,  1}
};

int ApplySobel(int *img, int x, int y) {
    int gradientX = 0;
    int gradientY = 0;

    for (int i = -1; i <= 1; i++) {
        for (int j = -1; j <= 1; j++) {
            int yy = y + i;
            int xx = x + j;

            if (xx >= 0 && xx < IMG_SIZE && yy >= 0 && yy < IMG_SIZE) {
                int pixelValue = img[INDEX(xx, yy)];
                gradientX += sobelKernelX[i + 1][j + 1] * pixelValue;
                gradientY += sobelKernelY[i + 1][j + 1] * pixelValue;
            }
        }
    }

    int magnitude = (int)sqrt((gradientX * gradientX) + (gradientY * gradientY));
    return magnitude;
}

void ApplySobelEdgeDetection(int *input, int *output) {
    for (int y = 0; y < IMG_SIZE; y++) {
        for (int x = 0; x < IMG_SIZE; x++) {
            int magnitude = ApplySobel(input, x, y);
            output[INDEX(x, y)] = (magnitude > 100) ? 1 : 0;
        }
    }
}

void BlackandWhiteImage(const char* filename, int *img) {
    Image image = LoadImage(filename);
    ImageResize(&image, IMG_SIZE, IMG_SIZE);
    ImageColorGrayscale(&image);
    Color *pixels = LoadImageColors(image);

    for (int y = 0; y < IMG_SIZE; y++) {
        for (int x = 0; x < IMG_SIZE; x++) {
            Color c = pixels[INDEX(x, y)];
            int luminance = (int)(0.299 * c.r + 0.587 * c.g + 0.114 * c.b);
            img[INDEX(x, y)] = luminance;
        }
    }

    UnloadImageColors(pixels);
    UnloadImage(image);
}

void GcodeGeneration(int *Edge, const char* gcodefilename) {
    FILE *gcode = fopen(gcodefilename, "w");
    if (!gcode) {
        TraceLog(LOG_ERROR, "Failed to open G-code file");
        return;
    }

    fprintf(gcode, "G21 ; Set units to mm\n");
    fprintf(gcode, "G90 ; Absolute positioning\n");
    fprintf(gcode, "G28 ; Home all axes\n");
    fprintf(gcode, "G0 Z5 ; Pen up\n");

    int visited[IMG_SIZE * IMG_SIZE] = {0};

    for (int y = 0; y < IMG_SIZE; y++) {
        for (int x = 0; x < IMG_SIZE; x++) {
            int idx = INDEX(x, y);

            if (Edge[idx] == 1 && !visited[idx]) {
                int flippedY = IMG_SIZE - y;

                fprintf(gcode, "G0 X%d Y%d Z5\n", x, flippedY);
                fprintf(gcode, "G1 Z0 ; Pen down\n");

                int px = x;
                int py = y;

                while (1) {
                    int foundNext = 0;
                    visited[INDEX(px, py)] = 1;
                    fprintf(gcode, "G1 X%d Y%d\n", px, IMG_SIZE - py);

                    for (int dy = -1; dy <= 1; dy++) {
                        for (int dx = -1; dx <= 1; dx++) {
                            if (dx == 0 && dy == 0) continue;

                            int nx = px + dx;
                            int ny = py + dy;

                            if (nx >= 0 && ny >= 0 && nx < IMG_SIZE && ny < IMG_SIZE) {
                                int nidx = INDEX(nx, ny);
                                if (Edge[nidx] == 1 && !visited[nidx]) {
                                    px = nx;
                                    py = ny;
                                    foundNext = 1;
                                    break;
                                }
                            }
                        }
                        if (foundNext) break;
                    }

                    if (!foundNext) break;
                }

                fprintf(gcode, "G1 Z5 ; Pen up\n");
            }
        }
    }

    fprintf(gcode, "G0 X0 Y0 ; Return to origin\n");
    fprintf(gcode, "M30 ; End of program\n");
    fclose(gcode);
    TraceLog(LOG_INFO, "G-code generated to %s", gcodefilename);
}


int main() {
    const int screenWidth = 600;
    const int screenHeight = 600;
    const float scale = 2.0f;

    int img[IMG_SIZE * IMG_SIZE] = {0};
    int Edge[IMG_SIZE * IMG_SIZE] = {0};
    const char* filename = "testtest.jpg";
    const char* gcodefilename = "test.gcode";

    BlackandWhiteImage(filename, img);
    ApplySobelEdgeDetection(img, Edge);
    GcodeGeneration(Edge, gcodefilename);

    for (int y = 0; y < IMG_SIZE; y++) {
        for (int x = 0; x < IMG_SIZE; x++) {
            printf("%d", Edge[INDEX(x, y)]);
        }
        printf("\n");
    }

    InitWindow(screenWidth, screenHeight, "test");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        for (int y = 0; y < IMG_SIZE; y++) {
            for (int x = 0; x < IMG_SIZE; x++) {
                if (Edge[INDEX(x, y)] == 1) {
                    DrawRectangle(x * scale, y * scale, scale, scale, BLACK);
                }
            }
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
