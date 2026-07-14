#pragma once
#include <SFML/Graphics.hpp>
#include "World.hpp"

class Player {
private:
    float x, y;
    float vx, vy;
    float width, height;
    bool  onGround;
    bool  moveLeft, moveRight;
    bool  wantUp = false; // зажата ли клавиша вверх (для плавания в воде и лазанья по лестнице)
    bool  wantDown = false; // зажата ли клавиша вниз (спуск по лестнице)
    bool  wasInWater = false;     // был ли в воде в прошлом кадре
    bool  justEnteredWater = false; // вошёл в воду именно в этом кадре (для звука плеска)
    float walkTimer; // время в текущем цикле ходьбы, для анимации ног
    bool  sleeping = false; // спит ли сейчас (лежит на кровати) — во время сна не двигается

    static constexpr float GRAVITY    = 1200.f;
    static constexpr float JUMP_FORCE = -550.f;
    static constexpr float MOVE_SPEED = 200.f;
    static constexpr float CLIMB_SPEED = 110.f; // скорость подъёма/спуска по лестнице

    // Высота, с которой падение ещё безопасно (чуть больше высоты обычного прыжка),
    // и урон за каждый лишний блок падения сверх неё
    static constexpr float FALL_DAMAGE_SAFE_HEIGHT = BLOCK_SIZE * 4.2f;
    static constexpr float FALL_DAMAGE_PER_BLOCK   = 1.0f;

    float pendingFallDamage = 0.f;

    // Отдача от взрыва: горизонтальная скорость толчка, затухающая за KNOCKBACK_DURATION секунд
    float knockbackVx = 0.f;
    float knockbackTimer = 0.f;
    static constexpr float KNOCKBACK_DURATION = 0.25f;

    void resolveCollisions(const World& world);
    bool isInWater(const World& world) const; // находится ли игрок (его центр) в воде
    bool checkLadder(const World& world) const; // проверка на месте: касается ли игрок лестницы

    bool onLadderState = false; // закэшированный результат checkLadder() за этот кадр

public:
    Player(float startX, float startY);

    void handleKeyPressed(sf::Keyboard::Key key);
    void handleKeyReleased(sf::Keyboard::Key key);

    void update(float deltaTime, const World& world);
    void draw(sf::RenderWindow& window);

    // Возвращает накопленный урон от последнего падения и обнуляет счётчик (вызывать раз в кадр)
    float takeFallDamage();

    // Резкий толчок (например, от взрыва). Действует поверх обычного управления
    // ещё KNOCKBACK_DURATION секунд, затем плавно перестаёт влиять на движение.
    void applyKnockback(float impulseX, float impulseY);

    // Полный сброс игрока в указанную точку (используется при смерти/респавне)
    void respawn(float startX, float startY);

    float getX()      const { return x; }
    float getY()      const { return y; }
    float getWidth()  const { return width; }
    float getHeight() const { return height; }
    bool  isOnGround() const { return onGround; }
    bool  isSleeping() const { return sleeping; }
    // Укладывает игрока спать прямо на кровать. bedX/bedY — верхний левый угол
    // клетки-изножья кровати (2 клетки в ширину, 1 в высоту) — не позиция стоя.
    void startSleep(float bedX, float bedY) {
        sleeping = true;
        x = bedX; y = bedY;
        vx = 0.f; vy = 0.f;
        moveLeft = moveRight = false;
    }
    void wakeUp() {
        sleeping = false;
        y -= (float)BLOCK_SIZE; // "координаты кровати" — это уровень ног, а не верх стоящего игрока
    }
    bool  isMoving()   const { return moveLeft || moveRight; }
    bool  isClimbing() const { return onLadderState; } // на лестнице ли сейчас (для звука шагов)
    bool  isClimbingMoving() const { return onLadderState && (wantUp || wantDown); } // реально лезет, не просто стоит
    bool  didJustEnterWater() const { return justEnteredWater; }
};