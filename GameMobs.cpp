#include "Game.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <random>

// Спавн и "добыча" мобов (курицы, коровы, овцы, враги).
// Вынесено из Game.cpp, чтобы тот не разрастался ещё больше.

int Game::findGroundSurface(int bx) const {
    for (int by = 0; by < WORLD_HEIGHT; by++) {
        Block b = world.getBlock(bx, by);
        if (!b.isSolid()) continue;
        // Листва и ствол — это дерево, а не земля; спавнить моба на кроне не годится
        if (b.isLeaf() || b.type == BlockType::WOOD) continue;
        return by;
    }
    return 0;
}

void Game::spawnChicken() {
    int bx = 5 + rand() % (WORLD_WIDTH - 10);
    int surfaceY = findGroundSurface(bx);
    float spawnPx = bx * (float)BLOCK_SIZE + 4.f;
    float spawnPy = (surfaceY - 1) * (float)BLOCK_SIZE;
    chickens.emplace_back(spawnPx, spawnPy);
}

void Game::killChicken(size_t index) {
    float cx = chickens[index].getX() + chickens[index].getWidth()  / 2.f;
    float cy = chickens[index].getY() + chickens[index].getHeight() / 2.f;

    // Пух и перья вместо обычных частиц блока
    static std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> angleDist(0.f, 6.2831853f);
    std::uniform_real_distribution<float> speedDist(50.f, 140.f);
    std::uniform_real_distribution<float> lifeDist(0.35f, 0.6f);
    sf::Color featherColors[2] = { sf::Color(250, 209, 60), sf::Color(255, 245, 220) };

    for (int i = 0; i < 8; i++) {
        Particle p;
        float angle = angleDist(rng);
        float speed = speedDist(rng);
        p.position    = {cx, cy};
        p.velocity    = {std::cos(angle) * speed, std::sin(angle) * speed - 80.f};
        p.color       = featherColors[i % 2];
        p.size        = 3.f + (float)(rand() % 3);
        p.maxLifetime = lifeDist(rng);
        p.lifetime    = p.maxLifetime;
        particles.push_back(p);
    }

    sounds.playChickenDeath(); // звук гибели курицы
    inventory.addItem(BlockType::RAW_CHICKEN, FOOD_PER_CHICKEN); // сырое мясо — есть сырым или приготовить в печи

    chickens.erase(chickens.begin() + index);
}

void Game::spawnCow() {
    int bx = 5 + rand() % (WORLD_WIDTH - 10);
    int surfaceY = findGroundSurface(bx);
    float spawnPx = bx * (float)BLOCK_SIZE;
    float spawnPy = (surfaceY - 2) * (float)BLOCK_SIZE; // корова выше курицы — она крупнее
    cows.emplace_back(spawnPx, spawnPy);
}

void Game::killCow(size_t index) {
    float cx = cows[index].getX() + cows[index].getWidth()  / 2.f;
    float cy = cows[index].getY() + cows[index].getHeight() / 2.f;

    // Клочки шерсти вместо перьев
    static std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> angleDist(0.f, 6.2831853f);
    std::uniform_real_distribution<float> speedDist(50.f, 140.f);
    std::uniform_real_distribution<float> lifeDist(0.35f, 0.6f);
    sf::Color furColors[2] = { sf::Color(150, 100, 70), sf::Color(240, 235, 225) };

    for (int i = 0; i < 8; i++) {
        Particle p;
        float angle = angleDist(rng);
        float speed = speedDist(rng);
        p.position    = {cx, cy};
        p.velocity    = {std::cos(angle) * speed, std::sin(angle) * speed - 80.f};
        p.color       = furColors[i % 2];
        p.size        = 3.f + (float)(rand() % 3);
        p.maxLifetime = lifeDist(rng);
        p.lifetime    = p.maxLifetime;
        particles.push_back(p);
    }

    sounds.playCowHurt(); // звук гибели коровы
    inventory.addItem(BlockType::RAW_BEEF, BEEF_PER_COW); // сырая говядина — есть сырой или приготовить в печи

    cows.erase(cows.begin() + index);
}

