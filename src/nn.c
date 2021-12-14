#include <raylib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

typedef struct DataHeader {
  unsigned int width;
  unsigned int height;
  unsigned int num_samples;
} DataHeader;

DataHeader *readHeader(FILE* fd) {
  DataHeader *header = malloc(sizeof(DataHeader));

  const unsigned int lines_to_skip = 21;

  unsigned int c = 0;
  unsigned int skipped_lines = 0;

  // Skip the 3 first lines where nothing happens
  while (skipped_lines < 3 && (c = fgetc(fd)) != EOF) {
    if (c == '\n' || c == '\r') {
      skipped_lines++;
    }
  }

  char name[64];
  unsigned int value;

  // Read header information about the data set
  // NOTE(anjo): buf can overflow, bigass security vuln.!
  while (skipped_lines < lines_to_skip && (c = fscanf(fd, "%s = %u\n", name, &value)) != EOF) {
    skipped_lines++;
    if (strncmp(name, "entwidth", 64) == 0) {
      header->width = value;
    } else if (strncmp(name, "entheight", 64) == 0) {
      header->height = value;
    } else if (strncmp(name, "ntot", 64) == 0) {
      header->num_samples = value;
    }
  }

  return header;
}

void readSample(FILE *fd, unsigned int *sample, unsigned int *correct_value) {
  unsigned int lines_read = 0;
  unsigned int c = 0;
  while (lines_read < 32 && (c = fgetc(fd)) != EOF) {
    if (c == '\n' || c == '\r') {
      lines_read++;
    } else {
      *sample++ = c - '0';
    }
  }

  if ((c = fgetc(fd)) != EOF && (c = fgetc(fd)) != EOF) {
    *correct_value = c - '0';
  }
  fgetc(fd);
}

static inline unsigned int sampleOffset(DataHeader *header, unsigned int sample, unsigned int x, unsigned int y) {
  return header->width*header->height*sample + y*header->width + x;
}

int main() {
  SetTraceLogLevel(LOG_WARNING);
  InitWindow(800, 600, "nn");
  SetTargetFPS(60);

  FILE *fd = fopen("archive.ics.uci.edu/ml/machine-learning-databases/optdigits/optdigits-orig.tra", "r");

  DataHeader *header = readHeader(fd);

  printf("Each sample is %dx%d pixels.\n", header->width, header->height);
  printf("There are %d samples.\n", header->num_samples);

  unsigned int *samples = malloc(sizeof(unsigned int) * header->width * header->height * header->num_samples);
  unsigned int *correct_values = malloc(sizeof(unsigned int) * header->num_samples);

  for (unsigned int i = 0; i < header->num_samples; ++i) {
    readSample(fd, samples + sampleOffset(header, i, 0, 0), &correct_values[i]);
  }

  fclose(fd);

  const unsigned int size = 8;
  while (!WindowShouldClose()) {
    ClearBackground(GRAY);
    BeginDrawing();

    const unsigned int sample_index = 1000;
    DrawText(TextFormat("Correct value: %d", correct_values[sample_index]), 50, header->height*size + 50, 20, BLACK);
    for (unsigned int y = 0; y < header->height; ++y) {
      for (unsigned int x = 0; x < header->width; ++x) {
        unsigned int pixel = *(samples + sampleOffset(header, sample_index, x, y));
        DrawRectangle(x * size, y*size, size, size, (Color){pixel * 255, pixel * 255 , pixel * 255, 255});
      }
    }

    EndDrawing();
  }

  CloseWindow();

  return 0;
}
