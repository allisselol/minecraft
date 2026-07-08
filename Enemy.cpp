#include "Enemy.hpp"
#include <cstdlib>
#include <algorithm>
#include <cmath>

Enemy::Enemy(float startX, float startY, EnemyType t)
    : type(t), x(startX), y(startY), vx(0.f), vy(0.f)
    , width(26.f), height(52.f)
    , onGround(false), facing(1)
    , attackCooldown(0.f), hurtFlash(0.f), growlCooldown(0.f)
{
    switch (type) {
        case EnemyType::STALKER: health = 6; break;
        case EnemyType::CREEPER: health = 5; width = 26.f; height = 46.f; break;
        case EnemyType::ARCHER:  health = 4; break;
        case EnemyType::JUMPER:  health = 5; width = 24.f; height = 44.f; break;
    }
}

float Enemy::walkSpeed() const {
    switch (type) {
        case EnemyType::STALKER: return 70.f;
        case EnemyType::CREEPER: return 60.f;
        case EnemyType::ARCHER:  return 55.f;
        case EnemyType::JUMPER:  return 110.f;
    }
    return 70.f;
}

bool Enemy::shouldGrowl(float playerX, float playerY, float deltaTime) {
    if (growlCooldown > 0.f) growlCooldown -= deltaTime;
    float dx = playerX - x, dy = playerY - y;
    float dist = std::sqrt(dx * dx + dy * dy) / BLOCK_SIZE;
    bool near = dist <= 5.f;
    if (near && growlCooldown <= 0.f) {
        growlCooldown = 3.0f;
        wasNear = near;
        return true;
    }
    wasNear = near;
    return false;
}

bool Enemy::wantsToShoot(float playerX, float playerY) {
    if (type != EnemyType::ARCHER) return false;
    if (attackCooldown > 0.f) return false;
    float dx = playerX - x, dy = playerY - y;
    float dist = std::sqrt(dx * dx + dy * dy) / BLOCK_SIZE;
    if (dist >= 3.f && dist <= 9.f) {  // стреляет со средней дистанции
        attackCooldown = 2.0f;
        return true;
    }
    return false;
}

bool Enemy::update(float deltaTime, const World& world, float playerX, float playerY) {
    if (attackCooldown > 0.f) attackCooldown -= deltaTime;
    if (hurtFlash > 0.f)      hurtFlash -= deltaTime;
    if (jumpCooldown > 0.f)   jumpCooldown -= deltaTime;

    float dx = playerX - x;
    float dyToPlayer = playerY - y;
    float dist = std::sqrt(dx * dx + dyToPlayer * dyToPlayer) / BLOCK_SIZE;

    int dir = 0;
    if (dx > 4.f)  { dir =  1; facing =  1; }
    else if (dx < -4.f) { dir = -1; facing = -1; }

    // Лучник держит дистанцию: если игрок слишком близко — отходит
    if (type == EnemyType::ARCHER && dist < 3.f) dir = -dir;

    // Ползун: подойдя вплотную — запускает фитиль
    if (type == EnemyType::CREEPER) {
        fuseJustLit = false;
        if (dist <= 1.6f && fuse < 0.f) { fuse = 1.2f; fuseJustLit = true; } // начать отсчёт + сигнал звука
        if (fuse >= 0.f) {
            fuse -= deltaTime;
            dir = 0; // замирает перед взрывом
            if (fuse <= 0.f) { exploded = true; }
        }
    }

    // Не шагаем с обрыва
    if (dir != 0 && onGround) {
        float aheadX = x + (dir > 0 ? width + 2.f : -2.f);
        float footY  = y + height + 4.f;
        int bx = (int)(aheadX / BLOCK_SIZE);
        int by = (int)(footY  / BLOCK_SIZE);
        bool floorAhead = world.inBounds(bx, by) && world.getBlock(bx, by).isSolid();
        if (!floorAhead) dir = 0;
    }

    vx = dir * walkSpeed();

    // Лучник: отмечаем шаги при движении по земле (для звука)
    steppedThisFrame = false;
    if (type == EnemyType::ARCHER && dir != 0 && onGround) {
        stepTimer -= deltaTime;
        if (stepTimer <= 0.f) {
            stepTimer = 0.45f; // интервал между шагами
            steppedThisFrame = true;
        }
    }

    // Прыгун: периодически подпрыгивает к игроку
    if (type == EnemyType::JUMPER && onGround && dir != 0 && jumpCooldown <= 0.f) {
        vy = -560.f;
        onGround = false;
        jumpCooldown = 1.2f;
    }

    // Перепрыгивание препятствий (кроме ползуна на фитиле)
    if (dir != 0 && onGround && type != EnemyType::JUMPER) {
        float aheadX = x + (dir > 0 ? width + 2.f : -2.f);
        int bx = (int)(aheadX / BLOCK_SIZE);
        int byHead = (int)((y + height * 0.5f) / BLOCK_SIZE);
        if (world.inBounds(bx, byHead) && world.getBlock(bx, byHead).isSolid()) {
            vy = -520.f;
            onGround = false;
        }
    }

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

    // Ближний удар (Бродяга и Прыгун)
    if ((type == EnemyType::STALKER || type == EnemyType::JUMPER)
        && dist <= ATTACK_RANGE && attackCooldown <= 0.f) {
        attackCooldown = ATTACK_INTERVAL;
        return true;
    }
    return false;
}

