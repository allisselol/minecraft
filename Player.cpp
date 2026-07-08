#include "Player.hpp"
#include <cmath>
#include <algorithm>

Player::Player(float startX, float startY)
    : x(startX), y(startY), vx(0), vy(0)
    , width(BLOCK_SIZE), height(BLOCK_SIZE * 2)
    , onGround(false), moveLeft(false), moveRight(false)
    , walkTimer(0.f)
{}

void Player::handleKeyPressed(sf::Keyboard::Key key) {
    if (key == sf::Keyboard::Key::A) moveLeft  = true;
    if (key == sf::Keyboard::Key::D) moveRight = true;
    if (key == sf::Keyboard::Key::W || key == sf::Keyboard::Key::Space) {
        wantUp = true;
        if (onGround) {
            vy = JUMP_FORCE;
            onGround = false;
        }
    }
}

void Player::handleKeyReleased(sf::Keyboard::Key key) {
    if (key == sf::Keyboard::Key::A) moveLeft  = false;
    if (key == sf::Keyboard::Key::D) moveRight = false;
    if (key == sf::Keyboard::Key::W || key == sf::Keyboard::Key::Space) wantUp = false;
}

bool Player::isInWater(const World& world) const {
    // Проверяем центр игрока — если там блок воды, считаем что плаваем
    int cx = (int)((x + width  / 2.f) / BLOCK_SIZE);
    int cy = (int)((y + height / 2.f) / BLOCK_SIZE);
    return world.inBounds(cx, cy) && world.getBlock(cx, cy).isWater();
}

void Player::update(float deltaTime, const World& world) {
    vx = 0;
    if (moveLeft)  vx = -MOVE_SPEED;
    if (moveRight) vx =  MOVE_SPEED;

    if (knockbackTimer > 0.f) {
        knockbackTimer = std::max(0.f, knockbackTimer - deltaTime);
        float t = knockbackTimer / KNOCKBACK_DURATION; // 1 -> 0, плавно гасим толчок
        vx += knockbackVx * t;
    }

    bool inWater = isInWater(world);
    justEnteredWater = inWater && !wasInWater; // вошёл в воду именно сейчас
    wasInWater = inWater;

    if (inWater) {
        // В воде: слабее гравитация, движемся медленнее, максимальная скорость погружения ограничена
        vx *= 0.6f;
        vy += GRAVITY * 0.25f * deltaTime;   // тонем медленно
        if (vy > 120.f) vy = 120.f;          // ограничение скорости погружения
        if (wantUp) vy = -160.f;             // гребок вверх — всплываем, пока зажат W/Space
    } else {
        vy += GRAVITY * deltaTime;
    }

    x += vx * deltaTime;
    resolveCollisions(world);

    bool wasOnGround = onGround;
    float impactVy = vy; // скорость падения непосредственно перед проверкой приземления

    y += vy * deltaTime;
    resolveCollisions(world);

    // Урон от падения — только если НЕ в воде (вода смягчает)
    if (!inWater && !wasOnGround && onGround && impactVy > 0.f) {
        float fallHeight = (impactVy * impactVy) / (2.f * GRAVITY);
        if (fallHeight > FALL_DAMAGE_SAFE_HEIGHT) {
            float extraBlocks = (fallHeight - FALL_DAMAGE_SAFE_HEIGHT) / BLOCK_SIZE;
            pendingFallDamage += extraBlocks * FALL_DAMAGE_PER_BLOCK;
        }
    }

    // Анимация ходьбы: таймер идёт только когда игрок реально движется по земле
    if (isMoving() && onGround) {
        walkTimer += deltaTime;
    } else if (!isMoving()) {
        walkTimer = 0.f; // сброс в состояние покоя (ноги вместе)
    }
}

float Player::takeFallDamage() {
    float d = pendingFallDamage;
    pendingFallDamage = 0.f;
    return d;
}

void Player::applyKnockback(float impulseX, float impulseY) {
    knockbackVx = impulseX;
    knockbackTimer = KNOCKBACK_DURATION;
    vy += impulseY; // мгновенный вертикальный импульс — гравитация сама вернёт вниз
    onGround = false;
}

void Player::respawn(float startX, float startY) {
    x = startX;
    y = startY;
    vx = 0.f;
    vy = 0.f;
    onGround = false;
    walkTimer = 0.f;
    pendingFallDamage = 0.f;
    knockbackVx = 0.f;
    knockbackTimer = 0.f;
}

void Player::resolveCollisions(const World& world) {
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
            } else {
                if (oT < oB) { y -= oT; vy = 0; onGround = true; }
                else          { y += oB; vy = 0; }
            }
        }
    }
}

