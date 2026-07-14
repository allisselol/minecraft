#pragma once
#include <SFML/Graphics.hpp>

enum class BlockType {
    AIR,
    GRASS,
    DIRT,
    STONE,
    WOOD,
    LEAVES,
    LEAVES_ACACIA, // листва акации — тот же тайл, что и LEAVES, но с другим тоном (см. getLeafTint)
    LEAVES_SAKURA, // листва сакуры — розовая
    LEAVES_SNOWY,  // листва снежного биома — тёмно-зелёная, со снежной шапкой сверху
    SAND,
    GRAVEL,
    GLOWING_MOSS, // светящийся мох в пещерах биома сакуры — светится, как факел
    TNT,
    TORCH,
    COAL_ORE,
    IRON_ORE,
    GOLD_ORE,
    DIAMOND_ORE,
    WATER,
    PLANKS,
    BRICK,
    SANDSTONE,
    PUMPKIN,
    BOOKSHELF,
    OBSIDIAN,
    WORKBENCH,
    CHEST,
    FURNACE,
    LADDER,
    DOOR,          // закрытая дверь (нижняя половина) — предмет в инвентаре и блок в мире по умолчанию
    DOOR_OPEN,     // открытая дверь (нижняя половина) — только состояние в мире
    DOOR_TOP,      // верхняя половина закрытой двери — ставится/ломается вместе с нижней автоматически
    DOOR_TOP_OPEN, // верхняя половина открытой двери
    BED,      // изножье кровати — предмет в инвентаре и то, что ставится в клик
    BED_HEAD, // изголовье — вторая половина, ставится/ломается вместе с BED автоматически

    // Эти четыре — не настоящие блоки, а предметы (еда и слитки из печи).
    // Их нельзя поставить в мир, только держать в инвентаре.
    RAW_CHICKEN,
    COOKED_CHICKEN,
    RAW_BEEF,
    COOKED_BEEF,
    IRON_INGOT,
    GOLD_INGOT,
    WOOL // шерсть — с овцы, идёт на кровать
};

struct Block {
    BlockType type;

    Block() : type(BlockType::AIR) {}
    Block(BlockType t) : type(t) {}

    // "Предметные" типы (еда, слитки) — не блоки, никогда не встречаются в мире
    bool isItem() const {
        return type == BlockType::RAW_CHICKEN || type == BlockType::COOKED_CHICKEN ||
               type == BlockType::RAW_BEEF    || type == BlockType::COOKED_BEEF    ||
               type == BlockType::IRON_INGOT  || type == BlockType::GOLD_INGOT     ||
               type == BlockType::WOOL;
    }

    bool isSolid() const {
        return type != BlockType::AIR && type != BlockType::TORCH && type != BlockType::WATER &&
               type != BlockType::LADDER &&
               type != BlockType::DOOR_OPEN && type != BlockType::DOOR_TOP_OPEN && !isItem();
    }

    // Любая из 4 половинок двери (закрытая/открытая, верх/низ)
    bool isDoorPart() const {
        return type == BlockType::DOOR || type == BlockType::DOOR_OPEN ||
               type == BlockType::DOOR_TOP || type == BlockType::DOOR_TOP_OPEN;
    }
    bool isDoorTop() const { return type == BlockType::DOOR_TOP || type == BlockType::DOOR_TOP_OPEN; }
    bool isDoorOpenState() const { return type == BlockType::DOOR_OPEN || type == BlockType::DOOR_TOP_OPEN; }

    bool isBedPart() const { return type == BlockType::BED || type == BlockType::BED_HEAD; }

    bool isLadder() const { return type == BlockType::LADDER; }

    bool isLeaf() const {
        return type == BlockType::LEAVES || type == BlockType::LEAVES_ACACIA ||
               type == BlockType::LEAVES_SAKURA || type == BlockType::LEAVES_SNOWY;
    }
    // Акация и сакура используют ровно ту же текстуру листвы, что и обычное дерево,
    // просто с другим цветовым тоном поверх (через sprite.setColor) — так не нужно
    // рисовать отдельные тайлы в атласе ради всего одного нового оттенка листьев.
    sf::Color getLeafTint() const {
        switch (type) {
            case BlockType::LEAVES_ACACIA: return sf::Color(110, 120, 55);  // приглушённый оливковый, не жёлтый
            case BlockType::LEAVES_SAKURA: return sf::Color(235, 110, 185);  // яркий розовый/малиновый, как на референсе
            case BlockType::LEAVES_SNOWY:  return sf::Color(35, 65, 35);    // тёмно-зелёная хвоя
            default:                       return sf::Color::White;        // без тона — обычная листва
        }
    }

    bool isWater() const {
        return type == BlockType::WATER;
    }

