#pragma once
#include <SFML/Graphics.hpp>
#include "World.hpp"

// Пассивный моб — даёт шерсть для кровати. Логика движения такая же, как у коровы/курицы.
class Sheep {
private:
    float x, y;
    float vx, vy;
    float width, height;
    bool  onGround;

    int   direction;      // -1 влево, 0 стоит, 1 вправо
    float directionTimer;  // сколько ещё секунд держать текущее направление

    // Отдача от взрыва — так же, как у остальных мобов
    float knockbackVx = 0.f;
    float knockbackTimer = 0.f;
    static constexpr float KNOCKBACK_DURATION = 0.25f;

    static constexpr float GRAVITY    = 1200.f;
    static constexpr float WALK_SPEED = 30.f;

    void resolveCollisions(const World& world);

public:
    Sheep(float startX, float startY);

    void update(float deltaTime, const World& world);
    void draw(sf::RenderWindow& window) const;

    void applyKnockback(float impulseX, float impulseY);

    float getX() const { return x; }
    float getY() const { return y; }
    float getWidth()  const { return width; }
    float getHeight() const { return height; }
};
