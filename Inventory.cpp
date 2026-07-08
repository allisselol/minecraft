#include "Inventory.hpp"
#include "HUD.hpp"
#include <algorithm>
#include <string>
#include <cstdint>

Inventory::Inventory() : selectedSlot(0) {
    // Настоящий survival: инвентарь стартует пустым, всё нужно добывать самой
    for (int i = 0; i < 9; i++)
        slots.emplace_back(BlockType::AIR, 0);

    // Рецепты крафта (свои, не майнкрафтовские): "безусловные" —
    // важно только суммарное количество ингредиентов, а не их положение в сетке.
    recipes.push_back({ {{BlockType::WOOD, 1}, {BlockType::COAL_ORE, 1}}, BlockType::TORCH, 4 });
    recipes.push_back({ {{BlockType::STONE, 2}, {BlockType::DIRT, 2}}, BlockType::TNT,   1 });
    // Из руды покрепче — более мощная взрывчатка (больше TNT за раз)
    recipes.push_back({ {{BlockType::COAL_ORE, 2}, {BlockType::SAND, 2}}, BlockType::TNT, 2 });
    // Строительные блоки из добытых материалов
    recipes.push_back({ {{BlockType::WOOD, 1}}, BlockType::PLANKS, 4 });                          // 1 бревно -> 4 доски
    recipes.push_back({ {{BlockType::STONE, 2}, {BlockType::DIRT, 1}}, BlockType::BRICK, 2 });    // кирпич
    recipes.push_back({ {{BlockType::SAND, 4}}, BlockType::SANDSTONE, 1 });                        // 4 песка -> песчаник
    recipes.push_back({ {{BlockType::WOOD, 3}, {BlockType::LEAVES, 1}}, BlockType::BOOKSHELF, 1 }); // книжная полка
    recipes.push_back({ {{BlockType::STONE, 4}, {BlockType::COAL_ORE, 1}}, BlockType::OBSIDIAN, 1 }); // обсидиан
    recipes.push_back({ {{BlockType::SAND, 2}, {BlockType::LEAVES, 2}}, BlockType::PUMPKIN, 1 });   // тыква
}

void Inventory::selectSlot(int index) {
    if (index >= 0 && index < (int)slots.size())
        selectedSlot = index;
}
void Inventory::nextSlot() { selectedSlot = (selectedSlot+1) % slots.size(); }
void Inventory::prevSlot() { selectedSlot = (selectedSlot-1+(int)slots.size()) % slots.size(); }

BlockType Inventory::getSelectedType() const {
    return slots[selectedSlot].type;
}

bool Inventory::hasSelected() const {
    return selectedSlot >= 0 && selectedSlot < (int)slots.size()
        && slots[selectedSlot].type != BlockType::AIR
        && slots[selectedSlot].count > 0;
}

void Inventory::consumeSelected() {
    if (!hasSelected()) return;
    slots[selectedSlot].count--;
    if (slots[selectedSlot].count <= 0) {
        slots[selectedSlot].count = 0;
        slots[selectedSlot].type = BlockType::AIR;
    }
}

void Inventory::addItem(BlockType type, int amount) {
    if (type == BlockType::AIR || amount <= 0) return;

    // Сначала докладываем в уже существующие стопки того же типа
    for (auto& slot : slots) {
        if (amount <= 0) break;
        if (slot.type == type && slot.count < 64) {
            int space = 64 - slot.count;
            int add = std::min(space, amount);
            slot.count += add;
            amount -= add;
        }
    }
    // Оставшееся кладём в первый свободный слот
    for (auto& slot : slots) {
        if (amount <= 0) break;
        if (slot.type == BlockType::AIR || slot.count <= 0) {
            slot.type = type;
            slot.count = std::min(64, amount);
            amount -= slot.count;
        }
    }
    // Если инвентарь совсем полон — лишнее просто теряется (как и в майнкрафте при переполнении)
}

