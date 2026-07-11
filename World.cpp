#include "World.hpp"
#include <cmath>
#include <cstdlib>
#include <random>

World::World()
    : map(WORLD_HEIGHT, std::vector<Block>(WORLD_WIDTH, Block(BlockType::AIR)))
    , fallStepTimer(0.f)
{
    std::random_device rd;
    seed = rd(); // каждый новый мир — случайный, если сид не задан явно
    generateWorld();
}

World::World(unsigned int worldSeed)
    : map(WORLD_HEIGHT, std::vector<Block>(WORLD_WIDTH, Block(BlockType::AIR)))
    , seed(worldSeed)
    , fallStepTimer(0.f)
{
    generateWorld();
}

float World::hash(int n) const {
    // Целочисленный хэш: из (n, seed) получаем "случайное" число в [-1, 1].
    // Один и тот же n+seed всегда даёт один и тот же результат — это и делает шум воспроизводимым.
    unsigned int h = static_cast<unsigned int>(n) * 374761393u + seed * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h = h ^ (h >> 16);
    return (float)(h % 20000) / 10000.f - 1.f; // -> диапазон [-1, 1]
}

float World::smoothNoise(float x) const {
    int xi = (int)std::floor(x);
    float xf = x - xi;

    float v0 = hash(xi);
    float v1 = hash(xi + 1);

    // smoothstep вместо линейной интерполяции — убирает резкие "изломы" между узлами решётки
    float t = xf * xf * (3.f - 2.f * xf);
    return v0 + (v1 - v0) * t;
}

float World::noise(float x) const {
    // fBm (fractal Brownian motion): складываем несколько "слоёв" шума.
    // Первый слой — крупные плавные холмы, каждый следующий — мельче и слабее (детали/шероховатость).
    float total     = 0.f;
    float amplitude = 8.f;
    float frequency = 0.03f;
    float maxAmp    = 0.f;

    for (int octave = 0; octave < 4; octave++) {
        total   += smoothNoise(x * frequency) * amplitude;
        maxAmp  += amplitude;
        amplitude *= 0.5f;  // каждая следующая октава вдвое тише
        frequency *= 2.3f;  // ...и заметно чаще
    }

    return (total / maxAmp) * 12.f; // нормализуем и задаём итоговый масштаб рельефа
}

bool World::isCave(int x, int y) const {
    // Дешёвый "2D" шум: объединяем x и y в одну координату двумя разными способами
    // и складываем — получаются вытянутые пятна, похожие на пещерные полости.
    float n = smoothNoise(x * 0.15f + y * 0.20f + (float)(seed % 1000))
            + smoothNoise(x * 0.35f - y * 0.30f);
    return n > 0.9f; // чем порог выше — тем пещеры реже и меньше
}

bool World::isBeach(int x) const {
    // Несколько гарантированных "пляжных пятен" на карту (не зависит от удачного/неудачного
    // порога шума — раньше при некоторых сидах пляжей не было вообще).
    const int NUM_BEACHES   = 3;
    const int BEACH_RADIUS  = 5;

    for (int i = 0; i < NUM_BEACHES; i++) {
        float h = hash(i * 7919 + 13); // своё псевдослучайное число для каждого пятна
        int center = (int)((h + 1.f) * 0.5f * WORLD_WIDTH);
        if (std::abs(x - center) <= BEACH_RADIUS) return true;
    }
    return false;
}

bool World::isGravelPocket(int x, int y) const {
    // Отдельный, независимый от пещер шум — карманы гравия в толще камня.
    // Используем другой сдвиг по seed, чтобы не совпадать с узором пещер.
    float n = smoothNoise(x * 0.22f + y * 0.28f + (float)(seed % 777) + 1000.f)
            + smoothNoise(x * 0.4f - y * 0.18f + 250.f);
    return n > 1.05f; // делаем реже и мельче пещер
}

