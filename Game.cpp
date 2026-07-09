#include "Game.hpp"
#include <algorithm>
#include <iostream>
#include <cmath>
#include <cstdlib>
#include <random>

Game::Game()
    : window(sf::VideoMode({800, 600}), "Minecraft 2D")
    , camera(sf::FloatRect({0.f, 0.f}, {800.f, 600.f}))
    , player((WORLD_WIDTH / 2) * BLOCK_SIZE, (WORLD_HEIGHT / 2 - 5) * BLOCK_SIZE)
    , inventoryOpen(false)
    , animTime(0.f)
    , spawnX((WORLD_WIDTH / 2) * BLOCK_SIZE)
    , spawnY((WORLD_HEIGHT / 2 - 5) * BLOCK_SIZE)
    , shakeTime(0.f)
    , shakeMagnitude(0.f)
    , dayTime(DAY_LENGTH * 0.25f) // старт утром, а не в полночь
{
    window.setFramerateLimit(60);
    cursor.setSize({(float)BLOCK_SIZE, (float)BLOCK_SIZE});
    cursor.setFillColor(sf::Color(255, 255, 255, 50));
    cursor.setOutlineColor(sf::Color(0, 0, 0, 180));
    cursor.setOutlineThickness(2.f);

    // Ставим игрока на реальную поверхность в колонке спавна (а не на фиксированную высоту,
    // иначе можно оказаться внутри камня/пещеры). Ищем верхний твёрдый блок земли,
    // пропуская листву и стволы деревьев (иначе встанем на крону).
    {
        int scx = WORLD_WIDTH / 2;
        int surfaceRow = 0;
        for (int by = 0; by < WORLD_HEIGHT; by++) {
            const Block& b = world.getBlock(scx, by);
            if (b.type == BlockType::LEAVES || b.type == BlockType::WOOD) continue;
            if (b.isSolid()) { surfaceRow = by; break; }
        }
        spawnX = scx * (float)BLOCK_SIZE;
        spawnY = (surfaceRow - 3) * (float)BLOCK_SIZE; // на 3 блока выше поверхности (высота игрока + запас)
        player.respawn(spawnX, spawnY);
    }

    // Позиции звёзд — фиксированные точки на фоне неба (не привязаны к миру)
    for (int i = 0; i < 60; i++) {
        starPositions.push_back({
            (float)(rand() % 800),
            (float)(rand() % 400) // звёзды только в верхней части неба
        });
    }

    // Облака — несколько штук в верхней части неба, разной высоты
    for (int i = 0; i < 6; i++) {
        cloudPositions.push_back({
            (float)(rand() % 900) - 50.f,
            40.f + (float)(rand() % 180) // облака в верхней трети неба
        });
    }

    // Дождевые капли — заранее раскиданы по экрану, со своей скоростью
    for (int i = 0; i < 350; i++) {
        rainDrops.push_back({
            (float)(rand() % 800),
            (float)(rand() % 600),
            550.f + (float)(rand() % 350)
        });
    }

    if (!blockTexture.loadFromFile("textures/blocks.png")) {
        std::cerr << "Failed to load: textures/blocks.png" << std::endl;
    }
    blockTexture.setSmooth(false); // чёткие пиксели без размытия
    inventory.setBlockTexture(&blockTexture); // иконки инвентаря рисуются реальной текстурой

    // Спавним несколько цыплят на поверхности в случайных местах мира
    for (int i = 0; i < MAX_CHICKENS; i++)
        spawnChicken();
    chickenRespawnTimer = 15.f + rand() % 15;
}