bool Inventory::canCraft(const Recipe& r) const {
    for (const auto& ing : r.ingredients) {
        int total = 0;
        for (const auto& slot : slots)
            if (slot.type == ing.first) total += slot.count;
        if (total < ing.second) return false;
    }
    return true;
}

void Inventory::craftRecipe(const Recipe& r) {
    for (const auto& ing : r.ingredients) {
        int remaining = ing.second;
        for (auto& slot : slots) {
            if (remaining <= 0) break;
            if (slot.type != ing.first) continue;
            int take = std::min(slot.count, remaining);
            slot.count -= take;
            remaining -= take;
            if (slot.count <= 0) {
                slot.count = 0;
                slot.type = BlockType::AIR;
            }
        }
    }
    addItem(r.result, r.resultCount);
}

bool Inventory::handleCraftClick(sf::Vector2i pixelPos) {
    sf::Vector2f p((float)pixelPos.x, (float)pixelPos.y);
    for (size_t i = 0; i < recipeHitboxes.size() && i < recipes.size(); i++) {
        if (recipeHitboxes[i].contains(p)) {
            if (canCraft(recipes[i])) {
                craftRecipe(recipes[i]);
                return true;
            }
            return false;
        }
    }
    return false;
}

void Inventory::drawIcon(sf::RenderWindow& window, BlockType type, float sx, float sy, float size) {
    if (type == BlockType::AIR) return;

    // Факел рисуем процедурно (в атласе его нет) — палочка и пламя
    if (type == BlockType::TORCH) {
        float cx = sx + size / 2.f;
        sf::RectangleShape stick({size * 0.14f, size * 0.55f});
        stick.setFillColor(sf::Color(120, 80, 40));
        stick.setPosition({cx - size * 0.07f, sy + size * 0.45f});
        window.draw(stick);
        sf::CircleShape flame(size * 0.18f);
        flame.setFillColor(sf::Color(255, 150, 40));
        flame.setOrigin({size * 0.18f, size * 0.18f});
        flame.setPosition({cx, sy + size * 0.4f});
        window.draw(flame);
        sf::CircleShape core(size * 0.09f);
        core.setFillColor(sf::Color(255, 230, 140));
        core.setOrigin({size * 0.09f, size * 0.09f});
        core.setPosition({cx, sy + size * 0.4f});
        window.draw(core);
        return;
    }

    // Если атлас доступен — рисуем реальную текстуру блока (как в мире)
    if (blockTexture) {
        sf::Sprite icon(*blockTexture);
        icon.setTextureRect(Block(type).getTextureRect());
        float scale = size / (float)Block::TEXTURE_SIZE;
        icon.setScale({scale, scale});
        icon.setPosition({sx, sy});
        if (type == BlockType::WATER) icon.setColor(sf::Color(255, 255, 255, 200));
        window.draw(icon);
        return;
    }

    // Запасной вариант — цветной квадрат
    sf::Color c = Block(type).getColor();
    sf::RectangleShape iconR({size, size});
    iconR.setPosition({sx, sy});
    iconR.setFillColor(c);
    window.draw(iconR);

    sf::RectangleShape it({size, 3.f});
    it.setPosition({sx, sy});
    it.setFillColor({(uint8_t)std::min(255, c.r + 70), (uint8_t)std::min(255, c.g + 70), (uint8_t)std::min(255, c.b + 70)});
    window.draw(it);

    sf::RectangleShape ir({3.f, size});
    ir.setPosition({sx + size - 3, sy});
    ir.setFillColor({(uint8_t)std::max(0, c.r - 50), (uint8_t)std::max(0, c.g - 50), (uint8_t)std::max(0, c.b - 50)});
    window.draw(ir);
}

