#pragma once
#include <SFML/Graphics.hpp>
#include "World.hpp"

// Простой пассивный моб: бродит туда-сюда, не падает с обрывов,
// разворачивается при ударе о стену. Гравитация и коллизии — как у игрока, но проще.
class Chicken {
private:
    float x, y;
    float vx, vy;
    float width, height;
    bool  onGround;

    int   direction;      // -1 влево, 0 стоит, 1 вправо
    float directionTimer;  // сколько ещё секунд держать текущее направление

    // Отдача от взрыва — так же, как у игрока
    float knockbackVx = 0.f;
    float knockbackTimer = 0.f;
    static constexpr float KNOCKBACK_DURATION = 0.25f;

    static constexpr float GRAVITY    = 1200.f;
    static constexpr float WALK_SPEED = 45.f;

    void resolveCollisions(const World& world);

public:
    Chicken(float startX, float startY);

    void update(float deltaTime, const World& world);
    void draw(sf::RenderWindow& window) const;

    void applyKnockback(float impulseX, float impulseY);

    float getX() const { return x; }
    float getY() const { return y; }
    float getWidth()  const { return width; }
    float getHeight() const { return height; }
};
