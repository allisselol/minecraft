#pragma once
#include <SFML/Audio.hpp>
#include "Block.hpp"
#include <map>
#include <string>
#include <memory>
#include <vector>
#include <iostream>
class SoundManager {
private:
    std::map<std::string, sf::SoundBuffer> buffers;
    std::vector<std::unique_ptr<sf::Sound>> sounds;
    int currentSound = 0;

    sf::Music rainMusic;        // фоновый зацикленный звук дождя
    bool rainLoaded = false;
    bool rainPlaying = false;

    /*void load(const std::string& name, const std::string& path) {
        sf::SoundBuffer buf;
        if (buf.loadFromFile(path))
            buffers[name] = std::move(buf);
    }*/
   void load(const std::string& name, const std::string& path) {
    sf::SoundBuffer buf;
    if (buf.loadFromFile(path))
        buffers[name] = std::move(buf);
    else
        std::cerr << "Failed to load: " << path << std::endl;
}

public:
    SoundManager() {
        load("dig_grass1",  "sounds/dig_grass1.ogg");
        load("dig_grass2",  "sounds/dig_grass2.ogg");
        load("dig_stone1",  "sounds/dig_stone1.ogg");
        load("dig_stone2",  "sounds/dig_stone2.ogg");
        load("dig_wood1",   "sounds/dig_wood1.ogg");
        load("dig_wood2",   "sounds/dig_wood2.ogg");
        load("step_grass1", "sounds/step_grass1.ogg");
        load("step_grass2", "sounds/step_grass2.ogg");
        load("step_stone1", "sounds/step_stone1.ogg");
        load("step_stone2", "sounds/step_stone2.ogg");
        load("step_wood1",  "sounds/step_wood1.ogg");
        load("step_wood2",  "sounds/step_wood2.ogg");
        load("fuse",   "sounds/fuse.wav");        // шипение факела/фитиля
        load("splash", "sounds/splash_weak.wav"); // плеск воды
        load("hurt1",  "sounds/hurt1.ogg");        // звук гибели курицы
        load("zombie", "sounds/zombie.ogg");       // рычание зомби при приближении
        load("enemy_step",  "sounds/enemy_step.ogg");  // шаги лучника
        load("enemy_death", "sounds/enemy_death.ogg"); // смерть лучника

        // Фоновый дождь — стримится с диска и зацикливается (ogg надёжнее mp3 в SFML)
        if (rainMusic.openFromFile("sounds/rain.ogg")) {
            rainMusic.setLooping(true);
            rainMusic.setVolume(60.f);
            rainLoaded = true;
        } else {
            std::cerr << "Failed to load: sounds/rain.ogg" << std::endl;
        }
    }

    // Управление фоновым дождём (вызывать при смене погоды)
    void startRain() {
        if (rainLoaded && !rainPlaying) {
            rainMusic.play();
            rainPlaying = true;
        }
    }
    void stopRain() {
        if (rainLoaded && rainPlaying) {
            rainMusic.stop();
            rainPlaying = false;
        }
    }

    void play(const std::string& name) {
        auto it = buffers.find(name);
        if (it == buffers.end()) return;

        // Создаём новый Sound с буфером
        auto s = std::make_unique<sf::Sound>(it->second);
        s->setVolume(80.f);
        s->play();

        // Чистим завершённые звуки
        sounds.erase(
            std::remove_if(sounds.begin(), sounds.end(),
                [](const std::unique_ptr<sf::Sound>& snd) {
                    return snd->getStatus() == sf::Sound::Status::Stopped;
                }),
            sounds.end());

        sounds.push_back(std::move(s));
    }

    void playDig(BlockType type) {
        int r = rand() % 2 + 1;
        switch (type) {
            case BlockType::GRASS:
            case BlockType::DIRT:
            case BlockType::SAND:
            case BlockType::WATER:
                play("dig_grass" + std::to_string(r)); break;
            case BlockType::STONE:
            case BlockType::GRAVEL:
            case BlockType::COAL_ORE:
            case BlockType::IRON_ORE:
            case BlockType::GOLD_ORE:
            case BlockType::DIAMOND_ORE:
            case BlockType::BRICK:
            case BlockType::SANDSTONE:
            case BlockType::OBSIDIAN:
                play("dig_stone" + std::to_string(r)); break;
            case BlockType::WOOD:
            case BlockType::LEAVES:
            case BlockType::TORCH:
            case BlockType::PLANKS:
            case BlockType::BOOKSHELF:
            case BlockType::PUMPKIN:
                play("dig_wood" + std::to_string(r)); break;
            default: break;
        }
    }

    void playStep(BlockType type) {
        int r = rand() % 2 + 1;
        switch (type) {
            case BlockType::GRASS:
            case BlockType::DIRT:
            case BlockType::SAND:
                play("step_grass" + std::to_string(r)); break;
            case BlockType::STONE:
            case BlockType::GRAVEL:
                play("step_stone" + std::to_string(r)); break;
            case BlockType::WOOD:
                play("step_wood" + std::to_string(r)); break;
            default:
                play("step_grass" + std::to_string(r)); break;
        }
    }

    void playFuse()   { play("fuse"); }    // звук факела / поджига TNT
    void playSplash() { play("splash"); }  // звук воды
    void playChickenDeath() { play("hurt1"); } // гибель курицы
    void playZombie()       { play("zombie"); } // рычание зомби

    // Проигрывает звук с громкостью, зависящей от расстояния (в блоках).
    // Дальше maxDist — не слышно; вблизи — полная громкость.
    void playAt(const std::string& name, float distBlocks, float maxDist) {
        if (distBlocks > maxDist) return;
        auto it = buffers.find(name);
        if (it == buffers.end()) return;
        auto s = std::make_unique<sf::Sound>(it->second);
        float vol = 100.f * (1.f - distBlocks / maxDist);
        s->setVolume(std::max(0.f, vol));
        s->play();
        sounds.push_back(std::move(s));
        if (sounds.size() > 32) sounds.erase(sounds.begin());
    }
};