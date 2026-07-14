#include "Sheep.hpp"
#include <cstdlib>
#include <algorithm>

Sheep::Sheep(float startX, float startY)
    : x(startX), y(startY), vx(0.f), vy(0.f)
    , width(28.f), height(22.f)
    , onGround(false)
    , direction(0), directionTimer(1.f)
{}

void Sheep::update(float deltaTime, const World& world) {
    // Раз в 2-4.5 сек выбираем новое направление: влево, стоять или вправо
    directionTimer -= deltaTime;
    if (directionTimer <= 0.f) {
        int r = rand() % 3;
        direction = (r == 0) ? -1 : (r == 1 ? 0 : 1);
        directionTimer = 2.f + (rand() % 250) / 100.f;
    }

    // Не шагаем с обрыва: если впереди по ходу движения нет опоры — разворачиваемся
    if (direction != 0 && onGround) {
        float aheadX = x + (direction > 0 ? width + 2.f : -2.f);
        float footY  = y + height + 4.f;
        int bx = (int)(aheadX / BLOCK_SIZE);
        int by = (int)(footY  / BLOCK_SIZE);
        bool floorAhead = world.inBounds(bx, by) && world.getBlock(bx, by).isSolid();
        if (!floorAhead) {
            direction = -direction;
            directionTimer = 1.5f;
        }
    }

    vx = direction * WALK_SPEED;

    if (knockbackTimer > 0.f) {
        knockbackTimer = std::max(0.f, knockbackTimer - deltaTime);
        float t = knockbackTimer / KNOCKBACK_DURATION;
        vx += knockbackVx * t;
    }

    vy += GRAVITY * deltaTime;

    x += vx * deltaTime;
    resolveCollisions(world);
    y += vy * deltaTime;
    resolveCollisions(world);
}

void Sheep::applyKnockback(float impulseX, float impulseY) {
    knockbackVx = impulseX;
    knockbackTimer = KNOCKBACK_DURATION;
    vy += impulseY;
    onGround = false;
}

void Sheep::resolveCollisions(const World& world) {
    int left   = (int)(x / BLOCK_SIZE);
    int right  = (int)((x + width  - 1) / BLOCK_SIZE);
    int top    = (int)(y / BLOCK_SIZE);
    int bottom = (int)((y + height - 1) / BLOCK_SIZE);

    onGround = false;

    for (int bx = left; bx <= right; bx++) {
        for (int by = top; by <= bottom; by++) {
            if (!world.inBounds(bx, by)) continue;
            if (!world.getBlock(bx, by).isSolid()) continue;

            float bL = bx * BLOCK_SIZE, bR = bL + BLOCK_SIZE;
            float bT = by * BLOCK_SIZE, bB = bT + BLOCK_SIZE;

            float oL = (x + width)  - bL;
            float oR = bR - x;
            float oT = (y + height) - bT;
            float oB = bB - y;

            float minX = std::min(oL, oR);
            float minY = std::min(oT, oB);

            if (minX < minY) {
                if (oL < oR) x -= oL; else x += oR;
                vx = 0;
                direction = -direction; // уткнулись в стену — развернулись
            } else {
                if (oT < oB) { y -= oT; vy = 0; onGround = true; }
                else          { y += oB; vy = 0; }
            }
        }
    }
}

void Sheep::draw(sf::RenderWindow& window) const {
    // Овца в сетке 18x14 суб-пикселей: пушистое белое тело-облако, чёрная голова/ноги.
    float psx = width  / 18.f;
    float psy = height / 14.f;

    auto rect = [&](int col, int row, int wc, int hr, sf::Color c) {
        sf::RectangleShape p({psx * wc + 0.5f, psy * hr + 0.5f});
        p.setPosition({x + col * psx, y + row * psy});
        p.setFillColor(c);
        window.draw(p);
    };

    sf::Color wool(240, 240, 235);    // белая шерсть
    sf::Color woolD(205, 205, 198);   // тень шерсти
    sf::Color face(45, 40, 38);       // тёмная морда/уши
    sf::Color legC(45, 40, 38);       // чёрные ножки

    // Мягкая тень на земле
    if (onGround) {
        sf::CircleShape shadow(width * 0.36f);
        shadow.setScale({1.f, 0.28f});
        shadow.setOrigin({width * 0.36f, width * 0.36f});
        shadow.setPosition({x + width / 2.f, y + height});
        shadow.setFillColor(sf::Color(0, 0, 0, 70));
        window.draw(shadow);
    }

    // ==== ПУШИСТОЕ ТЕЛО (столбцы 1..13, строки 2..8) — "рваные" края для эффекта шерсти ====
    rect(2, 2, 10, 1, wool);
    rect(1, 3, 12, 5, wool);
    rect(2, 8, 10, 1, woolD);
    // клочки по краям, чтобы силуэт не был идеальным прямоугольником
    rect(0, 4, 1, 3, wool);
    rect(13, 4, 1, 3, wool);
    rect(3, 1, 2, 1, wool);
    rect(8, 1, 2, 1, wool);

    // ==== ГОЛОВА (справа, тёмная) ====
    rect(13, 3, 4, 4, face);
    rect(16, 2, 1, 1, face); // ушко

    // ==== НОГИ ====
    rect(2, 9, 2, 4, legC);
    rect(6, 9, 2, 4, legC);
    rect(9, 9, 2, 4, legC);
    rect(12, 9, 2, 4, legC);
}
