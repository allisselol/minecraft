#include "Cow.hpp"
#include <cstdlib>
#include <algorithm>

Cow::Cow(float startX, float startY)
    : x(startX), y(startY), vx(0.f), vy(0.f)
    , width(34.f), height(26.f)
    , onGround(false)
    , direction(0), directionTimer(1.f)
{}

void Cow::update(float deltaTime, const World& world) {
    // Раз в 2-4.5 сек выбираем новое направление: влево, стоять или вправо
    // (корова спокойнее курицы — дольше стоит на месте)
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

void Cow::applyKnockback(float impulseX, float impulseY) {
    knockbackVx = impulseX;
    knockbackTimer = KNOCKBACK_DURATION;
    vy += impulseY;
    onGround = false;
}

bool Cow::shouldMoo(float playerX, float playerY, float deltaTime) {
    if (mooCooldown > 0.f) mooCooldown -= deltaTime;
    float dx = playerX - (x + width / 2.f);
    float dy = playerY - (y + height / 2.f);
    float dist = std::sqrt(dx * dx + dy * dy) / BLOCK_SIZE;
    if (dist <= 6.f && mooCooldown <= 0.f) {
        mooCooldown = 6.f + (float)(rand() % 800) / 100.f; // 6-14 сек — корова мычит редко
        return true;
    }
    return false;
}

void Cow::resolveCollisions(const World& world) {
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

void Cow::draw(sf::RenderWindow& window) const {
    // Корова в сетке 20x14 суб-пикселей: прямоугольное тело с пятнами,
    // голова с рожками и розовым носом, чёрные ножки, хвостик.
    float psx = width  / 20.f;
    float psy = height / 14.f;

    auto rect = [&](int col, int row, int wc, int hr, sf::Color c) {
        sf::RectangleShape p({psx * wc + 0.5f, psy * hr + 0.5f});
        p.setPosition({x + col * psx, y + row * psy});
        p.setFillColor(c);
        window.draw(p);
    };

    sf::Color body(150, 100, 70);     // коричневое тело
    sf::Color bodyD(115, 75, 50);     // тень тела
    sf::Color patch(240, 235, 225);   // белые пятна
    sf::Color legC(40, 34, 30);       // чёрные ножки
    sf::Color eye(30, 26, 22);
    sf::Color snout(222, 160, 160);   // розовый нос
    sf::Color horn(225, 220, 205);    // рожки

    // Мягкая тень под коровой на земле
    if (onGround) {
        sf::CircleShape shadow(width * 0.36f);
        shadow.setScale({1.f, 0.28f});
        shadow.setOrigin({width * 0.36f, width * 0.36f});
        shadow.setPosition({x + width / 2.f, y + height});
        shadow.setFillColor(sf::Color(0, 0, 0, 70));
        window.draw(shadow);
    }

    // ==== ТЕЛО (столбцы 2..15, строки 3..9) ====
    rect(2, 3, 14, 6, body);
    rect(2, 2, 14, 1, body);          // спинка
    rect(2, 9, 14, 1, bodyD);         // тень снизу
    // пятна
    rect(4, 4, 3, 3, patch);
    rect(9, 5, 4, 3, patch);
    rect(3, 7, 2, 2, patch);

    // ==== ХВОСТИК (слева) ====
    rect(0, 4, 2, 1, bodyD);
    rect(0, 5, 1, 3, bodyD);

    // ==== ГОЛОВА (справа, столбцы 15..20, строки 1..7) ====
    rect(15, 2, 5, 5, body);
    rect(15, 1, 3, 1, body);
    // рожки
    rect(15, 0, 1, 1, horn); rect(18, 0, 1, 1, horn);
    // ухо
    rect(14, 2, 1, 1, bodyD);
    // глаз
    rect(18, 3, 1, 1, eye);
    // морда/нос
    rect(19, 4, 1, 3, snout);
    rect(17, 6, 3, 1, snout);

    // ==== НОГИ (чёрные снизу) ====
    rect(3, 10, 2, 4, legC);
    rect(7, 10, 2, 4, legC);
    rect(11, 10, 2, 4, legC);
    rect(14, 10, 2, 4, legC);
}
