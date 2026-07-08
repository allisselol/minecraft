#pragma once
#include <SFML/Graphics.hpp>
#include <algorithm>

class HUD {
private:
    int maxHealth = 10;
    int health    = 10;
    int maxFood   = 10;
    int food      = 10;

    float hungerTimer = 0.f;
    float starveTimer = 0.f;
    float regenTimer  = 0.f;

    // Единая полоса внизу: 20px (сердечки) + 56px (хотбар) = 76px
    static constexpr float PANEL_H = 76.f;
    static constexpr float PANEL_Y = 600.f - PANEL_H;

    void drawHeart(sf::RenderWindow& window, float x, float y, bool full) {
        bool p[7][8] = {
            {0,1,1,0,0,1,1,0},
            {1,1,1,1,1,1,1,1},
            {1,1,1,1,1,1,1,1},
            {1,1,1,1,1,1,1,1},
            {0,1,1,1,1,1,1,0},
            {0,0,1,1,1,1,0,0},
            {0,0,0,1,1,0,0,0},
        };
        sf::Color fill = full ? sf::Color(210, 25, 25) : sf::Color(55, 12, 12);
        sf::Color hi   = full ? sf::Color(255, 120, 120) : sf::Color(80, 25, 25);

        sf::RectangleShape px({1.6f, 1.6f});
        for (int r = 0; r < 7; r++) {
            for (int c = 0; c < 8; c++) {
                if (!p[r][c]) continue;
                px.setFillColor((r == 1 && (c == 1 || c == 2)) ? hi : fill);
                px.setPosition({x + c * 1.6f, y + r * 1.6f});
                window.draw(px);
            }
        }
    }

    void drawFood(sf::RenderWindow& window, float x, float y, bool full) {
        bool p[7][7] = {
            {0,0,1,1,1,0,0},
            {0,1,1,1,1,1,0},
            {0,1,1,1,1,1,0},
            {0,0,1,1,1,0,0},
            {0,0,0,1,0,0,0},
            {0,0,1,1,0,0,0},
            {0,1,1,0,0,0,0},
        };
        sf::Color c = full ? sf::Color(200, 130, 50) : sf::Color(60, 40, 15);
        sf::RectangleShape px({1.6f, 1.6f});
        for (int r = 0; r < 7; r++) {
            for (int cc = 0; cc < 7; cc++) {
                if (!p[r][cc]) continue;
                px.setFillColor(c);
                px.setPosition({x + cc * 1.6f, y + r * 1.6f});
                window.draw(px);
            }
        }
    }

public:
    static float getPanelY() { return PANEL_Y; }
    static float getPanelH() { return PANEL_H; }

    int getHealth() const { return health; }
    int getFood()   const { return food; }
    void setHealth(int h) { health = std::max(0, std::min(maxHealth, h)); }
    void setFood(int f)   { food   = std::max(0, std::min(maxFood, f)); }
    void damage(int v)    { setHealth(health - v); }
    void heal(int v)      { setHealth(health + v); }
    bool isDead() const   { return health <= 0; }

    // Вызывать раз в кадр: голод медленно падает, при полном голоде — голодание,
    // при сытости — медленная пассивная регенерация здоровья
    void update(float deltaTime) {
        const float HUNGER_INTERVAL = 45.f; // 1 очко еды каждые 45 секунд (было 12 — слишком быстро)
        const float STARVE_INTERVAL = 4.f;  // при голоде — 1 сердце каждые 4 секунды
        const float REGEN_INTERVAL  = 3.f;  // при сытости — 1 сердце каждые 3 секунды

        hungerTimer += deltaTime;
        if (hungerTimer >= HUNGER_INTERVAL) {
            hungerTimer -= HUNGER_INTERVAL;
            if (food > 0) food--;
        }

        if (food <= 0) {
            // Голодание не убивает — останавливается на 1 сердце
            starveTimer += deltaTime;
            if (starveTimer >= STARVE_INTERVAL) {
                starveTimer -= STARVE_INTERVAL;
                if (health > 1) health--;
            }
        } else {
            starveTimer = 0.f;
        }

        if (food >= 8 && health > 0 && health < maxHealth) {
            regenTimer += deltaTime;
            if (regenTimer >= REGEN_INTERVAL) {
                regenTimer -= REGEN_INTERVAL;
                health++;
                food = std::max(0, food - 1); // регенерация немного "ест" сытость
            }
        } else {
            regenTimer = 0.f;
        }
    }

    // Рисует фон полосы — вызывается ДО хотбара
    void drawPanel(sf::RenderWindow& window) {
        sf::View ui(sf::FloatRect({0.f, 0.f}, {800.f, 600.f}));
        sf::View prev = window.getView();
        window.setView(ui);

        // Сплошная тёмная полоса до самого низа
        sf::RectangleShape bg({800.f, PANEL_H});
        bg.setPosition({0.f, PANEL_Y});
        bg.setFillColor(sf::Color(35, 35, 35, 245));
        window.draw(bg);

        // Тонкая светлая линия сверху полосы
        sf::RectangleShape line({800.f, 1.f});
        line.setPosition({0.f, PANEL_Y});
        line.setFillColor(sf::Color(90, 90, 90));
        window.draw(line);

        window.setView(prev);
    }

    // Рисует сердечки и еду — вызывается ПОСЛЕ хотбара
    void draw(sf::RenderWindow& window) {
        sf::View ui(sf::FloatRect({0.f, 0.f}, {800.f, 600.f}));
        sf::View prev = window.getView();
        window.setView(ui);

        float rowY = PANEL_Y + 4.f;

        // Сердечки слева, компактно
        for (int i = 0; i < maxHealth; i++)
            drawHeart(window, 12.f + i * 15.f, rowY, i < health);

        // Еда справа, компактно
        for (int i = 0; i < maxFood; i++)
            drawFood(window, 800.f - 12.f - (maxFood - i) * 14.f, rowY, i < food);

        window.setView(prev);
    }
};