    // Сколько сытости восстанавливает предмет, если его съесть (0 — это не еда)
    int getFoodValue() const {
        switch (type) {
            case BlockType::RAW_CHICKEN:    return 2;
            case BlockType::COOKED_CHICKEN: return 5; // приготовленное мясо сытнее сырого
            case BlockType::RAW_BEEF:       return 3;
            case BlockType::COOKED_BEEF:    return 6; // говядина чуть сытнее курицы
            default:                        return 0;
        }
    }
    bool isFood() const { return getFoodValue() > 0; }

    // Блоки, которые падают под действием "гравитации", если под ними пусто
    bool isFalling() const {
        return type == BlockType::SAND || type == BlockType::GRAVEL;
    }

    sf::Color getColor() const {
        switch (type) {
            case BlockType::GRASS:  return sf::Color(76, 120, 40);
            case BlockType::DIRT:   return sf::Color(120, 85, 55);
            case BlockType::STONE:  return sf::Color(100, 100, 100);
            case BlockType::WOOD:   return sf::Color(90, 65, 35);
            case BlockType::LEAVES: return sf::Color(40, 110, 25);
            case BlockType::LEAVES_ACACIA: return sf::Color(150, 145, 45);
            case BlockType::LEAVES_SAKURA: return sf::Color(235, 130, 175);
            case BlockType::LEAVES_SNOWY:  return sf::Color(35, 65, 35);
            case BlockType::SAND:   return sf::Color(208, 189, 137);
            case BlockType::GRAVEL: return sf::Color(128, 122, 116);
            case BlockType::GLOWING_MOSS: return sf::Color(70, 150, 90);
            case BlockType::TNT:    return sf::Color(196, 40, 32);
            case BlockType::TORCH:  return sf::Color(200, 140, 60);
            case BlockType::COAL_ORE:    return sf::Color(90, 90, 92);
            case BlockType::IRON_ORE:    return sf::Color(150, 130, 115);
            case BlockType::GOLD_ORE:    return sf::Color(180, 155, 80);
            case BlockType::DIAMOND_ORE: return sf::Color(110, 190, 190);
            case BlockType::WATER:  return sf::Color(58, 118, 210);
            case BlockType::PLANKS:    return sf::Color(156, 127, 78);
            case BlockType::BRICK:     return sf::Color(197, 106, 79);
            case BlockType::SANDSTONE: return sf::Color(216, 209, 157);
            case BlockType::PUMPKIN:   return sf::Color(210, 125, 30);
            case BlockType::BOOKSHELF: return sf::Color(120, 85, 55);
            case BlockType::OBSIDIAN:  return sf::Color(30, 26, 45);
            case BlockType::WORKBENCH: return sf::Color(137, 101, 60);
            case BlockType::CHEST:     return sf::Color(109, 74, 38);
            case BlockType::FURNACE:   return sf::Color(115, 115, 115);
            case BlockType::LADDER:    return sf::Color(153, 114, 66);
            case BlockType::DOOR:
            case BlockType::DOOR_OPEN:
            case BlockType::DOOR_TOP:
            case BlockType::DOOR_TOP_OPEN: return sf::Color(133, 94, 52);
            case BlockType::BED:
            case BlockType::BED_HEAD: return sf::Color(190, 45, 45);
            case BlockType::RAW_CHICKEN:    return sf::Color(235, 180, 170);
            case BlockType::COOKED_CHICKEN: return sf::Color(180, 120, 60);
            case BlockType::RAW_BEEF:       return sf::Color(205, 70, 70);
            case BlockType::COOKED_BEEF:    return sf::Color(120, 65, 40);
            case BlockType::IRON_INGOT:     return sf::Color(220, 220, 225);
            case BlockType::GOLD_INGOT:     return sf::Color(250, 210, 60);
            case BlockType::WOOL:           return sf::Color(235, 235, 230);
            default:                return sf::Color::Transparent;
        }
    }

    sf::Color getTopColor() const {
        switch (type) {
            case BlockType::GRASS:  return sf::Color(100, 170, 50);
            case BlockType::DIRT:   return sf::Color(140, 100, 65);
            case BlockType::STONE:  return sf::Color(130, 130, 130);
            case BlockType::WOOD:   return sf::Color(110, 85, 50);
            case BlockType::LEAVES: return sf::Color(55, 140, 35);
            case BlockType::SAND:   return sf::Color(230, 214, 165);
            case BlockType::GRAVEL: return sf::Color(170, 164, 156);
            case BlockType::TNT:    return sf::Color(230, 70, 55);
            case BlockType::TORCH:  return sf::Color(230, 180, 90);
            case BlockType::COAL_ORE:    return sf::Color(110, 110, 112);
            case BlockType::IRON_ORE:    return sf::Color(175, 155, 140);
            case BlockType::GOLD_ORE:    return sf::Color(210, 185, 105);
            case BlockType::DIAMOND_ORE: return sf::Color(140, 220, 220);
            default:                return sf::Color::Transparent;
        }
    }

