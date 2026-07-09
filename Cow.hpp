#pragma once
#include <SFML/Graphics.hpp>
#include "World.hpp"

// Пассивный моб, крупнее курицы: бродит туда-сюда, не падает с обрывов,
// разворачивается при ударе о стену. Логика движения — как у курицы.
class Cow {
private:
    float x, y;
    float vx, vy;
    float width, height;
    bool  onGround;

    int   direction;      // -1 влево, 0 стоит, 1 вправо
    float directionTimer;  // сколько ещё секунд держать текущее направление

    // Отдача от взрыва — так же, как у игрока и курицы
    float knockbackVx = 0.f;
    float knockbackTimer = 0.f;
    static constexpr float KNOCKBACK_DURATION = 0.25f;

    float mooCooldown = 0.f; // чтобы не мычала каждый кадр, пока игрок рядом

    static constexpr float GRAVITY    = 1200.f;
    static constexpr float WALK_SPEED = 32.f; // корова медленнее курицы

    void resolveCollisions(const World& world);

public:
    Cow(float startX, float startY);

    void update(float deltaTime, const World& world);
    void draw(sf::RenderWindow& window) const;

    void applyKnockback(float impulseX, float impulseY);

    // true — самое время промычать (игрок рядом и кулдаун истёк)
    bool shouldMoo(float playerX, float playerY, float deltaTime);

    float getX() const { return x; }
    float getY() const { return y; }
    float getWidth()  const { return width; }
    float getHeight() const { return height; }
};
