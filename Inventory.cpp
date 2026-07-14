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

    // Верстак крафтится из досок в обычном инвентаре — как и в оригинале, без него
    // нельзя открыть сетку 3x3, но сам стол ставится из простого рецепта.
    recipes.push_back({ {{BlockType::PLANKS, 4}}, BlockType::WORKBENCH, 1 });

    // Сетка верстака (3x3) — стартует пустой, игрок сам раскладывает по ней предметы
    craftGrid.assign(9, InventorySlot(BlockType::AIR, 0));

    // Рецепты верстака — здесь, в отличие от recipes[], важно точное расположение
    // ингредиентов в сетке 3x3. Индексы pattern[row][col], AIR = ячейка должна быть пустой.
    {
        // Сундук: кольцо из 8 досок вокруг пустой середины — как в оригинале
        ShapedRecipe chest{};
        BlockType ring[3][3] = {
            { BlockType::PLANKS, BlockType::PLANKS, BlockType::PLANKS },
            { BlockType::PLANKS, BlockType::AIR,     BlockType::PLANKS },
            { BlockType::PLANKS, BlockType::PLANKS, BlockType::PLANKS },
        };
        for (int r = 0; r < 3; r++)
            for (int c = 0; c < 3; c++)
                chest.pattern[r][c] = ring[r][c];
        chest.result = BlockType::CHEST;
        chest.resultCount = 1;
        shapedRecipes.push_back(chest);
    }
    {
        // Печь: то же кольцо, но из камня — как в оригинале
        ShapedRecipe furnace{};
        BlockType ring[3][3] = {
            { BlockType::STONE, BlockType::STONE, BlockType::STONE },
            { BlockType::STONE, BlockType::AIR,    BlockType::STONE },
            { BlockType::STONE, BlockType::STONE, BlockType::STONE },
        };
        for (int r = 0; r < 3; r++)
            for (int c = 0; c < 3; c++)
                furnace.pattern[r][c] = ring[r][c];
        furnace.result = BlockType::FURNACE;
        furnace.resultCount = 1;
        shapedRecipes.push_back(furnace);
    }
    {
        // Дверь: 2 колонки досок на всю высоту сетки — как в оригинале (6 досок -> 3 двери)
        ShapedRecipe door{};
        BlockType pattern[3][3] = {
            { BlockType::PLANKS, BlockType::PLANKS, BlockType::AIR },
            { BlockType::PLANKS, BlockType::PLANKS, BlockType::AIR },
            { BlockType::PLANKS, BlockType::PLANKS, BlockType::AIR },
        };
        for (int r = 0; r < 3; r++)
            for (int c = 0; c < 3; c++)
                door.pattern[r][c] = pattern[r][c];
        door.result = BlockType::DOOR;
        door.resultCount = 3;
        shapedRecipes.push_back(door);
    }

    // Лестница — рецепт простой (не требует верстака), пачкой по 3, как в оригинале
    recipes.push_back({ {{BlockType::PLANKS, 2}}, BlockType::LADDER, 3 });
    {
        // Кровать — теперь на верстаке: 3 шерсти сверху, 3 доски снизу, как в оригинале
        ShapedRecipe bed{};
        BlockType pattern[3][3] = {
            { BlockType::WOOL,   BlockType::WOOL,   BlockType::WOOL },
            { BlockType::PLANKS, BlockType::PLANKS, BlockType::PLANKS },
            { BlockType::AIR,    BlockType::AIR,    BlockType::AIR },
        };
        for (int r = 0; r < 3; r++)
            for (int c = 0; c < 3; c++)
                bed.pattern[r][c] = pattern[r][c];
        bed.result = BlockType::BED;
        bed.resultCount = 1;
        shapedRecipes.push_back(bed);
    }
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

void Inventory::transferSlot(InventorySlot& from, std::vector<InventorySlot>& to) {
    if (from.type == BlockType::AIR || from.count <= 0) return;
    BlockType type = from.type;
    int amount = from.count;

    // Сначала доукомплектовываем существующие стопки того же типа
    for (auto& slot : to) {
        if (amount <= 0) break;
        if (slot.type == type && slot.count < 64) {
            int space = 64 - slot.count;
            int add = std::min(space, amount);
            slot.count += add;
            amount -= add;
        }
    }
    // Оставшееся — в первый свободный слот
    for (auto& slot : to) {
        if (amount <= 0) break;
        if (slot.type == BlockType::AIR || slot.count <= 0) {
            slot.type = type;
            slot.count = std::min(64, amount);
            amount -= slot.count;
        }
    }

    // Что не поместилось — остаётся там, где было (ничего не теряем)
    from.count = amount;
    if (from.count <= 0) {
        from.count = 0;
        from.type = BlockType::AIR;
    }
}

