#include "Game.hpp"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <cstdint>
#include <cmath>
#include <cstdlib>
#include <random>

// Меню (главный экран, выбор мира) и сохранение/загрузка игры.
// Вынесено из Game.cpp, чтобы тот не разрастался ещё больше.

void Game::startNewWorld() {
    // Полностью новый случайный мир — пересоздаём World (свежий случайный сид)
    world = World();

    // Ставим игрока на реальную поверхность в колонке спавна (а не на фиксированную высоту,
    // иначе можно оказаться внутри камня/пещеры или на кроне дерева).
    int scx = WORLD_WIDTH / 2;
    int surfaceRow = findGroundSurface(scx);
    spawnX = scx * (float)BLOCK_SIZE;
    spawnY = (surfaceRow - 3) * (float)BLOCK_SIZE; // на 3 блока выше поверхности (высота игрока + запас)
    player.respawn(spawnX, spawnY);

    dayTime = DAY_LENGTH * 0.25f;
    hud.setHealth(10);
    hud.setFood(10);

    chickens.clear(); cows.clear(); sheep.clear(); enemies.clear();
    chestStorage.clear(); furnaceStorage.clear();
    activeTNT.clear(); particles.clear(); arrows.clear();

    // Спавним несколько цыплят на поверхности в случайных местах мира
    for (int i = 0; i < MAX_CHICKENS; i++)
        spawnChicken();
    chickenRespawnTimer = 15.f + rand() % 15;

    // И несколько коров рядом с ними
    for (int i = 0; i < MAX_COWS; i++)
        spawnCow();
    cowRespawnTimer = 20.f + rand() % 15;

    // И овец
    for (int i = 0; i < MAX_SHEEP; i++)
        spawnSheep();
    sheepRespawnTimer = 20.f + rand() % 15;

    sounds.stopMenuMusic();
    state = GameState::Playing;
}

bool Game::hasSaveFile() const {
    std::ifstream f(SAVE_FILE, std::ios::binary);
    return f.good();
}

void Game::saveGame() const {
    std::ofstream f(SAVE_FILE, std::ios::binary | std::ios::trunc);
    if (!f) { std::cerr << "Не удалось сохранить игру в " << SAVE_FILE << std::endl; return; }

    const char magic[8] = {'M', 'C', '2', 'D', 'S', 'A', 'V', '1'};
    f.write(magic, 8);

    unsigned int seed = world.getSeed();
    f.write((const char*)&seed, sizeof(seed));

    // Блоки мира — целиком, по одному байту на клетку (типов блоков сильно меньше 256)
    for (int y = 0; y < WORLD_HEIGHT; y++) {
        for (int x = 0; x < WORLD_WIDTH; x++) {
            std::uint8_t t = (std::uint8_t)world.getBlock(x, y).type;
            f.write((const char*)&t, 1);
        }
    }

    // Игрок и общее состояние
    float px = player.getX(), py = player.getY();
    f.write((const char*)&px, sizeof(px));
    f.write((const char*)&py, sizeof(py));
    int health = hud.getHealth(), food = hud.getFood();
    f.write((const char*)&health, sizeof(health));
    f.write((const char*)&food, sizeof(food));
    f.write((const char*)&spawnX, sizeof(spawnX));
    f.write((const char*)&spawnY, sizeof(spawnY));
    f.write((const char*)&dayTime, sizeof(dayTime));

    // Личный инвентарь
    int slotCount = inventory.getSlotCount();
    f.write((const char*)&slotCount, sizeof(slotCount));
    for (int i = 0; i < slotCount; i++) {
        std::uint8_t t = (std::uint8_t)inventory.getSlotType(i);
        int amount = inventory.getSlotAmount(i);
        f.write((const char*)&t, 1);
        f.write((const char*)&amount, sizeof(amount));
    }

    // Сундуки
    int chestCount = (int)chestStorage.size();
    f.write((const char*)&chestCount, sizeof(chestCount));
    for (const auto& kv : chestStorage) {
        int bx = kv.first.first, by = kv.first.second;
        f.write((const char*)&bx, sizeof(bx));
        f.write((const char*)&by, sizeof(by));
        int n = (int)kv.second.size();
        f.write((const char*)&n, sizeof(n));
        for (const auto& slot : kv.second) {
            std::uint8_t t = (std::uint8_t)slot.type;
            f.write((const char*)&t, 1);
            f.write((const char*)&slot.count, sizeof(slot.count));
        }
    }

    // Печи
    int furnaceCount = (int)furnaceStorage.size();
    f.write((const char*)&furnaceCount, sizeof(furnaceCount));
    for (const auto& kv : furnaceStorage) {
        int bx = kv.first.first, by = kv.first.second;
        f.write((const char*)&bx, sizeof(bx));
        f.write((const char*)&by, sizeof(by));
        const FurnaceState& fs = kv.second;
        for (const auto& slot : fs.slots) {
            std::uint8_t t = (std::uint8_t)slot.type;
            f.write((const char*)&t, 1);
            f.write((const char*)&slot.count, sizeof(slot.count));
        }
        f.write((const char*)&fs.burnTimeLeft, sizeof(fs.burnTimeLeft));
        f.write((const char*)&fs.burnTimeMax, sizeof(fs.burnTimeMax));
        f.write((const char*)&fs.cookProgress, sizeof(fs.cookProgress));
    }
}

