#pragma once
#include <SFML/Graphics.hpp>
#include "Block.hpp"
#include <vector>
#include <cmath>
#include <utility>

struct InventorySlot {
    BlockType type;
    int count;
    InventorySlot(BlockType t, int c) : type(t), count(c) {}
};

// Простой безусловный (shapeless) рецепт крафта — для личного инвентаря:
// нужно суммарно N предметов каждого типа-ингредиента (неважно, в каком слоте они лежат).
struct Recipe {
    std::vector<std::pair<BlockType, int>> ingredients;
    BlockType result;
    int resultCount;
};

// "Формованный" рецепт верстака — как в оригинале: важно, в какой именно из 9 ячеек
// сетки 3x3 что лежит. AIR в ячейке означает "тут должно быть пусто".
// Сетка сравнивается с паттерном по обрезанному до занятых ячеек прямоугольнику,
// поэтому рецепт можно собрать в любом месте сетки — не только строго в углу.
struct ShapedRecipe {
    BlockType pattern[3][3];
    BlockType result;
    int resultCount;
};

class Inventory {
private:
    std::vector<InventorySlot> slots;
    int selectedSlot;

    const sf::Texture* blockTexture = nullptr; // атлас текстур блoков (для иконок)
    const sf::Texture* sakuraLeafTexture = nullptr; // отдельный перекрашенный тайл листвы сакуры

    std::vector<Recipe> recipes;
    std::vector<sf::FloatRect> recipeHitboxes; // пересчитываются при каждой отрисовке крафта
    sf::FloatRect closeButtonBox; // кнопка-крестик закрытия инвентаря

    std::vector<ShapedRecipe> shapedRecipes;   // рецепты, доступные только на сетке верстака
    std::vector<InventorySlot> craftGrid;      // 9 ячеек сетки верстака (3x3), заполняются вручную игроком
    InventorySlot heldItem{BlockType::AIR, 0}; // предмет "на курсоре" при работе с сеткой верстака

    static constexpr int SLOT_SIZE    = 50;
    static constexpr int SLOT_PADDING = 6;

    void drawSlot(sf::RenderWindow& window, int i, float sx, float sy, bool selected);
    void drawSlotGeneric(sf::RenderWindow& window, BlockType type, int count, float sx, float sy, bool selected);
    void drawIcon(sf::RenderWindow& window, BlockType type, float sx, float sy, float size);
    void drawNumber(sf::RenderWindow& window, int number, float rx, float ry, float digitH); // пиксельные цифры
    void drawRecipeRow(sf::RenderWindow& window, const Recipe& recipe, float rx, float ry, bool craftable);
    void drawCraftingSection(sf::RenderWindow& window, float x, float y);
    // Сколько колонок/строк займёт список простых рецептов — используется и при отрисовке
    // окна инвентаря (чтобы посчитать нужный размер), и самим drawCraftingSection.
    void computeCraftLayout(int& cols, int& rowsPerCol) const;

    bool canCraft(const Recipe& r) const;
    void craftRecipe(const Recipe& r);

    // Проверяет, совпадает ли текущее содержимое craftGrid с формой рецепта r
    bool matchShaped(const ShapedRecipe& r) const;
    // Возвращает первый подходящий сейчас рецепт верстака или nullptr
    const ShapedRecipe* findMatchingShaped() const;
    // Тратит по 1 предмету из каждой занятой ячейки сетки и выдаёт результат в инвентарь
    void craftFromGrid();
    // Универсальный клик по одному слоту (ЛКМ — взять/положить/слить/поменять местами
    // целую стопку; ПКМ — взять половину стопки или положить один предмет) —
    // так же, как в оригинальном майнкрафте, только без перетаскивания мышью.
    void clickSlot(InventorySlot& slot, sf::Mouse::Button button);

    // Переносит содержимое одного слота целиком в первый подходящий слот(ы) вектора to
    // (сперва доукомплектовывает такие же стопки, потом кладёт в свободные). Остаток,
    // не поместившийся в to, остаётся в from (ничего не пропадает, в отличие от addItem).
    static void transferSlot(InventorySlot& from, std::vector<InventorySlot>& to);

    // Хитбоксы окна открытого сундука (пересчитываются при каждой отрисовке)
    std::vector<sf::FloatRect> chestSlotBoxes;       // слоты сундука
    std::vector<sf::FloatRect> chestPlayerSlotBoxes; // личные слоты игрока в этом же окне
    sf::FloatRect chestCloseButtonBox;

    // Хитбоксы окна верстака (пересчитываются при каждой отрисовке)
    std::vector<sf::FloatRect> gridCellBoxes;        // 9 ячеек сетки 3x3
    std::vector<sf::FloatRect> gridPlayerSlotBoxes;  // личные слоты игрока в этом же окне
    sf::FloatRect gridResultBox;                     // слот результата крафта
    sf::FloatRect gridCloseButtonBox;

