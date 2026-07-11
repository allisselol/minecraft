#pragma once
#include <vector>
#include "Block.hpp"

constexpr int WORLD_WIDTH  = 400; // мир заметно шире — чтобы по пути попадалось несколько разных биомов
constexpr int WORLD_HEIGHT = 100;
constexpr int BLOCK_SIZE   = 32;

class World {
private:
    std::vector<std::vector<Block>> map; // map[y][x]
    unsigned int seed; // сид мира — определяет и рельеф, и пещеры

    // Псевдослучайное значение [-1, 1] для целочисленной координаты (зависит от seed)
    float hash(int n) const;

    // Сглаженный (интерполированный) 1D-шум на основе hash()
    float smoothNoise(float x) const;

    // Многооктавный (фрактальный) шум рельефа — сумма нескольких sloyov smoothNoise
    // разной частоты и амплитуды, поэтому рельеф выглядит естественнее, чем от чистых синусоид
    float noise(float x) const;

    // Есть ли пещера в данной точке (используется только под землёй, ниже слоя дёрна)
    bool isCave(int x, int y) const;

    // "Пляжный" биом: широкие полосы по x, где вместо травы/земли — песок
    bool isBeach(int x) const;

    // Карманы гравия в толще камня (отдельный шум, реже и мельче пещер)
    bool isGravelPocket(int x, int y) const;

    // Тип руды в данной точке камня (или AIR, если руды нет).
    // Зависит от глубины: уголь/железо ближе к поверхности, золото/алмаз глубже.
    BlockType oreAt(int x, int y, int surfaceY) const;

    float fallStepTimer; // накопленное время до следующего "тика" падения блоков

public:
    World();
    explicit World(unsigned int worldSeed); // для воспроизводимого мира по конкретному сиду

    void generateWorld();

    // Двигает падающие блоки (песок/гравий) на одну клетку вниз за "тик", если под ними пусто
    void updateFallingBlocks(float deltaTime);

    Block& getBlock(int x, int y);
    const Block& getBlock(int x, int y) const;
    void setBlock(int x, int y, BlockType type);

    bool inBounds(int x, int y) const;

    int getWidth()  const { return WORLD_WIDTH; }
    int getHeight() const { return WORLD_HEIGHT; }
    unsigned int getSeed() const { return seed; }

    // Какой "лесной" биом в этом x: 0 — обычный дуб, 1 — акация (саванна), 2 — сакура.
    // Публичный — нужен и генерации мира, и отрисовке (чтобы тонировать траву под биом).
    int biomeAt(int x) const;
};