void Inventory::drawNumber(sf::RenderWindow& window, int number, float rx, float ry, float digitH) {
    // Рисуем число маленькими "пиксельными" цифрами 3x5 сегментов, без шрифта.
    // Каждая цифра — карта 3(шир)x5(выс) точек.
    static const uint8_t glyphs[10][5] = {
        {0b111,0b101,0b101,0b101,0b111}, // 0
        {0b010,0b110,0b010,0b010,0b111}, // 1
        {0b111,0b001,0b111,0b100,0b111}, // 2
        {0b111,0b001,0b111,0b001,0b111}, // 3
        {0b101,0b101,0b111,0b001,0b001}, // 4
        {0b111,0b100,0b111,0b001,0b111}, // 5
        {0b111,0b100,0b111,0b101,0b111}, // 6
        {0b111,0b001,0b010,0b010,0b010}, // 7
        {0b111,0b101,0b111,0b101,0b111}, // 8
        {0b111,0b101,0b111,0b001,0b111}, // 9
    };

    std::string s = std::to_string(number);
    float dot = digitH / 5.f;          // размер одной точки
    float digitW = 3.f * dot;
    float gap = dot;                    // промежуток между цифрами
    float totalW = s.size() * digitW + (s.size() - 1) * gap;
    // Выравниваем по правому краю (rx — правая граница)
    float startX = rx - totalW;

    for (size_t k = 0; k < s.size(); k++) {
        int d = s[k] - '0';
        float gx = startX + k * (digitW + gap);
        for (int row = 0; row < 5; row++) {
            for (int col = 0; col < 3; col++) {
                if (glyphs[d][row] & (1 << (2 - col))) {
                    // тёмная "тень" со сдвигом для читаемости на любом фоне
                    sf::RectangleShape sh({dot + 0.5f, dot + 0.5f});
                    sh.setPosition({gx + col * dot + 1.f, ry + row * dot + 1.f});
                    sh.setFillColor(sf::Color(0, 0, 0, 200));
                    window.draw(sh);
                    sf::RectangleShape p({dot + 0.5f, dot + 0.5f});
                    p.setPosition({gx + col * dot, ry + row * dot});
                    p.setFillColor(sf::Color(255, 255, 255));
                    window.draw(p);
                }
            }
        }
    }
}

void Inventory::drawSlot(sf::RenderWindow& window, int i, float sx, float sy, bool selected) {
    float S = (float)SLOT_SIZE;

    // === Стиль референса: тёмный слот с тонкой светло-серой рамкой ===
    sf::RectangleShape frame({S, S});
    frame.setPosition({sx, sy});
    frame.setFillColor(sf::Color(150, 150, 150));
    window.draw(frame);

    sf::RectangleShape inner({S - 4, S - 4});
    inner.setPosition({sx + 2, sy + 2});
    inner.setFillColor(sf::Color(45, 45, 45, 240));
    window.draw(inner);

    if (i < (int)slots.size() && slots[i].type != BlockType::AIR && slots[i].count > 0) {
        float pad = 8.f;
        drawIcon(window, slots[i].type, sx + pad, sy + pad, S - pad * 2);
        // Число количества — в правом нижнем углу слота (показываем, если больше 1)
        if (slots[i].count > 1) {
            drawNumber(window, slots[i].count, sx + S - 4.f, sy + S - 14.f, 10.f);
        }
    }

    if (selected) {
        sf::RectangleShape sel({S + 4, S + 4});
        sel.setPosition({sx - 2, sy - 2});
        sel.setFillColor(sf::Color::Transparent);
        sel.setOutlineColor(sf::Color(255, 255, 255));
        sel.setOutlineThickness(3.f);
        window.draw(sel);
    }
}