void Inventory::drawChestScreen(sf::RenderWindow& window, std::vector<InventorySlot>& chestSlots) {
    chestSlotBoxes.clear();
    chestPlayerSlotBoxes.clear();

    sf::View ui(sf::FloatRect({0.f, 0.f}, {800.f, 600.f}));
    sf::View prev = window.getView();
    window.setView(ui);

    sf::RectangleShape overlay({800.f, 600.f});
    overlay.setFillColor(sf::Color(0, 0, 0, 160));
    window.draw(overlay);

    int cols = 9;
    int chestRows = ((int)chestSlots.size() + cols - 1) / cols;
    float slotGap = 4.f;
    float gridW = cols * (SLOT_SIZE + slotGap) - slotGap;

    float winW = gridW + 40.f;
    float winH = chestRows * (SLOT_SIZE + slotGap) + SLOT_SIZE + slotGap + 90.f;
    float winX = (800.f - winW) / 2.f, winY = (600.f - winH) / 2.f;

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

    // Кнопка-крестик
    float xbSize = 26.f;
    float xbX = winX + winW - xbSize - 10.f;
    float xbY = winY + 10.f;
    chestCloseButtonBox = sf::FloatRect({xbX, xbY}, {xbSize, xbSize});

    sf::RectangleShape xbBg({xbSize, xbSize});
    xbBg.setPosition({xbX, xbY});
    xbBg.setFillColor(sf::Color(170, 170, 170));
    xbBg.setOutlineColor(sf::Color(90, 90, 90));
    xbBg.setOutlineThickness(2.f);
    window.draw(xbBg);

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
    float xpad = 7.f;
    drawXLine({xbX + xpad, xbY + xpad}, {xbX + xbSize - xpad, xbY + xbSize - xpad});
    drawXLine({xbX + xbSize - xpad, xbY + xpad}, {xbX + xpad, xbY + xbSize - xpad});

    // Сетка сундука — сверху
    float gx = winX + (winW - gridW) / 2.f;
    float gy = winY + 30.f;
    for (int i = 0; i < (int)chestSlots.size(); i++) {
        int col = i % cols, row = i / cols;
        float sx = gx + col * (SLOT_SIZE + slotGap);
        float sy = gy + row * (SLOT_SIZE + slotGap);
        drawSlotGeneric(window, chestSlots[i].type, chestSlots[i].count, sx, sy, false);
        chestSlotBoxes.push_back(sf::FloatRect({sx, sy}, {(float)SLOT_SIZE, (float)SLOT_SIZE}));
    }

    // Разделитель
    float sepY = gy + chestRows * (SLOT_SIZE + slotGap) + 8.f;
    sf::RectangleShape sep({winW - 40.f, 2.f});
    sep.setPosition({winX + 20.f, sepY});
    sep.setFillColor(sf::Color(150, 150, 150));
    window.draw(sep);

    // Личные слоты игрока — снизу, одной строкой
    float py = sepY + 16.f;
    float px = winX + (winW - (9 * (SLOT_SIZE + slotGap) - slotGap)) / 2.f;
    for (int i = 0; i < (int)slots.size(); i++) {
        float sx = px + i * (SLOT_SIZE + slotGap);
        drawSlotGeneric(window, slots[i].type, slots[i].count, sx, py, i == selectedSlot);
        chestPlayerSlotBoxes.push_back(sf::FloatRect({sx, py}, {(float)SLOT_SIZE, (float)SLOT_SIZE}));
    }

    window.setView(prev);
}

bool Inventory::handleChestClick(sf::Vector2i pixelPos, std::vector<InventorySlot>& chestSlots) {
    sf::Vector2f p((float)pixelPos.x, (float)pixelPos.y);

    // Клик по слоту сундука -> забираем стек в инвентарь игрока
    for (size_t i = 0; i < chestSlotBoxes.size() && i < chestSlots.size(); i++) {
        if (chestSlotBoxes[i].contains(p)) {
            if (chestSlots[i].type == BlockType::AIR || chestSlots[i].count <= 0) return false;
            transferSlot(chestSlots[i], slots);
            return true;
        }
    }

    // Клик по личному слоту в этом окне -> кладём стек в сундук
    for (size_t i = 0; i < chestPlayerSlotBoxes.size() && i < slots.size(); i++) {
        if (chestPlayerSlotBoxes[i].contains(p)) {
            if (slots[i].type == BlockType::AIR || slots[i].count <= 0) return false;
            transferSlot(slots[i], chestSlots);
            return true;
        }
    }

    return false;
}

bool Inventory::matchShaped(const ShapedRecipe& r) const {
    BlockType g[3][3];
    for (int i = 0; i < 9; i++)
        g[i / 3][i % 3] = (craftGrid[i].count > 0) ? craftGrid[i].type : BlockType::AIR;

    int gMinR = 3, gMaxR = -1, gMinC = 3, gMaxC = -1;
    for (int rr = 0; rr < 3; rr++)
        for (int cc = 0; cc < 3; cc++)
            if (g[rr][cc] != BlockType::AIR) {
                gMinR = std::min(gMinR, rr); gMaxR = std::max(gMaxR, rr);
                gMinC = std::min(gMinC, cc); gMaxC = std::max(gMaxC, cc);
            }
    if (gMaxR < 0) return false; // сетка пуста

    int pMinR = 3, pMaxR = -1, pMinC = 3, pMaxC = -1;
    for (int rr = 0; rr < 3; rr++)
        for (int cc = 0; cc < 3; cc++)
            if (r.pattern[rr][cc] != BlockType::AIR) {
                pMinR = std::min(pMinR, rr); pMaxR = std::max(pMaxR, rr);
                pMinC = std::min(pMinC, cc); pMaxC = std::max(pMaxC, cc);
            }

    // Обрезанные до занятых ячеек прямоугольники должны совпасть по размеру...
    if ((gMaxR - gMinR) != (pMaxR - pMinR) || (gMaxC - gMinC) != (pMaxC - pMinC))
        return false;

    // ...и по содержимому — ячейка за ячейкой
    for (int i = 0; i <= gMaxR - gMinR; i++)
        for (int j = 0; j <= gMaxC - gMinC; j++)
            if (g[gMinR + i][gMinC + j] != r.pattern[pMinR + i][pMinC + j])
                return false;

    return true;
}