void Player::draw(sf::RenderWindow& window) {
    // Приземистый Стив: крупная голова, короткое широкое тело, короткие ноги.
    // Сетка 16 (шир.) x 24 (выс.) суб-пикселей.
    float ps  = width  / 16.f;
    float psy = height / 24.f;

    auto rect = [&](int col, int row, int wc, int hr, sf::Color c) {
        sf::RectangleShape p({ps * wc + 0.5f, psy * hr + 0.5f});
        p.setPosition({x + col * ps, y + row * psy});
        p.setFillColor(c);
        window.draw(p);
    };
    auto px = [&](int col, int row, sf::Color c) { rect(col, row, 1, 1, c); };

    sf::Color hair(74, 46, 22);
    sf::Color skin(214, 172, 126);
    sf::Color skinD(190, 148, 104);
    sf::Color eyeW(238, 238, 238);
    sf::Color eyeB(94, 70, 170);
    sf::Color mouth(150, 100, 70);
    sf::Color shirt(38, 158, 150);
    sf::Color shirtD(28, 130, 122);
    sf::Color arm(214, 172, 126);
    sf::Color armSleeve(38, 158, 150);
    sf::Color pants(60, 66, 150);
    sf::Color pantsD(46, 50, 120);
    sf::Color boot(66, 66, 74);

    // ==== ГОЛОВА (крупная: столбцы 3..12 = 10 шир., строки 0..10) ====
    int hx = 3;
    rect(hx, 0, 10, 2, hair);              // шапка волос
    rect(hx, 2, 1, 5, hair);               // левая прядь
    rect(hx + 9, 2, 1, 5, hair);           // правая прядь
    rect(hx + 1, 2, 8, 6, skin);           // лицо
    // глаза (строка 4)
    rect(hx + 2, 4, 1, 1, eyeW); rect(hx + 3, 4, 1, 1, eyeB);
    rect(hx + 6, 4, 1, 1, eyeB); rect(hx + 7, 4, 1, 1, eyeW);
    // нос (строка 5)
    px(hx + 4, 5, skinD); px(hx + 5, 5, skinD);
    // рот (строка 6)
    rect(hx + 4, 6, 2, 1, mouth);
    // подбородок
    rect(hx + 2, 8, 6, 1, skinD);

    // ==== ШЕЯ (короткая) ====
    rect(6, 9, 4, 1, skinD);

    // ==== ТУЛОВИЩЕ (короткое и широкое: столбцы 4..11 = 8 шир., строки 10..17) ====
    int bx = 4;
    rect(bx, 10, 8, 7, shirt);
    rect(bx + 7, 10, 1, 7, shirtD);        // тень справа
    rect(bx, 16, 8, 1, shirtD);            // низ рубашки

    // ==== РУКИ (по бокам, короткие) с анимацией ====
    float armSwing = std::sin(walkTimer * 12.f) * psy * 1.2f;
    // левая рука
    {
        float ax = x + 1 * ps, ay = y + 10 * psy + armSwing;
        sf::RectangleShape sleeve({ps * 3 + 0.5f, psy * 4 + 0.5f});
        sleeve.setPosition({ax, ay}); sleeve.setFillColor(armSleeve); window.draw(sleeve);
        sf::RectangleShape hand({ps * 3 + 0.5f, psy * 2 + 0.5f});
        hand.setPosition({ax, ay + psy * 4}); hand.setFillColor(arm); window.draw(hand);
    }
    // правая рука (в противофазе)
    {
        float ax = x + 12 * ps, ay = y + 10 * psy - armSwing;
        sf::RectangleShape sleeve({ps * 3 + 0.5f, psy * 4 + 0.5f});
        sleeve.setPosition({ax, ay}); sleeve.setFillColor(armSleeve); window.draw(sleeve);
        sf::RectangleShape hand({ps * 3 + 0.5f, psy * 2 + 0.5f});
        hand.setPosition({ax, ay + psy * 4}); hand.setFillColor(arm); window.draw(hand);
    }

    // ==== НОГИ (короткие: столбцы 4..11, строки 17..24) с анимацией ====
    float legSwing = std::sin(walkTimer * 12.f) * psy * 1.0f;
    // левая нога
    {
        float lx = x + 4 * ps, ly = y + 17 * psy + legSwing;
        sf::RectangleShape leg({ps * 4 + 0.5f, psy * 5 + 0.5f});
        leg.setPosition({lx, ly}); leg.setFillColor(pants); window.draw(leg);
        sf::RectangleShape shoe({ps * 4 + 0.5f, psy * 2 + 0.5f});
        shoe.setPosition({lx, ly + psy * 5}); shoe.setFillColor(boot); window.draw(shoe);
    }
    // правая нога (в противофазе)
    {
        float rx = x + 8 * ps, ry = y + 17 * psy - legSwing;
        sf::RectangleShape leg({ps * 4 + 0.5f, psy * 5 + 0.5f});
        leg.setPosition({rx, ry}); leg.setFillColor(pantsD); window.draw(leg);
        sf::RectangleShape shoe({ps * 4 + 0.5f, psy * 2 + 0.5f});
        shoe.setPosition({rx, ry + psy * 5}); shoe.setFillColor(boot); window.draw(shoe);
    }
}