void Inventory::drawRecipeRow(sf::RenderWindow& window, const Recipe& recipe, float rx, float ry, bool craftable) {
    const float cellSize = 24.f;
    const float cellGap  = 2.f;

    // Раскладываем ингредиенты по 2x2 сетке: каждый ингредиент занимает
    // столько ячеек, сколько его нужно по рецепту (максимум 4 ячейки всего)
    BlockType cells[4] = { BlockType::AIR, BlockType::AIR, BlockType::AIR, BlockType::AIR };
    int idx = 0;
    for (const auto& ing : recipe.ingredients)
        for (int k = 0; k < ing.second && idx < 4; k++)
            cells[idx++] = ing.first;

    for (int i = 0; i < 4; i++) {
        int col = i % 2, row = i / 2;
        float cx = rx + col * (cellSize + cellGap);
        float cy = ry + row * (cellSize + cellGap);

        sf::RectangleShape bg({cellSize, cellSize});
        bg.setPosition({cx, cy});
        bg.setFillColor(sf::Color(45, 45, 45, 240));
        bg.setOutlineColor(sf::Color(120, 120, 120));
        bg.setOutlineThickness(1.f);
        window.draw(bg);

        if (cells[i] != BlockType::AIR)
            drawIcon(window, cells[i], cx + 3.f, cy + 3.f, cellSize - 6.f);
    }

    // Стрелка результата
    float gridW = 2 * cellSize + cellGap;
    float gridH = 2 * cellSize + cellGap;
    float arrowX = rx + gridW + 14.f;
    float arrowY = ry + gridH / 2.f;

    sf::ConvexShape arrow(3);
    arrow.setPoint(0, {arrowX, arrowY - 8.f});
    arrow.setPoint(1, {arrowX, arrowY + 8.f});
    arrow.setPoint(2, {arrowX + 12.f, arrowY});
    arrow.setFillColor(craftable ? sf::Color(230, 230, 230) : sf::Color(90, 90, 90));
    window.draw(arrow);

    // Итог крафта
    float resultSize = 36.f;
    float resX = arrowX + 26.f;
    float resY = ry + (gridH - resultSize) / 2.f;

    sf::RectangleShape resBg({resultSize, resultSize});
    resBg.setPosition({resX, resY});
    resBg.setFillColor(sf::Color(45, 45, 45, 240));
    resBg.setOutlineColor(craftable ? sf::Color(240, 210, 90) : sf::Color(110, 110, 110));
    resBg.setOutlineThickness(2.f);
    window.draw(resBg);

    drawIcon(window, recipe.result, resX + 5.f, resY + 5.f, resultSize - 10.f);

    // Если ингредиентов не хватает — слегка затемняем всю строку полупрозрачным слоем
    float rowW = resX + resultSize - rx;
    float rowH = gridH;
    if (!craftable) {
        sf::RectangleShape dim({rowW, rowH});
        dim.setPosition({rx, ry});
        dim.setFillColor(sf::Color(0, 0, 0, 90));
        window.draw(dim);
    }

    recipeHitboxes.push_back(sf::FloatRect({rx, ry}, {rowW, rowH}));
}

void Inventory::drawCraftingSection(sf::RenderWindow& window, float x, float y) {
    recipeHitboxes.clear();
    float rowGap = 10.f;
    float rowH = 2 * 24.f + 2.f;              // высота одной строки рецепта (под cellSize=24)
    float colGap = 30.f;
    float colW = 200.f;                        // ширина колонки рецепта
    int perColumn = (int)recipes.size() >= 4 ? (int(recipes.size()) + 1) / 2 : (int)recipes.size();

    for (size_t i = 0; i < recipes.size(); i++) {
        int col = (int)i / perColumn;          // 0 — левая колонка, 1 — правая
        int row = (int)i % perColumn;
        float rx = x + col * (colW + colGap);
        float ry = y + row * (rowH + rowGap);
        drawRecipeRow(window, recipes[i], rx, ry, canCraft(recipes[i]));
    }
}

void Inventory::draw(sf::RenderWindow& window) {
    sf::View ui(sf::FloatRect({0.f, 0.f}, {800.f, 600.f}));
    sf::View prev = window.getView();
    window.setView(ui);

    int n = (int)slots.size(); // 9 слотов
    float slotGap = 2.f;
    float totalW = n * SLOT_SIZE + (n - 1) * slotGap;
    float startX = (800.f - totalW) / 2.f;

    float panelH = HUD::getPanelH();
    float panelY = HUD::getPanelY();
    float slotY  = panelY + panelH - SLOT_SIZE - 3.f;

    for (int i = 0; i < n; i++) {
        float sx = startX + i * (SLOT_SIZE + slotGap);
        drawSlot(window, i, sx, slotY, i == selectedSlot);
    }

    window.setView(prev);
}

