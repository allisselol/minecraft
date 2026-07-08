#pragma once
#include <SFML/Graphics.hpp>

enum class BlockType {
    AIR,
    GRASS,
    DIRT,
    STONE,
    WOOD,
    LEAVES,
    SAND,
    GRAVEL,
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
    OBSIDIAN
};

struct Block {
    BlockType type;

    Block() : type(BlockType::AIR) {}
    Block(BlockType t) : type(t) {}

    bool isSolid() const {
        return type != BlockType::AIR && type != BlockType::TORCH && type != BlockType::WATER;
    }

    bool isWater() const {
        return type == BlockType::WATER;
    }

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
            case BlockType::SAND:   return sf::Color(208, 189, 137);
            case BlockType::GRAVEL: return sf::Color(128, 122, 116);
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
            case BlockType::STONE:  index = 7; break;   // камень
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
            default:                index = 1; break;
        }
        return sf::IntRect({index * TEXTURE_SIZE, 0}, {TEXTURE_SIZE, TEXTURE_SIZE});
    }
};