    // Хитбоксы окна печи (пересчитываются при каждой отрисовке)
    std::vector<sf::FloatRect> furnaceSlotBoxes;       // 3 слота: сырьё, топливо, результат
    std::vector<sf::FloatRect> furnacePlayerSlotBoxes; // личные слоты игрока в этом же окне
    sf::FloatRect furnaceCloseButtonBox;

public:
    Inventory();

    void setBlockTexture(const sf::Texture* tex) { blockTexture = tex; }
    void setSakuraLeafTexture(const sf::Texture* tex) { sakuraLeafTexture = tex; }

    void selectSlot(int index);
    void nextSlot();
    void prevSlot();

    BlockType getSelectedType() const;
    int getSelectedSlot() const { return selectedSlot; }
    int getSlotCount() const { return (int)slots.size(); }
    BlockType getSlotType(int i) const { return (i >= 0 && i < (int)slots.size()) ? slots[i].type : BlockType::AIR; }
    int getSlotAmount(int i) const { return (i >= 0 && i < (int)slots.size()) ? slots[i].count : 0; }
    void setSlot(int i, BlockType type, int count) {
        if (i >= 0 && i < (int)slots.size()) { slots[i].type = type; slots[i].count = count; }
    }

    // Настоящая (survival) логика инвентаря
    bool hasSelected() const;      // можно ли поставить блок из выбранного слота
    void consumeSelected();        // тратим 1 из выбранного слота при установке
    void addItem(BlockType type, int amount); // добавляем добытый блок в инвентарь

    // Клик мышью по открытому окну инвентаря — используется для крафта.
    // Возвращает true, если что-то было успешно скрафчено.
    bool handleCraftClick(sf::Vector2i pixelPos);
    bool handleCloseClick(sf::Vector2i pixelPos) const { // клик по крестику закрытия
        return closeButtonBox.contains(sf::Vector2f((float)pixelPos.x, (float)pixelPos.y));
    }

    void draw(sf::RenderWindow& window);
    void drawFullScreen(sf::RenderWindow& window);

    // === Сундук ===
    // Хранилище сундука Game хранит сам (по координатам блока) и передаёт сюда ссылкой —
    // Inventory лишь рисует окно и переносит предметы между ним и личными слотами игрока.
    void drawChestScreen(sf::RenderWindow& window, std::vector<InventorySlot>& chestSlots);
    // Клик по слоту сундука или по личному слоту в этом окне — переносит стек целиком.
    // Возвращает true, если что-то реально переложили (для звука).
    bool handleChestClick(sf::Vector2i pixelPos, std::vector<InventorySlot>& chestSlots);
    bool handleChestCloseClick(sf::Vector2i pixelPos) const {
        return chestCloseButtonBox.contains(sf::Vector2f((float)pixelPos.x, (float)pixelPos.y));
    }

    // === Верстак: настоящая сетка 3x3, как в оригинале ===
    // Рисует окно верстака: сетка 3x3 + стрелка + слот результата + личные слоты снизу.
    void drawWorkbenchScreen(sf::RenderWindow& window);
    // Клик по ячейке сетки, по слоту результата или по личному слоту в этом окне.
    // button важен: ЛКМ берёт/кладёт/сливает/меняет целую стопку, ПКМ — половину/один предмет.
    // Возвращает true, если что-то произошло (для звука).
    bool handleWorkbenchClick(sf::Vector2i pixelPos, sf::Mouse::Button button);
    bool handleWorkbenchCloseClick(sf::Vector2i pixelPos) const {
        return gridCloseButtonBox.contains(sf::Vector2f((float)pixelPos.x, (float)pixelPos.y));
    }
    // Возвращает все предметы из сетки и с курсора обратно в инвентарь и очищает сетку —
    // вызывается при закрытии окна верстака (как и в оригинале, стол не хранит вещи между открытиями).
    void closeWorkbenchScreen();

    // === Печь ===
    // Состояние самой печи (3 слота + таймеры) Game хранит по координатам блока и передаёт
    // сюда ссылкой — так же, как и для сундука. burnFrac/cookFrac — 0..1, для индикаторов огня и прогресса.
    void drawFurnaceScreen(sf::RenderWindow& window, std::vector<InventorySlot>& furnaceSlots,
                            float burnFrac, float cookFrac);
    // Клик по слоту сырья/топлива/результата или по личному слоту в этом окне.
    // ЛКМ/ПКМ работают так же, как на сетке верстака (целая стопка / половина / один предмет).
    bool handleFurnaceClick(sf::Vector2i pixelPos, std::vector<InventorySlot>& furnaceSlots, sf::Mouse::Button button);
    bool handleFurnaceCloseClick(sf::Vector2i pixelPos) const {
        return furnaceCloseButtonBox.contains(sf::Vector2f((float)pixelPos.x, (float)pixelPos.y));
    }
    // Возвращает предмет с курсора обратно в инвентарь (сама печь свои слоты не теряет —
    // в отличие от верстака, у печи есть постоянное хранилище, как у сундука).
    void closeFurnaceScreen();
};