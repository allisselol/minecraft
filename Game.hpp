#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <map>
#include <utility>
#include <string>
#include "World.hpp"
#include "Player.hpp"
#include "Inventory.hpp"
#include "SoundManager.hpp"
#include "HUD.hpp"
#include "Particle.hpp"
#include "Chicken.hpp"
#include "Cow.hpp"
#include "Sheep.hpp"
#include "Enemy.hpp"

class Game {
private:
    sf::RenderWindow window;
    sf::View         camera;
    World            world;
    Player           player;
    Inventory        inventory;
    SoundManager     sounds;
    HUD              hud;
    bool             inventoryOpen;

    // === Сундуки ===
    // Содержимое каждого поставленного сундука хранится отдельно от блока в мире,
    // по его координатам (bx, by). Появляется в момент первого открытия.
    static constexpr int CHEST_SLOT_COUNT = 27; // 3 ряда по 9, как маленький сундук в оригинале
    std::map<std::pair<int, int>, std::vector<InventorySlot>> chestStorage;
    bool chestOpen = false;
    std::pair<int, int> openChestPos{-1, -1};

    // === Верстак ===
    // Само окно (сетка 3x3) верстак не хранит по координатам — как и в оригинале,
    // содержимое сетки не сохраняется между открытиями, поэтому достаточно одного
    // общего состояния "открыто/закрыто".
    bool workbenchOpen = false;

    // === Печь ===
    // В отличие от верстака, печь хранит своё состояние по координатам (как сундук) —
    // предметы и таймеры готовки не пропадают, пока печь стоит в мире.
    struct FurnaceState {
        std::vector<InventorySlot> slots; // 0 = сырьё, 1 = топливо, 2 = результат
        float burnTimeLeft = 0.f;         // сколько ещё "гореть" текущей порции топлива
        float burnTimeMax  = 0.f;         // сколько горела эта порция изначально (для индикатора)
        float cookProgress = 0.f;         // накопленное время готовки текущего сырья
        float crackleTimer = 0.f;         // через сколько секунд снова "треснуть" (пока горит)
        FurnaceState() : slots(3, InventorySlot(BlockType::AIR, 0)) {}
    };
    std::map<std::pair<int, int>, FurnaceState> furnaceStorage;
    bool furnaceOpen = false;
    std::pair<int, int> openFurnacePos{-1, -1};
    void updateFurnaces(float deltaTime);

    sf::Texture blockTexture; // атлас текстур блоков (textures/blocks.png)
    sf::Texture glowTexture;  // процедурный мягкий радиальный градиент — для солнца/луны/факелов/печи,
                              // без колец: одна текстура с плавным затуханием альфы к краям
    sf::Texture sakuraLeafTexture; // тайл листвы, перекрашенный в малиновый с сохранением тёмных "дырок"

    std::vector<Particle> particles;
    void spawnBreakParticles(int bx, int by, const Block& block);
    void updateParticles(float deltaTime);
    void drawParticles();

    float animTime; // общее время для анимаций окружения (покачивание травы и т.п.)
    float spawnX, spawnY; // точка возрождения игрока после смерти

    // Цикл дня и ночи
    static constexpr float DAY_LENGTH = 180.f; // полный цикл день+ночь, в секундах
    float dayTime; // 0..DAY_LENGTH
    float getBrightness() const; // 0 = глубокая ночь, 1 = полдень
    std::vector<sf::Vector2f> starPositions;

    // Облака: позиции на небе (в координатах UI 800x600), медленно плывут по X
    std::vector<sf::Vector2f> cloudPositions;
    // Дождь: капли в экранных координатах, падают вниз
    struct RainDrop { float x, y, speed; };
    std::vector<RainDrop> rainDrops;
    bool raining = false;      // идёт ли дождь сейчас
    float weatherTimer = 0.f;  // таймер смены погоды
    void updateWeather(float deltaTime);
    void drawClouds();
    void drawRain();

    std::vector<Chicken> chickens;
    // Ищет первый настоящий блок земли в столбце bx сверху вниз — пропускает листву и стволы
    // деревьев, чтобы мобы не заспавнились на кроне дерева, а только на реальной земле.
    int findGroundSurface(int bx) const;

