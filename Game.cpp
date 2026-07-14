#include "Game.hpp"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <cstdint>
#include <cmath>
#include <cstdlib>
#include <random>

const std::string Game::SAVE_FILE = "savegame.dat";

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

    // Шрифт для текста в меню — своего файла шрифта в проекте нет, пробуем системный (macOS).
    // Если не найдётся ни один — меню всё равно работает, просто без подписей на кнопках.
    {
        const char* candidates[] = {
            "/System/Library/Fonts/Supplemental/Arial Bold.ttf",
            "/System/Library/Fonts/Supplemental/Arial.ttf",
            "/System/Library/Fonts/Helvetica.ttc",
            "/System/Library/Fonts/SFNSRounded.ttf",
        };
        for (const char* path : candidates) {
            if (font.openFromFile(path)) {
                fontLoaded = true;
                break;
            }
        }
        if (!fontLoaded) std::cerr << "Не нашла системный шрифт — меню будет без текста на кнопках" << std::endl;
    }

    // Фон для экранов меню
    if (menuBgTexture.loadFromFile("textures/menu_bg.png")) {
        menuBgLoaded = true;
        menuBgTexture.setSmooth(false);
    } else {
        std::cerr << "Failed to load: textures/menu_bg.png" << std::endl;
    }

    // Генерируем мягкий радиальный градиент один раз при старте: белый круг с плавно
    // (по smoothstep) затухающей к краю альфой — без единой видимой границы/кольца.
    // Дальше это просто растягивается спрайтом под нужный радиус (солнце крупнее, факел мельче)
    // и тонируется нужным цветом через setColor — так свечение выглядит цельным, а не кольцами.
    {
        const unsigned int GLOW_TEX_SIZE = 256;
        sf::Image glowImg;
        glowImg.resize({GLOW_TEX_SIZE, GLOW_TEX_SIZE}, sf::Color::Transparent);
        float center = GLOW_TEX_SIZE / 2.f;
        for (unsigned int gy = 0; gy < GLOW_TEX_SIZE; gy++) {
            for (unsigned int gx = 0; gx < GLOW_TEX_SIZE; gx++) {
                float dx = (float)gx - center, dy = (float)gy - center;
                float dist = std::sqrt(dx * dx + dy * dy) / center; // 0 в центре, 1 на краю
                float t = std::clamp(1.f - dist, 0.f, 1.f);
                float alpha = t * t * (3.f - 2.f * t); // smoothstep — плавно и без ступенек
                glowImg.setPixel({gx, gy}, sf::Color(255, 255, 255, (std::uint8_t)(alpha * 255.f)));
            }
        }
        if (!glowTexture.loadFromImage(glowImg)) {
            std::cerr << "Failed to generate glow texture" << std::endl;
        }
        glowTexture.setSmooth(true); // билинейная интерполяция при растяжении — никаких резких краёв
    }

    // Малиновая листва сакуры — честная перекраска тайла обычной листвы, а не полупрозрачный
    // слой поверх (тот способ размывал контраст между "листиками" и тёмными "дырками").
    // Берём исходные пиксели тайла: тёмные (дырки) оставляем тёмными, яркие (листики)
    // перекрашиваем в малиновый с той же относительной яркостью — рисунок сохраняется, меняется тон.
    {
        sf::Image atlasImg;
        if (atlasImg.loadFromFile("textures/blocks.png")) {
            const unsigned int TS = (unsigned int)Block::TEXTURE_SIZE;
            const unsigned int LEAF_INDEX = 4; // индекс тайла обычной листвы в атласе
            sf::Image leafImg;
            leafImg.resize({TS, TS}, sf::Color::Black);
            sf::Color raspberry(240, 140, 195); // нежный светло-розовый — целевой цвет "листиков"

            for (unsigned int py = 0; py < TS; py++) {
                for (unsigned int px = 0; px < TS; px++) {
                    sf::Color src = atlasImg.getPixel({LEAF_INDEX * TS + px, py});
                    int lum = src.r + src.g + src.b;
                    if (lum < 60) {
                        leafImg.setPixel({px, py}, sf::Color::Black); // тёмная "дырка" — оставляем как есть
                    } else {
                        // Яркость пикселя листика (зелёный канал доминирует в исходной текстуре)
                        float t = std::clamp((src.g - 100.f) / (245.f - 100.f), 0.55f, 1.f);
                        leafImg.setPixel({px, py}, sf::Color(
                            (std::uint8_t)(raspberry.r * t),
                            (std::uint8_t)(raspberry.g * t),
                            (std::uint8_t)(raspberry.b * t)));
                    }
                }
            }
            if (!sakuraLeafTexture.loadFromImage(leafImg)) {
                std::cerr << "Failed to generate sakura leaf texture" << std::endl;
            }
        } else {
            std::cerr << "Failed to reload textures/blocks.png for sakura recolor" << std::endl;
        }
    }
    inventory.setSakuraLeafTexture(&sakuraLeafTexture);

    // Мир и мобы больше НЕ создаются здесь — игрок сначала видит главный экран и
    // выбирает "Продолжить" или "Новый мир" (см. startNewWorld()/loadGame()).

    sounds.startMenuMusic(); // играет, пока не зайдём в мир
}



