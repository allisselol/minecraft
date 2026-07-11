#pragma once
#include <SFML/Graphics.hpp>

// Маленькая частица-обломок, разлетающаяся при разрушении блока.
// Живёт недолго, падает под гравитацией и постепенно исчезает (fade out).
struct Particle {
    sf::Vector2f position;
    sf::Vector2f velocity;
    sf::Color    color;
    float        size;
    float        lifetime;    // сколько секунд ещё проживёт
    float        maxLifetime; // изначальная продолжительность жизни (для расчёта прозрачности)
    float        gravityScale = 1.f; // 1 = обычные обломки; маленькое значение — медленно парящие лепестки

    bool isAlive() const { return lifetime > 0.f; }
};