const ShapedRecipe* Inventory::findMatchingShaped() const {
    for (const auto& r : shapedRecipes)
        if (matchShaped(r))
            return &r;
    return nullptr;
}

void Inventory::craftFromGrid() {
    const ShapedRecipe* r = findMatchingShaped();
    if (!r) return;

    // Тратим по 1 предмету из каждой занятой ячейки — можно крафтить подряд,
    // пока в ячейках хватает материала (как повторные клики по результату в оригинале)
    for (auto& slot : craftGrid) {
        if (slot.type != BlockType::AIR && slot.count > 0) {
            slot.count--;
            if (slot.count <= 0) {
                slot.count = 0;
                slot.type = BlockType::AIR;
            }
        }
    }
    addItem(r->result, r->resultCount);
}

void Inventory::clickSlot(InventorySlot& slot, sf::Mouse::Button button) {
    bool heldEmpty = (heldItem.type == BlockType::AIR || heldItem.count <= 0);
    bool slotEmpty = (slot.type == BlockType::AIR || slot.count <= 0);

    if (button == sf::Mouse::Button::Left) {
        if (heldEmpty && !slotEmpty) {
            // забираем всю стопку на курсор
            heldItem = slot;
            slot.type = BlockType::AIR;
            slot.count = 0;
        } else if (!heldEmpty && slotEmpty) {
            // кладём всё, что на курсоре
            slot = heldItem;
            heldItem.type = BlockType::AIR;
            heldItem.count = 0;
        } else if (!heldEmpty && !slotEmpty && slot.type == heldItem.type) {
            // доукомплектовываем стопку в слоте
            int space = 64 - slot.count;
            int add = std::min(space, heldItem.count);
            slot.count += add;
            heldItem.count -= add;
            if (heldItem.count <= 0) { heldItem.count = 0; heldItem.type = BlockType::AIR; }
        } else if (!heldEmpty && !slotEmpty) {
            // разные предметы — меняем местами
            std::swap(slot, heldItem);
        }
        // heldEmpty && slotEmpty -> нечего делать
    } else if (button == sf::Mouse::Button::Right) {
        if (heldEmpty && !slotEmpty) {
            // забираем половину стопки (округляя забираемую часть вверх)
            int take = (slot.count + 1) / 2;
            heldItem = InventorySlot(slot.type, take);
            slot.count -= take;
            if (slot.count <= 0) { slot.count = 0; slot.type = BlockType::AIR; }
        } else if (!heldEmpty && (slotEmpty || slot.type == heldItem.type) && slot.count < 64) {
            // кладём ровно один предмет из руки
            slot.type = heldItem.type;
            slot.count += 1;
            heldItem.count -= 1;
            if (heldItem.count <= 0) { heldItem.count = 0; heldItem.type = BlockType::AIR; }
        }
        // держим предмет, а в слоте лежит другой тип -> ничего не делаем
    }
}

bool Inventory::handleWorkbenchClick(sf::Vector2i pixelPos, sf::Mouse::Button button) {
    sf::Vector2f p((float)pixelPos.x, (float)pixelPos.y);

    // Клик по слоту результата — забираем скрафченный предмет (только ЛКМ, как в оригинале)
    if (button == sf::Mouse::Button::Left && gridResultBox.contains(p)) {
        bool hadRecipe = findMatchingShaped() != nullptr;
        craftFromGrid();
        return hadRecipe;
    }

    for (size_t i = 0; i < gridCellBoxes.size() && i < craftGrid.size(); i++) {
        if (gridCellBoxes[i].contains(p)) {
            clickSlot(craftGrid[i], button);
            return true;
        }
    }

    for (size_t i = 0; i < gridPlayerSlotBoxes.size() && i < slots.size(); i++) {
        if (gridPlayerSlotBoxes[i].contains(p)) {
            clickSlot(slots[i], button);
            return true;
        }
    }

    return false;
}

void Inventory::closeWorkbenchScreen() {
    // Верстак не хранит вещи между открытиями — как и в оригинале, всё возвращается в инвентарь
    for (auto& slot : craftGrid) {
        if (slot.type != BlockType::AIR && slot.count > 0)
            addItem(slot.type, slot.count);
        slot.type = BlockType::AIR;
        slot.count = 0;
    }
    if (heldItem.type != BlockType::AIR && heldItem.count > 0)
        addItem(heldItem.type, heldItem.count);
    heldItem.type = BlockType::AIR;
    heldItem.count = 0;
}

