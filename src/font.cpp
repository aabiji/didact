#include <utf8.h>

#include "error.h"
#include "font.h"

FontCache::~FontCache() {
  TTF_CloseFont(m_font);
  for (int i = 0; i < m_textures.size(); i++) {
    SDL_DestroyTexture(m_textures[i]);
  }
}

FontCache::FontCache(SDL_Renderer* renderer, const char* path, int size,
                     SDL_Color color) {
  if (!TTF_WasInit())
    TTF_Init();

  m_renderer = renderer;
  m_color = color;
  m_font = TTF_OpenFont(path, size);
  if (m_font == nullptr)
    throw Error(SDL_GetError());

  // The texture size is the largest texture size possible
  SDL_PropertiesID props = SDL_GetRendererProperties(renderer);
  m_texture_size =
      SDL_GetNumberProperty(props, SDL_PROP_RENDERER_MAX_TEXTURE_SIZE_NUMBER, 0);
  if (m_texture_size <= 0)
    m_texture_size = 2048; // arbitrary default value

  m_x_offset = 0;
  m_y_offset = 0;
  m_texture_offset = 0;
  m_row_height = 0;

  // Fill the cache with the printable ASCII charset (space to tilde)
  create_new_texture();
  for (uint32_t codepoint = 32; codepoint < 127; codepoint++) {
    get_glyph(codepoint);
  }
}

void FontCache::render(std::string str, Vec2 p) {
  using iter = std::string::const_iterator;
  utf8::iterator<iter> it(str.begin(), str.begin(), str.end());
  utf8::iterator<iter> end(str.end(), str.begin(), str.end());
  float text_x = p.x;

  while (it != end) {
    Glyph glyph = get_glyph(*it);
    SDL_FRect target_rect = {.x = text_x, .y = p.y, .w = glyph.rect.w, .h = glyph.rect.h};
    SDL_RenderTexture(m_renderer, m_textures[glyph.texture_offset], &glyph.rect,
                      &target_rect);
    text_x += glyph.rect.w;
    ++it;
  }
}

Glyph FontCache::get_glyph(unsigned int codepoint) {
  if (m_glyphs.contains(codepoint))
    return m_glyphs[codepoint];

  SDL_Surface* surface = TTF_RenderGlyph_Blended(m_font, codepoint, m_color);

  if (m_x_offset + surface->w >= m_texture_size) {
    m_y_offset += m_row_height;
    m_x_offset = 0;
    m_row_height = 0;
  }

  if (m_y_offset + surface->h >= m_texture_size) {
    create_new_texture();
    m_x_offset = 0;
    m_y_offset = 0;
    m_texture_offset += 1;
  }

  // Compute the glyph position
  Glyph glyph = {.rect = {.x = (float)m_x_offset,
                          .y = (float)m_y_offset,
                          .w = (float)surface->w,
                          .h = (float)surface->h},
                 .texture_offset = m_texture_offset};

  m_row_height = std::max(m_row_height, surface->h);
  m_x_offset += surface->w;

  // Blit the glyph to its texture
  SDL_Texture* texture = SDL_CreateTextureFromSurface(m_renderer, surface);
  SDL_SetRenderTarget(m_renderer, m_textures[glyph.texture_offset]);
  SDL_RenderTexture(m_renderer, texture, nullptr, &glyph.rect);
  SDL_SetRenderTarget(m_renderer, nullptr);
  SDL_DestroySurface(surface);
  SDL_DestroyTexture(texture);

  m_glyphs.insert({codepoint, glyph});
  return glyph;
}

void FontCache::create_new_texture() {
  SDL_Texture* texture =
      SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
                        m_texture_size, m_texture_size);
  if (texture == nullptr)
    throw Error(SDL_GetError());

  SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
  SDL_SetRenderTarget(m_renderer, texture);
  SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
  SDL_RenderClear(m_renderer);
  SDL_SetRenderTarget(m_renderer, nullptr);
  m_textures.push_back(texture);
}

Vec2 FontCache::text_size(std::string str) {
  using iter = std::string::const_iterator;
  utf8::iterator<iter> it(str.begin(), str.begin(), str.end());
  utf8::iterator<iter> end(str.end(), str.begin(), str.end());

  Vec2 size = {0.0, 0.0};
  while (it != end) {
    Glyph glyph = get_glyph(*it);
    size.x += glyph.rect.w;
    size.y = std::max(glyph.rect.h, size.y);
    ++it;
  }
  return size;
}