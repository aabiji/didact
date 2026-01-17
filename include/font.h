#pragma once

#include <SDL3_ttf/SDL_ttf.h>
#include <unordered_map>

struct Vec2 {
  float x, y;
  Vec2 operator+(Vec2 b) { return {x + b.x, y + b.y}; }
};

struct Glyph {
  SDL_FRect rect;
  int texture_offset;
};

class FontCache {
public:
  ~FontCache();
  void init(SDL_Renderer* renderer, const char* path, int size, SDL_Color color);

  void render(std::string str, Vec2 position);
  Vec2 text_size(std::string str);

private:
  Glyph get_glyph(unsigned int codepoint);
  void create_new_texture();

  int m_row_height;
  int m_x_offset;
  int m_y_offset;
  int m_texture_offset;

  std::unordered_map<unsigned int, Glyph> m_glyphs;
  std::vector<SDL_Texture*> m_textures;

  int m_texture_size;
  SDL_Color m_color;
  TTF_Font* m_font;
  SDL_Renderer* m_renderer;
};