void Game::spawnChicken() {
    int bx = 5 + rand() % (WORLD_WIDTH - 10);
    int surfaceY = 0;
    for (int by = 0; by < WORLD_HEIGHT; by++) {
        if (world.getBlock(bx, by).isSolid()) {
            surfaceY = by;
            break;
        }
    }
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

void Game::spawnEnemy() {
    // Спавним врага на поверхности, но не слишком близко к игроку (не менее 10 блоков)
    int px = (int)(player.getX() / BLOCK_SIZE);
    int bx;
    int tries = 0;
    do {
        bx = 5 + rand() % (WORLD_WIDTH - 10);
        tries++;
    } while (std::abs(bx - px) < 10 && tries < 20);

    int surfaceY = 0;
    for (int by = 0; by < WORLD_HEIGHT; by++) {
        if (world.getBlock(bx, by).isSolid()) { surfaceY = by; break; }
    }
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

void Game::run() {
    sf::Clock clock;
    sf::Clock stepClock;
    while (window.isOpen()) {
        float deltaTime = clock.restart().asSeconds();
        handleEvents();
        update(deltaTime);

        if (player.isMoving() && player.isOnGround()) {
            if (stepClock.getElapsedTime().asSeconds() > 0.4f) {
                int bx = (int)((player.getX() + player.getWidth() / 2.f) / BLOCK_SIZE);
                int by = (int)((player.getY() + player.getHeight() + 2) / BLOCK_SIZE);
                BlockType underType = BlockType::GRASS;
                if (world.inBounds(bx, by) && world.getBlock(bx, by).isSolid())
                    underType = world.getBlock(bx, by).type;
                sounds.playStep(underType);
                stepClock.restart();
            }
        } else if (!player.isMoving()) {
            stepClock.restart();
        }

        render();
    }
}

void Game::handleEvents() {
    while (const std::optional event = window.pollEvent()) {
        if (event->is<sf::Event::Closed>())
            window.close();

        if (const auto* kp = event->getIf<sf::Event::KeyPressed>()) {
            if (kp->code == sf::Keyboard::Key::E) {
                if (workbenchOpen) {
                    // E закрывает верстак — всё из сетки возвращается в инвентарь
                    inventory.closeWorkbenchScreen();
                    workbenchOpen = false;
                } else if (furnaceOpen) {
                    inventory.closeFurnaceScreen();
                    furnaceOpen = false;
                } else if (chestOpen) {
                    // E закрывает открытый сундук, как и крестик
                    chestOpen = false;
                } else {
                    inventoryOpen = !inventoryOpen;
                }
                player.handleKeyReleased(sf::Keyboard::Key::A);
                player.handleKeyReleased(sf::Keyboard::Key::D);
            }
            if (kp->code == sf::Keyboard::Key::Left)  inventory.prevSlot();
            if (kp->code == sf::Keyboard::Key::Right) inventory.nextSlot();
            if (kp->code == sf::Keyboard::Key::Num1) inventory.selectSlot(0);
            if (kp->code == sf::Keyboard::Key::Num2) inventory.selectSlot(1);
            if (kp->code == sf::Keyboard::Key::Num3) inventory.selectSlot(2);
            if (kp->code == sf::Keyboard::Key::Num4) inventory.selectSlot(3);
            if (kp->code == sf::Keyboard::Key::Num5) inventory.selectSlot(4);

            if (!inventoryOpen && !chestOpen && !workbenchOpen && !furnaceOpen) {
                if (kp->code == sf::Keyboard::Key::A ||
                    kp->code == sf::Keyboard::Key::D ||
                    kp->code == sf::Keyboard::Key::W ||
                    kp->code == sf::Keyboard::Key::Space)
                    player.handleKeyPressed(kp->code);
            }
        }

        if (const auto* kr = event->getIf<sf::Event::KeyReleased>())
            player.handleKeyReleased(kr->code);

        if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>()) {
            if (!inventoryOpen && !chestOpen && !workbenchOpen && !furnaceOpen) {
                handleBlockInteraction(mb->button, sf::Vector2i(mb->position.x, mb->position.y));
            } else {
                // Инвентарь рисуется в фиксированной системе 800x600, а клик приходит
                // в реальных пикселях окна. В полноэкранном режиме окно больше 800x600,
                // поэтому пиксель клика нужно перевести в координаты UI-вида.
                sf::View uiView(sf::FloatRect({0.f, 0.f}, {800.f, 600.f}));
                sf::Vector2f uiPos = window.mapPixelToCoords({mb->position.x, mb->position.y}, uiView);
                sf::Vector2i mpos((int)uiPos.x, (int)uiPos.y);

                if (workbenchOpen) {
                    // У сетки верстака работают и ЛКМ (взять/положить/слить/поменять целую
                    // стопку), и ПКМ (взять половину стопки или положить один предмет).
                    if (mb->button == sf::Mouse::Button::Left && inventory.handleWorkbenchCloseClick(mpos)) {
                        inventory.closeWorkbenchScreen();
                        workbenchOpen = false;
                    } else if (mb->button == sf::Mouse::Button::Left || mb->button == sf::Mouse::Button::Right) {
                        if (inventory.handleWorkbenchClick(mpos, mb->button))
                            sounds.playDig(BlockType::WOOD); // звук перекладывания/крафта
                    }
                } else if (furnaceOpen) {
                    // То же самое (ЛКМ/ПКМ), только слоты — сырьё/топливо/результат этой печи
                    if (mb->button == sf::Mouse::Button::Left && inventory.handleFurnaceCloseClick(mpos)) {
                        inventory.closeFurnaceScreen();
                        furnaceOpen = false;
                    } else if (mb->button == sf::Mouse::Button::Left || mb->button == sf::Mouse::Button::Right) {
                        auto it = furnaceStorage.find(openFurnacePos);
                        if (it == furnaceStorage.end()) {
                            furnaceOpen = false; // печь куда-то делась (не должно случаться)
                        } else if (inventory.handleFurnaceClick(mpos, it->second.slots, mb->button)) {
                            sounds.playDig(BlockType::WOOD);
                        }
                    }
                } else if (mb->button != sf::Mouse::Button::Left) {
                    // остальные окна (инвентарь, сундук) реагируют только на ЛКМ
                } else if (chestOpen) {
                    auto it = chestStorage.find(openChestPos);
                    if (it == chestStorage.end()) {
                        chestOpen = false; // сундук куда-то делся (не должно случаться) — просто закрываем
                    } else if (inventory.handleChestCloseClick(mpos)) {
                        chestOpen = false;
                    } else if (inventory.handleChestClick(mpos, it->second)) {
                        sounds.playDig(BlockType::WOOD); // звук перекладывания вещей
                    }
                } else if (inventory.handleCloseClick(mpos)) {
                    inventoryOpen = false; // клик по крестику закрывает инвентарь
                } else if (inventory.handleCraftClick(mpos)) {
                    sounds.playDig(BlockType::STONE); // временный звук "крафта"
                }
            }
        }
    }
}

bool Game::isWithinReach(int bx, int by) const {
    // Расстояние считаем от центра игрока до центра блока, в блоках
    float pcx = (player.getX() + player.getWidth()  / 2.f) / BLOCK_SIZE;
    float pcy = (player.getY() + player.getHeight() / 2.f) / BLOCK_SIZE;
    float dx = (bx + 0.5f) - pcx;
    float dy = (by + 0.5f) - pcy;
    return std::sqrt(dx * dx + dy * dy) <= MAX_REACH_BLOCKS;
}

void Game::spawnBreakParticles(int bx, int by, const Block& block) {
    static std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> angleDist(0.f, 6.2831853f); // 0..2*pi
    std::uniform_real_distribution<float> speedDist(60.f, 160.f);
    std::uniform_real_distribution<float> lifeDist(0.35f, 0.65f);
    std::uniform_real_distribution<float> sizeDist(3.f, 6.f);

    float cx = bx * (float)BLOCK_SIZE + BLOCK_SIZE / 2.f;
    float cy = by * (float)BLOCK_SIZE + BLOCK_SIZE / 2.f;

    // Берём три оттенка блока, чтобы обломки не были однотонными
    sf::Color colors[3] = { block.getColor(), block.getTopColor(), block.getDarkColor() };

    for (int i = 0; i < 8; i++) {
        Particle p;
        float angle = angleDist(rng);
        float speed = speedDist(rng);
        p.position    = {cx, cy};
        p.velocity    = {std::cos(angle) * speed, std::sin(angle) * speed - 60.f}; // лёгкий подброс вверх
        p.color       = colors[i % 3];
        p.size        = sizeDist(rng);
        p.maxLifetime = lifeDist(rng);
        p.lifetime    = p.maxLifetime;
        particles.push_back(p);
    }
}

void Game::updateParticles(float deltaTime) {
    const float GRAVITY = 700.f;
    for (auto& p : particles) {
        p.velocity.y += GRAVITY * deltaTime;
        p.position   += p.velocity * deltaTime;
        p.lifetime   -= deltaTime;
    }
    particles.erase(
        std::remove_if(particles.begin(), particles.end(),
            [](const Particle& p) { return !p.isAlive(); }),
        particles.end());
}

void Game::drawParticles() {
    sf::RectangleShape shape;
    for (const auto& p : particles) {
        shape.setSize({p.size, p.size});
        shape.setOrigin({p.size / 2.f, p.size / 2.f});
        shape.setPosition(p.position);

        float t = p.lifetime / p.maxLifetime; // 1 -> 0, для затухания
        sf::Color c = p.color;
        c.a = (std::uint8_t)(255.f * t);
        shape.setFillColor(c);

        window.draw(shape);
    }
}

void Game::armTnt(int bx, int by, float fuse) {
    // Не поджигаем повторно уже горящий блок
    for (const auto& t : activeTNT) {
        if (t.bx == bx && t.by == by) return;
    }
    if (!world.inBounds(bx, by) || world.getBlock(bx, by).type != BlockType::TNT) return;

    activeTNT.push_back({bx, by, fuse, fuse});
    sounds.playFuse(); // шипение подожжённого фитиля

    // Немного искр в момент поджига — для обратной связи
    float cx = bx * (float)BLOCK_SIZE + BLOCK_SIZE / 2.f;
    float cy = by * (float)BLOCK_SIZE + 4.f;
    for (int i = 0; i < 5; i++) {
        Particle p;
        float angle = -1.2f + (float)(rand() % 100) / 100.f * 2.4f; // веером вверх
        float speed = 40.f + rand() % 60;
        p.position    = {cx, cy};
        p.velocity    = {std::cos(angle) * speed, std::sin(angle) * speed - 40.f};
        p.color       = sf::Color(255, 190, 60);
        p.size        = 2.f + (float)(rand() % 3);
        p.maxLifetime = 0.25f + (float)(rand() % 20) / 100.f;
        p.lifetime    = p.maxLifetime;
        particles.push_back(p);
    }
}

void Game::updateFurnaces(float deltaTime) {
    for (auto& kv : furnaceStorage) {
        FurnaceState& f = kv.second;
        InventorySlot& in   = f.slots[0];
        InventorySlot& fuel = f.slots[1];
        InventorySlot& out  = f.slots[2];

        BlockType smeltResult;
        float cookTime;
        bool hasRecipe = in.count > 0 && getSmeltResult(in.type, smeltResult, cookTime);
        bool outputFits = (out.type == BlockType::AIR || out.count <= 0) ||
                           (out.type == smeltResult && out.count < 64);
        bool canSmelt = hasRecipe && outputFits;

        if (canSmelt) {
            if (f.burnTimeLeft <= 0.f) {
                float fuelSeconds = getFuelBurnSeconds(fuel.type);
                if (fuel.count > 0 && fuelSeconds > 0.f) {
                    fuel.count--;
                    if (fuel.count <= 0) { fuel.count = 0; fuel.type = BlockType::AIR; }
                    f.burnTimeLeft = f.burnTimeMax = fuelSeconds;
                }
            }

            if (f.burnTimeLeft > 0.f) {
                f.burnTimeLeft = std::max(0.f, f.burnTimeLeft - deltaTime);
                f.cookProgress += deltaTime;

                if (f.cookProgress >= cookTime) {
                    f.cookProgress -= cookTime;
                    in.count--;
                    if (in.count <= 0) { in.count = 0; in.type = BlockType::AIR; }
                    if (out.type == BlockType::AIR || out.count <= 0) {
                        out.type = smeltResult;
                        out.count = 0;
                    }
                    out.count = std::min(64, out.count + 1);
                }
            }
        } else {
            // Готовить нечего или некуда — прогресс сбрасывается (топливо не тратится впустую)
            f.cookProgress = 0.f;
        }
    }
}

void Game::updateExplosives(float deltaTime) {
    for (auto& t : activeTNT)
        t.fuseTime -= deltaTime;

    // Взрываем все, у кого фитиль догорел, отдельным проходом
    // (explodeAt может добавить в activeTNT новые заряды через цепную реакцию)
    for (size_t i = 0; i < activeTNT.size(); i++) {
        if (activeTNT[i].fuseTime <= 0.f) {
            int bx = activeTNT[i].bx;
            int by = activeTNT[i].by;
            activeTNT.erase(activeTNT.begin() + i);
            i--;
            explodeAt(bx, by);
        }
    }
}

void Game::explodeAt(int bx, int by) {
    const float BLAST_RADIUS = 4.2f; // в блоках

    // Убираем сам блок TNT
    world.setBlock(bx, by, BlockType::AIR);

    float cx = bx * (float)BLOCK_SIZE + BLOCK_SIZE / 2.f;
    float cy = by * (float)BLOCK_SIZE + BLOCK_SIZE / 2.f;

    int r = (int)std::ceil(BLAST_RADIUS);
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            if (dx * dx + dy * dy > BLAST_RADIUS * BLAST_RADIUS) continue;

            int wx = bx + dx, wy = by + dy;
            if (!world.inBounds(wx, wy)) continue;

            Block block = world.getBlock(wx, wy);
            if (!block.isSolid()) continue;

            if (block.type == BlockType::TNT) {
                // Цепная реакция — соседний TNT загорается с небольшой случайной задержкой
                armTnt(wx, wy, 0.15f + (float)(rand() % 25) / 100.f);
                continue; // не уничтожаем сразу — пусть тоже взорвётся
            }

            spawnBreakParticles(wx, wy, block);
            world.setBlock(wx, wy, BlockType::AIR);
        }
    }

    // Яркая вспышка частиц в эпицентре
    static std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> angleDist(0.f, 6.2831853f);
    std::uniform_real_distribution<float> speedDist(120.f, 320.f);
    std::uniform_real_distribution<float> lifeDist(0.3f, 0.6f);
    sf::Color flashColors[3] = { sf::Color(255,200,60), sf::Color(255,120,30), sf::Color(80,80,80) };
    for (int i = 0; i < 24; i++) {
        Particle p;
        float angle = angleDist(rng);
        float speed = speedDist(rng);
        p.position    = {cx, cy};
        p.velocity    = {std::cos(angle) * speed, std::sin(angle) * speed};
        p.color       = flashColors[i % 3];
        p.size        = 4.f + (float)(rand() % 5);
        p.maxLifetime = lifeDist(rng);
        p.lifetime    = p.maxLifetime;
        particles.push_back(p);
    }

    // Урон и отдача игроку — тем сильнее, чем ближе к эпицентру
    const float DAMAGE_RADIUS = BLAST_RADIUS + 2.0f; // чуть больше радиуса разрушения блоков
    float distToPlayer = std::sqrt(
        (cx - player.getX()) * (cx - player.getX()) +
        (cy - player.getY()) * (cy - player.getY())) / BLOCK_SIZE;

    if (distToPlayer < DAMAGE_RADIUS) {
        float dmg = (DAMAGE_RADIUS - distToPlayer) * 1.3f;
        hud.damage((int)std::ceil(dmg));

        float dx = (player.getX() + player.getWidth()  / 2.f) - cx;
        float dy = (player.getY() + player.getHeight() / 2.f) - cy;
        float len = std::max(1.f, std::sqrt(dx * dx + dy * dy));
        float t = std::max(0.f, 1.f - distToPlayer / DAMAGE_RADIUS);
        float force = 300.f + 500.f * t;
        player.applyKnockback((dx / len) * force, -250.f - 300.f * t);
    }

    // Отдача цыплятам в радиусе (без урона — у них пока нет здоровья)
    for (auto& chicken : chickens) {
        float dxc = chicken.getX() - cx;
        float dyc = chicken.getY() - cy;
        float distC = std::sqrt(dxc * dxc + dyc * dyc) / BLOCK_SIZE;
        if (distC < DAMAGE_RADIUS) {
            float lenC = std::max(1.f, std::sqrt(dxc * dxc + dyc * dyc));
            float tC = std::max(0.f, 1.f - distC / DAMAGE_RADIUS);
            float forceC = 250.f + 400.f * tC;
            chicken.applyKnockback((dxc / lenC) * forceC, -220.f - 250.f * tC);
        }
    }

    // Урон и отдача врагам в радиусе взрыва
    for (auto& e : enemies) {
        float dxe = e.getX() - cx;
        float dye = e.getY() - cy;
        float distE = std::sqrt(dxe * dxe + dye * dye) / BLOCK_SIZE;
        if (distE < DAMAGE_RADIUS) {
            float lenE = std::max(1.f, std::sqrt(dxe * dxe + dye * dye));
            float tE = std::max(0.f, 1.f - distE / DAMAGE_RADIUS);
            e.takeDamage((int)std::ceil((DAMAGE_RADIUS - distE) * 2.f)); // взрыв ранит врагов
            float forceE = 300.f + 450.f * tE;
            e.applyKnockback((dxe / lenE) * forceE, -240.f - 260.f * tE);
        }
    }

    // Тряска камеры, сильнее — если взрыв рядом с игроком
    float closeness = std::max(0.f, 1.f - distToPlayer / 12.f);
    if (closeness > 0.f) {
        shakeMagnitude = std::max(shakeMagnitude, 10.f * closeness);
        shakeTime = std::max(shakeTime, 0.35f);
    }

    sounds.playDig(BlockType::STONE);
    sounds.playDig(BlockType::STONE);
}

