#include "Chicken.hpp"
#include <cstdlib>
#include <algorithm>

Chicken::Chicken(float startX, float startY)
    : x(startX), y(startY), vx(0.f), vy(0.f)
    , width(22.f), height(22.f)
    , onGround(false)
    , direction(0), directionTimer(1.f)
{}

void Chicken::update(float deltaTime, const World& world) {
    // Раз в 1.5-3.5 сек выбираем новое направление: влево, стоять или вправо
    directionTimer -= deltaTime;
    if (directionTimer <= 0.f) {
        int r = rand() % 3;
        direction = (r == 0) ? -1 : (r == 1 ? 0 : 1);
        directionTimer = 1.5f + (rand() % 200) / 100.f;
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

void Chicken::applyKnockback(float impulseX, float impulseY) {
    knockbackVx = impulseX;
    knockbackTimer = KNOCKBACK_DURATION;
    vy += impulseY;
    onGround = false;
}

void Chicken::resolveCollisions(const World& world) {
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

void Chicken::draw(sf::RenderWindow& window) const {
    // Цыплёнок в сетке 16x16 суб-пикселей: пухлое тельце, голова с клювом,
    // хвостик и две лапки. Никаких резких квадратов на весь блок.
    float ps = width / 16.f;

    auto rect = [&](int col, int row, int wc, int hr, sf::Color c) {
        sf::RectangleShape p({ps * wc + 0.5f, ps * hr + 0.5f});
        p.setPosition({x + col * ps, y + row * ps});
        p.setFillColor(c);
        window.draw(p);
    };

    sf::Color body(248, 214, 78);    // жёлтое тело
    sf::Color bodyD(226, 186, 52);   // тень тела
    sf::Color bodyL(255, 236, 140);  // светлый верх
    sf::Color eye(40, 34, 28);       // глаз
    sf::Color beak(236, 152, 46);    // клюв
    sf::Color leg(224, 150, 60);     // лапки
    sf::Color comb(226, 92, 70);     // гребешок

    // Мягкая тень под цыплёнком на земле — рисуем первой, чтобы была ПОД тельцем
    if (onGround) {
        sf::CircleShape shadow(width * 0.34f);
        shadow.setScale({1.f, 0.3f});
        shadow.setOrigin({width * 0.34f, width * 0.34f});
        shadow.setPosition({x + width / 2.f, y + height});
        shadow.setFillColor(sf::Color(0, 0, 0, 70));
        window.draw(shadow);
    }

    // ==== ТЕЛЬЦЕ (пухлый овал, столбцы 3..12, строки 6..13) ====
    rect(4, 6, 8, 7, body);
    rect(3, 7, 1, 5, body);          // левый бок
    rect(12, 7, 1, 5, body);         // правый бок
    rect(4, 6, 8, 1, bodyL);         // светлая спинка
    rect(4, 12, 8, 1, bodyD);        // тень снизу
    rect(3, 11, 10, 1, bodyD);

    // ==== ХВОСТИК (слева сверху) ====
    rect(2, 7, 2, 2, bodyD);
    rect(1, 8, 1, 2, body);

    // ==== ГОЛОВА (справа сверху, столбцы 9..14, строки 2..7) ====
    rect(9, 3, 5, 4, body);
    rect(9, 2, 4, 1, body);          // макушка
    rect(10, 2, 2, 1, bodyL);
    // гребешок
    rect(10, 1, 1, 1, comb); rect(11, 1, 1, 1, comb);
    // глаз
    rect(12, 4, 1, 1, eye);
    // клюв (торчит вправо)
    rect(14, 4, 2, 1, beak);
    rect(14, 5, 1, 1, beak);

    // ==== ЛАПКИ ====
    rect(6, 13, 1, 2, leg);
    rect(9, 13, 1, 2, leg);
    rect(5, 15, 2, 1, leg);          // ступни
    rect(9, 15, 2, 1, leg);
}