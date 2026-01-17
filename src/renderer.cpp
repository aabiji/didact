#define CLAY_IMPLEMENTATION
#include "renderer.h"
#include "error.h"

#include <iostream>
#include <math.h>

Renderer::Renderer(SDL_Window* window, float window_width, float window_height) {
  // Initialize the renderer and font cache
  m_renderer = SDL_CreateRenderer(window, nullptr);
  if (!m_renderer)
    throw Error(SDL_GetError());

  SDL_SetRenderVSync(m_renderer, 1);
  m_font.init(m_renderer, "../assets/Roboto-Regular.ttf", 18, {255, 255, 255, 255});

  // Initialize the layout
  unsigned int memsize = Clay_MinMemorySize();
  Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(memsize, malloc(memsize));

  auto handle_error = [](Clay_ErrorData data) {
    std::cout << data.errorText.chars << "\n";
  };
  Clay_Initialize(arena, {window_width, window_height}, {handle_error, nullptr});

  auto measure_text = [](Clay_StringSlice text, Clay_TextElementConfig* config,
                         void* data) {
    // TODO: adjust using `config`
    Renderer* renderer = (Renderer*)data;
    std::string str(text.chars, text.length);
    Vec2 size = renderer->m_font.text_size(str);
    return (Clay_Dimensions){size.x, size.y};
  };
  Clay_SetMeasureTextFunction(measure_text, this);
}

Renderer::~Renderer() { SDL_DestroyRenderer(m_renderer); }

void Renderer::clear(SDL_FColor color) {
  SDL_SetRenderDrawColor(m_renderer, color.r, color.g, color.b, color.a);
  SDL_RenderClear(m_renderer);
}

void Renderer::present() { SDL_RenderPresent(m_renderer); }

void Renderer::render_rectangle(SDL_FRect rect, SDL_FColor color) {
  SDL_SetRenderDrawColor(m_renderer, color.r, color.g, color.b, color.a);
  SDL_RenderFillRect(m_renderer, &rect);
}

void Renderer::render_round_rect(float x, float y, float w, float h, float radius,
                                 SDL_FColor color) {
  // Draw the base rectangles
  SDL_FRect rects[3] = {
      {x, y + radius, radius, h - radius * 2},              // Left edge
      {x + radius, y, w - radius * 2, h},                   // Center
      {x + w - radius, y + radius, radius, h - radius * 2}, // Right edge
  };
  SDL_RenderRects(m_renderer, rects, 3);

  // Draw the four rounded corners using a quarter circle approximated by triangles
  Vec2 corner_centers[] = {
      {x + radius, y + radius},         // top left
      {x + radius, y + h - radius},     // bottom left
      {x + w - radius, y + radius},     // top right
      {x + w - radius, y + h - radius}, // bottom right
  };
  float start_angles[] = {M_PI, M_PI / 2.0f, 0, 3 * M_PI / 2.0f};

  for (int corner = 0; corner < 4; corner++) {
    Vec2 center = corner_centers[corner];
    float start_angle = start_angles[corner];

    const int segments = 8; // triangles per corner
    SDL_Vertex vertices[segments + 2];

    // center vertex
    vertices[0].position.x = center.x;
    vertices[0].position.y = center.y;
    vertices[0].color = color;

    // perimeter vertices
    for (int i = 0; i <= segments; i++) {
      float angle = start_angle + (M_PI / 2.0f) * ((float)i / segments);
      vertices[i + 1].position.x = center.x + radius * cosf(angle);
      vertices[i + 1].position.y = center.y + radius * sinf(angle);
      vertices[i + 1].color = color;
    }

    // create indexes for the constructed triangle fan
    int indices[segments + 3];
    for (int i = 0; i < segments; i++) {
      indices[i * 3] = 0;         // center
      indices[i * 3 + 1] = i + 1; // current perimeter point
      indices[i * 3 + 2] = i + 2; // next perimeter point
    }

    SDL_RenderGeometry(m_renderer, nullptr, vertices, segments + 2, indices,
                       segments + 3);
  }
}

void Renderer::render_layout(Clay_RenderCommandArray* commands) {
  for (int i = 0; i < commands->length; i++) {
    Clay_RenderCommand* cmd = Clay_RenderCommandArray_Get(commands, i);
  }
}