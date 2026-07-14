CXX = g++
CXXFLAGS = -std=c++17 -I/opt/homebrew/include
LDFLAGS = -L/opt/homebrew/lib -lsfml-graphics -lsfml-window -lsfml-system -lsfml-audio

SRCS = main.cpp Game.cpp World.cpp Player.cpp Inventory.cpp Chicken.cpp Cow.cpp Sheep.cpp Enemy.cpp
TARGET = minecraft2d

all:
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)