#pragma once

#include "font.h"
#include <clay.h>

class Renderer {
public:
  Renderer(SDL_Window* window, float window_width, float window_height);
  ~Renderer();

  void render_layout(Clay_RenderCommandArray* commands);
  void render_rectangle(SDL_FRect rect, SDL_FColor color);

  void present();
  void clear(SDL_FColor color);

private:
  void render_round_rect(float x, float y, float w, float h, float radius,
                         SDL_FColor color);

  FontCache m_font; // TODO: make this support multiple font sizes
  SDL_Renderer* m_renderer;
};