void Game::handleBlockInteraction(sf::Mouse::Button button, sf::Vector2i mousePixelPos) {
    sf::Vector2f worldPos = window.mapPixelToCoords(mousePixelPos, camera);

    if (button == sf::Mouse::Button::Left) {
        // Сначала проверяем удар по врагу — левый клик наносит урон и отбрасывает его
        for (size_t i = 0; i < enemies.size(); i++) {
            sf::FloatRect eb(
                {enemies[i].getX(), enemies[i].getY()},
                {enemies[i].getWidth(), enemies[i].getHeight()});
            if (eb.contains(worldPos)) {
                int ebx = (int)(enemies[i].getX() / BLOCK_SIZE);
                int eby = (int)(enemies[i].getY() / BLOCK_SIZE);
                if (isWithinReach(ebx, eby)) {
                    enemies[i].takeDamage(3);
                    // отбрасываем врага от игрока
                    float dir = (enemies[i].getX() > player.getX()) ? 1.f : -1.f;
                    enemies[i].applyKnockback(dir * 260.f, -180.f);
                    sounds.playDig(BlockType::WOOD);
                }
                return;
            }
        }

        // Затем — не кликнули ли по цыплёнку (добыча)
        for (size_t i = 0; i < chickens.size(); i++) {
            sf::FloatRect bounds(
                {chickens[i].getX(), chickens[i].getY()},
                {chickens[i].getWidth(), chickens[i].getHeight()});
            if (bounds.contains(worldPos)) {
                int cbx = (int)(chickens[i].getX() / BLOCK_SIZE);
                int cby = (int)(chickens[i].getY() / BLOCK_SIZE);
                if (isWithinReach(cbx, cby)) {
                    killChicken(i);
                }
                return;
            }
        }
    }

    int bx = (int)(worldPos.x / BLOCK_SIZE);
    int by = (int)(worldPos.y / BLOCK_SIZE);
    if (!world.inBounds(bx, by)) return;
    if (!isWithinReach(bx, by)) return; // слишком далеко — игнорируем клик

    if (button == sf::Mouse::Button::Left) {
        Block block = world.getBlock(bx, by);
        bool breakable = block.isSolid() || block.type == BlockType::TORCH;
        if (breakable) {
            if (block.type == BlockType::TNT) {
                armTnt(bx, by, 1.6f); // поджигаем, а не ломаем сразу
            } else {
                sounds.playDig(block.type);
                spawnBreakParticles(bx, by, block);
                inventory.addItem(block.type, 1); // добытый блок отправляется в инвентарь
                if (block.type == BlockType::CHEST) {
                    // Ломаем сундук — всё, что в нём лежало, высыпаем игроку в инвентарь
                    auto it = chestStorage.find({bx, by});
                    if (it != chestStorage.end()) {
                        for (auto& slot : it->second)
                            if (slot.type != BlockType::AIR && slot.count > 0)
                                inventory.addItem(slot.type, slot.count);
                        chestStorage.erase(it);
                    }
                }
                if (block.type == BlockType::FURNACE) {
                    // Ломаем печь — сырьё, топливо и уже готовый результат высыпаются игроку
                    auto it = furnaceStorage.find({bx, by});
                    if (it != furnaceStorage.end()) {
                        for (auto& slot : it->second.slots)
                            if (slot.type != BlockType::AIR && slot.count > 0)
                                inventory.addItem(slot.type, slot.count);
                        furnaceStorage.erase(it);
                    }
                }
                world.setBlock(bx, by, BlockType::AIR);
            }
        }
    } else if (button == sf::Mouse::Button::Right) {
        Block clicked = world.getBlock(bx, by);
        if (clicked.type == BlockType::WORKBENCH) {
            // Открываем настоящую сетку 3x3 — как у верстака в оригинале
            workbenchOpen = true;
            return;
        }
        if (clicked.type == BlockType::CHEST) {
            std::pair<int, int> key(bx, by);
            if (chestStorage.find(key) == chestStorage.end())
                chestStorage[key] = std::vector<InventorySlot>(CHEST_SLOT_COUNT, InventorySlot(BlockType::AIR, 0));
            chestOpen = true;
            openChestPos = key;
            return;
        }
        if (clicked.type == BlockType::FURNACE) {
            std::pair<int, int> key(bx, by);
            furnaceStorage[key]; // создаёт пустое состояние печи при первом открытии, если его ещё нет
            furnaceOpen = true;
            openFurnacePos = key;
            return;
        }

        // Открытие контейнеров — в приоритете. Если клик пришёлся не по ним, но в руке
        // еда — едим её вместо попытки "поставить" как блок (есть можно, глядя куда угодно).
        if (inventory.hasSelected()) {
            BlockType selType = inventory.getSelectedType();
            Block selBlock(selType);
            if (selBlock.isFood()) {
                hud.setFood(hud.getFood() + selBlock.getFoodValue());
                inventory.consumeSelected();
                sounds.playDig(BlockType::WOOD); // временный звук еды
                return;
            }
            if (selBlock.isItem()) {
                // слитки и прочие "предметы" нельзя поставить как блок в мир
                return;
            }
        }

        if (!world.getBlock(bx, by).isSolid() && inventory.hasSelected()) {
            BlockType t = inventory.getSelectedType();
            if (t == BlockType::TORCH)      sounds.playFuse();   // потрескивание факела
            else if (t == BlockType::WATER) sounds.playSplash(); // плеск при установке воды
            else                            sounds.playDig(t);
            world.setBlock(bx, by, t);
            inventory.consumeSelected();
        }
    }
}