bool Game::loadGame() {
    std::ifstream f(SAVE_FILE, std::ios::binary);
    if (!f) return false;

    char magic[8];
    f.read(magic, 8);
    if (!f || std::string(magic, 8) != "MC2DSAV1") {
        std::cerr << "Файл сохранения повреждён или другой версии" << std::endl;
        return false;
    }

    unsigned int seed = 0;
    f.read((char*)&seed, sizeof(seed));
    world = World(seed); // пересоздаём мир с тем же сидом (только чтобы взять правильный размер), затем перезапишем блоки

    for (int y = 0; y < WORLD_HEIGHT; y++) {
        for (int x = 0; x < WORLD_WIDTH; x++) {
            std::uint8_t t = 0;
            f.read((char*)&t, 1);
            world.setBlock(x, y, (BlockType)t);
        }
    }

    float px = 0.f, py = 0.f;
    f.read((char*)&px, sizeof(px));
    f.read((char*)&py, sizeof(py));
    player.respawn(px, py);

    int health = 10, food = 10;
    f.read((char*)&health, sizeof(health));
    f.read((char*)&food, sizeof(food));
    hud.setHealth(health);
    hud.setFood(food);

    f.read((char*)&spawnX, sizeof(spawnX));
    f.read((char*)&spawnY, sizeof(spawnY));
    f.read((char*)&dayTime, sizeof(dayTime));

    int slotCount = 0;
    f.read((char*)&slotCount, sizeof(slotCount));
    for (int i = 0; i < slotCount; i++) {
        std::uint8_t t = 0; int amount = 0;
        f.read((char*)&t, 1);
        f.read((char*)&amount, sizeof(amount));
        inventory.setSlot(i, (BlockType)t, amount);
    }

    chestStorage.clear();
    int chestCount = 0;
    f.read((char*)&chestCount, sizeof(chestCount));
    for (int c = 0; c < chestCount; c++) {
        int bx = 0, by = 0, n = 0;
        f.read((char*)&bx, sizeof(bx));
        f.read((char*)&by, sizeof(by));
        f.read((char*)&n, sizeof(n));
        std::vector<InventorySlot> slots;
        for (int i = 0; i < n; i++) {
            std::uint8_t t = 0; int count = 0;
            f.read((char*)&t, 1);
            f.read((char*)&count, sizeof(count));
            slots.push_back(InventorySlot((BlockType)t, count));
        }
        chestStorage[{bx, by}] = slots;
    }

    furnaceStorage.clear();
    int furnaceCount = 0;
    f.read((char*)&furnaceCount, sizeof(furnaceCount));
    for (int c = 0; c < furnaceCount; c++) {
        int bx = 0, by = 0;
        f.read((char*)&bx, sizeof(bx));
        f.read((char*)&by, sizeof(by));
        FurnaceState fs;
        for (int i = 0; i < 3; i++) {
            std::uint8_t t = 0; int count = 0;
            f.read((char*)&t, 1);
            f.read((char*)&count, sizeof(count));
            fs.slots[i] = InventorySlot((BlockType)t, count);
        }
        f.read((char*)&fs.burnTimeLeft, sizeof(fs.burnTimeLeft));
        f.read((char*)&fs.burnTimeMax, sizeof(fs.burnTimeMax));
        f.read((char*)&fs.cookProgress, sizeof(fs.cookProgress));
        furnaceStorage[{bx, by}] = fs;
    }

    // Мобов не сохраняем — они возобновляемые, просто спавним заново
    chickens.clear(); cows.clear(); sheep.clear(); enemies.clear();
    activeTNT.clear(); particles.clear(); arrows.clear();
    for (int i = 0; i < MAX_CHICKENS; i++) spawnChicken();
    chickenRespawnTimer = 15.f + rand() % 15;
    for (int i = 0; i < MAX_COWS; i++) spawnCow();
    cowRespawnTimer = 20.f + rand() % 15;
    for (int i = 0; i < MAX_SHEEP; i++) spawnSheep();
    sheepRespawnTimer = 20.f + rand() % 15;

    sounds.stopMenuMusic();
    state = GameState::Playing;
    return true;
}