void Game::run() {
    sf::Clock clock;
    sf::Clock stepClock;
    while (window.isOpen()) {
        float deltaTime = clock.restart().asSeconds();

        if (state != GameState::Playing) {
            // На экранах меню обрабатываем только закрытие окна и клики по кнопкам —
            // никакой игровой логики (мира ещё может не быть) в этом состоянии не крутим.
            while (const std::optional event = window.pollEvent()) {
                if (event->is<sf::Event::Closed>()) window.close();
                if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>()) {
                    if (mb->button == sf::Mouse::Button::Left)
                        handleMenuClick(sf::Vector2i(mb->position.x, mb->position.y));
                }
            }
            if (state == GameState::MainMenu) renderMainMenu();
            else if (state == GameState::WorldMenu) renderWorldMenu();
            continue;
        }

        handleEvents();
        update(deltaTime);

        if (player.isMoving() && player.isOnGround()) {
            if (stepClock.getElapsedTime().asSeconds() > 0.4f) {
                int bx = (int)((player.getX() + player.getWidth() / 2.f) / BLOCK_SIZE);
                int by = (int)((player.getY() + player.getHeight() + 2) / BLOCK_SIZE);
                if (world.inBounds(bx, 0) && world.biomeAt(bx) == 3) {
                    sounds.playSnowStep(); // снежный биом — свой звук шагов, независимо от блока под ногами
                } else {
                    BlockType underType = BlockType::GRASS;
                    if (world.inBounds(bx, by) && world.getBlock(bx, by).isSolid())
                        underType = world.getBlock(bx, by).type;
                    sounds.playStep(underType);
                }
                stepClock.restart();
            }
        } else if (player.isClimbingMoving()) {
            // Лазанье по лестнице — свой периодический звук, вместо обычных шагов
            if (stepClock.getElapsedTime().asSeconds() > 0.35f) {
                sounds.playLadderStep();
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
        if (event->is<sf::Event::Closed>()) {
            saveGame(); // сохраняем прогресс перед выходом
            window.close();
        }

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
                    sounds.playChestClose();
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

            if (!inventoryOpen && !chestOpen && !workbenchOpen && !furnaceOpen && !player.isSleeping()) {
                if (kp->code == sf::Keyboard::Key::A ||
                    kp->code == sf::Keyboard::Key::D ||
                    kp->code == sf::Keyboard::Key::W ||
                    kp->code == sf::Keyboard::Key::S ||
                    kp->code == sf::Keyboard::Key::Space)
                    player.handleKeyPressed(kp->code);
            }
        }

        if (const auto* kr = event->getIf<sf::Event::KeyReleased>())
            player.handleKeyReleased(kr->code);

        if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>()) {
          if (!player.isSleeping()) {
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
                        sounds.playChestClose();
                    } else if (inventory.handleChestClick(mpos, it->second)) {
                        sounds.playDig(BlockType::WOOD); // звук перекладывания вещей
                    }
                } else if (inventory.handleCloseClick(mpos)) {
                    inventoryOpen = false; // клик по крестику закрывает инвентарь
                } else if (inventory.handleCraftClick(mpos)) {
                    sounds.playClick(); // подтверждение крафта
                }
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
        p.velocity.y += GRAVITY * p.gravityScale * deltaTime;
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

        // Потрескивание — пока горит топливо, независимо от того, готовится что-то или нет
        // (то же условие, что и у видимого огонька в render())
        if (f.burnTimeLeft > 0.f) {
            f.crackleTimer -= deltaTime;
            if (f.crackleTimer <= 0.f) {
                float fx = (kv.first.first + 0.5f) * BLOCK_SIZE;
                float fy = (kv.first.second + 0.5f) * BLOCK_SIZE;
                float pcx = player.getX() + player.getWidth() / 2.f;
                float pcy = player.getY() + player.getHeight() / 2.f;
                float dist = std::sqrt((fx - pcx) * (fx - pcx) + (fy - pcy) * (fy - pcy)) / BLOCK_SIZE;
                sounds.playFurnaceCrackle(dist);
                f.crackleTimer = 1.5f + (float)(rand() % 150) / 100.f; // следующее потрескивание через 1.5–3.0 сек
            }
        } else {
            f.crackleTimer = 0.f;
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

    for (auto& cow : cows) {
        float dxc = cow.getX() - cx;
        float dyc = cow.getY() - cy;
        float distC = std::sqrt(dxc * dxc + dyc * dyc) / BLOCK_SIZE;
        if (distC < DAMAGE_RADIUS) {
            float lenC = std::max(1.f, std::sqrt(dxc * dxc + dyc * dyc));
            float tC = std::max(0.f, 1.f - distC / DAMAGE_RADIUS);
            float forceC = 250.f + 400.f * tC;
            cow.applyKnockback((dxc / lenC) * forceC, -220.f - 250.f * tC);
        }
    }

    for (auto& s : sheep) {
        float dxc = s.getX() - cx;
        float dyc = s.getY() - cy;
        float distC = std::sqrt(dxc * dxc + dyc * dyc) / BLOCK_SIZE;
        if (distC < DAMAGE_RADIUS) {
            float lenC = std::max(1.f, std::sqrt(dxc * dxc + dyc * dyc));
            float tC = std::max(0.f, 1.f - distC / DAMAGE_RADIUS);
            float forceC = 250.f + 400.f * tC;
            s.applyKnockback((dxc / lenC) * forceC, -220.f - 250.f * tC);
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

        // И не по корове ли (тоже добыча)
        for (size_t i = 0; i < cows.size(); i++) {
            sf::FloatRect bounds(
                {cows[i].getX(), cows[i].getY()},
                {cows[i].getWidth(), cows[i].getHeight()});
            if (bounds.contains(worldPos)) {
                int cbx = (int)(cows[i].getX() / BLOCK_SIZE);
                int cby = (int)(cows[i].getY() / BLOCK_SIZE);
                if (isWithinReach(cbx, cby)) {
                    killCow(i);
                }
                return;
            }
        }

        // И не по овце ли (тоже добыча)
        for (size_t i = 0; i < sheep.size(); i++) {
            sf::FloatRect bounds(
                {sheep[i].getX(), sheep[i].getY()},
                {sheep[i].getWidth(), sheep[i].getHeight()});
            if (bounds.contains(worldPos)) {
                int cbx = (int)(sheep[i].getX() / BLOCK_SIZE);
                int cby = (int)(sheep[i].getY() / BLOCK_SIZE);
                if (isWithinReach(cbx, cby)) {
                    killSheep(i);
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
        bool breakable = block.isSolid() || block.type == BlockType::TORCH ||
                          block.isLadder() || block.isDoorPart();
        if (breakable) {
            if (block.type == BlockType::TNT) {
                armTnt(bx, by, 1.6f); // поджигаем, а не ломаем сразу
            } else if (block.isDoorPart()) {
                // Дверь целиком в 2 клетки — ломаем обе половинки, а не только ту, по которой кликнули
                sounds.playDig(BlockType::WOOD);
                spawnBreakParticles(bx, by, block);
                bool isTop = block.isDoorTop();
                int bottomY = isTop ? by + 1 : by;
                int topY    = isTop ? by     : by - 1;
                world.setBlock(bx, bottomY, BlockType::AIR);
                world.setBlock(bx, topY,    BlockType::AIR);
                inventory.addItem(BlockType::DOOR, 1); // только 1 дверь за обе половинки
            } else if (block.isBedPart()) {
                // Кровать целиком в 2 клетки по горизонтали — тоже ломаем обе половинки разом
                sounds.playDig(BlockType::WOOD);
                spawnBreakParticles(bx, by, block);
                bool isHead = (block.type == BlockType::BED_HEAD);
                int footX = isHead ? bx - 1 : bx;
                int headX = isHead ? bx     : bx + 1;
                world.setBlock(footX, by, BlockType::AIR);
                world.setBlock(headX, by, BlockType::AIR);
                inventory.addItem(BlockType::BED, 1); // только 1 кровать за обе половинки
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
            sounds.playChestOpen();
            return;
        }
        if (clicked.type == BlockType::FURNACE) {
            std::pair<int, int> key(bx, by);
            furnaceStorage[key]; // создаёт пустое состояние печи при первом открытии, если его ещё нет
            furnaceOpen = true;
            openFurnacePos = key;
            return;
        }
        if (clicked.type == BlockType::DOOR      || clicked.type == BlockType::DOOR_OPEN ||
            clicked.type == BlockType::DOOR_TOP  || clicked.type == BlockType::DOOR_TOP_OPEN) {
            // Дверь занимает 2 клетки по высоте — определяем, где у неё низ, а где верх,
            // и переключаем обе половинки разом (клик по любой из них сработает одинаково).
            bool isTop = clicked.isDoorTop();
            int bottomY = isTop ? by + 1 : by;
            int topY    = isTop ? by     : by - 1;
            bool opening = !clicked.isDoorOpenState();
            world.setBlock(bx, bottomY, opening ? BlockType::DOOR_OPEN     : BlockType::DOOR);
            world.setBlock(bx, topY,    opening ? BlockType::DOOR_TOP_OPEN : BlockType::DOOR_TOP);
            if (opening) sounds.playDoorOpen(); else sounds.playDoorClose();
            return;
        }

        if (clicked.isBedPart()) {
            // Кровать занимает 2 клетки по горизонтали — находим изножье (для точки спавна и позы)
            bool isHead = (clicked.type == BlockType::BED_HEAD);
            int footX = isHead ? bx - 1 : bx;

            // Ложимся спать, только если сейчас реально ночь — просто поставить кровать
            // или ткнуть её днём для этого недостаточно. Ночь пройдёт не сразу, а через
            // несколько секунд "сна", и точка возрождения переносится в этот момент.
            if (!player.isSleeping() && getBrightness() < 0.3f) {
                spawnX = footX * (float)BLOCK_SIZE;
                spawnY = (by - 2) * (float)BLOCK_SIZE;
                player.startSleep(footX * (float)BLOCK_SIZE, by * (float)BLOCK_SIZE);
                sleepTimer = SLEEP_DURATION;
                sounds.playClick(); // временный звук — своего "укладывания в кровать" пока нет
            }
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
                sounds.playEat();
                return;
            }
            if (selBlock.isItem()) {
                // слитки и прочие "предметы" нельзя поставить как блок в мир
                return;
            }
        }

        if (!world.getBlock(bx, by).isSolid() && inventory.hasSelected()) {
            BlockType t = inventory.getSelectedType();

            if (t == BlockType::DOOR) {
                // Дверь ростом в 2 клетки: низ — там, куда кликнули, верх — клетка над ней.
                // Ставим, только если обе клетки свободны (не заняты чем-то твёрдым).
                if (by - 1 >= 0 && !world.getBlock(bx, by - 1).isSolid()) {
                    world.setBlock(bx, by, BlockType::DOOR);
                    world.setBlock(bx, by - 1, BlockType::DOOR_TOP);
                    sounds.playDig(BlockType::WOOD);
                    inventory.consumeSelected();
                }
                return;
            }

            if (t == BlockType::BED) {
                // Кровать шириной в 2 клетки: изножье — там, куда кликнули, изголовье — клетка справа.
                if (world.inBounds(bx + 1, by) && !world.getBlock(bx + 1, by).isSolid()) {
                    world.setBlock(bx, by, BlockType::BED);
                    world.setBlock(bx + 1, by, BlockType::BED_HEAD);
                    sounds.playDig(BlockType::WOOD);
                    inventory.consumeSelected();
                }
                return;
            }

            if (t == BlockType::TORCH)      sounds.playFuse();   // потрескивание факела
            else if (t == BlockType::WATER) sounds.playSplash(); // плеск при установке воды
            else                            sounds.playDig(t);
            world.setBlock(bx, by, t);
            inventory.consumeSelected();
        }
    }
}

void Game::update(float deltaTime) {
    if (!inventoryOpen && !chestOpen && !workbenchOpen && !furnaceOpen && !player.isSleeping()) {
        player.update(deltaTime, world);
        if (player.didJustEnterWater()) sounds.playSplash(); // плеск при входе в воду
    }

    // Сон — несколько секунд лежим, потом сами встаём и наступает утро
    if (player.isSleeping()) {
        sleepTimer -= deltaTime;
        if (sleepTimer <= 0.f) {
            player.wakeUp();
            dayTime = DAY_LENGTH * 0.25f; // то же "утро", с которого стартует новая игра
        }
    }

    updateParticles(deltaTime);
    world.updateFallingBlocks(deltaTime);
    updateExplosives(deltaTime);
    updateFurnaces(deltaTime);
    animTime += deltaTime;

    // Лепестки сакуры — медленно парят вниз, пока игрок стоит в биоме сакуры.
    // Это однозначный, "настоящий" признак биома, а не просто оттенок травы,
    // который легко принять за случайность/баг.
    {
        int playerBx = (int)((player.getX() + player.getWidth() / 2.f) / BLOCK_SIZE);
        if (world.inBounds(playerBx, 0) && world.biomeAt(playerBx) == 2) {
            petalTimer -= deltaTime;
            if (petalTimer <= 0.f) {
                petalTimer = 0.15f + (float)(rand() % 15) / 100.f; // новый лепесток каждые ~0.15-0.3 сек
                float spawnX = player.getX() + (float)(rand() % 500) - 250.f; // в районе экрана над игроком
                float spawnY = player.getY() - 250.f - (float)(rand() % 100);
                Particle petal;
                petal.position = {spawnX, spawnY};
                petal.velocity = {(float)(rand() % 40) - 20.f, 25.f + (float)(rand() % 20)};
                petal.color = sf::Color(255, 170, 205, 230);
                petal.size = 3.f + (float)(rand() % 3);
                petal.maxLifetime = 4.f + (float)(rand() % 200) / 100.f;
                petal.lifetime = petal.maxLifetime;
                petal.gravityScale = 0.03f; // почти невесомый, парит, а не падает камнем
                particles.push_back(petal);
            }
        }
    }

    // Снег — идёт постоянно, пока игрок в снежном биоме (а не по случайной погоде)
    {
        int playerBx = (int)((player.getX() + player.getWidth() / 2.f) / BLOCK_SIZE);
        if (world.inBounds(playerBx, 0) && world.biomeAt(playerBx) == 3) {
            snowTimer -= deltaTime;
            if (snowTimer <= 0.f) {
                snowTimer = 0.03f + (float)(rand() % 5) / 100.f; // снежинки чаще лепестков — гуще снегопад
                float spawnX = player.getX() + (float)(rand() % 700) - 350.f;
                float spawnY = player.getY() - 300.f - (float)(rand() % 100);
                Particle flake;
                flake.position = {spawnX, spawnY};
                flake.velocity = {(float)(rand() % 30) - 15.f, 45.f + (float)(rand() % 25)};
                flake.color = sf::Color(255, 255, 255, 220);
                flake.size = 2.f + (float)(rand() % 3);
                flake.maxLifetime = 3.f + (float)(rand() % 150) / 100.f;
                flake.lifetime = flake.maxLifetime;
                flake.gravityScale = 0.05f; // тоже почти невесомый — снег кружит, а не сыплется камнем
                particles.push_back(flake);
            }
        }
    }

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

    for (auto& cow : cows)
        cow.update(deltaTime, world);

    cowRespawnTimer -= deltaTime;
    if (cowRespawnTimer <= 0.f) {
        if ((int)cows.size() < MAX_COWS)
            spawnCow();
        cowRespawnTimer = 20.f + rand() % 15;
    }

    for (auto& s : sheep)
        s.update(deltaTime, world);

    sheepRespawnTimer -= deltaTime;
    if (sheepRespawnTimer <= 0.f) {
        if ((int)sheep.size() < MAX_SHEEP)
            spawnSheep();
        sheepRespawnTimer = 20.f + rand() % 15;
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

        // Коровы иногда мычат, когда игрок рядом
        for (auto& cow : cows) {
            if (cow.shouldMoo(pcx, pcy, deltaTime)) sounds.playCowMoo();
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

    // Мягкое многослойное пламя (как у настоящего факела): широкое оранжевое основание,
    // жёлто-оранжевая середина, светлое ядро и маленький красноватый кончик наверху.
    // baseY — точка, где пламя "сидит"; scale — общий множитель размера (у печи он меньше).
    auto drawFlame = [&](float cx, float baseY, float scale, float flicker) {
        sf::CircleShape base(6.5f * flicker * scale);
        base.setScale({0.85f, 1.15f});
        base.setOrigin({6.5f * flicker * scale, 6.5f * flicker * scale});
        base.setPosition({cx, baseY});
        base.setFillColor(sf::Color(224, 100, 28, 235));
        window.draw(base);

        sf::CircleShape mid(4.6f * flicker * scale);
        mid.setScale({0.85f, 1.2f});
        mid.setOrigin({4.6f * flicker * scale, 4.6f * flicker * scale});
        mid.setPosition({cx, baseY - 3.f * scale});
        mid.setFillColor(sf::Color(255, 165, 40));
        window.draw(mid);

        sf::CircleShape core(2.3f * flicker * scale);
        core.setOrigin({2.3f * flicker * scale, 2.3f * flicker * scale});
        core.setPosition({cx, baseY - 4.6f * scale});
        core.setFillColor(sf::Color(255, 235, 170));
        window.draw(core);

        sf::CircleShape tip(1.5f * flicker * scale);
        tip.setScale({0.8f, 1.3f});
        tip.setOrigin({1.5f * flicker * scale, 1.5f * flicker * scale});
        tip.setPosition({cx, baseY - 8.f * scale});
        tip.setFillColor(sf::Color(206, 46, 24, 235));
        window.draw(tip);
    };

    for (int y = startY; y < endY; y++) {
        for (int x = startX; x < endX; x++) {
            const Block& block = world.getBlock(x, y);
            bool isTorch    = (block.type == BlockType::TORCH);
            bool isLadder   = (block.type == BlockType::LADDER);
            bool isDoor     = block.isDoorPart();
            if (block.isWater()) continue; // вода рисуется отдельным полупрозрачным проходом
            if (!block.isSolid() && !isTorch && !isLadder && !isDoor) continue;

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

                float flicker = 0.92f + 0.08f * std::sin(animTime * 10.f + (x * 13 + y * 7));
                float flameY = py + BLOCK_SIZE - stickH - 2.f;
                drawFlame(cx, flameY, 1.f, flicker);

                lightSources.push_back({cx, py + BLOCK_SIZE / 2.f});
                continue;
            }

            if (isLadder) {
                // Лестница — процедурно: два рельса по бокам клетки и перекладины между ними
                sf::Color rail(140, 100, 55);
                sf::Color rung(170, 125, 70);
                float railW = BLOCK_SIZE * 0.12f;
                sf::RectangleShape left({railW, (float)BLOCK_SIZE});
                left.setPosition({px + BLOCK_SIZE * 0.1f, py});
                left.setFillColor(rail);
                window.draw(left);
                sf::RectangleShape right({railW, (float)BLOCK_SIZE});
                right.setPosition({px + BLOCK_SIZE * 0.78f, py});
                right.setFillColor(rail);
                window.draw(right);
                for (int i = 0; i < 3; i++) {
                    sf::RectangleShape r({BLOCK_SIZE * 0.58f, BLOCK_SIZE * 0.12f});
                    r.setPosition({px + BLOCK_SIZE * 0.21f, py + BLOCK_SIZE * (0.18f + i * 0.32f)});
                    r.setFillColor(rung);
                    window.draw(r);
                }
                continue;
            }

            if (isDoor) {
                // Дверь — процедурно, ростом в 2 клетки: закрытая перекрывает проём целиком,
                // открытая "распахнута" к одному краю (тонкая полоска сбоку) и не мешает проходу.
                // Верх и низ рисуются без зазора между собой — только с внешним отступом сверху/снизу.
                bool open  = block.isDoorOpenState();
                bool isTop = block.isDoorTop();
                sf::Color panel(150, 108, 60);
                sf::Color frame(105, 74, 40);
                sf::Color handle(230, 200, 90);

                float doorW = open ? BLOCK_SIZE * 0.16f : BLOCK_SIZE * 0.7f;
                float doorX = open ? px + BLOCK_SIZE - doorW : px + BLOCK_SIZE * 0.15f;
                float padTop    = isTop  ? 1.f : 0.f;
                float padBottom = isTop  ? 0.f : 1.f;

                sf::RectangleShape panelShape({doorW, (float)BLOCK_SIZE - padTop - padBottom});
                panelShape.setPosition({doorX, py + padTop});
                panelShape.setFillColor(panel);
                panelShape.setOutlineColor(frame);
                panelShape.setOutlineThickness(2.f);
                window.draw(panelShape);

                if (!isTop) {
                    // Ручка — примерно на уровне руки, на нижней половинке
                    sf::CircleShape knob(BLOCK_SIZE * 0.045f);
                    knob.setPosition({doorX + (open ? -BLOCK_SIZE * 0.05f : doorW - BLOCK_SIZE * 0.12f),
                                      py + BLOCK_SIZE * 0.15f});
                    knob.setFillColor(handle);
                    window.draw(knob);
                }
                continue;
            }

            if (block.isBedPart()) {
                // Кровать — процедурно, шириной в 2 клетки: тонкая рама снизу + одеяло сверху,
                // подушка только на изголовье. Рисуется без зазора между половинками.
                bool isHead = (block.type == BlockType::BED_HEAD);
                sf::Color frame(90, 65, 35);
                sf::Color blanket(190, 45, 45);
                sf::Color pillow(235, 235, 240);

                sf::RectangleShape frameShape({(float)BLOCK_SIZE, BLOCK_SIZE * 0.18f});
                frameShape.setPosition({px, py + BLOCK_SIZE * 0.82f});
                frameShape.setFillColor(frame);
                window.draw(frameShape);

                sf::RectangleShape blanketShape({(float)BLOCK_SIZE, BLOCK_SIZE * 0.38f});
                blanketShape.setPosition({px, py + BLOCK_SIZE * 0.44f});
                blanketShape.setFillColor(blanket);
                window.draw(blanketShape);

                if (isHead) {
                    sf::RectangleShape pillowShape({BLOCK_SIZE * 0.34f, BLOCK_SIZE * 0.3f});
                    pillowShape.setPosition({px + BLOCK_SIZE * 0.58f, py + BLOCK_SIZE * 0.46f});
                    pillowShape.setFillColor(pillow);
                    window.draw(pillowShape);
                }
                continue;
            }

            if (block.type == BlockType::LEAVES_SAKURA) {
                // Честная перекраска (сохраняет рисунок "листики/дырки"), а не полупрозрачный слой
                sf::Sprite sakuraTile(sakuraLeafTexture);
                sakuraTile.setPosition({px - overlap, py - overlap});
                window.draw(sakuraTile);
            } else {
                tile.setTextureRect(block.getTextureRect());
                tile.setPosition({px - overlap, py - overlap});
                window.draw(tile);
            }

            // Листва акации и трава/стволы под биом кладём ПОВЕРХ полупрозрачным
            // прямоугольником, а не через setColor(умножение): умножение на тёмно-зелёной
            // текстуре может только затемнять, а не сдвигать цвет в розовый/жёлтый — отсюда
            // была грязно-бордовая "трава" вместо розовой. Сакура теперь красится честно (см. выше).
            sf::Color overlayTint = sf::Color::Transparent;
            if (block.type == BlockType::LEAVES_ACACIA || block.type == BlockType::LEAVES_SNOWY) {
                overlayTint = block.getLeafTint();
            } else if (block.type == BlockType::WOOD && world.biomeAt(x) == 3) {
                overlayTint = sf::Color(45, 32, 22); // снежный биом — тёмно-коричневый ствол
            } else if (block.type == BlockType::GLOWING_MOSS) {
                overlayTint = sf::Color(60, 200, 110); // светящийся мох — зелёный поверх камня
            }
            // Земля/трава везде обычная — биом виден только по деревьям (и лепесткам сакуры),
            // не по перекрашенной почве.
            if (overlayTint.a > 0) {
                sf::RectangleShape overlay({(float)BLOCK_SIZE + overlap * 2.f, (float)BLOCK_SIZE + overlap * 2.f});
                overlay.setPosition({px - overlap, py - overlap});
                overlay.setFillColor(sf::Color(overlayTint.r, overlayTint.g, overlayTint.b, 170));
                window.draw(overlay);
            }

            // Мох ещё и светится в темноте, как факел (используем ту же систему свечения)
            if (block.type == BlockType::GLOWING_MOSS) {
                lightSources.push_back({px + BLOCK_SIZE / 2.f, py + BLOCK_SIZE / 2.f});
            }

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
                    float flicker = 0.92f + 0.08f * std::sin(animTime * 10.f + (x * 13 + y * 7));
                    float mouthY = py + BLOCK_SIZE * 0.66f; // примерно где тёмное отверстие на текстуре печи
                    drawFlame(cx, mouthY, 0.55f, flicker);

                    lightSources.push_back({cx, py + BLOCK_SIZE / 2.f});
                }
            }

            // Травинки над блоком травы — покачиваются от "ветра", если сверху воздух.
            // В снежном биоме вместо травинок — снежная шапка сверху блока, причём
            // рисуем её всегда (даже под кроной дерева) — иначе под деревьями сквозь
            // снег проглядывала бы голая зелёная трава.
            bool topAir = !world.inBounds(x, y - 1) || !world.getBlock(x, y - 1).isSolid();
            bool isSnowBiome = world.biomeAt(x) == 3;

            if (block.type == BlockType::GRASS) {
                if (isSnowBiome) {
                    sf::RectangleShape snowCap({(float)BLOCK_SIZE + overlap * 2.f, BLOCK_SIZE * 0.28f});
                    snowCap.setPosition({px - overlap, py - overlap});
                    snowCap.setFillColor(sf::Color(240, 245, 250));
                    window.draw(snowCap);
                } else if (topAir) {
                    sf::Color bladeColor = sf::Color(80, 160, 30); // везде обычный зелёный — биом виден по деревьям

                    for (int b = 0; b < 3; b++) {
                        int h = x * 928371 + b * 5237 + 17; // детерминированный "хэш" для стабильной позиции травинки
                        float offsetX     = 4.f + b * 10.f + (float)((h >> 3) % 5);
                        float bladeHeight = 5.f + (float)((h >> 7) % 4);
                        float sway = std::sin(animTime * 3.f + x * 0.7f + b * 1.3f) * 1.6f;

                        sf::RectangleShape blade({2.f, bladeHeight});
                        blade.setFillColor(bladeColor);
                        blade.setPosition({px + offsetX + sway, py - bladeHeight + 2.f});
                        window.draw(blade);
                    }
                }
            }

            // В снежном биоме стволы и хвоя тоже припорошены снегом сверху
            if (isSnowBiome && topAir && (block.type == BlockType::WOOD || block.type == BlockType::LEAVES_SNOWY)) {
                sf::RectangleShape snowCap({(float)BLOCK_SIZE + overlap * 2.f, BLOCK_SIZE * 0.3f});
                snowCap.setPosition({px - overlap, py - overlap});
                snowCap.setFillColor(sf::Color(240, 245, 250, 230));
                window.draw(snowCap);
            }
        }
    }

    drawParticles();

    for (const auto& chicken : chickens)
        chicken.draw(window);

    for (const auto& cow : cows)
        cow.draw(window);

    for (const auto& s : sheep)
        s.draw(window);

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

        // Факелы пробивают тьму тёплым светом (аддитивное смешивание поверх затемнения) —
        // тот же градиент, что и у солнца/луны: одна текстура с плавной альфой, без колец.
        if (darkAlpha > 1.f && !lightSources.empty()) {
            sf::RenderStates addStates;
            addStates.blendMode = sf::BlendAdd;
            float k = darkAlpha / 165.f; // сила свечения растёт вместе с темнотой

            float radius = 42.f; // насколько далеко расходится свет одного факела/печи
            float scale = radius / 128.f;
            for (const auto& src : lightSources) {
                sf::Sprite glowSprite(glowTexture);
                glowSprite.setOrigin({128.f, 128.f});
                glowSprite.setScale({scale, scale});
                glowSprite.setPosition(src);
                glowSprite.setColor(sf::Color(255, 180, 90, (std::uint8_t)(200.f * k)));
                window.draw(glowSprite, addStates);
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