void Game::update(float deltaTime) {
    if (!inventoryOpen && !chestOpen && !workbenchOpen && !furnaceOpen) {
        player.update(deltaTime, world);
        if (player.didJustEnterWater()) sounds.playSplash(); // плеск при входе в воду
    }

    updateParticles(deltaTime);
    world.updateFallingBlocks(deltaTime);
    updateExplosives(deltaTime);
    updateFurnaces(deltaTime);
    animTime += deltaTime;

    dayTime += deltaTime;
    if (dayTime >= DAY_LENGTH) dayTime -= DAY_LENGTH;

    updateWeather(deltaTime);

    for (auto& chicken : chickens)
        chicken.update(deltaTime, world);

    chickenRespawnTimer -= deltaTime;
    if (chickenRespawnTimer <= 0.f) {
        if ((int)chickens.size() < MAX_CHICKENS)
            spawnChicken();
        chickenRespawnTimer = 15.f + rand() % 15;
    }

    // Враги: преследуют игрока, бьют вблизи
    if (!inventoryOpen && !chestOpen && !workbenchOpen && !furnaceOpen) {
        float pcx = player.getX() + player.getWidth()  / 2.f;
        float pcy = player.getY() + player.getHeight() / 2.f;
        for (auto& e : enemies) {
            bool hit = e.update(deltaTime, world, pcx, pcy);
            if (hit) hud.damage(2); // враг ударил игрока
            if (e.shouldGrowl(pcx, pcy, deltaTime)) sounds.playZombie(); // рычит при приближении

            // Ползун зажёг фитиль — тот же звук, что у TNT
            if (e.didLightFuse()) sounds.playFuse();

            // Лучник: звук шагов, слышен в радиусе 5 блоков
            if (e.getType() == EnemyType::ARCHER && e.didStep()) {
                float ddx = (e.getX() - player.getX());
                float ddy = (e.getY() - player.getY());
                float d = std::sqrt(ddx*ddx + ddy*ddy) / BLOCK_SIZE;
                sounds.playAt("enemy_step", d, 5.f);
            }

            // Лучник выстрелил — создаём снаряд-стрелу, летящую в игрока
            if (e.wantsToShoot(pcx, pcy)) {
                float ex = e.getX() + e.getWidth() / 2.f;
                float ey = e.getY() + e.getHeight() * 0.4f;
                float ddx = pcx - ex, ddy = pcy - ey;
                float len = std::max(1.f, std::sqrt(ddx*ddx + ddy*ddy));
                float sp = 420.f;
                arrows.push_back({ex, ey, ddx/len*sp, ddy/len*sp, 3.f});
            }
        }

        // Ползуны, готовые взорваться — устраиваем взрыв в их точке и убираем
        for (size_t i = enemies.size(); i-- > 0; ) {
            if (enemies[i].isExploding()) {
                int cbx = (int)((enemies[i].getX() + enemies[i].getWidth()/2.f) / BLOCK_SIZE);
                int cby = (int)((enemies[i].getY() + enemies[i].getHeight()/2.f) / BLOCK_SIZE);
                enemies.erase(enemies.begin() + i);
                explodeAt(cbx, cby); // переиспользуем систему взрыва TNT
            }
        }

        // Убираем мёртвых врагов (с эффектом гибели)
        for (size_t i = enemies.size(); i-- > 0; ) {
            if (enemies[i].isDead()) killEnemy(i);
        }

        // Обновляем стрелы лучников
        for (size_t i = arrows.size(); i-- > 0; ) {
            Arrow& a = arrows[i];
            a.x += a.vx * deltaTime;
            a.y += a.vy * deltaTime;
            a.life -= deltaTime;
            // попадание в игрока
            sf::FloatRect pb({player.getX(), player.getY()}, {player.getWidth(), player.getHeight()});
            bool hitPlayer = pb.contains({a.x, a.y});
            int abx = (int)(a.x / BLOCK_SIZE), aby = (int)(a.y / BLOCK_SIZE);
            bool hitBlock = world.inBounds(abx, aby) && world.getBlock(abx, aby).isSolid();
            if (hitPlayer) { hud.damage(2); arrows.erase(arrows.begin() + i); }
            else if (hitBlock || a.life <= 0.f) { arrows.erase(arrows.begin() + i); }
        }
    }

    // Зомби появляются только ночью; днём — сгорают на солнце и гибнут
    float brightness = getBrightness();
    bool isNight = brightness < 0.35f;

    if (isNight) {
        enemySpawnTimer -= deltaTime;
        if (enemySpawnTimer <= 0.f) {
            if ((int)enemies.size() < 6)   // не более 6 мобов одновременно
                spawnEnemy();
            enemySpawnTimer = 2.5f + (rand() % 20) / 10.f; // спавн чаще: раз в 2.5-4.5 сек
        }
    } else {
        // Днём зомби постепенно "горят" — раз в ~0.5 сек теряют здоровье
        static float burnTimer = 0.f;
        burnTimer += deltaTime;
        if (burnTimer >= 0.5f) {
            burnTimer = 0.f;
            for (auto& e : enemies) e.takeDamage(1); // урон от солнца
        }
    }

    if (!inventoryOpen && !chestOpen && !workbenchOpen && !furnaceOpen) {
        hud.update(deltaTime);

        float fallDamage = player.takeFallDamage();
        if (fallDamage > 0.f) {
            hud.damage((int)std::ceil(fallDamage));
        }

        if (hud.isDead()) {
            player.respawn(spawnX, spawnY);
            hud.setHealth(10);
            hud.setFood(10);
        }
    }

    float camX = player.getX() + player.getWidth()  / 2.f;
    float camY = player.getY() + player.getHeight() / 2.f;
    float halfW = 400.f, halfH = 300.f;
    camX = std::max(halfW, std::min(camX, WORLD_WIDTH  * BLOCK_SIZE - halfW));
    camY = std::max(halfH, std::min(camY, WORLD_HEIGHT * BLOCK_SIZE - halfH));

    if (shakeTime > 0.f) {
        shakeTime = std::max(0.f, shakeTime - deltaTime);
        float t = shakeTime; // затухание тряски к нулю
        camX += ((float)(rand() % 200) / 100.f - 1.f) * shakeMagnitude * t;
        camY += ((float)(rand() % 200) / 100.f - 1.f) * shakeMagnitude * t;
    }

    camera.setCenter({camX, camY});

    sf::Vector2i mousePixel = sf::Mouse::getPosition(window);
    sf::Vector2f worldPos   = window.mapPixelToCoords(mousePixel, camera);
    int bx = (int)(worldPos.x / BLOCK_SIZE);
    int by = (int)(worldPos.y / BLOCK_SIZE);
    cursor.setPosition({(float)(bx * BLOCK_SIZE), (float)(by * BLOCK_SIZE)});

    // Курсор краснеет, если блок под мышкой дальше, чем можно дотянуться
    if (isWithinReach(bx, by)) {
        cursor.setOutlineColor(sf::Color(0, 0, 0, 180));
    } else {
        cursor.setOutlineColor(sf::Color(220, 40, 40, 200));
    }
}

