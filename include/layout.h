#pragma once

struct vec2 {
  float x, y;
};

struct color {
  float r, g, b, a;
};

class Element {
public:
  virtual ~Element() = default;
  virtual void render(SDL_Renderer* renderer, vec2 position) = 0;
};

class Rectangle : public Element {
public:
  Rectangle(vec2 size) : m_size(size) {}
  Rectangle() : m_size(0, 0) {}
  ~Rectangle() override {}

  void render(SDL_Renderer* renderer, vec2 position) override {
    // Draw the border
    SDL_FRect rect = {.x = position.x, .y = position.y, .w = m_size.x, .h = m_size.y};
    SDL_SetRenderDrawColor(renderer, m_border.r, m_border.g, m_border.b, m_border.a);
    SDL_RenderFillRect(renderer, &rect);

    // Draw the background
    rect.x += m_border_size.x;
    rect.y += m_border_size.y;
    rect.w -= m_border_size.x * 2;
    rect.h -= m_border_size.y * 2;
    SDL_SetRenderDrawColor(renderer, m_bg.r, m_bg.g, m_bg.b, m_bg.a);
    SDL_RenderFillRect(renderer, &rect);
  }

  vec2 m_size;
  vec2 m_border_size;
  color m_border;
  color m_bg;
};

class DemoContainer : public Element {
public:
  DemoContainer() {
    Rectangle* rect = new Rectangle({100, 100});
    rect->m_border_size = {5, 5};
    rect->m_bg = {255, 255, 255, 255};
    rect->m_border = {255, 0, 255, 255};
    m_children.push_back(rect);
  }

  ~DemoContainer() override {
    for (Element* e : m_children)
      free(e);
  }

  void render(SDL_Renderer* renderer, vec2 position) override {
    for (Element* e : m_children)
      e->render(renderer, position);
  }

private:
  std::vector<Element*> m_children;
};