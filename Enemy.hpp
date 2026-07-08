#pragma once
#include <SFML/Graphics.hpp>
#include "World.hpp"

// Тип враждебного моба
enum class EnemyType {
    STALKER,   // Бродяга — идёт к игроку, бьёт вблизи
    CREEPER,   // Ползун — подходит вплотную и взрывается
    ARCHER,    // Лучник — держит дистанцию, стреляет
    JUMPER     // Прыгун — быстрый, прыгает к игроку
};

// Враждебный моб. Поведение зависит от типа. Физика (гравитация, коллизии) — как у игрока.
class Enemy {
private:
    EnemyType type;
    float x, y;
    float vx, vy;
    float width, height;
    bool  onGround;
    int   facing;          // -1 влево, 1 вправо (для отрисовки)

    int   health;
    float attackCooldown;  // сколько ещё секунд до следующего удара
    float hurtFlash;       // таймер "вспышки" при получении урона (для отрисовки)
    float growlCooldown;   // чтобы не рычал каждый кадр
    bool  wasNear = false; // был ли игрок близко в прошлом кадре

    // Для ползуна: заряд взрыва
    float fuse = -1.f;     // -1 = не активирован; >=0 = обратный отсчёт до взрыва
    bool  exploded = false;
    bool  fuseJustLit = false; // фитиль загорелся именно в этом кадре (для звука)

    // Для прыгуна: кулдаун прыжка
    float jumpCooldown = 0.f;
    // Для лучника: таймер звука шагов
    float stepTimer = 0.f;
    bool  steppedThisFrame = false;

    // Отдача (от удара игрока / взрыва)
    float knockbackVx = 0.f;
    float knockbackTimer = 0.f;
    static constexpr float KNOCKBACK_DURATION = 0.25f;

    static constexpr float GRAVITY     = 1200.f;
    static constexpr float ATTACK_RANGE = 1.2f;  // дистанция удара, в блоках
    static constexpr float ATTACK_INTERVAL = 1.0f;

    float walkSpeed() const;  // скорость зависит от типа

    void resolveCollisions(const World& world);

public:
    Enemy(float startX, float startY, EnemyType t = EnemyType::STALKER);

    // Обновление: преследует игрока. Возвращает true, если моб бьёт игрока в этом кадре.
    bool update(float deltaTime, const World& world, float playerX, float playerY);

    void draw(sf::RenderWindow& window) const;

    void applyKnockback(float impulseX, float impulseY);
    void takeDamage(int dmg);
    bool isDead() const { return health <= 0; }

    bool shouldGrowl(float playerX, float playerY, float deltaTime);

    // Ползун: готов ли взорваться прямо сейчас (Game создаст взрыв и удалит моба)
    bool isExploding() const { return exploded; }

    // Ползун: загорелся ли фитиль в этом кадре (для звука)
    bool didLightFuse() const { return fuseJustLit; }

    // Лучник: хочет ли выстрелить (Game создаст снаряд). Сбрасывает внутренний флаг.
    bool wantsToShoot(float playerX, float playerY);

    // Лучник: сделал ли шаг в этом кадре (для звука шагов)
    bool didStep() const { return steppedThisFrame; }

    EnemyType getType() const { return type; }
    float getX() const { return x; }
    float getY() const { return y; }
    float getWidth()  const { return width; }
    float getHeight() const { return height; }
};