float Game::getBrightness() const {
    // phase: 0 в полночь, 0.5 в полдень, 1 снова полночь
    float phase = dayTime / DAY_LENGTH;
    // 0 = глубокая ночь, 1 = яркий полдень; плавный переход по косинусу
    return (1.f - std::cos(phase * 2.f * 3.14159265f)) * 0.5f;
}

void Game::updateWeather(float deltaTime) {
    // Облака медленно плывут вправо, при уходе за край возвращаются слева
    for (auto& c : cloudPositions) {
        c.x += 8.f * deltaTime; // скорость ветра
        if (c.x > 850.f) {
            c.x = -100.f;
            c.y = 40.f + (float)(rand() % 180);
        }
    }

    // Смена погоды примерно раз в 30-60 секунд
    weatherTimer -= deltaTime;
    if (weatherTimer <= 0.f) {
        raining = (rand() % 100) < 35; // ~35% шанс дождя
        weatherTimer = 30.f + rand() % 30;
        if (raining) sounds.startRain();
        else         sounds.stopRain();
    }

    // Капли дождя падают вниз (и чуть вбок), при уходе вниз — заново сверху
    if (raining) {
        for (auto& d : rainDrops) {
            d.y += d.speed * deltaTime;
            d.x += 60.f * deltaTime; // наклон дождя ветром
            if (d.y > 600.f) { d.y = -10.f; d.x = (float)(rand() % 900) - 50.f; }
            if (d.x > 820.f) d.x -= 870.f;
        }
    }
}

