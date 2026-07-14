#include "Game.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <random>

// Погода и небо (облака, дождь, звёзды, солнце/луна).
// Вынесено из Game.cpp, чтобы тот не разрастался ещё больше.

void Game::updateWeather(float deltaTime) {
    // Облака медленно плывут вправо, при уходе за край возвращаются слева
    for (auto& c : cloudPositions) {
        c.x += 8.f * deltaTime; // скорость ветра
        if (c.x > 850.f) {
            c.x = -100.f;
            c.y = 40.f + (float)(rand() % 180);
        }
    }

    // Смена погоды примерно раз в 30-60 секунд
    weatherTimer -= deltaTime;
    if (weatherTimer <= 0.f) {
        raining = (rand() % 100) < 35; // ~35% шанс дождя
        weatherTimer = 30.f + rand() % 30;
        if (raining) sounds.startRain();
        else         sounds.stopRain();
    }

    // Капли дождя падают вниз (и чуть вбок), при уходе вниз — заново сверху
    if (raining) {
        for (auto& d : rainDrops) {
            d.y += d.speed * deltaTime;
            d.x += 60.f * deltaTime; // наклон дождя ветром
            if (d.y > 600.f) { d.y = -10.f; d.x = (float)(rand() % 900) - 50.f; }
            if (d.x > 820.f) d.x -= 870.f;
        }
    }
}

void Game::drawClouds() {
    sf::View uiView(sf::FloatRect({0.f, 0.f}, {800.f, 600.f}));
    window.setView(uiView);

    float brightness = getBrightness();
    std::uint8_t base = (std::uint8_t)(215 + 40 * brightness);
    std::uint8_t alpha = raining ? 245 : 205;
    sf::Color body(base, base, (std::uint8_t)std::min(255, base + 8), alpha);
    sf::Color shade((std::uint8_t)(base - 45), (std::uint8_t)(base - 45), (std::uint8_t)(base - 32), alpha);

    // Пиксельное облако: карта 10x5 «пикселей». 1 = тело, 2 = тень (нижняя кромка), 0 = пусто.
    // Края намеренно рваные/ступенчатые для пиксель-арт вида.
    const int CW = 10, CH = 5;
    const float U = 11.f; // размер одного «пикселя» облака
    int map[CH][CW] = {
        {0,0,1,1,0,0,1,1,0,0},
        {0,1,1,1,1,1,1,1,1,0},
        {1,1,1,1,1,1,1,1,1,1},
        {1,1,1,1,1,1,1,1,1,1},
        {0,2,2,0,2,2,0,2,2,0},
    };

    for (const auto& c : cloudPositions) {
        for (int ry = 0; ry < CH; ry++) {
            for (int rx = 0; rx < CW; rx++) {
                int v = map[ry][rx];
                if (v == 0) continue;
                sf::RectangleShape px({U + 0.5f, U + 0.5f});
                px.setPosition({c.x + rx * U, c.y + ry * U});
                px.setFillColor(v == 2 ? shade : body);
                window.draw(px);
            }
        }
    }

    window.setView(camera);
}

void Game::drawRain() {
    if (!raining) return;

    // Для обрезки капель по земле нужно знать, где поверхность в каждой экранной колонке.
    // Переводим экранную X в мировую, находим верхний твёрдый блок, переводим его Y обратно в экран.
    sf::Vector2f viewCenter = camera.getCenter();
    sf::Vector2f viewSize   = camera.getSize();
    float viewLeft = viewCenter.x - viewSize.x / 2.f;
    float viewTop  = viewCenter.y - viewSize.y / 2.f;
    // Коэффициенты перевода "мир -> экран(800x600)"
    float sx = 800.f / viewSize.x;
    float sy = 600.f / viewSize.y;

    auto groundScreenY = [&](float screenX) -> float {
        // экран -> мир
        float worldX = viewLeft + (screenX / 800.f) * viewSize.x;
        int bx = (int)(worldX / BLOCK_SIZE);
        if (!world.inBounds(bx, 0)) return 600.f;
        // ищем первый твёрдый блок сверху вниз в этой колонке
        int startBY = std::max(0, (int)(viewTop / BLOCK_SIZE));
        for (int by = startBY; by < WORLD_HEIGHT; by++) {
            const Block& b = world.getBlock(bx, by);
            // Дождь протекает сквозь кроны деревьев (листва) и стволы — они не считаются "землёй"
            if (b.type == BlockType::LEAVES || b.type == BlockType::WOOD) continue;
            if (b.isSolid()) {
                float worldY = by * (float)BLOCK_SIZE;
                return (worldY - viewTop) * sy; // мир -> экран
            }
        }
        return 600.f; // земли нет — капля падает до низа экрана
    };

    sf::View uiView(sf::FloatRect({0.f, 0.f}, {800.f, 600.f}));
    window.setView(uiView);

    sf::VertexArray lines(sf::PrimitiveType::Lines, rainDrops.size() * 2);
    std::size_t vi = 0;
    for (size_t i = 0; i < rainDrops.size(); i++) {
        const auto& d = rainDrops[i];
        float limit = groundScreenY(d.x); // до какой Y можно рисовать каплю
        if (d.y >= limit) continue;        // капля уже "под землёй" — не рисуем

        float tailY = d.y + 24.f;
        if (tailY > limit) tailY = limit;  // хвост капли обрезаем по поверхности
        float tailX = d.x - 6.f * ((tailY - d.y) / 24.f);

        lines[vi].position     = {d.x, d.y};
        lines[vi].color        = sf::Color(170, 200, 245, 200);
        vi++;
        lines[vi].position     = {tailX, tailY};
        lines[vi].color        = sf::Color(150, 185, 235, 120);
        vi++;
    }
    lines.resize(vi); // оставляем только реально нарисованные вершины
    window.draw(lines);

    window.setView(camera);
}