    sf::Color getDarkColor() const {
        switch (type) {
            case BlockType::GRASS:  return sf::Color(50, 85, 25);
            case BlockType::DIRT:   return sf::Color(90, 60, 35);
            case BlockType::STONE:  return sf::Color(70, 70, 70);
            case BlockType::WOOD:   return sf::Color(60, 45, 20);
            case BlockType::LEAVES: return sf::Color(25, 80, 15);
            case BlockType::SAND:   return sf::Color(180, 160, 110);
            case BlockType::GRAVEL: return sf::Color(90, 86, 82);
            case BlockType::TNT:    return sf::Color(30, 25, 22);
            case BlockType::TORCH:  return sf::Color(140, 90, 40);
            case BlockType::COAL_ORE:    return sf::Color(50, 50, 52);
            case BlockType::IRON_ORE:    return sf::Color(120, 100, 85);
            case BlockType::GOLD_ORE:    return sf::Color(150, 125, 55);
            case BlockType::DIAMOND_ORE: return sf::Color(70, 160, 160);
            default:                return sf::Color::Transparent;
        }
    }

    // Размер одного тайла в текстурном атласе (textures/blocks.png), в пикселях
    static constexpr int TEXTURE_SIZE = 32;

    // Прямоугольник текстуры для этого блока в атласе (для отрисовки спрайтом)
    sf::IntRect getTextureRect() const {
        int index = 0;
        switch (type) {
            case BlockType::GRASS:  index = 1; break;   // трава
            case BlockType::DIRT:   index = 2; break;   // земля
            case BlockType::WOOD:   index = 3; break;   // дерево/доски
            case BlockType::LEAVES: index = 4; break;   // листва
            case BlockType::LEAVES_ACACIA: index = 4; break; // тот же тайл, цвет — через getLeafTint()
            case BlockType::LEAVES_SAKURA: index = 4; break;
            case BlockType::LEAVES_SNOWY:  index = 4; break;
            case BlockType::STONE:  index = 7; break;   // камень
            case BlockType::GLOWING_MOSS: index = 7; break; // тот же камень, зелёный тон при отрисовке
            case BlockType::GRAVEL: index = 8; break;   // булыжник/гравий
            case BlockType::SAND:   index = 9; break;   // песок
            case BlockType::TNT:    index = 11; break;  // TNT
            case BlockType::WATER:  index = 14; break;  // вода (голубой тайл)
            case BlockType::PLANKS:    index = 5;  break; // доски
            case BlockType::BRICK:     index = 6;  break; // кирпич
            case BlockType::SANDSTONE: index = 10; break; // песчаник
            case BlockType::PUMPKIN:   index = 17; break; // тыква
            case BlockType::BOOKSHELF: index = 18; break; // книжная полка
            case BlockType::OBSIDIAN:  index = 28; break; // обсидиан
            case BlockType::COAL_ORE:    index = 22; break; // камень с редкими чёрными вкраплениями
            case BlockType::IRON_ORE:    index = 23; break; // камень с бежевыми вкраплениями
            case BlockType::GOLD_ORE:    index = 24; break; // камень с жёлтыми вкраплениями
            case BlockType::DIAMOND_ORE: index = 25; break; // камень с голубыми вкраплениями
            case BlockType::WORKBENCH:   index = 20; break; // верстак (уже был в атласе)
            case BlockType::CHEST:       index = 30; break; // сундук
            case BlockType::FURNACE:     index = 21; break; // печь (уже была в атласе)
            default:                index = 1; break;
        }
        return sf::IntRect({index * TEXTURE_SIZE, 0}, {TEXTURE_SIZE, TEXTURE_SIZE});
    }
};

// === Печь: что из чего плавится и что можно жечь как топливо ===
// Вынесено свободными функциями (а не в Block), чтобы одинаково использовать
// и в Game (симуляция готовки), и в Inventory (отрисовка окна печи).

// Есть ли рецепт плавки для этого сырья; если да — возвращает результат и время готовки (в секундах)
inline bool getSmeltResult(BlockType input, BlockType& outResult, float& outCookTime) {
    switch (input) {
        case BlockType::RAW_CHICKEN: outResult = BlockType::COOKED_CHICKEN; outCookTime = 5.f; return true;
        case BlockType::RAW_BEEF:    outResult = BlockType::COOKED_BEEF;    outCookTime = 5.f; return true;
        case BlockType::IRON_ORE:    outResult = BlockType::IRON_INGOT;     outCookTime = 6.f; return true;
        case BlockType::GOLD_ORE:    outResult = BlockType::GOLD_INGOT;     outCookTime = 6.f; return true;
        default: return false;
    }
}

// Сколько секунд горит ОДНА единица топлива такого типа (0 — не топливо)
inline float getFuelBurnSeconds(BlockType fuel) {
    switch (fuel) {
        case BlockType::COAL_ORE: return 16.f; // уголь — самое долгоиграющее топливо
        case BlockType::WOOD:     return 8.f;
        case BlockType::PLANKS:   return 4.f;
        default:                  return 0.f;
    }
}
inline bool isFuel(BlockType t) { return getFuelBurnSeconds(t) > 0.f; }