BlockType World::oreAt(int x, int y, int surfaceY) const {
    int depth = y - surfaceY; // насколько глубоко под поверхностью

    // Отдельный шумовой "жила"-фактор: если ниже порога — руды нет
    float vein = smoothNoise(x * 0.5f + y * 0.5f + (float)(seed % 333) + 2000.f)
               + smoothNoise(x * 0.9f - y * 0.7f + 400.f);

    // Каждый тип руды — своя редкость и диапазон глубин.
    // Проверяем от самой ценной/глубокой к самой частой.
    if (depth > 40 && vein > 1.35f) return BlockType::DIAMOND_ORE;
    if (depth > 30 && vein > 1.28f) return BlockType::GOLD_ORE;
    if (depth > 12 && vein > 1.15f) return BlockType::IRON_ORE;
    if (depth > 6  && vein > 1.02f) return BlockType::COAL_ORE;
    return BlockType::AIR; // руды нет — останется обычный камень
}

int World::biomeAt(int x) const {
    // Биомы больше не накладываются друг на друга (раньше снег был отдельным шумом
    // поверх остальных трёх и мог "врезаться" в середину акации/сакуры). Теперь мир
    // просто поделен на подряд идущие непересекающиеся участки, у каждого — один биом:
    // 0 — обычный дуб, 1 — акация, 2 — сакура, 3 — снег.
    const int SEGMENT_WIDTH = 70;
    int seg = x / SEGMENT_WIDTH;

    auto rawBiome = [&](int s) -> int {
        unsigned int h = (unsigned int)((s + 1) * 2654435761u + seed * 668265263u);
        h ^= h >> 13; h *= 1274126177u; h ^= h >> 16;
        return (int)(h % 4u);
    };

    // Идём от начала мира и досчитываем до нужного участка, каждый раз сверяясь
    // с итоговым (а не сырым) биомом предыдущего — иначе после сдвига на "не повторять"
    // соседние участки всё равно иногда могли случайно совпасть.
    int prevFinal = -1;
    int biome = 0;
    for (int s = 0; s <= seg; s++) {
        biome = rawBiome(s);
        if (s > 0 && biome == prevFinal)
            biome = (biome + 1) % 4;
        prevFinal = biome;
    }
    return biome;
}