void Game::drawSky() {
    // Небо — плавный вертикальный градиент через один квад (интерполяция цвета по вершинам),
    // вместо полос это даёт по-настоящему гладкий переход без ступенек.
    sf::View uiView(sf::FloatRect({0.f, 0.f}, {800.f, 600.f}));
    window.setView(uiView);

    float brightness = getBrightness();

    auto lerpColor = [](sf::Color a, sf::Color b, float t) {
        return sf::Color(
            (std::uint8_t)(a.r + (b.r - a.r) * t),
            (std::uint8_t)(a.g + (b.g - a.g) * t),
            (std::uint8_t)(a.b + (b.b - a.b) * t));
    };

    sf::Color dayTop(100, 165, 225),   dayBottom(195, 215, 235);
    sf::Color nightTop(8, 8, 32),      nightBottom(40, 32, 62);

    sf::Color topColor    = lerpColor(nightTop, dayTop, brightness);
    sf::Color bottomColor = lerpColor(nightBottom, dayBottom, brightness);

    sf::VertexArray gradient(sf::PrimitiveType::TriangleStrip, 4);
    gradient[0].position = {0.f, 0.f};     gradient[0].color = topColor;
    gradient[1].position = {800.f, 0.f};   gradient[1].color = topColor;
    gradient[2].position = {0.f, 600.f};   gradient[2].color = bottomColor;
    gradient[3].position = {800.f, 600.f}; gradient[3].color = bottomColor;
    window.draw(gradient);

    // Солнце и луна двигаются по окружности с постоянной угловой скоростью.
    // Важно: радиус по X и по Y должен быть ОДИНАКОВЫМ — иначе (даже со сдвигом фаз)
    // скорость на дуге всё равно будет неравномерной, "зависая" то на восходе/закате,
    // то в зените. Заодно радиус поменьше держит светила ближе к центру, а не у самых краёв.
    float angle      = (dayTime / DAY_LENGTH) * 2.f * 3.14159265f;
    float R          = 190.f;
    float sunX       = 400.f - std::sin(angle) * R;
    float sunY       = 290.f + std::cos(angle) * R;
    float moonAngle  = angle + 3.14159265f;
    float moonX      = 400.f - std::sin(moonAngle) * R;
    float moonY      = 290.f + std::cos(moonAngle) * R;
    float moonBright = 1.f - brightness;

    sf::RenderStates addStates;
    addStates.blendMode = sf::BlendAdd;

    auto drawGlowDisc = [&](float cx, float cy, float strength, sf::Color core, sf::Color glow) {
        if (strength <= 0.02f) return;
        float radius = 100.f; // насколько далеко расходится свечение солнца/луны
        sf::Sprite glowSprite(glowTexture);
        glowSprite.setOrigin({128.f, 128.f});
        float scale = radius / 128.f;
        glowSprite.setScale({scale, scale});
        glowSprite.setPosition({cx, cy});
        glowSprite.setColor(sf::Color(glow.r, glow.g, glow.b, (std::uint8_t)(190.f * strength)));
        window.draw(glowSprite, addStates);

        // Квадратное, чуть "пиксельное" солнце/луна — как в оригинале, не гладкий круг
        float discSize = 30.f;
        sf::RectangleShape disc({discSize, discSize});
        disc.setOrigin({discSize / 2.f, discSize / 2.f});
        disc.setPosition({cx, cy});
        disc.setFillColor(core);
        window.draw(disc);

        // Небольшая внутренняя рамка чуть темнее — намёк на пиксельную текстуру, а не заливку
        sf::Color shade(
            (std::uint8_t)(core.r * 0.85f),
            (std::uint8_t)(core.g * 0.85f),
            (std::uint8_t)(core.b * 0.85f));
        float innerSize = discSize - 8.f;
        sf::RectangleShape inner({innerSize, innerSize});
        inner.setOrigin({innerSize / 2.f, innerSize / 2.f});
        inner.setPosition({cx, cy});
        inner.setFillColor(shade);
        window.draw(inner);
    };

    drawGlowDisc(sunX, sunY, brightness, sf::Color(255, 250, 210), sf::Color(255, 225, 120));
    drawGlowDisc(moonX, moonY, moonBright * 0.8f, sf::Color(225, 230, 245), sf::Color(175, 195, 235));

    // Звёзды — мягко мерцают, а не просто статично включены/выключены
    if (brightness < 0.55f) {
        float starVisibility = (0.55f - brightness) / 0.55f;
        sf::CircleShape star(1.3f);
        star.setOrigin({1.3f, 1.3f});
        for (size_t i = 0; i < starPositions.size(); i++) {
            float twinkle = 0.6f + 0.4f * std::sin(animTime * 1.5f + (float)i * 0.9f);
            std::uint8_t a = (std::uint8_t)(220.f * starVisibility * twinkle);
            star.setPosition(starPositions[i]);
            star.setFillColor(sf::Color(255, 255, 255, a));
            window.draw(star);
        }
    }

    window.setView(camera);
}