void Game::drawClouds() {
    sf::View uiView(sf::FloatRect({0.f, 0.f}, {800.f, 600.f}));
    window.setView(uiView);

    float brightness = getBrightness();
    std::uint8_t base = (std::uint8_t)(215 + 40 * brightness);
    std::uint8_t alpha = raining ? 245 : 205;
    sf::Color body(base, base, (std::uint8_t)std::min(255, base + 8), alpha);
    sf::Color shade((std::uint8_t)(base - 45), (std::uint8_t)(base - 45), (std::uint8_t)(base - 32), alpha);

    // Пиксельное облако: карта 10x5 «пикселей». 1 = тело, 2 = тень (нижняя кромка), 0 = пусто.
    // Края намеренно рваные/ступенчатые для пиксель-арт вида.
    const int CW = 10, CH = 5;
    const float U = 11.f; // размер одного «пикселя» облака
    int map[CH][CW] = {
        {0,0,1,1,0,0,1,1,0,0},
        {0,1,1,1,1,1,1,1,1,0},
        {1,1,1,1,1,1,1,1,1,1},
        {1,1,1,1,1,1,1,1,1,1},
        {0,2,2,0,2,2,0,2,2,0},
    };

    for (const auto& c : cloudPositions) {
        for (int ry = 0; ry < CH; ry++) {
            for (int rx = 0; rx < CW; rx++) {
                int v = map[ry][rx];
                if (v == 0) continue;
                sf::RectangleShape px({U + 0.5f, U + 0.5f});
                px.setPosition({c.x + rx * U, c.y + ry * U});
                px.setFillColor(v == 2 ? shade : body);
                window.draw(px);
            }
        }
    }

    window.setView(camera);
}

void Game::drawRain() {
    if (!raining) return;

    // Для обрезки капель по земле нужно знать, где поверхность в каждой экранной колонке.
    // Переводим экранную X в мировую, находим верхний твёрдый блок, переводим его Y обратно в экран.
    sf::Vector2f viewCenter = camera.getCenter();
    sf::Vector2f viewSize   = camera.getSize();
    float viewLeft = viewCenter.x - viewSize.x / 2.f;
    float viewTop  = viewCenter.y - viewSize.y / 2.f;
    // Коэффициенты перевода "мир -> экран(800x600)"
    float sx = 800.f / viewSize.x;
    float sy = 600.f / viewSize.y;

    auto groundScreenY = [&](float screenX) -> float {
        // экран -> мир
        float worldX = viewLeft + (screenX / 800.f) * viewSize.x;
        int bx = (int)(worldX / BLOCK_SIZE);
        if (!world.inBounds(bx, 0)) return 600.f;
        // ищем первый твёрдый блок сверху вниз в этой колонке
        int startBY = std::max(0, (int)(viewTop / BLOCK_SIZE));
        for (int by = startBY; by < WORLD_HEIGHT; by++) {
            const Block& b = world.getBlock(bx, by);
            // Дождь протекает сквозь кроны деревьев (листва) и стволы — они не считаются "землёй"
            if (b.type == BlockType::LEAVES || b.type == BlockType::WOOD) continue;
            if (b.isSolid()) {
                float worldY = by * (float)BLOCK_SIZE;
                return (worldY - viewTop) * sy; // мир -> экран
            }
        }
        return 600.f; // земли нет — капля падает до низа экрана
    };

    sf::View uiView(sf::FloatRect({0.f, 0.f}, {800.f, 600.f}));
    window.setView(uiView);

    sf::VertexArray lines(sf::PrimitiveType::Lines, rainDrops.size() * 2);
    std::size_t vi = 0;
    for (size_t i = 0; i < rainDrops.size(); i++) {
        const auto& d = rainDrops[i];
        float limit = groundScreenY(d.x); // до какой Y можно рисовать каплю
        if (d.y >= limit) continue;        // капля уже "под землёй" — не рисуем

        float tailY = d.y + 24.f;
        if (tailY > limit) tailY = limit;  // хвост капли обрезаем по поверхности
        float tailX = d.x - 6.f * ((tailY - d.y) / 24.f);

        lines[vi].position     = {d.x, d.y};
        lines[vi].color        = sf::Color(170, 200, 245, 200);
        vi++;
        lines[vi].position     = {tailX, tailY};
        lines[vi].color        = sf::Color(150, 185, 235, 120);
        vi++;
    }
    lines.resize(vi); // оставляем только реально нарисованные вершины
    window.draw(lines);

    window.setView(camera);
}