void Inventory::drawWorkbenchScreen(sf::RenderWindow& window) {
    gridCellBoxes.clear();
    gridPlayerSlotBoxes.clear();

    sf::View ui(sf::FloatRect({0.f, 0.f}, {800.f, 600.f}));
    sf::View prev = window.getView();
    window.setView(ui);

    sf::RectangleShape overlay({800.f, 600.f});
    overlay.setFillColor(sf::Color(0, 0, 0, 160));
    window.draw(overlay);

    float slotGap = 4.f;
    float gridW = 3 * (SLOT_SIZE + slotGap) - slotGap;
    float gridH = gridW;

    float winW = gridW + 220.f; // сетка + стрелка + слот результата
    float winH = gridH + SLOT_SIZE + slotGap + 100.f;
    float winX = (800.f - winW) / 2.f, winY = (600.f - winH) / 2.f;

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

    // Кнопка-крестик
    float xbSize = 26.f;
    float xbX = winX + winW - xbSize - 10.f;
    float xbY = winY + 10.f;
    gridCloseButtonBox = sf::FloatRect({xbX, xbY}, {xbSize, xbSize});

    sf::RectangleShape xbBg({xbSize, xbSize});
    xbBg.setPosition({xbX, xbY});
    xbBg.setFillColor(sf::Color(170, 170, 170));
    xbBg.setOutlineColor(sf::Color(90, 90, 90));
    xbBg.setOutlineThickness(2.f);
    window.draw(xbBg);

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
    float xpad = 7.f;
    drawXLine({xbX + xpad, xbY + xpad}, {xbX + xbSize - xpad, xbY + xbSize - xpad});
    drawXLine({xbX + xbSize - xpad, xbY + xpad}, {xbX + xpad, xbY + xbSize - xpad});

    // Сетка 3x3
    float gx = winX + 30.f;
    float gy = winY + 30.f;
    for (int i = 0; i < 9; i++) {
        int col = i % 3, row = i / 3;
        float sx = gx + col * (SLOT_SIZE + slotGap);
        float sy = gy + row * (SLOT_SIZE + slotGap);
        drawSlotGeneric(window, craftGrid[i].type, craftGrid[i].count, sx, sy, false);
        gridCellBoxes.push_back(sf::FloatRect({sx, sy}, {(float)SLOT_SIZE, (float)SLOT_SIZE}));
    }

    // Стрелка + слот результата
    const ShapedRecipe* matched = findMatchingShaped();
    bool craftable = (matched != nullptr);

    float arrowX = gx + gridW + 20.f;
    float arrowY = gy + gridH / 2.f;
    sf::ConvexShape arrow(3);
    arrow.setPoint(0, {arrowX, arrowY - 10.f});
    arrow.setPoint(1, {arrowX, arrowY + 10.f});
    arrow.setPoint(2, {arrowX + 16.f, arrowY});
    arrow.setFillColor(craftable ? sf::Color(230, 230, 230) : sf::Color(90, 90, 90));
    window.draw(arrow);

    float resX = arrowX + 34.f;
    float resY = gy + (gridH - SLOT_SIZE) / 2.f;
    BlockType previewType = craftable ? matched->result      : BlockType::AIR;
    int previewCount      = craftable ? matched->resultCount : 0;
    drawSlotGeneric(window, previewType, previewCount, resX, resY, false);
    gridResultBox = sf::FloatRect({resX, resY}, {(float)SLOT_SIZE, (float)SLOT_SIZE});
    if (!craftable) {
        sf::RectangleShape dim({(float)SLOT_SIZE, (float)SLOT_SIZE});
        dim.setPosition({resX, resY});
        dim.setFillColor(sf::Color(0, 0, 0, 90));
        window.draw(dim);
    }

    // Разделитель
    float sepY = gy + gridH + 16.f;
    sf::RectangleShape sep({winW - 40.f, 2.f});
    sep.setPosition({winX + 20.f, sepY});
    sep.setFillColor(sf::Color(150, 150, 150));
    window.draw(sep);

    // Личные слоты игрока — одной строкой снизу
    float py = sepY + 16.f;
    float px = winX + (winW - (9 * (SLOT_SIZE + slotGap) - slotGap)) / 2.f;
    for (int i = 0; i < (int)slots.size(); i++) {
        float sx = px + i * (SLOT_SIZE + slotGap);
        drawSlotGeneric(window, slots[i].type, slots[i].count, sx, py, i == selectedSlot);
        gridPlayerSlotBoxes.push_back(sf::FloatRect({sx, py}, {(float)SLOT_SIZE, (float)SLOT_SIZE}));
    }

    // Предмет "на курсоре" — рисуем поверх всего, у самой мыши
    if (heldItem.type != BlockType::AIR && heldItem.count > 0) {
        sf::Vector2i mp = sf::Mouse::getPosition(window);
        sf::Vector2f mUi = window.mapPixelToCoords(mp, ui);
        float iconSize = (float)SLOT_SIZE - 12.f;
        drawIcon(window, heldItem.type, mUi.x - iconSize / 2.f, mUi.y - iconSize / 2.f, iconSize);
        if (heldItem.count > 1)
            drawNumber(window, heldItem.count, mUi.x + iconSize / 2.f + 6.f, mUi.y + iconSize / 2.f - 10.f, 10.f);
    }

    window.setView(prev);
}

