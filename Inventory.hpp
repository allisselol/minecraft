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

// Простой безусловный (shapeless) рецепт крафта: нужно суммарно N предметов
// каждого типа-ингредиента (неважно, в каком слоте они лежат) -> получаем результат.
struct Recipe {
    std::vector<std::pair<BlockType, int>> ingredients;
    BlockType result;
    int resultCount;
};

class Inventory {
private:
    std::vector<InventorySlot> slots;
    int selectedSlot;

    const sf::Texture* blockTexture = nullptr; // атлас текстур блoков (для иконок)

    std::vector<Recipe> recipes;
    std::vector<sf::FloatRect> recipeHitboxes; // пересчитываются при каждой отрисовке крафта
    sf::FloatRect closeButtonBox; // кнопка-крестик закрытия инвентаря

    static constexpr int SLOT_SIZE    = 50;
    static constexpr int SLOT_PADDING = 6;

    void drawSlot(sf::RenderWindow& window, int i, float sx, float sy, bool selected);
    void drawIcon(sf::RenderWindow& window, BlockType type, float sx, float sy, float size);
    void drawNumber(sf::RenderWindow& window, int number, float rx, float ry, float digitH); // пиксельные цифры
    void drawRecipeRow(sf::RenderWindow& window, const Recipe& recipe, float rx, float ry, bool craftable);
    void drawCraftingSection(sf::RenderWindow& window, float x, float y);

    bool canCraft(const Recipe& r) const;
    void craftRecipe(const Recipe& r);

public:
    Inventory();

    void setBlockTexture(const sf::Texture* tex) { blockTexture = tex; }

    void selectSlot(int index);
    void nextSlot();
    void prevSlot();

    BlockType getSelectedType() const;
    int getSelectedSlot() const { return selectedSlot; }
    int getSlotCount() const { return (int)slots.size(); }

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
};