void Game::drawSky() {
    // Небо — плавный вертикальный градиент через один квад (интерполяция цвета по вершинам),
    // вместо полос это даёт по-настоящему гладкий переход без ступенек.
    sf::View uiView(sf::FloatRect({0.f, 0.f}, {800.f, 600.f}));
    window.setView(uiView);

    float brightness = getBrightness();

    auto lerpColor = [](sf::Color a, sf::Color b, float t) {
        return sf::Color(
            (std::uint8_t)(a.r + (b.r - a.r) * t),
            (std::uint8_t)(a.g + (b.g - a.g) * t),
            (std::uint8_t)(a.b + (b.b - a.b) * t));
    };

    sf::Color dayTop(100, 165, 225),   dayBottom(195, 215, 235);
    sf::Color nightTop(8, 8, 32),      nightBottom(40, 32, 62);

    sf::Color topColor    = lerpColor(nightTop, dayTop, brightness);
    sf::Color bottomColor = lerpColor(nightBottom, dayBottom, brightness);

    sf::VertexArray gradient(sf::PrimitiveType::TriangleStrip, 4);
    gradient[0].position = {0.f, 0.f};     gradient[0].color = topColor;
    gradient[1].position = {800.f, 0.f};   gradient[1].color = topColor;
    gradient[2].position = {0.f, 600.f};   gradient[2].color = bottomColor;
    gradient[3].position = {800.f, 600.f}; gradient[3].color = bottomColor;
    window.draw(gradient);

    // Солнце и луна двигаются по дуге в противофазе друг другу
    float angle      = (dayTime / DAY_LENGTH) * 2.f * 3.14159265f;
    float sunX       = 400.f - std::cos(angle) * 340.f;
    float sunY       = 480.f - brightness * 380.f;
    float moonBright = 1.f - brightness;
    float moonAngle  = angle + 3.14159265f;
    float moonX      = 400.f - std::cos(moonAngle) * 340.f;
    float moonY      = 480.f - moonBright * 380.f;

    sf::RenderStates addStates;
    addStates.blendMode = sf::BlendAdd;

    auto drawGlowDisc = [&](float cx, float cy, float strength, sf::Color core, sf::Color glow) {
        if (strength <= 0.02f) return;
        float radii[3] = {70.f, 45.f, 25.f};
        for (float r : radii) {
            sf::CircleShape g(r);
            g.setOrigin({r, r});
            g.setPosition({cx, cy});
            std::uint8_t a = (std::uint8_t)(38.f * strength * (25.f / r));
            g.setFillColor(sf::Color(glow.r, glow.g, glow.b, a));
            window.draw(g, addStates);
        }
        sf::CircleShape disc(16.f);
        disc.setOrigin({16.f, 16.f});
        disc.setPosition({cx, cy});
        disc.setFillColor(core);
        window.draw(disc);
    };

    drawGlowDisc(sunX, sunY, brightness, sf::Color(255, 250, 210), sf::Color(255, 225, 120));
    drawGlowDisc(moonX, moonY, moonBright * 0.8f, sf::Color(225, 230, 245), sf::Color(175, 195, 235));

    // Звёзды — мягко мерцают, а не просто статично включены/выключены
    if (brightness < 0.55f) {
        float starVisibility = (0.55f - brightness) / 0.55f;
        sf::CircleShape star(1.3f);
        star.setOrigin({1.3f, 1.3f});
        for (size_t i = 0; i < starPositions.size(); i++) {
            float twinkle = 0.6f + 0.4f * std::sin(animTime * 1.5f + (float)i * 0.9f);
            std::uint8_t a = (std::uint8_t)(220.f * starVisibility * twinkle);
            star.setPosition(starPositions[i]);
            star.setFillColor(sf::Color(255, 255, 255, a));
            window.draw(star);
        }
    }

    window.setView(camera);
}