    void spawnChicken();                 // ищет случайное место на поверхности и добавляет цыплёнка
    void killChicken(size_t index);       // "добыча" цыплёнка: частицы + еда + удаление

    std::vector<Cow> cows;
    void spawnCow();                     // ищет случайное место на поверхности и добавляет корову
    void killCow(size_t index);          // "добыча" коровы: частицы + сырая говядина + удаление

    std::vector<Sheep> sheep;
    void spawnSheep();                   // ищет случайное место на поверхности и добавляет овцу
    void killSheep(size_t index);        // "добыча" овцы: частицы + шерсть + удаление

    std::vector<Enemy> enemies;
    void spawnEnemy();                   // спавнит врага на поверхности подальше от игрока
    void killEnemy(size_t index);        // смерть врага: частицы + удаление
    float enemySpawnTimer = 0.f;         // таймер появления новых врагов (чаще ночью)

    // Стрелы лучников
    struct Arrow { float x, y, vx, vy, life; };
    std::vector<Arrow> arrows;
    float chickenRespawnTimer;            // через сколько секунд появится следующий цыплёнок (если их мало)
    static constexpr int MAX_CHICKENS = 6;
    static constexpr int FOOD_PER_CHICKEN = 1; // сколько сырого мяса выпадает с одной курицы

    float cowRespawnTimer = 0.f;          // через сколько секунд появится следующая корова (если их мало)
    static constexpr int MAX_COWS = 4;
    static constexpr int BEEF_PER_COW = 1; // сколько сырой говядины выпадает с одной коровы

    float sheepRespawnTimer = 0.f;        // через сколько секунд появится следующая овца (если их мало)
    static constexpr int MAX_SHEEP = 6; // было 4 — маловато
    static constexpr int WOOL_PER_SHEEP = 3; // сколько шерсти выпадает с одной овцы (для быстрого крафта кровати)

    float petalTimer = 0.f; // таймер падающих лепестков сакуры (только в биоме сакуры рядом с игроком)

    // === Сон ===
    static constexpr float SLEEP_DURATION = 2.5f; // сколько секунд игрок лежит в кровати перед рассветом
    float sleepTimer = 0.f;
    float snowTimer = 0.f;  // таймер падающего снега (только в снежном биоме рядом с игроком)

    // Заложенный и подожжённый TNT — ждёт взрыва
    struct ActiveTNT {
        int bx, by;
        float fuseTime;
        float maxFuseTime;
    };
    std::vector<ActiveTNT> activeTNT;

    void armTnt(int bx, int by, float fuse);
    void updateExplosives(float deltaTime);
    void explodeAt(int bx, int by);

    // Лёгкая тряска камеры при взрыве
    float shakeTime;
    float shakeMagnitude;

    // Максимальная дальность, на которой можно ломать/ставить блоки (в блоках)
    static constexpr float MAX_REACH_BLOCKS = 5.f;
    bool isWithinReach(int bx, int by) const;

    sf::RectangleShape cursor;

    sf::Font font;       // для текста в меню (кнопки/заголовок) — не используется в самой игре
    bool fontLoaded = false;
    sf::Texture menuBgTexture; // фоновая картинка для экранов меню
    bool menuBgLoaded = false;

    void handleEvents();
    void handleBlockInteraction(sf::Mouse::Button button, sf::Vector2i mousePixelPos);
    void update(float deltaTime);
    void drawSky();
    void render();

    // === Экраны меню и сохранение/загрузка ===
    enum class GameState { MainMenu, WorldMenu, Playing };
    GameState state = GameState::MainMenu;

    static const std::string SAVE_FILE; // путь к файлу сохранения на диске

    sf::FloatRect playButtonBox;      // "Играть" на главном экране
    sf::FloatRect continueButtonBox;  // "Продолжить игру" (только если есть сохранение)
    sf::FloatRect newWorldButtonBox;  // "Новый мир"

    bool hasSaveFile() const;
    void startNewWorld();  // генерирует свежий мир, спавнит игрока/мобов заново
    void saveGame() const; // сохраняет текущее состояние в SAVE_FILE
    bool loadGame();       // загружает состояние из SAVE_FILE; false, если не получилось

    void renderMainMenu();
    void renderWorldMenu();
    void handleMenuClick(sf::Vector2i pixelPos);

public:
    Game();
    void run();
};