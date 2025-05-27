#include <stdlib.h>
#include <stdio.h>
#include "raylib.h"
#include <math.h>
#include <stdbool.h>

#define TARGET_SIZE 180

#define INDEX(x, y, w) ((y) * (w) + (x))

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

void BlackandWhiteImage(const char* filename, int **img, int *w, int *h) {
    Image image = LoadImage(filename);
    *w = image.width;
    *h = image.height;

    ImageColorGrayscale(&image);
    Color *pixels = LoadImageColors(image);

    *img = (int*)malloc(sizeof(int) * (*w) * (*h));

    for (int y = 0; y < *h; y++) {
        for (int x = 0; x < *w; x++) {
            Color c = pixels[INDEX(x, y, *w)];
            int luminance = (int)(0.299 * c.r + 0.587 * c.g + 0.114 * c.b);
            (*img)[INDEX(x, y, *w)] = luminance;
        }
    }

    UnloadImageColors(pixels);
    UnloadImage(image);
}

void ApplySobelEdgeDetection(int *input, int *output, int w, int h) {
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int gx = 0, gy = 0;
            for (int i = -1; i <= 1; i++) {
                for (int j = -1; j <= 1; j++) {
                    int yy = y + i;
                    int xx = x + j;
                    if (xx >= 0 && xx < w && yy >= 0 && yy < h) {
                        int val = input[INDEX(xx, yy, w)];
                        gx += sobelKernelX[i + 1][j + 1] * val;
                        gy += sobelKernelY[i + 1][j + 1] * val;
                    }
                }
            }
            int magnitude = (int)sqrt(gx * gx + gy * gy);
            output[INDEX(x, y, w)] = (magnitude > 60) ? 1 : 0;
        }
    }
}

bool TraceLine(int *Edge, int *visited, int startX, int startY, int w, int h, FILE *gcode) {
    int x = startX, y = startY;
    int idx = INDEX(x, y, w);
    if (visited[idx]) return false;

    int scaledX = (x * TARGET_SIZE) / w;
    int scaledY = (y * TARGET_SIZE) / h;

    fprintf(gcode, "G0 X%d Y%d Z5\n", scaledX, TARGET_SIZE - scaledY);
    fprintf(gcode, "G1 Z0 ; Pen down\n");

    visited[idx] = 1;

    while (true) {
        bool moved = false;
        for (int dy = -2; dy <= 2; dy++) {
            for (int dx = -2; dx <= 2; dx++) {
                if (dx == 0 && dy == 0) continue;

                int nx = x + dx, ny = y + dy;
                if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;

                int nidx = INDEX(nx, ny, w);
                if (Edge[nidx] == 1 && !visited[nidx]) {
                    x = nx; y = ny;
                    visited[nidx] = 1;

                    scaledX = (x * TARGET_SIZE) / w;
                    scaledY = (y * TARGET_SIZE) / h;
                    fprintf(gcode, "G1 X%d Y%d\n", scaledX, TARGET_SIZE - scaledY);
                    moved = true;
                    goto NEXT;
                }
            }
        }
    NEXT:
        if (!moved) break;
    }

    fprintf(gcode, "G1 Z5 ; Pen up\n");
    return true;
}

void GcodeGeneration(int *Edge, int w, int h, const char* gcodefilename) {
    FILE *gcode = fopen(gcodefilename, "w");
    if (!gcode) {
        TraceLog(LOG_ERROR, "Failed to open G-code file");
        return;
    }

    fprintf(gcode, "G21 ; Set units to mm\n");
    fprintf(gcode, "G90 ; Absolute positioning\n");
    fprintf(gcode, "G28 ; Home all axes\n");
    fprintf(gcode, "G0 Z5 ; Pen up\n");

    int *visited = (int*)calloc(w * h, sizeof(int));

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int idx = INDEX(x, y, w);
            if (Edge[idx] == 1 && !visited[idx]) {
                TraceLine(Edge, visited, x, y, w, h, gcode);
            }
        }
    }

    fprintf(gcode, "G0 X0 Y0 ; Return to origin\n");
    fprintf(gcode, "M30 ; End of program\n");
    fclose(gcode);
    free(visited);
}

int main() {
    const int screenWidth = 600;
    const int screenHeight = 600;

    int *img = NULL;
    int *Edge = NULL;
    int imgW, imgH;
    const char* filename = "images/testtest.jpg";
    const char* gcodefilename = "test.gcode";

    BlackandWhiteImage(filename, &img, &imgW, &imgH);
    Edge = (int*)calloc(imgW * imgH, sizeof(int));

    ApplySobelEdgeDetection(img, Edge, imgW, imgH);
    GcodeGeneration(Edge, imgW, imgH, gcodefilename);

    InitWindow(screenWidth, screenHeight, "Edge Display");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        // Display the image scaled to fit window
        for (int y = 0; y < imgH; y++) {
            for (int x = 0; x < imgW; x++) {
                if (Edge[INDEX(x, y, imgW)] == 1) {
                    int drawX = (x * TARGET_SIZE) / imgW;
                    int drawY = (y * TARGET_SIZE) / imgH;
                    float scale = screenWidth / (float)TARGET_SIZE;
                    DrawRectangle(drawX * scale, drawY * scale, scale, scale, BLACK);
                }
            }
        }

        EndDrawing();
    }

    CloseWindow();
    free(img);
    free(Edge);
    return 0;
}