void Game::render() {
    window.clear(sf::Color(100, 160, 220));
    drawSky();
    drawClouds();
    window.setView(camera);

    sf::FloatRect viewBounds(camera.getCenter() - camera.getSize() / 2.f, camera.getSize());
    int startX = std::max(0, (int)(viewBounds.position.x / BLOCK_SIZE));
    int endX   = std::min(WORLD_WIDTH,  (int)((viewBounds.position.x + viewBounds.size.x) / BLOCK_SIZE) + 1);
    int startY = std::max(0, (int)(viewBounds.position.y / BLOCK_SIZE));
    int endY   = std::min(WORLD_HEIGHT, (int)((viewBounds.position.y + viewBounds.size.y) / BLOCK_SIZE) + 1);

    sf::Sprite tile(blockTexture);
    // Масштаб текстуры (тайл 32px -> размер блока), с лёгким перекрытием против щелей
    float overlap = 1.0f;
    float scale = ((float)BLOCK_SIZE + overlap * 2.f) / (float)Block::TEXTURE_SIZE;
    tile.setScale({scale, scale});

    std::vector<sf::Vector2f> lightSources; // центры факелов в кадре — для свечения в темноте

    for (int y = startY; y < endY; y++) {
        for (int x = startX; x < endX; x++) {
            const Block& block = world.getBlock(x, y);
            bool isTorch = (block.type == BlockType::TORCH);
            if (block.isWater()) continue; // вода рисуется отдельным полупрозрачным проходом
            if (!block.isSolid() && !isTorch) continue;

            float px = (float)(x * BLOCK_SIZE);
            float py = (float)(y * BLOCK_SIZE);

            if (isTorch) {
                // Факел рисуется процедурно — тонкая палочка и мерцающее пламя, а не текстурой
                float cx = px + BLOCK_SIZE / 2.f;
                float stickW = 4.f;
                float stickH = BLOCK_SIZE * 0.55f;

                sf::RectangleShape stick({stickW, stickH});
                stick.setFillColor(sf::Color(90, 60, 30));
                stick.setPosition({cx - stickW / 2.f, py + BLOCK_SIZE - stickH});
                window.draw(stick);

                float flicker = 0.8f + 0.2f * std::sin(animTime * 14.f + (x * 13 + y * 7));
                float flameY = py + BLOCK_SIZE - stickH - 2.f;

                sf::CircleShape flameOuter(6.f * flicker);
                flameOuter.setOrigin({6.f * flicker, 6.f * flicker});
                flameOuter.setPosition({cx, flameY});
                flameOuter.setFillColor(sf::Color(255, 150, 40));
                window.draw(flameOuter);

                sf::CircleShape flameCore(3.f * flicker);
                flameCore.setOrigin({3.f * flicker, 3.f * flicker});
                flameCore.setPosition({cx, flameY});
                flameCore.setFillColor(sf::Color(255, 230, 140));
                window.draw(flameCore);

                lightSources.push_back({cx, py + BLOCK_SIZE / 2.f});
                continue;
            }

            tile.setTextureRect(block.getTextureRect());
            tile.setPosition({px - overlap, py - overlap});
            window.draw(tile);

            // Горящий TNT мигает белым, мигание ускоряется по мере приближения взрыва
            if (block.type == BlockType::TNT) {
                for (const auto& t : activeTNT) {
                    if (t.bx == x && t.by == y) {
                        float elapsed = t.maxFuseTime - t.fuseTime;
                        float period = std::max(0.08f, 0.05f + 0.15f * (t.fuseTime / t.maxFuseTime));
                        bool flashOn = std::fmod(elapsed, period) < period * 0.5f;
                        if (flashOn) {
                            sf::RectangleShape overlay({(float)BLOCK_SIZE, (float)BLOCK_SIZE});
                            overlay.setPosition({px, py});
                            overlay.setFillColor(sf::Color(255, 255, 255, 150));
                            window.draw(overlay);
                        }
                        break;
                    }
                }
            }

            // Горящая печь — маленький огонёк в её "пасти" + источник света в темноте
            if (block.type == BlockType::FURNACE) {
                auto fit = furnaceStorage.find({x, y});
                bool lit = (fit != furnaceStorage.end() && fit->second.burnTimeLeft > 0.f);
                if (lit) {
                    float cx = px + BLOCK_SIZE / 2.f;
                    float flicker = 0.8f + 0.2f * std::sin(animTime * 14.f + (x * 13 + y * 7));
                    float mouthY = py + BLOCK_SIZE * 0.6f; // примерно где тёмное отверстие на текстуре печи

                    sf::CircleShape glow(5.f * flicker);
                    glow.setOrigin({5.f * flicker, 5.f * flicker});
                    glow.setPosition({cx, mouthY});
                    glow.setFillColor(sf::Color(255, 140, 40, 220));
                    window.draw(glow);

                    sf::CircleShape core(2.5f * flicker);
                    core.setOrigin({2.5f * flicker, 2.5f * flicker});
                    core.setPosition({cx, mouthY});
                    core.setFillColor(sf::Color(255, 220, 140, 230));
                    window.draw(core);

                    lightSources.push_back({cx, py + BLOCK_SIZE / 2.f});
                }
            }

            // Травинки над блоком травы — покачиваются от "ветра", если сверху воздух
            if (block.type == BlockType::GRASS) {
                bool topAir = !world.inBounds(x, y - 1) || !world.getBlock(x, y - 1).isSolid();
                if (topAir) {
                    for (int b = 0; b < 3; b++) {
                        int h = x * 928371 + b * 5237 + 17; // детерминированный "хэш" для стабильной позиции травинки
                        float offsetX     = 4.f + b * 10.f + (float)((h >> 3) % 5);
                        float bladeHeight = 5.f + (float)((h >> 7) % 4);
                        float sway = std::sin(animTime * 3.f + x * 0.7f + b * 1.3f) * 1.6f;

                        sf::RectangleShape blade({2.f, bladeHeight});
                        blade.setFillColor(sf::Color(80, 160, 30));
                        blade.setPosition({px + offsetX + sway, py - bladeHeight + 2.f});
                        window.draw(blade);
                    }
                }
            }
        }
    }

    drawParticles();

    for (const auto& chicken : chickens)
        chicken.draw(window);

    for (const auto& e : enemies)
        e.draw(window);

    // Стрелы лучников
    for (const auto& a : arrows) {
        sf::RectangleShape shaft({14.f, 3.f});
        shaft.setOrigin({7.f, 1.5f});
        shaft.setPosition({a.x, a.y});
        shaft.setRotation(sf::radians(std::atan2(a.vy, a.vx)));
        shaft.setFillColor(sf::Color(90, 70, 45));
        window.draw(shaft);
    }

    player.draw(window);

    // Вода — полупрозрачный проход поверх мира и игрока (игрок просвечивает под водой)
    for (int y = startY; y < endY; y++) {
        for (int x = startX; x < endX; x++) {
            const Block& block = world.getBlock(x, y);
            if (!block.isWater()) continue;
            float px = (float)(x * BLOCK_SIZE);
            float py = (float)(y * BLOCK_SIZE);
            tile.setTextureRect(block.getTextureRect());
            tile.setColor(sf::Color(255, 255, 255, 180)); // полупрозрачность воды
            tile.setPosition({px - overlap, py - overlap});
            window.draw(tile);
            tile.setColor(sf::Color(255, 255, 255, 255)); // вернуть непрозрачность для остальных
        }
    }

    window.draw(cursor);

    // Ночное затемнение мира (небо темнеет отдельно в drawSky, а это — сама сцена)
    float brightness = getBrightness();
    if (brightness < 0.95f) {
        float darkAlpha = (1.f - brightness) * 165.f;
        sf::RectangleShape overlay({viewBounds.size.x + 4.f, viewBounds.size.y + 4.f});
        overlay.setPosition({viewBounds.position.x - 2.f, viewBounds.position.y - 2.f});
        overlay.setFillColor(sf::Color(10, 10, 40, (std::uint8_t)darkAlpha));
        window.draw(overlay);

        // Факелы пробивают тьму тёплым светом (аддитивное смешивание поверх затемнения)
        if (darkAlpha > 1.f && !lightSources.empty()) {
            sf::RenderStates addStates;
            addStates.blendMode = sf::BlendAdd;
            float k = darkAlpha / 165.f; // сила свечения растёт вместе с темнотой

            for (const auto& src : lightSources) {
                sf::CircleShape glowOuter(150.f);
                glowOuter.setOrigin({150.f, 150.f});
                glowOuter.setPosition(src);
                glowOuter.setFillColor(sf::Color(255, 170, 80, (std::uint8_t)(40 * k)));
                window.draw(glowOuter, addStates);

                sf::CircleShape glowMid(90.f);
                glowMid.setOrigin({90.f, 90.f});
                glowMid.setPosition(src);
                glowMid.setFillColor(sf::Color(255, 190, 110, (std::uint8_t)(75 * k)));
                window.draw(glowMid, addStates);

                sf::CircleShape glowInner(45.f);
                glowInner.setOrigin({45.f, 45.f});
                glowInner.setPosition(src);
                glowInner.setFillColor(sf::Color(255, 215, 150, (std::uint8_t)(115 * k)));
                window.draw(glowInner, addStates);
            }
        }
    }

    drawRain(); // дождь поверх мира, но под интерфейсом

    hud.drawPanel(window);
    inventory.draw(window);
    hud.draw(window);

    if (workbenchOpen) {
        inventory.drawWorkbenchScreen(window);
    } else if (furnaceOpen) {
        auto it = furnaceStorage.find(openFurnacePos);
        if (it != furnaceStorage.end()) {
            float burnFrac = (it->second.burnTimeMax > 0.f) ? (it->second.burnTimeLeft / it->second.burnTimeMax) : 0.f;
            BlockType dummyResult; float cookTime = 1.f;
            const InventorySlot& in = it->second.slots[0];
            float cookFrac = 0.f;
            if (in.count > 0 && getSmeltResult(in.type, dummyResult, cookTime) && cookTime > 0.f)
                cookFrac = it->second.cookProgress / cookTime;
            inventory.drawFurnaceScreen(window, it->second.slots, burnFrac, cookFrac);
        }
    } else if (chestOpen) {
        auto it = chestStorage.find(openChestPos);
        if (it != chestStorage.end())
            inventory.drawChestScreen(window, it->second);
    } else if (inventoryOpen) {
        inventory.drawFullScreen(window);
    }

    window.display();
}