void Enemy::applyKnockback(float impulseX, float impulseY) {
    knockbackVx = impulseX;
    knockbackTimer = KNOCKBACK_DURATION;
    vy += impulseY;
    onGround = false;
}

void Enemy::takeDamage(int dmg) {
    health -= dmg;
    hurtFlash = 0.2f;
}

void Enemy::resolveCollisions(const World& world) {
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

void Enemy::draw(sf::RenderWindow& window) const {
    float ps = width / 8.f;
    float psy = height / 16.f;
    bool flash = (hurtFlash > 0.f);

    // Ползун мигает белым перед взрывом
    bool creeperFlash = (type == EnemyType::CREEPER && fuse >= 0.f
                         && std::fmod(fuse, 0.25f) < 0.125f);

    auto rect = [&](int col, int row, int wc, int hr, sf::Color c) {
        if (flash) c = sf::Color(255, 120, 120);
        if (creeperFlash) c = sf::Color(255, 255, 255);
        sf::RectangleShape p({ps * wc + 0.5f, psy * hr + 0.5f});
        p.setPosition({x + col * ps, y + row * psy});
        p.setFillColor(c);
        window.draw(p);
    };
    auto eyes = [&](sf::Color ec) {
        if (flash || creeperFlash) return;
        sf::RectangleShape e1({ps + 0.5f, psy + 0.5f});
        e1.setFillColor(ec);
        e1.setPosition({x + 2 * ps, y + 2 * psy});
        window.draw(e1);
        sf::RectangleShape e2({ps + 0.5f, psy + 0.5f});
        e2.setFillColor(ec);
        e2.setPosition({x + 5 * ps, y + 2 * psy});
        window.draw(e2);
    };

    // Палитра по типу
    sf::Color body, bodyD, eyeCol;
    switch (type) {
        case EnemyType::STALKER: body={72,96,58};  bodyD={54,74,42};  eyeCol={220,60,50}; break;
        case EnemyType::CREEPER: body={70,170,70};  bodyD={48,120,48};  eyeCol={20,20,20};  break;
        case EnemyType::ARCHER:  body={200,196,180}; bodyD={150,146,132}; eyeCol={40,40,60}; break;
        case EnemyType::JUMPER:  body={60,40,90};    bodyD={42,28,66};   eyeCol={190,120,240}; break;
    }

    if (type == EnemyType::CREEPER) {
        // Ползун: компактное тело-столбик, 4 ножки, тёмные глаза-щёлочки
        rect(1, 0, 6, 6, body);            // голова/тело слитно
        rect(1, 5, 6, 6, body);
        rect(1, 10, 6, 1, bodyD);
        eyes(eyeCol);
        rect(3, 4, 2, 2, bodyD);           // «рот» пятном
        // ножки
        rect(1, 11, 2, 5, bodyD);
        rect(5, 11, 2, 5, bodyD);
        // тень
        if (onGround) {
            sf::CircleShape sh(width*0.5f); sh.setScale({1.f,0.25f});
            sh.setOrigin({width*0.5f,width*0.5f});
            sh.setPosition({x+width/2.f,y+height}); sh.setFillColor(sf::Color(0,0,0,80));
            window.draw(sh);
        }
        return;
    }

    // Общий гуманоид (Бродяга / Лучник / Прыгун) с разной палитрой
    rect(1, 0, 6, 5, body);
    rect(1, 0, 6, 1, bodyD);
    eyes(eyeCol);
    rect(2, 4, 4, 1, bodyD);
    rect(1, 5, 6, 6, bodyD);   // тело
    rect(0, 5, 1, 5, body);    // руки
    rect(7, 5, 1, 5, body);
    rect(1, 11, 2, 5, bodyD);  // ноги
    rect(5, 11, 2, 5, bodyD);

    // Лучник: «лук» — дуга сбоку
    if (type == EnemyType::ARCHER) {
        sf::RectangleShape bow({2.f, psy * 8.f});
        bow.setFillColor(sf::Color(110, 80, 40));
        bow.setPosition({x + (facing > 0 ? width - 2.f : 0.f), y + 4.f * psy});
        window.draw(bow);
    }

    if (onGround) {
        sf::CircleShape shadow(width * 0.5f);
        shadow.setScale({1.f, 0.25f});
        shadow.setOrigin({width * 0.5f, width * 0.5f});
        shadow.setPosition({x + width / 2.f, y + height});
        shadow.setFillColor(sf::Color(0, 0, 0, 80));
        window.draw(shadow);
    }
}