void Game::spawnSheep() {
    int bx = 5 + rand() % (WORLD_WIDTH - 10);
    int surfaceY = findGroundSurface(bx);
    float spawnPx = bx * (float)BLOCK_SIZE;
    float spawnPy = (surfaceY - 2) * (float)BLOCK_SIZE; // как у коровы — овца крупнее курицы, нужен запас
    sheep.emplace_back(spawnPx, spawnPy);
}

void Game::killSheep(size_t index) {
    float cx = sheep[index].getX() + sheep[index].getWidth()  / 2.f;
    float cy = sheep[index].getY() + sheep[index].getHeight() / 2.f;

    // Клочки шерсти разлетаются при добыче
    static std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> angleDist(0.f, 6.2831853f);
    std::uniform_real_distribution<float> speedDist(50.f, 140.f);
    std::uniform_real_distribution<float> lifeDist(0.35f, 0.6f);
    sf::Color woolColor(240, 240, 235);

    for (int i = 0; i < 8; i++) {
        Particle p;
        float angle = angleDist(rng);
        float speed = speedDist(rng);
        p.position    = {cx, cy};
        p.velocity    = {std::cos(angle) * speed, std::sin(angle) * speed - 80.f};
        p.color       = woolColor;
        p.size        = 3.f + (float)(rand() % 3);
        p.maxLifetime = lifeDist(rng);
        p.lifetime    = p.maxLifetime;
        particles.push_back(p);
    }

    sounds.playSheepSound(); // блеяние овцы
    inventory.addItem(BlockType::WOOL, WOOL_PER_SHEEP); // шерсть — на кровать

    sheep.erase(sheep.begin() + index);
}

void Game::spawnEnemy() {
    // Спавним врага на поверхности, но не слишком близко к игроку (не менее 10 блоков)
    int px = (int)(player.getX() / BLOCK_SIZE);
    int bx;
    int tries = 0;
    do {
        bx = 5 + rand() % (WORLD_WIDTH - 10);
        tries++;
    } while (std::abs(bx - px) < 10 && tries < 20);

    int surfaceY = findGroundSurface(bx);
    float spawnPx = bx * (float)BLOCK_SIZE + 3.f;
    float spawnPy = (surfaceY - 2) * (float)BLOCK_SIZE;

    // Равный шанс каждого типа моба — по 25%
    int roll = rand() % 4;
    EnemyType t;
    if (roll == 0)      t = EnemyType::STALKER;
    else if (roll == 1) t = EnemyType::CREEPER;
    else if (roll == 2) t = EnemyType::ARCHER;
    else                t = EnemyType::JUMPER;

    enemies.emplace_back(spawnPx, spawnPy, t);
}

void Game::killEnemy(size_t index) {
    float cx = enemies[index].getX() + enemies[index].getWidth()  / 2.f;
    float cy = enemies[index].getY() + enemies[index].getHeight() / 2.f;

    static std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> angleDist(0.f, 6.2831853f);
    std::uniform_real_distribution<float> speedDist(60.f, 160.f);
    std::uniform_real_distribution<float> lifeDist(0.35f, 0.6f);
    sf::Color colors[2] = { sf::Color(72, 96, 58), sf::Color(54, 74, 42) };

    for (int i = 0; i < 10; i++) {
        Particle p;
        float angle = angleDist(rng);
        float speed = speedDist(rng);
        p.position    = {cx, cy};
        p.velocity    = {std::cos(angle) * speed, std::sin(angle) * speed - 60.f};
        p.color       = colors[i % 2];
        p.size        = 3.f + (float)(rand() % 3);
        p.maxLifetime = lifeDist(rng);
        p.lifetime    = p.maxLifetime;
        particles.push_back(p);
    }

    // Звук смерти лучника — слышен в радиусе 5 блоков
    if (enemies[index].getType() == EnemyType::ARCHER) {
        float ddx = enemies[index].getX() - player.getX();
        float ddy = enemies[index].getY() - player.getY();
        float d = std::sqrt(ddx*ddx + ddy*ddy) / BLOCK_SIZE;
        sounds.playAt("enemy_death", d, 5.f);
    } else {
        sounds.playDig(BlockType::STONE);
    }
    enemies.erase(enemies.begin() + index);
}
