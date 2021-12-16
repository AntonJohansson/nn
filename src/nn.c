#include <raylib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include <stdint.h>
typedef int8_t    i8;
typedef uint8_t   u8;
typedef int16_t  i16;
typedef uint16_t u16;
typedef int32_t  i32;
typedef uint32_t u32;
typedef int32_t  i64;
typedef uint32_t u64;

#define MAX(a, b) \
  ((a > b) ? a : b)
#define MIN(a, b) \
  ((a < b) ? a : b)
#define CLAMP(value, a, b) \
  MIN(b, MAX(a,value))


typedef struct DataHeader {
  u32 width;
  u32 height;
  u32 num_samples;
} DataHeader;

typedef u8 PixelType;
typedef u8 ValueType;

DataHeader *readHeader(FILE* fd) {
  DataHeader *header = malloc(sizeof(DataHeader));

  const u32 lines_to_skip = 21;

  u32 c = 0;
  u32 skipped_lines = 0;

  // Skip the 3 first lines where nothing happens
  while (skipped_lines < 3 && (c = fgetc(fd)) != EOF) {
    if (c == '\n' || c == '\r') {
      skipped_lines++;
    }
  }

  char name[64];
  u32 value;

  // Read header information about the data set
  // NOTE(anjo): buf can overflow, bigass security vuln.!
  while (skipped_lines < lines_to_skip &&
         fscanf(fd, "%s = %u\n", name, &value) != EOF) {
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

void readSample(DataHeader *header, FILE *fd, PixelType *sample,
                ValueType *correct_value) {
  u32 lines_read = 0;
  u32 c = 0;
  while (lines_read < header->height && (c = fgetc(fd)) != EOF) {
    if (c == '\n' || c == '\r') {
      lines_read++;
    } else {
      *sample++ = c - '0';
    }
  }

  if ((c = fgetc(fd)) != EOF && (c = fgetc(fd)) != EOF) {
    *correct_value = c - '0';
  }
  if (c != EOF) {
    fgetc(fd);
  }
}

static inline u32 sampleOffset(u32 width, u32 height, u32 sample_index) {
  return width*height*sample_index;
}

static inline u32 pixelOffset(u32 width, u32 height, u32 x, u32 y) {
  return y*width + x;
}

typedef struct Window {
  const char *title;
  i32 x;
  i32 y;
  i32 corner_x;
  i32 corner_y;
  u32 width;
  u32 height;
  u32 border_size;
  RenderTexture2D render_texture;
} Window;

void beginWindow(Window *window) {
  BeginTextureMode(window->render_texture);
}

void endWindow(Window *window) {
  EndTextureMode();
}

void calculateWindowSize(Window *window,
                         u32 region_width,
                         u32 region_height,
                         u32 border_size) {
  window->width = region_width + 2*border_size;
  window->height = region_height + 3*border_size;
  window->border_size = border_size;
}

void resetWindowDrawPosition(Window *window) {
  window->x =   window->border_size;
  window->y = 2*window->border_size;
}

bool isMouseInWindow(Window *window) {
  i32 mouse_x = GetMouseX();
  i32 mouse_y = GetMouseY();

  return (mouse_x >= window->corner_x && mouse_x <= window->corner_x + window->width &&
          mouse_y >= window->corner_y && mouse_y <= window->corner_y + window->height);
}

void drawBorder(Window *window, bool should_highlight) {
  DrawRectangle(0, 0,
                window->width, window->height,
                (should_highlight) ? WHITE : DARKGRAY);
  DrawText(window->title,
           0 + window->border_size,
           0 + window->border_size/4, 1.5*window->border_size,
           (should_highlight) ? DARKGRAY : WHITE);
}

void drawSample(Window *window, PixelType *sample,
                u32 width, u32 height,
                u32 pixel_size) {
  for (u32 y = 0; y < height; ++y) {
    for (u32 x = 0; x < width; ++x) {
      PixelType pixel = *(sample + pixelOffset(width, height, x, y));
      DrawRectangle(window->x + x * pixel_size,
                    window->y + y * pixel_size,
                    pixel_size, pixel_size,
                    (Color){pixel * 255, pixel * 255, pixel * 255, 255});
    }
  }
}

i32 main() {
  SetTraceLogLevel(LOG_WARNING);
  InitWindow(800, 600, "nn");
  SetTargetFPS(60);

  FILE *fd = fopen("archive.ics.uci.edu/ml/machine-learning-databases/optdigits/optdigits-orig.tra",
                   "r");

  DataHeader *header = readHeader(fd);

  const u32 pixel_count = header->width * header->height;
  PixelType *samples = malloc(sizeof(PixelType) * pixel_count * header->num_samples);
  ValueType *correct_values = malloc(sizeof(ValueType) * header->num_samples);

  for (u32 i = 0; i < header->num_samples; ++i) {
    PixelType *sample = samples + sampleOffset(header->width, header->height, i);
    readSample(header, fd, sample, &correct_values[i]);
  }

  fclose(fd);

  const u32 drawing_region_width = 400;
  const u32 drawing_region_height = 400;
  PixelType *drawing_region = malloc(sizeof(PixelType) * drawing_region_width * drawing_region_height);
  memset(drawing_region, 0, sizeof(PixelType) * drawing_region_width * drawing_region_height);

  const u32 sample_index = 500;
  Window sample_window = {
    .title = "Sample(s)",
    .corner_x = 10,
    .corner_y = 40,
  };
  calculateWindowSize(&sample_window, header->width*8, header->height*8, 10);
  sample_window.render_texture = LoadRenderTexture(sample_window.width, sample_window.height);

  Window input_window = {
    .title = "Input",
    .corner_x = 300,
    .corner_y = 40,
  };
  calculateWindowSize(&input_window, drawing_region_width, drawing_region_height, 10);
  input_window.render_texture = LoadRenderTexture(input_window.width, input_window.height);

  bool is_window_being_dragged = false;
  u32 window_drag_dx = 0;
  u32 window_drag_dy = 0;

#define WINDOW_STACK_SIZE 16
  u32 num_windows = 2;
  Window *window_stack[WINDOW_STACK_SIZE] = {
    &sample_window,
    &input_window,
  };

  Window *hovered_window = NULL;
  u32 hover_index = 0;

  while (!WindowShouldClose()) {
    ClearBackground(GRAY);
    BeginDrawing();

    if (!hovered_window) {
      for (u32 i = 0; i < num_windows; ++i) {
        if (isMouseInWindow(window_stack[i])) {
          hover_index = i;
          hovered_window = window_stack[i];
          break;
        }
      }
    }

    if (hovered_window) {
      for (u32 i = hover_index; i > 0; --i) {
        Window *tmp = window_stack[i];
        window_stack[i] = window_stack[i-1];
        window_stack[i-1] = tmp;
      }
    }

    if (hovered_window) {
      if (!is_window_being_dragged &&
          IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        is_window_being_dragged = true;
        window_drag_dx = GetMouseX() - hovered_window->corner_x;
        window_drag_dy = GetMouseY() - hovered_window->corner_y;
      }

      if (is_window_being_dragged) {
        hovered_window->corner_x = -window_drag_dx + CLAMP(GetMouseX(), 0, GetScreenWidth());
        hovered_window->corner_y = -window_drag_dy + CLAMP(GetMouseY(), 0, GetScreenHeight());
        if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) {
          is_window_being_dragged = false;
        }
      }
    }

    /* Draw sample */
    {
      Window *window = &sample_window;
      beginWindow(window);
      resetWindowDrawPosition(window);
      bool in_window = window == hovered_window;
      drawBorder(window, in_window);
      drawSample(window,
                 samples + sampleOffset(header->width, header->height, sample_index),
                 header->width, header->height, 8);
      endWindow(window);
    }

    /* Interactive drawing input thing */
    {
      Window *window = &input_window;
      beginWindow(window);
      resetWindowDrawPosition(window);
      bool in_window = window == hovered_window;
      drawBorder(window, in_window);
      drawSample(window, drawing_region, drawing_region_width, drawing_region_height, 1);
      endWindow(window);
    }

    if (hovered_window && !is_window_being_dragged) {
      hovered_window = NULL;
    }

    for (i32 i = num_windows-1; i >= 0; --i) {
      DrawTextureRec(window_stack[i]->render_texture.texture,
                     (Rectangle){
                     .x = 0,
                     .y = 0,
                     .width = (int) window_stack[i]->width,
                     .height = -(int) window_stack[i]->height
                     },
                     (Vector2){
                     window_stack[i]->corner_x,
                     window_stack[i]->corner_y
                     },
                     WHITE);
    }

    EndDrawing();
  }

  UnloadRenderTexture(sample_window.render_texture);
  UnloadRenderTexture(input_window.render_texture);

  CloseWindow();

  return 0;
}
