#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>
#include <utility>

#include "error.h"
#include "font.h"

class Cursor {
public:
  Cursor() {
    m_default = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
    m_pointer = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_POINTER);
  }

  ~Cursor() {
    SDL_DestroyCursor(m_default);
    SDL_DestroyCursor(m_pointer);
  }

  void update(Vec2 pos, Vec2 size) {
    float mouse_x = 0, mouse_y = 0;
    SDL_GetMouseState(&mouse_x, &mouse_y);

    bool x_inside = mouse_x >= pos.x && mouse_x <= pos.x + size.x;
    bool y_inside = mouse_y >= pos.y && mouse_y <= pos.y + size.y;
    SDL_SetCursor(x_inside && y_inside ? m_pointer : m_default);
  }

private:
  SDL_Cursor* m_default;
  SDL_Cursor* m_pointer;
};

class SVG {
public:
  ~SVG() {
    if (m_initialized && m_tex)
      SDL_DestroyTexture(m_tex);
  }

  SVG() : m_initialized(false) {}

  SVG(SDL_Renderer* renderer, const char* path, Vec2 size, SDL_Color color) {
    SDL_IOStream* ops = SDL_IOFromFile(path, "rb");
    if (!ops)
      throw Error(SDL_GetError());

    SDL_Surface* surf = IMG_LoadSizedSVG_IO(ops, size.x, size.y);
    m_tex = SDL_CreateTextureFromSurface(renderer, surf);
    m_size = size;
    m_initialized = true;
    SDL_DestroySurface(surf);

    // NOTE: This only works if the original svg is white
    SDL_SetTextureColorMod(m_tex, color.r, color.g, color.b);
  }

  SVG(const SVG&) = delete;
  SVG& operator=(const SVG&) = delete;

  SVG(SVG&& other) noexcept {
    m_tex = std::exchange(other.m_tex, nullptr);
    m_size = std::exchange(other.m_size, {0, 0});
    m_initialized = std::exchange(other.m_initialized, false);
  }

  SVG& operator=(SVG&& other) noexcept {
    if (this != &other) {
      if (m_initialized)
        SDL_DestroyTexture(m_tex);

      m_tex = std::exchange(other.m_tex, nullptr);
      m_size = std::exchange(other.m_size, {0, 0});
      m_initialized = std::exchange(other.m_initialized, false);
    }
    return *this;
  }

  Vec2 size() { return m_size; }

  void render(SDL_Renderer* renderer, Vec2 p) {
    if (!m_initialized)
      return;
    SDL_FRect icon_rect = {.x = p.x, .y = p.y, .w = m_size.x, .h = m_size.y};
    SDL_RenderTexture(renderer, m_tex, nullptr, &icon_rect);
  }

private:
  bool m_initialized;
  Vec2 m_size;
  SDL_Texture* m_tex;
};

class Button {
public:
  Button(SDL_Renderer* renderer, FontCache* font)
      : m_have_icon(false), m_icon_size(0), m_label_size(0, 0), m_spacing(8),
        m_renderer(renderer), m_font(font) {}

  void set_label(std::string label) {
    m_label = label;
    m_label_size = m_font->text_size(label);
  }

  void set_icon(const char* path, Vec2 size, SDL_Color color = {255, 255, 255, 255}) {
    m_icon = SVG(m_renderer, path, size, color);
    m_icon_size = size;
    m_have_icon = true;
  }

  Vec2 size() {
    return {m_icon_size.x + m_spacing + m_label_size.x,
            std::max(m_icon_size.x, m_label_size.y)};
  }

  void render(Vec2 p, Cursor& cursor) {
    cursor.update(p, size());

    // Vertically center the icon and label with each other
    bool icon_larger = m_icon_size.y >= m_label_size.y;
    Vec2 icon_offset = {0, icon_larger ? 0.0f : (m_label_size.y - m_icon_size.y) / 2.0f};
    Vec2 label_offset = {
        m_have_icon ? m_spacing + m_icon_size.x : m_spacing,
        icon_larger ? (m_icon_size.y - m_label_size.y) / 2.0f : 0.0f,
    };

    if (m_have_icon)
      m_icon.render(m_renderer, p + icon_offset);
    if (m_label.size() > 0)
      m_font->render(m_label, p + label_offset);
  }

private:
  SVG m_icon;
  Vec2 m_icon_size;
  bool m_have_icon;

  std::string m_label;
  Vec2 m_label_size;

  float m_x, m_y;
  float m_spacing; // betweeen the icon and the label

  SDL_Renderer* m_renderer;
  FontCache* m_font;
};