void World::generateWorld() {
    const int BASE_HEIGHT = WORLD_HEIGHT / 2;
    const int DIRT_DEPTH  = 5;

    std::vector<int> surfaceY(WORLD_WIDTH);

    for (int x = 0; x < WORLD_WIDTH; x++) {
        surfaceY[x] = BASE_HEIGHT + (int)noise((float)x);
        bool beach = isBeach(x);

        for (int y = 0; y < WORLD_HEIGHT; y++) {
            if (y < surfaceY[x])
                map[y][x] = Block(BlockType::AIR);
            else if (y == surfaceY[x])
                map[y][x] = Block(beach ? BlockType::SAND : BlockType::GRASS);
            else if (y < surfaceY[x] + DIRT_DEPTH)
                map[y][x] = Block(beach ? BlockType::SAND : BlockType::DIRT);
            else if (y > surfaceY[x] + DIRT_DEPTH + 3 && isCave(x, y))
                map[y][x] = Block(BlockType::AIR); // пещера — вырезаем камень
            else if (y > surfaceY[x] + DIRT_DEPTH + 3 && isGravelPocket(x, y))
                map[y][x] = Block(BlockType::GRAVEL); // карман гравия в толще камня
            else {
                // Обычный камень, но иногда — жила руды (зависит от глубины)
                BlockType ore = oreAt(x, y, surfaceY[x]);
                map[y][x] = Block(ore != BlockType::AIR ? ore : BlockType::STONE);
            }
        }
    }

    // Водоёмы: там, где рельеф опустился ниже "уровня моря", заполняем впадину водой.
    const int SEA_LEVEL = BASE_HEIGHT + 3; // всё, что ниже этой линии и есть воздух — станет водой
    for (int x = 0; x < WORLD_WIDTH; x++) {
        for (int y = SEA_LEVEL; y < surfaceY[x]; y++) {
            if (map[y][x].type == BlockType::AIR)
                map[y][x] = Block(BlockType::WATER);
        }
        // Верхний слой земли под водой заменим на песок (дно озера)
        if (surfaceY[x] > SEA_LEVEL) {
            if (map[surfaceY[x]][x].type == BlockType::GRASS)
                map[surfaceY[x]][x] = Block(BlockType::SAND);
        }
    }

    // Деревья — высокие с круглой кроной (или другой формой/цветом — см. биом).
    // Частота одинаковая для всех биомов — как и было изначально, чтобы не мешать ходить.
    srand(seed);
    for (int x = 6; x < WORLD_WIDTH - 6; x++) {
        if (rand() % 8 != 0) continue; // как и было изначально — не слишком часто

        int sy = surfaceY[x];
        int treeHeight = 7 + rand() % 4; // высокие деревья 7-10
        int biome = biomeAt(x);

        // Ствол
        for (int y = sy - treeHeight; y < sy; y++) {
            if (y >= 0 && y < WORLD_HEIGHT)
                map[y][x] = Block(BlockType::WOOD);
        }

        // Крона: обычный дуб — круглая; акация — приплюснутая и широкая "зонтиком";
        // сакура — пышная и чуть шире дуба, только розовая
        int crownY = sy - treeHeight;
        int rX = 3, rY = 3;
        BlockType leafType = BlockType::LEAVES;

        if (biome == 1) {
            leafType = BlockType::LEAVES_ACACIA;
            rX = 4; rY = 3;        // широкая, но округлая крона-кустик (не плоский блин)
            crownY += 1;           // сидит чуть ниже верхушки ствола
        } else if (biome == 2) {
            leafType = BlockType::LEAVES_SAKURA;
            rX = 4; rY = 3;       // пышнее обычного дуба
        } else if (biome == 3) {
            leafType = BlockType::LEAVES_SNOWY;
            rX = 3; rY = 3;       // как обычный дуб — тёмная хвоя со снегом сверху достаточно отличает
        }

        for (int ly = crownY - rY; ly <= crownY + rY; ly++) {
            for (int lx = x - rX; lx <= x + rX; lx++) {
                if (ly < 0 || ly >= WORLD_HEIGHT) continue;
                if (lx < 0 || lx >= WORLD_WIDTH) continue;
                // Проверяем принадлежность эллипсу
                float dx = (float)(lx - x) / rX;
                float dy = (float)(ly - crownY) / rY;
                if (dx*dx + dy*dy <= 1.2f && map[ly][lx].type == BlockType::AIR)
                    map[ly][lx] = Block(leafType);
            }
        }
    }
}

void World::updateFallingBlocks(float deltaTime) {
    const float FALL_STEP_INTERVAL = 0.06f; // как часто падающий блок сдвигается на 1 клетку вниз

    fallStepTimer += deltaTime;
    if (fallStepTimer < FALL_STEP_INTERVAL) return;
    fallStepTimer = 0.f;

    // Идём снизу вверх: так блок, который только что упал на строку ниже,
    // не будет обработан повторно в этом же тике (он "старее" по проходу).
    for (int y = WORLD_HEIGHT - 2; y >= 0; y--) {
        for (int x = 0; x < WORLD_WIDTH; x++) {
            Block& block = map[y][x];
            if (!block.isFalling()) continue;
            if (map[y + 1][x].isSolid()) continue; // снизу есть опора — не падаем

            map[y + 1][x] = block;
            map[y][x] = Block(BlockType::AIR);
        }
    }
}

bool World::inBounds(int x, int y) const {
    return x >= 0 && x < WORLD_WIDTH && y >= 0 && y < WORLD_HEIGHT;
}

Block& World::getBlock(int x, int y) {
    return map[y][x];
}

const Block& World::getBlock(int x, int y) const {
    return map[y][x];
}

void World::setBlock(int x, int y, BlockType type) {
    if (inBounds(x, y))
        map[y][x].type = type;
}