void Inventory::closeFurnaceScreen() {
    // У печи есть постоянное хранилище (как у сундука) — сбрасывать нечего,
    // кроме предмета, который мог остаться "на курсоре" при закрытии окна.
    if (heldItem.type != BlockType::AIR && heldItem.count > 0)
        addItem(heldItem.type, heldItem.count);
    heldItem.type = BlockType::AIR;
    heldItem.count = 0;
}

bool Inventory::handleFurnaceClick(sf::Vector2i pixelPos, std::vector<InventorySlot>& furnaceSlots, sf::Mouse::Button button) {
    sf::Vector2f p((float)pixelPos.x, (float)pixelPos.y);

    for (size_t i = 0; i < furnaceSlotBoxes.size() && i < furnaceSlots.size(); i++) {
        if (furnaceSlotBoxes[i].contains(p)) {
            clickSlot(furnaceSlots[i], button);
            return true;
        }
    }
    for (size_t i = 0; i < furnacePlayerSlotBoxes.size() && i < slots.size(); i++) {
        if (furnacePlayerSlotBoxes[i].contains(p)) {
            clickSlot(slots[i], button);
            return true;
        }
    }
    return false;
}

void Inventory::drawFurnaceScreen(sf::RenderWindow& window, std::vector<InventorySlot>& furnaceSlots,
                                   float burnFrac, float cookFrac) {
    furnaceSlotBoxes.clear();
    furnacePlayerSlotBoxes.clear();

    sf::View ui(sf::FloatRect({0.f, 0.f}, {800.f, 600.f}));
    sf::View prev = window.getView();
    window.setView(ui);

    sf::RectangleShape overlay({800.f, 600.f});
    overlay.setFillColor(sf::Color(0, 0, 0, 160));
    window.draw(overlay);

    float slotGap = 4.f;
    float winW = 9 * (SLOT_SIZE + slotGap) - slotGap + 80.f; // чтобы влезла нижняя строка личных слотов
    float winH = (float)SLOT_SIZE * 2.f + slotGap + SLOT_SIZE + slotGap + 120.f;
    float winX = (800.f - winW) / 2.f, winY = (600.f - winH) / 2.f;

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

    // Кнопка-крестик
    float xbSize = 26.f;
    float xbX = winX + winW - xbSize - 10.f;
    float xbY = winY + 10.f;
    furnaceCloseButtonBox = sf::FloatRect({xbX, xbY}, {xbSize, xbSize});

    sf::RectangleShape xbBg({xbSize, xbSize});
    xbBg.setPosition({xbX, xbY});
    xbBg.setFillColor(sf::Color(170, 170, 170));
    xbBg.setOutlineColor(sf::Color(90, 90, 90));
    xbBg.setOutlineThickness(2.f);
    window.draw(xbBg);

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
    float xpad = 7.f;
    drawXLine({xbX + xpad, xbY + xpad}, {xbX + xbSize - xpad, xbY + xbSize - xpad});
    drawXLine({xbX + xbSize - xpad, xbY + xpad}, {xbX + xpad, xbY + xbSize - xpad});

    // Слот сырья (сверху) и топлива (снизу) — левая колонка, вся секция отцентрована по ширине окна
    float sectionW = 2.f * SLOT_SIZE + 74.f; // input/fuel + промежуток + стрелка + промежуток + результат
    float gx = winX + (winW - sectionW) / 2.f;
    float gy = winY + 30.f;
    if (furnaceSlots.size() >= 1) {
        drawSlotGeneric(window, furnaceSlots[0].type, furnaceSlots[0].count, gx, gy, false);
        furnaceSlotBoxes.push_back(sf::FloatRect({gx, gy}, {(float)SLOT_SIZE, (float)SLOT_SIZE}));
    }

    // Огонёк между сырьём и топливом — высота отражает остаток горения текущего топлива
    float fireCx = gx + SLOT_SIZE / 2.f;
    float fireTopY = gy + SLOT_SIZE + 6.f;
    float fireH = SLOT_SIZE + slotGap - 12.f;
    sf::RectangleShape fireBg({10.f, fireH});
    fireBg.setPosition({fireCx - 5.f, fireTopY});
    fireBg.setFillColor(sf::Color(70, 70, 70));
    window.draw(fireBg);
    if (burnFrac > 0.f) {
        float litH = fireH * std::clamp(burnFrac, 0.f, 1.f);
        sf::RectangleShape fireFg({10.f, litH});
        fireFg.setPosition({fireCx - 5.f, fireTopY + (fireH - litH)});
        fireFg.setFillColor(sf::Color(255, 140, 30));
        window.draw(fireFg);
    }

    float fuelY = gy + SLOT_SIZE + slotGap;
    if (furnaceSlots.size() >= 2) {
        drawSlotGeneric(window, furnaceSlots[1].type, furnaceSlots[1].count, gx, fuelY, false);
        furnaceSlotBoxes.push_back(sf::FloatRect({gx, fuelY}, {(float)SLOT_SIZE, (float)SLOT_SIZE}));
    }

    // Стрелка с прогресс-баром готовки
    float arrowX = gx + SLOT_SIZE + 24.f;
    float arrowY = gy + SLOT_SIZE + slotGap / 2.f;
    sf::RectangleShape arrowBg({28.f, 10.f});
    arrowBg.setPosition({arrowX, arrowY - 5.f});
    arrowBg.setFillColor(sf::Color(120, 120, 120));
    window.draw(arrowBg);
    if (cookFrac > 0.f) {
        float w = 28.f * std::clamp(cookFrac, 0.f, 1.f);
        sf::RectangleShape arrowFg({w, 10.f});
        arrowFg.setPosition({arrowX, arrowY - 5.f});
        arrowFg.setFillColor(sf::Color(255, 200, 60));
        window.draw(arrowFg);
    }
    sf::ConvexShape arrowHead(3);
    arrowHead.setPoint(0, {arrowX + 28.f, arrowY - 9.f});
    arrowHead.setPoint(1, {arrowX + 28.f, arrowY + 9.f});
    arrowHead.setPoint(2, {arrowX + 40.f, arrowY});
    arrowHead.setFillColor(sf::Color(200, 200, 200));
    window.draw(arrowHead);

    // Слот результата
    float resX = arrowX + 50.f;
    float resY = gy + (SLOT_SIZE + slotGap) / 2.f;
    if (furnaceSlots.size() >= 3) {
        drawSlotGeneric(window, furnaceSlots[2].type, furnaceSlots[2].count, resX, resY, false);
        furnaceSlotBoxes.push_back(sf::FloatRect({resX, resY}, {(float)SLOT_SIZE, (float)SLOT_SIZE}));
    }

    // Разделитель
    float sepY = fuelY + SLOT_SIZE + 16.f;
    sf::RectangleShape sep({winW - 40.f, 2.f});
    sep.setPosition({winX + 20.f, sepY});
    sep.setFillColor(sf::Color(150, 150, 150));
    window.draw(sep);

    // Личные слоты игрока — одной строкой снизу (не влезают в ширину окна печи, поэтому уже,
    // чем в других окнах — прокручивать тут нечего, просто уменьшаем промежуток)
    float py = sepY + 16.f;
    float px = winX + (winW - (9 * (SLOT_SIZE + slotGap) - slotGap)) / 2.f;
    for (int i = 0; i < (int)slots.size(); i++) {
        float sx = px + i * (SLOT_SIZE + slotGap);
        drawSlotGeneric(window, slots[i].type, slots[i].count, sx, py, i == selectedSlot);
        furnacePlayerSlotBoxes.push_back(sf::FloatRect({sx, py}, {(float)SLOT_SIZE, (float)SLOT_SIZE}));
    }

    // Предмет "на курсоре"
    if (heldItem.type != BlockType::AIR && heldItem.count > 0) {
        sf::Vector2i mp = sf::Mouse::getPosition(window);
        sf::Vector2f mUi = window.mapPixelToCoords(mp, ui);
        float iconSize = (float)SLOT_SIZE - 12.f;
        drawIcon(window, heldItem.type, mUi.x - iconSize / 2.f, mUi.y - iconSize / 2.f, iconSize);
        if (heldItem.count > 1)
            drawNumber(window, heldItem.count, mUi.x + iconSize / 2.f + 6.f, mUi.y + iconSize / 2.f - 10.f, 10.f);
    }

    window.setView(prev);
}