void Inventory::drawFullScreen(sf::RenderWindow& window) {
    sf::View ui(sf::FloatRect({0.f, 0.f}, {800.f, 600.f}));
    sf::View prev = window.getView();
    window.setView(ui);

    sf::RectangleShape overlay({800.f, 600.f});
    overlay.setFillColor(sf::Color(0, 0, 0, 160));
    window.draw(overlay);

    float winW = 580.f, winH = 490.f;
    float winX = (800.f - winW) / 2.f, winY = (600.f - winH) / 2.f;

    // Серое окно как в майнкрафте
    sf::RectangleShape bg({winW, winH});
    bg.setFillColor(sf::Color(198, 198, 198));
    bg.setOutlineColor(sf::Color(30, 30, 30));
    bg.setOutlineThickness(3.f);
    bg.setPosition({winX, winY});
    window.draw(bg);

    sf::RectangleShape wl({winW, 3.f});
    wl.setPosition({winX, winY});
    wl.setFillColor(sf::Color(255, 255, 255, 180));
    window.draw(wl);

    sf::RectangleShape wd({winW, 3.f});
    wd.setPosition({winX, winY + winH - 3});
    wd.setFillColor(sf::Color(85, 85, 85));
    window.draw(wd);

    // Кнопка-крестик в правом верхнем углу (как в референсе)
    float xbSize = 26.f;
    float xbX = winX + winW - xbSize - 10.f;
    float xbY = winY + 10.f;
    closeButtonBox = sf::FloatRect({xbX, xbY}, {xbSize, xbSize});

    sf::RectangleShape xbBg({xbSize, xbSize});
    xbBg.setPosition({xbX, xbY});
    xbBg.setFillColor(sf::Color(170, 170, 170));
    xbBg.setOutlineColor(sf::Color(90, 90, 90));
    xbBg.setOutlineThickness(2.f);
    window.draw(xbBg);

    // Сам крестик (две диагональные линии)
    auto drawXLine = [&](sf::Vector2f a, sf::Vector2f b) {
        sf::Vector2f d = b - a;
        float len = std::sqrt(d.x * d.x + d.y * d.y);
        sf::RectangleShape line({len, 3.f});
        line.setOrigin({0.f, 1.5f});
        line.setPosition(a);
        line.setRotation(sf::degrees(std::atan2(d.y, d.x) * 180.f / 3.14159265f));
        line.setFillColor(sf::Color(150, 40, 40));
        window.draw(line);
    };
    float pad = 7.f;
    drawXLine({xbX + pad, xbY + pad}, {xbX + xbSize - pad, xbY + xbSize - pad});
    drawXLine({xbX + xbSize - pad, xbY + pad}, {xbX + pad, xbY + xbSize - pad});

    int cols = 5;
    float slotGap = 4.f;
    float gx = winX + (winW - cols * (SLOT_SIZE + slotGap) + slotGap) / 2.f;
    float gy = winY + 30.f;

    for (int i = 0; i < (int)slots.size(); i++) {
        int col = i % cols, row = i / cols;
        drawSlot(window, i, gx + col * (SLOT_SIZE + slotGap), gy + row * (SLOT_SIZE + slotGap), i == selectedSlot);
    }

    // Разделительная линия между инвентарём и крафтом
    float sepY = gy + 2 * (SLOT_SIZE + slotGap) + 10.f;
    sf::RectangleShape sep({winW - 40.f, 2.f});
    sep.setPosition({winX + 20.f, sepY});
    sep.setFillColor(sf::Color(150, 150, 150));
    window.draw(sep);

    drawCraftingSection(window, winX + 40.f, sepY + 16.f);

    window.setView(prev);
}