void Game::renderMainMenu() {
    window.clear(sf::Color(28, 28, 36));
    sf::View ui(sf::FloatRect({0.f, 0.f}, {800.f, 600.f}));
    window.setView(ui);

    if (menuBgLoaded) {
        sf::Sprite bg(menuBgTexture);
        window.draw(bg);
        // Лёгкое затемнение поверх — чтобы заголовок и кнопки не терялись на фоне
        sf::RectangleShape dim({800.f, 600.f});
        dim.setFillColor(sf::Color(0, 0, 0, 90));
        window.draw(dim);
    }

    // Заголовок: розовый текст с тёмной обводкой
    if (fontLoaded) {
        sf::Text title(font, "Minecraft 2D", 52);
        title.setFillColor(sf::Color(235, 110, 180));
        title.setOutlineColor(sf::Color(75, 25, 50));
        title.setOutlineThickness(4.f);
        sf::FloatRect tb = title.getLocalBounds();
        title.setPosition({400.f - tb.size.x / 2.f - tb.position.x, 140.f});
        window.draw(title);
    } else {
        sf::RectangleShape titlePlate({420.f, 70.f});
        titlePlate.setPosition({190.f, 140.f});
        titlePlate.setFillColor(sf::Color(60, 60, 20));
        window.draw(titlePlate);
    }

    // Кнопка "Играть"
    playButtonBox = sf::FloatRect({300.f, 320.f}, {200.f, 58.f});
    sf::RectangleShape btn({200.f, 58.f});
    btn.setPosition({300.f, 320.f});
    btn.setFillColor(sf::Color(95, 95, 95));
    btn.setOutlineColor(sf::Color(20, 20, 20));
    btn.setOutlineThickness(2.f);
    window.draw(btn);
    sf::RectangleShape btnHighlight({196.f, 4.f});
    btnHighlight.setPosition({302.f, 322.f});
    btnHighlight.setFillColor(sf::Color(140, 140, 140));
    window.draw(btnHighlight);

    if (fontLoaded) {
        sf::Text label(font, "Play", 28);
        label.setFillColor(sf::Color::White);
        sf::FloatRect lb = label.getLocalBounds();
        label.setPosition({400.f - lb.size.x / 2.f - lb.position.x, 349.f - lb.size.y / 2.f - lb.position.y});
        window.draw(label);
    }

    window.display();
}

void Game::renderWorldMenu() {
    window.clear(sf::Color(28, 28, 36));
    sf::View ui(sf::FloatRect({0.f, 0.f}, {800.f, 600.f}));
    window.setView(ui);

    if (menuBgLoaded) {
        sf::Sprite bg(menuBgTexture);
        window.draw(bg);
        sf::RectangleShape dim({800.f, 600.f});
        dim.setFillColor(sf::Color(0, 0, 0, 90));
        window.draw(dim);
    }

    if (fontLoaded) {
        sf::Text title(font, "Select World", 34);
        title.setFillColor(sf::Color(220, 220, 220));
        sf::FloatRect tb = title.getLocalBounds();
        title.setPosition({400.f - tb.size.x / 2.f - tb.position.x, 150.f});
        window.draw(title);
    }

    bool hasSave = hasSaveFile();

    auto drawButton = [&](sf::FloatRect box, const char* text, bool enabled) {
        sf::RectangleShape btn({box.size.x, box.size.y});
        btn.setPosition(box.position);
        btn.setFillColor(enabled ? sf::Color(95, 95, 95) : sf::Color(55, 55, 55));
        btn.setOutlineColor(sf::Color(20, 20, 20));
        btn.setOutlineThickness(2.f);
        window.draw(btn);
        if (fontLoaded) {
            sf::Text label(font, text, 24);
            label.setFillColor(enabled ? sf::Color::White : sf::Color(140, 140, 140));
            sf::FloatRect lb = label.getLocalBounds();
            label.setPosition({box.position.x + box.size.x / 2.f - lb.size.x / 2.f - lb.position.x,
                                box.position.y + box.size.y / 2.f - lb.size.y / 2.f - lb.position.y});
            window.draw(label);
        }
    };

    continueButtonBox = sf::FloatRect({275.f, 260.f}, {250.f, 58.f});
    drawButton(continueButtonBox, "Continue", hasSave);

    newWorldButtonBox = sf::FloatRect({275.f, 335.f}, {250.f, 58.f});
    drawButton(newWorldButtonBox, "New World", true);

    window.display();
}

void Game::handleMenuClick(sf::Vector2i pixelPos) {
    sf::View uiView(sf::FloatRect({0.f, 0.f}, {800.f, 600.f}));
    sf::Vector2f p = window.mapPixelToCoords(pixelPos, uiView);

    if (state == GameState::MainMenu) {
        if (playButtonBox.contains(p)) {
            sounds.playClick();
            state = GameState::WorldMenu;
        }
    } else if (state == GameState::WorldMenu) {
        if (hasSaveFile() && continueButtonBox.contains(p)) {
            sounds.playClick();
            if (!loadGame()) startNewWorld(); // повреждённый файл — не страшно, начинаем новый мир
        } else if (newWorldButtonBox.contains(p)) {
            sounds.playClick();
            startNewWorld();
        }
    }
}