void Inventory::drawIcon(sf::RenderWindow& window, BlockType type, float sx, float sy, float size) {
    if (type == BlockType::AIR) return;

    // Факел рисуем процедурно (в атласе его нет) — палочка и мягкое многослойное пламя
    if (type == BlockType::TORCH) {
        float cx = sx + size / 2.f;
        sf::RectangleShape stick({size * 0.14f, size * 0.55f});
        stick.setFillColor(sf::Color(120, 80, 40));
        stick.setPosition({cx - size * 0.07f, sy + size * 0.45f});
        window.draw(stick);

        float baseY = sy + size * 0.42f;

        sf::CircleShape base(size * 0.19f);
        base.setScale({0.85f, 1.15f});
        base.setOrigin({size * 0.19f, size * 0.19f});
        base.setPosition({cx, baseY});
        base.setFillColor(sf::Color(224, 100, 28, 235));
        window.draw(base);

        sf::CircleShape mid(size * 0.135f);
        mid.setScale({0.85f, 1.2f});
        mid.setOrigin({size * 0.135f, size * 0.135f});
        mid.setPosition({cx, baseY - size * 0.09f});
        mid.setFillColor(sf::Color(255, 165, 40));
        window.draw(mid);

        sf::CircleShape core(size * 0.068f);
        core.setOrigin({size * 0.068f, size * 0.068f});
        core.setPosition({cx, baseY - size * 0.135f});
        core.setFillColor(sf::Color(255, 235, 170));
        window.draw(core);

        sf::CircleShape tip(size * 0.044f);
        tip.setScale({0.8f, 1.3f});
        tip.setOrigin({size * 0.044f, size * 0.044f});
        tip.setPosition({cx, baseY - size * 0.235f});
        tip.setFillColor(sf::Color(206, 46, 24, 235));
        window.draw(tip);
        return;
    }

    // Мясо (сырое/готовое, курица или говядина) — рисуем процедурно: овальный кусок с прожилками
    if (type == BlockType::RAW_CHICKEN || type == BlockType::COOKED_CHICKEN ||
        type == BlockType::RAW_BEEF    || type == BlockType::COOKED_BEEF) {
        bool cooked = (type == BlockType::COOKED_CHICKEN || type == BlockType::COOKED_BEEF);
        bool beef   = (type == BlockType::RAW_BEEF || type == BlockType::COOKED_BEEF);

        sf::Color base, stripe;
        if (beef) {
            base   = cooked ? sf::Color(120, 65, 40)  : sf::Color(205, 70, 70);
            stripe = cooked ? sf::Color(80, 40, 22)   : sf::Color(160, 40, 45);
        } else {
            base   = cooked ? sf::Color(170, 110, 55) : sf::Color(235, 175, 165);
            stripe = cooked ? sf::Color(120, 70, 30)  : sf::Color(215, 130, 120);
        }

        sf::CircleShape meat(size * 0.42f);
        meat.setFillColor(base);
        meat.setOrigin({size * 0.42f, size * 0.42f});
        meat.setPosition({sx + size * 0.5f, sy + size * 0.5f});
        meat.setScale({1.f, 0.8f});
        window.draw(meat);

        for (int i = -1; i <= 1; i++) {
            sf::RectangleShape line({size * 0.5f, size * 0.06f});
            line.setFillColor(stripe);
            line.setOrigin({size * 0.25f, size * 0.03f});
            line.setPosition({sx + size * 0.5f, sy + size * (0.5f + i * 0.16f)});
            line.setRotation(sf::degrees(-25.f));
            window.draw(line);
        }
        return;
    }

    // Лестница — рисуем процедурно: два рельса и перекладины между ними
    if (type == BlockType::LADDER) {
        sf::Color rail(140, 100, 55);
        sf::Color rung(170, 125, 70);
        float railW = size * 0.14f;
        sf::RectangleShape left({railW, size * 0.82f});
        left.setPosition({sx + size * 0.12f, sy + size * 0.09f});
        left.setFillColor(rail);
        window.draw(left);
        sf::RectangleShape right({railW, size * 0.82f});
        right.setPosition({sx + size * 0.74f, sy + size * 0.09f});
        right.setFillColor(rail);
        window.draw(right);
        for (int i = 0; i < 3; i++) {
            sf::RectangleShape r({size * 0.5f, size * 0.1f});
            r.setPosition({sx + size * 0.26f, sy + size * (0.2f + i * 0.28f)});
            r.setFillColor(rung);
            window.draw(r);
        }
        return;
    }

    // Дверь — рисуем процедурно: деревянная панель с рамкой и ручкой
    if (type == BlockType::DOOR || type == BlockType::DOOR_OPEN) {
        sf::Color panel(150, 108, 60);
        sf::Color frame(105, 74, 40);
        sf::Color handle(230, 200, 90);
        sf::RectangleShape door({size * 0.62f, size * 0.86f});
        door.setPosition({sx + size * 0.18f, sy + size * 0.08f});
        door.setFillColor(panel);
        door.setOutlineColor(frame);
        door.setOutlineThickness(size * 0.03f);
        window.draw(door);
        sf::CircleShape knob(size * 0.045f);
        knob.setPosition({sx + size * 0.66f, sy + size * 0.48f});
        knob.setFillColor(handle);
        window.draw(knob);
        return;
    }

    // Кровать — рисуем процедурно, вид сбоку: красное одеяло, белая подушка, деревянные ножки
    if (type == BlockType::BED || type == BlockType::BED_HEAD) {
        sf::Color blanket(190, 45, 45);
        sf::Color pillow(235, 235, 240);
        sf::Color legs(90, 65, 35);

        sf::RectangleShape bedTop({size * 0.9f, size * 0.32f});
        bedTop.setPosition({sx + size * 0.05f, sy + size * 0.42f});
        bedTop.setFillColor(blanket);
        window.draw(bedTop);

        sf::RectangleShape pillowShape({size * 0.28f, size * 0.24f});
        pillowShape.setPosition({sx + size * 0.08f, sy + size * 0.38f});
        pillowShape.setFillColor(pillow);
        window.draw(pillowShape);

        sf::RectangleShape leg1({size * 0.08f, size * 0.16f});
        leg1.setPosition({sx + size * 0.08f, sy + size * 0.74f});
        leg1.setFillColor(legs);
        window.draw(leg1);
        sf::RectangleShape leg2({size * 0.08f, size * 0.16f});
        leg2.setPosition({sx + size * 0.84f, sy + size * 0.74f});
        leg2.setFillColor(legs);
        window.draw(leg2);
        return;
    }

    // Шерсть — рисуем процедурно: пушистый белый комок с лёгкой рябью
    if (type == BlockType::WOOL) {
        sf::Color wool(240, 240, 235);
        sf::Color shade(210, 210, 203);
        sf::RectangleShape base({size * 0.8f, size * 0.8f});
        base.setPosition({sx + size * 0.1f, sy + size * 0.1f});
        base.setFillColor(wool);
        window.draw(base);
        for (int i = 0; i < 4; i++) {
            sf::RectangleShape dot({size * 0.14f, size * 0.14f});
            float dx = (i % 2 == 0) ? size * 0.2f : size * 0.55f;
            float dy = (i < 2) ? size * 0.22f : size * 0.55f;
            dot.setPosition({sx + dx, sy + dy});
            dot.setFillColor(shade);
            window.draw(dot);
        }
        return;
    }

    // Слитки железа/золота — рисуем процедурно: трапециевидный брусок
    if (type == BlockType::IRON_INGOT || type == BlockType::GOLD_INGOT) {
        bool gold = (type == BlockType::GOLD_INGOT);
        sf::Color top   = gold ? sf::Color(255, 230, 120) : sf::Color(235, 235, 240);
        sf::Color side  = gold ? sf::Color(205, 165, 40)  : sf::Color(170, 170, 178);

        sf::ConvexShape bar(6);
        float w0 = size * 0.24f, w1 = size * 0.34f; // верх уже, низ шире (трапеция)
        float top_y = sy + size * 0.36f, bot_y = sy + size * 0.68f;
        float cx = sx + size * 0.5f;
        bar.setPoint(0, {cx - w0, top_y});
        bar.setPoint(1, {cx + w0, top_y});
        bar.setPoint(2, {cx + w1, (top_y + bot_y) / 2.f});
        bar.setPoint(3, {cx + w1, bot_y});
        bar.setPoint(4, {cx - w1, bot_y});
        bar.setPoint(5, {cx - w1, (top_y + bot_y) / 2.f});
        bar.setFillColor(side);
        window.draw(bar);

        sf::RectangleShape highlight({w0 * 2.f - 4.f, (bot_y - top_y) * 0.35f});
        highlight.setFillColor(top);
        highlight.setPosition({cx - w0 + 2.f, top_y + 2.f});
        window.draw(highlight);
        return;
    }

    // Сакура — честная перекраска (своя текстура), рисуем её раньше проверки общего атласа
    if (type == BlockType::LEAVES_SAKURA && sakuraLeafTexture) {
        sf::Sprite icon(*sakuraLeafTexture);
        float scale = size / (float)Block::TEXTURE_SIZE;
        icon.setScale({scale, scale});
        icon.setPosition({sx, sy});
        window.draw(icon);
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
        // Акация — тон поверх (умножение на тёмно-зелёном только темнит, не желтит)
        if (type == BlockType::LEAVES_ACACIA) {
            sf::Color t = Block(type).getLeafTint();
            sf::RectangleShape overlay({size, size});
            overlay.setPosition({sx, sy});
            overlay.setFillColor(sf::Color(t.r, t.g, t.b, 150));
            window.draw(overlay);
        }
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

void Inventory::drawSlotGeneric(sf::RenderWindow& window, BlockType type, int count, float sx, float sy, bool selected) {
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

    if (type != BlockType::AIR && count > 0) {
        float pad = 8.f;
        drawIcon(window, type, sx + pad, sy + pad, S - pad * 2);
        // Число количества — в правом нижнем углу слота (показываем, если больше 1)
        if (count > 1) {
            drawNumber(window, count, sx + S - 4.f, sy + S - 14.f, 10.f);
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

void Inventory::drawSlot(sf::RenderWindow& window, int i, float sx, float sy, bool selected) {
    BlockType type = (i < (int)slots.size()) ? slots[i].type  : BlockType::AIR;
    int count      = (i < (int)slots.size()) ? slots[i].count : 0;
    drawSlotGeneric(window, type, count, sx, sy, selected);
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

void Inventory::computeCraftLayout(int& cols, int& rowsPerCol) const {
    int n = (int)recipes.size();
    if (n <= 0) { cols = 1; rowsPerCol = 0; return; }
    const int maxRowsPerCol = 5; // больше — начинаем новую колонку, а не растягиваем окно вниз
    cols = (n + maxRowsPerCol - 1) / maxRowsPerCol;
    if (cols < 1) cols = 1;
    rowsPerCol = (n + cols - 1) / cols; // реальное число строк при таком числе колонок
}

void Inventory::drawCraftingSection(sf::RenderWindow& window, float x, float y) {
    recipeHitboxes.clear();

    float rowGap = 10.f;
    float rowH = 2 * 24.f + 2.f;              // высота одной строки рецепта (под cellSize=24)
    float colGap = 30.f;
    float colW = 200.f;                        // ширина колонки рецепта
    int cols, rowsPerCol;
    computeCraftLayout(cols, rowsPerCol);
    if (rowsPerCol <= 0) return;

    for (size_t i = 0; i < recipes.size(); i++) {
        int col = (int)i / rowsPerCol;
        int row = (int)i % rowsPerCol;
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

    // Размер окна подстраиваем под реальное число рецептов, чтобы список никогда не
    // обрезался снизу и не вылезал за края, сколько бы рецептов ни было добавлено потом.
    {
        int cols, rowsPerCol;
        computeCraftLayout(cols, rowsPerCol);
        float rowGap = 10.f, rowH = 2 * 24.f + 2.f, colGap = 30.f, colW = 200.f;
        float craftW = cols * colW + std::max(0, cols - 1) * colGap;
        float craftH = rowsPerCol > 0 ? rowsPerCol * rowH + std::max(0, rowsPerCol - 1) * rowGap : 0.f;

        float invGridW = 5 * (SLOT_SIZE + 4.f) - 4.f; // 5 колонок личного инвентаря
        float topPad = 30.f, invH = 2 * (SLOT_SIZE + 4.f), sepGap = 26.f, sidePad = 40.f, bottomPad = 30.f;

        winW = std::max(invGridW, craftW) + sidePad * 2.f;
        winH = topPad + invH + sepGap + craftH + bottomPad;
    }
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