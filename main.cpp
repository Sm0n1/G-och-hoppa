#include <EnTT.h>
#include <fstream>
#include <iostream>
#include "fpm/fixed.hpp"
#include <limits>
#include <algorithm>
#include <random>
#include <vector>
#include <cmath>
#include <stdexcept>
#include <unordered_map>
#include <string>
#include <string_view>
#include "SDL.h"
#include "SDL_image.h"

std::string stof(Uint32 flags)
{
	std::string string{ "UNKOWN_FLAG" };

	switch (flags)
	{
	case SDL_WINDOW_FULLSCREEN:
		string = "SDL_WINDOW_FULLSCREEN";
		break;
	case SDL_RENDERER_ACCELERATED:
		string = "SDL_RENDERER_ACCELERATED";
		break;
	default:
		break;
	}

	return string;
}

// Component defenitions

struct VisualComponent
{
	std::string texture;
	SDL_Rect srcRect;
	SDL_Rect dstRect;
	SDL_RendererFlip flip;
};

struct SpatialComponent
{
	int x;
	int y;
	int w;
	int h;
};

struct VelocityComponent
{
	float x;
	float y;
};

struct AccelerationComponent
{
	float x;
	float y;
};

struct GravityComponent
{
	float g;
};

struct MoveComponent
{
	int x;
	int y;
	float xr;
	float yr;
};

struct JumpComponent
{
	bool canJump;
	float strength;
	int buffer;
};

struct RunComponent
{
	float speed;
	float acceleration;
	float deceleration;
};

struct CollectableComponent
{
	int x;
	int y;
	int w;
	int h;
};

struct AccumulatorComponent
{
	int coins;
};

struct DebugComponent
{
	bool toggle;
};

struct Vector2D
{
	int x;
	int y;
};

// Collision detection function

bool collideAt(SpatialComponent spt1, SpatialComponent spt2, Vector2D v = {0, 0}) 
{
	bool isIntersectingX{ ((spt1.x + spt1.w + v.x - 1) >= spt2.x) && ((spt2.x + spt2.w - 1) >= (spt1.x + v.x)) };
	bool isIntersectingY{ ((spt1.y + spt1.h + v.y - 1) >= spt2.y) && ((spt2.y + spt2.h - 1) >= (spt1.y + v.y)) };

	return isIntersectingX && isIntersectingY;
}

// Contains functions and data related to RNG

namespace Random {
	std::mt19937 mt{ std::random_device{}() };

	int get(int min, int max) {
		std::uniform_int_distribution die{ min, max };
		return die(mt);
	}

	void randomizeCoinLocation(entt::registry& registry,  int tileswidth, int tilesheight, int tilescale) {
		auto coinview{ registry.view<CollectableComponent>() };
		auto spatialview{ registry.view<SpatialComponent>() };

		std::cout << tilescale << ' ' << tileswidth << ' ' << tilesheight << '\n';

		for (auto coin : coinview) {
			auto& collectable{ registry.get<CollectableComponent>(coin) };

			// generate a possible location
			SpatialComponent coinSpawnpoint{ 0, 0, collectable.w, collectable.h};

			// test legality of location
			bool isLegalPosition{ false };

			while (!isLegalPosition) {
				isLegalPosition = true;
				coinSpawnpoint.x = Random::get(0, tileswidth * 2) * tilescale / 2;
				coinSpawnpoint.y = Random::get(0, tilesheight * 2) * tilescale / 2;

				for (auto spatial : spatialview) {
					auto& spatialdata{ registry.get<SpatialComponent>(spatial) };
					if (collideAt(coinSpawnpoint, spatialdata)) {
						isLegalPosition = false;
						std::cout << "Coin(" << coinSpawnpoint.w << ", " << coinSpawnpoint.h << ") spawned at (" << coinSpawnpoint.x / tilescale << ", " << coinSpawnpoint.y / tilescale << "). Retrying...\n";
						break;
					}
					else if ((tileswidth <= coinSpawnpoint.x / tilescale) || (tilesheight <= coinSpawnpoint.y / tilescale)) {
						isLegalPosition = false;
						std::cout << "Coin(" << coinSpawnpoint.w << ", " << coinSpawnpoint.h << ") spawned out of bounds. Retrying...\n";
						break;
					}
				}
			}

			// spawn coin
			collectable.x = coinSpawnpoint.x;
			collectable.y = coinSpawnpoint.y;

			std::cout << "Coin(" << coinSpawnpoint.w << ", " << coinSpawnpoint.h << ") spawned at (" << coinSpawnpoint.x / tilescale << ", " << coinSpawnpoint.y / tilescale << ").\n";
		}
	}
}

int main(int argc, char *argv[]) {
	constexpr std::string_view linebreak{ "***********************************************\n" };

	try {
		// <INIT>
		std::cout << "<INIT>\n";
		{
			if (SDL_Init(SDL_INIT_EVERYTHING) == 0) {
				std::cout << "SDL initialized...\n";
			}
			else {
				std::cerr << "SDL_Init(): " << SDL_GetError() << '\n';
				throw std::runtime_error("Init failed");
			}

			if (IMG_Init(IMG_INIT_PNG) == IMG_INIT_PNG) {
				std::cout << "SDL_image initialized...\n";
			}
			else {
				std::cerr << "IMG_Init(): " << IMG_GetError() << '\n';
				throw std::runtime_error("Init failed");
			}
		}

		std::cout << linebreak;

		// <CONFIG>
		std::cout << "<CONFIG>\n";

		std::string name{};
		int tilesize{};
		int worldwidth{};
		int worldheight{};
		int worldscale{};
		Uint32 windowflags{};
		Uint32 rendererflags{};
		
		std::string configfile{ "assets/config.txt" };

		std::ifstream inFile(configfile);
		if (inFile.is_open()) {
			std::cout << "File opened...('" << configfile << "')\n";
		}
		else {
			std::cerr << "File failed to load...('" << configfile << "')\n";
			throw std::runtime_error("Config failed");
		}

		// A less than stallar way to read a config file. It reads each line and attemnpts to detect keywords with zero error checking

		std::string current{};
		while (inFile >> current) {
			if (current == "name:") {
				inFile >> name;
			}
			else if (current == "tilesize:") {
				inFile >> tilesize;
			}
			else if (current == "worldwidth:") {
				inFile >> worldwidth;
			}
			else if (current == "worldheight:") {
				inFile >> worldheight;
			}
			else if (current == "worldscale:") {
				inFile >> worldscale;
			}
			else if (current == "windowflags:") {
				while (inFile >> current) {
					if (current == "SDL_WINDOW_FULLSCREEN") {
						windowflags += SDL_WINDOW_FULLSCREEN;
					}
					else if (current == "<") {
						continue;
					}
					else if (current == ">") {
						break;
					}
				}
			}
			else if (current == "rendererflags:") {
				while (inFile >> current) {
					if (current == "SDL_RENDERER_ACCELERATED") {
						rendererflags += SDL_RENDERER_ACCELERATED;
					}
					else if (current == "<") {
						continue;
					}
					else if (current == ">") {
						break;
					}
				}
			}
		}

		inFile.close();
		std::cout << "File closed...('" << configfile << "')\n";

		std::cout << "name\t\t==\t" << name << '\n'
			<< "tilesize\t==\t" << tilesize << '\n'
			<< "worldwidth\t==\t" << worldwidth << '\n'
			<< "worldheight\t==\t" << worldheight << '\n'
			<< "worldscale\t==\t" << worldscale << '\n';

		std::cout << "windowflags\t==\t";
		if (windowflags && SDL_WINDOW_FULLSCREEN) {
			std::cout << "SDL_WINDOW_FULLSCREEN\n";
		}
		else {
			std::cout << '\n';
		}

		std::cout << "rendererflags\t==\t";
		if (rendererflags && SDL_RENDERER_ACCELERATED) {
			std::cout << "SDL_RENDERER_ACCELERATED\n";
		}
		else {
			std::cout << "NONE\n";
		}

		std::cout << linebreak;

		// <SETUP>
		std::cout << "<SETUP>\n";

		entt::registry registry{};
		std::cout << "Registry created...\n";

		SDL_Window* window{ SDL_CreateWindow(name.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, tilesize * worldscale * worldwidth, tilesize * worldscale * worldheight, windowflags) };
		if (window) {
			std::cout << "Window created...\n";
		}
		else {
			std::cerr << "SDL_CreateWindow(): " << SDL_GetError() << '\n';
			throw std::runtime_error("Setup failed");
		}

		SDL_Renderer* renderer{ SDL_CreateRenderer(window, -1, rendererflags) };
		if (renderer) {
			std::cout << "Renderer created...\n";
		}
		else {
			std::cerr << "SDL_CreateRenderer(): " << SDL_GetError() << '\n';
			throw std::runtime_error("Failed SDL_CreateRenderer()");
		}

		std::unordered_map<std::string, SDL_Texture*> textures{};

		std::string texturefile{ "assets/texture.png" };

		SDL_Texture* texture{ IMG_LoadTexture(renderer, texturefile.c_str()) };

		if (texture)
		{
			std::cout << "Texture created...('" << texturefile << "')\n";
		}
		else
		{
			std::cerr << "IMG_LoadTexture(): " << IMG_GetError() << '\n';
			throw std::runtime_error("Setup failed");
		}

		textures.emplace(texturefile, texture);

		int playerwidth{ 4 * worldscale};
		int playerheight{ 8 * worldscale };
		int locationX{ 250 };
		int locationY{ 100 };
		int spriteX{ 4 * tilesize };
		int spriteY{ 7 * tilesize };

		auto player{ registry.create() };
		registry.emplace<VisualComponent>(player, "assets/texture.png", SDL_Rect{ spriteX, spriteY, tilesize / 2, tilesize }, SDL_Rect{ locationX, locationY, tilesize * worldscale / 2, tilesize * worldscale }, SDL_FLIP_NONE);
		registry.emplace<SpatialComponent>(player, locationX, locationY, playerwidth, playerheight);
		registry.emplace<VelocityComponent>(player);
		registry.emplace<AccelerationComponent>(player);
		registry.emplace<GravityComponent>(player, 0.5f);
		registry.emplace<MoveComponent>(player);
		registry.emplace<JumpComponent>(player, false, 12.0f, 0);
		registry.emplace<RunComponent>(player, 4.0f, 2.0f, 1.0f);
		registry.emplace<AccumulatorComponent>(player);
		registry.emplace<DebugComponent>(player, true);

		int coinwidth{ 4 * worldscale };
		int coinheight{ 4 * worldscale };
		int coinLocationX{ 0 };
		int coinLocationY{ 0 };
		int coinSpriteX{ 0 * tilesize / 2 };
		int coinSpriteY{ 12 * tilesize / 2 };

		auto coin{ registry.create() };
		registry.emplace<VisualComponent>(coin, "assets/texture.png", SDL_Rect{coinSpriteX, coinSpriteY, tilesize / 2, tilesize / 2 }, SDL_Rect{ coinLocationX, coinLocationY, tilesize * worldscale / 2, tilesize * worldscale / 2});
		registry.emplace<CollectableComponent>(coin, coinLocationX, coinLocationY, coinwidth, coinheight);
		registry.emplace<DebugComponent>(coin, true);

		std::string levelfile{ "assets/level_1.txt" };

		inFile.open(levelfile);
		if (inFile.is_open()) {
			std::cout << "File opened...('" << levelfile << "')\n";
		}
		else {
			std::cerr << "File failed to load...('" << levelfile << "')\n";
			throw std::runtime_error("Setup failed");
		}

		int tilecol{ 0 };
		int tilerow{ 0 };

		while (inFile >> current) {
			SDL_Point filepoint{ -1, -1 };
			bool isCollidable{ false };

			if (current == "w") {
				filepoint = { 2, 1 };
				isCollidable = true;
			}
			else if (current == "wl") {
				filepoint = { 3, 1 };
				isCollidable = true;
			}
			else if (current == "wr") {
				filepoint = { 1, 1 };
				isCollidable = true;
			}
			else if (current == "wd") {
				filepoint = { 2, 0 };
				isCollidable = true;
			}
			else if (current == "wu") {
				filepoint = { 2, 2 };
				isCollidable = true;
			}
			else if (current == "wld") {
				filepoint = { 3, 0 };
				isCollidable = true;
			}
			else if (current == "wrd") {
				filepoint = { 1, 0 };
				isCollidable = true;
			}
			else if (current == "wlu") {
				filepoint = { 3, 2 };
				isCollidable = true;
			}
			else if (current == "wru") {
				filepoint = { 1, 2 };
				isCollidable = true;
			}
			else if (current == "vld") {
				filepoint = { 1, 4 };
				isCollidable = true;
			}
			else if (current == "vrd") {
				filepoint = { 2, 4 };
				isCollidable = true;
			}
			else if (current == "vlu") {
				filepoint = { 1, 3 };
				isCollidable = true;
			}
			else if (current == "vru") {
				filepoint = { 2, 3 };
				isCollidable = true;
			}
			else if (current == "s") {
				filepoint = { 5, 1 };
				isCollidable = false;
			}
			else if (current == "sc") {
				filepoint = { 5, 0 };
				isCollidable = false;
			}
			else if (current == "sb") {
				filepoint = { 4, 0 };
				isCollidable = false;
			}
			else if (current == "stl") {
				filepoint = { 4, 1 };
				isCollidable = false;
			}
			else if (current == "str") {
				filepoint = { 6, 1 };
				isCollidable = false;
			}
			else if (current == "s1") {
				filepoint = { 6, 0 };
				isCollidable = false;
			}
			else if (current == "s2") {
				filepoint = { 4, 2 };
				isCollidable = false;
			}
			else if (current == "s3") {
				filepoint = { 5, 2 };
				isCollidable = false;
			}
			else if (current == "s4") {
				filepoint = { 6, 2 };
				isCollidable = false;
			}
			else if (current == "wf") {
				filepoint = { 3, 3 };
				isCollidable = true;
			}
			else if (current == "wbu") {
				filepoint = { 3, 5 };
				isCollidable = true;
			}
			else if (current == "wbd") {
				filepoint = { 3, 4 };
				isCollidable = true;
			}
			else if (current == "wbl") {
				filepoint = { 2, 5 };
				isCollidable = true;
			}
			else if (current == "wbr") {
			filepoint = { 1, 5 };
			isCollidable = true;
			}

			if (filepoint.x != -1 && filepoint.y != -1)
			{
				auto tile{ registry.create() };

				std::string texturename{ "assets/texture.png" };
				SDL_Rect srcRect{ filepoint.x * tilesize, filepoint.y * tilesize, tilesize, tilesize };
				SDL_Rect dstRect{ tilecol * tilesize * worldscale, tilerow * tilesize * worldscale, tilesize * worldscale, tilesize * worldscale };
				registry.emplace<VisualComponent>(tile, texturename, srcRect, dstRect, SDL_FLIP_NONE);

				if (isCollidable) {
					int x{ dstRect.x };
					int y{ dstRect.y };
					int w{ dstRect.w };
					int h{ dstRect.h };

					registry.emplace<SpatialComponent>(tile, x, y, w, h);
					registry.emplace<DebugComponent>(tile, true);
				}

				tilecol += 1;

				if (!(tilecol <  worldwidth)) {
					tilecol = 0;
					tilerow += 1;
				}
			}
		}

		inFile.close();
		std::cout << "File closed...('" << levelfile << "')\n";

		std::cout << linebreak;

		Random::randomizeCoinLocation(registry, worldwidth, worldheight, worldscale * tilesize);

		// <RUN>
		std::cout << "<RUN>\n";

		int coinsToWin{ 5 };

		bool isRunning{ true };

		SDL_Event event{};

		while (isRunning) {

			// Input
			{
				while (SDL_PollEvent(&event) != 0) {
					auto runview{ registry.view<VelocityComponent, RunComponent>() };
					auto jumpview{ registry.view <VelocityComponent, JumpComponent>() };
					auto visualview{ registry.view <RunComponent, VisualComponent>() };
					auto debugview{ registry.view<DebugComponent>() };
					auto keyboard{ SDL_GetKeyboardState(nullptr) };

					switch (event.type)
					{
					case SDL_QUIT:
						isRunning = false;
						break;

					case SDL_KEYDOWN:
						if (!event.key.repeat) {
							// std::cout << event.key.keysym.scancode << "\tjust pressed\n";

							switch (event.key.keysym.scancode) {
							case SDL_SCANCODE_SPACE:

								for (auto entity : jumpview) {
									auto& velocity{ registry.get<VelocityComponent>(entity) };
									auto& jump{ registry.get<JumpComponent>(entity) };

									if (jump.canJump) {
										velocity.y = -jump.strength;
									}
								}

								break;

							case SDL_SCANCODE_A:

								for (auto entity : runview) {
									auto& velocity{ registry.get<VelocityComponent>(entity) };
									if (!keyboard[SDL_SCANCODE_D]) {
										velocity.x = -4;
									}
								}

								for (auto entity : visualview) {
									auto& visual{ registry.get<VisualComponent>(entity) };
									if (!keyboard[SDL_SCANCODE_D]) {
										visual.flip = SDL_FLIP_NONE;
									}
								}

								break;

							case SDL_SCANCODE_D:

								for (auto entity : runview) {
									auto& velocity{ registry.get<VelocityComponent>(entity) };
									if (!keyboard[SDL_SCANCODE_A]) {
										velocity.x = 4;
									}
								}

								for (auto entity : visualview) {
									auto& visual{ registry.get<VisualComponent>(entity) };
									if (!keyboard[SDL_SCANCODE_A]) {
										visual.flip = SDL_FLIP_HORIZONTAL;
									}
								}

								break;

							case SDL_SCANCODE_F3:

								for (auto entity : debugview) {
									auto& debug{ registry.get<DebugComponent>(entity) };
									debug.toggle = !debug.toggle;
									std::cout << "Debug(entity: " << static_cast<int>(entity) << ")\t==\t" << std::boolalpha << debug.toggle << '\n';
								}

								break;

							case SDL_SCANCODE_F11:

								if (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP) {
									SDL_SetWindowFullscreen(window, 0);
								}
								else {
									SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
								}

								break;

							case SDL_SCANCODE_C:

								Random::randomizeCoinLocation(registry, worldwidth, worldheight, worldscale * tilesize);

								break;

							default:
								break;
							}
						}
						break;

					case SDL_KEYUP:
						// std::cout << event.key.keysym.scancode << "\tjust released\n";

						switch (event.key.keysym.scancode) {
						case SDL_SCANCODE_A:

							for (auto entity : runview) {
								auto& velocity{ registry.get<VelocityComponent>(entity) };
								if (keyboard[SDL_SCANCODE_D]) {
									velocity.x = 4;
								}
								else {
									velocity.x = 0;
								}
							}

							break;

						case SDL_SCANCODE_D:

							for (auto entity : runview) {
								auto& velocity{ registry.get<VelocityComponent>(entity) };
								if (keyboard[SDL_SCANCODE_A]) {
									velocity.x = -4;
								}
								else {
									velocity.x = 0;
								}
							}

							break;

						default:
							break;
						}
						break;

					default:
						break;
					}
				}
			}

			// Update
			{
				// Apply Gravity To Velocity System
				{
					auto view{ registry.view<VelocityComponent, GravityComponent>() };

					for (auto entity : view) {
						auto& velocity{ registry.get<VelocityComponent>(entity) };
						const auto& gravity{ registry.get<GravityComponent>(entity) };

						velocity.y += gravity.g;
					}
				}

				// Apply Acceleration To Velocity System
				{
					auto view{ registry.view<VelocityComponent, AccelerationComponent>() };

					for (auto entity : view) {
						auto& velocity{ registry.get<VelocityComponent>(entity) };
						const auto& acceleration{ registry.get<AccelerationComponent>(entity) };

						velocity.x += acceleration.x;
						velocity.y += acceleration.y;

						if (velocity.x > 16) {
							velocity.x = 16;
						}
						if (velocity.y > 16) {
							velocity.y = 16;
						}
					}
				}

				// Apply Velocty To Move System
				{
					auto view{ registry.view<VelocityComponent, MoveComponent>() };

					for (auto entity : view) {
						auto& move{ registry.get<MoveComponent>(entity) };
						const auto& velocity{ registry.get<VelocityComponent>(entity) };

						move.xr += velocity.x;
						move.yr += velocity.y;

						move.x = static_cast<int>(std::round(move.xr));
						move.y = static_cast<int>(std::round(move.yr));

						move.xr -= move.x;
						move.yr -= move.y;
					}
				}

				// Update Position System
				{
					auto view1{ registry.view<MoveComponent, SpatialComponent>() };
					auto view2{ registry.view<SpatialComponent>() };

					for (auto entity1 : view1) {
						auto& spatial1{ registry.get<SpatialComponent>(entity1) };
						auto& move1{ registry.get<MoveComponent>(entity1) };

						// Move along the Y-axis one pixel at a time and test for collisions every step, only moving if there is nothing ahead
						{
							int sign{ (move1.y > 0) - (move1.y < 0) }; // Computes the sign (or false if still) of the Y-vector. Either 1 (downwards), 0 (still) or -1 (upwards)
							bool stop{ false };

							while (move1.y) {
								for (auto entity2 : view2) {
									if (entity2 != entity1) {
										const auto& spatial2{ registry.get<SpatialComponent>(entity2) };

										if (collideAt(spatial1, spatial2, Vector2D{ 0, sign })) {
											stop = true;
										}
									}
								}

								if (!stop) {
									spatial1.y += sign;
									move1.y -= sign;
								}
								else {
									break;
								}
							}
						}

						// Move along the X-axis one pixel at a time and test for collisions every step, only moving if there is nothing ahead
						{
							int sign{ (move1.x > 0) - (move1.x < 0) }; // Computes the sign (or false if still) of the X-vector. Either 1 (right), 0 (still) or -1 (left)
							bool stop{ false };

							while (move1.x) {
								for (auto entity2 : view2) {
									if (entity2 != entity1) {
										const auto& spatial2{ registry.get<SpatialComponent>(entity2) };

										if (collideAt(spatial1, spatial2, Vector2D{ sign, 0 })) {
											stop = true;
										}
									}
								}

								if (!stop) {
									spatial1.x += sign;
									move1.x -= sign;
								}
								else {
									break;
								}
							}
						}


					

						// Window Bounds. Make sure nothing can move outside the window frame
						if (spatial1.x < 0) spatial1.x = 0;
						if (spatial1.y < 0) spatial1.y = 0;
						if (spatial1.x + spatial1.w > tilesize * worldscale * worldwidth) spatial1.x = tilesize * worldscale * worldwidth - spatial1.w;
						if (spatial1.y + spatial1.h > tilesize * worldscale * worldheight) spatial1.y = tilesize * worldscale * worldheight - spatial1.h;
					}


					// Grounded Check System
					{
						auto jumpview{ registry.view<JumpComponent, SpatialComponent>() };
						auto velocityview{ registry.view<VelocityComponent, SpatialComponent>() };
						auto floorview{ registry.view<SpatialComponent>() };

						// *If grounded entity can jump*

						for (auto jump : jumpview) {
							auto& jumpdata{ registry.get<JumpComponent>(jump) };
							auto& positiondata{ registry.get<SpatialComponent>(jump) };

							jumpdata.canJump = false;

							for (auto floor : floorview) {
								if (jump != floor) {
									auto& floordata{ registry.get<SpatialComponent>(floor) };

									if (collideAt(positiondata, floordata, Vector2D{ 0, 1 })) {
										jumpdata.canJump = true;
									}
								}
							}
						}

						// *If grounded entity has no downwards velocity*

						for (auto velocity : velocityview) {
							auto& velocitydata{ registry.get<VelocityComponent>(velocity) };
							auto& positiondata{ registry.get<SpatialComponent>(velocity) };

							for (auto floor : floorview) {
								if (velocity != floor) {
									auto& floordata{ registry.get<SpatialComponent>(floor) };

									if (collideAt(positiondata, floordata, Vector2D{ 0, 1 })) {
										velocitydata.y = 0;
									}
								}
							}
						}
					}

					// Headbounce System
					{
						auto velocityview{ registry.view<VelocityComponent, SpatialComponent>() };
						auto ceilingview{ registry.view<SpatialComponent>() };

						for (auto velocity : velocityview) {
							auto& velocitydata{ registry.get<VelocityComponent>(velocity) };
							auto& positiondata{ registry.get<SpatialComponent>(velocity) };

							for (auto ceiling : ceilingview) {
								if (velocity != ceiling) {
									auto& ceilingdata{ registry.get<SpatialComponent>(ceiling) };

									if (collideAt(positiondata, ceilingdata, Vector2D{ 0, -1 })) {
										velocitydata.y = 0;
									}
								}
							}
						}
					}

					// Coin Collection System
					{
						auto playerview{ registry.view<SpatialComponent, AccumulatorComponent>() };
						auto coinview{ registry.view<CollectableComponent>() };

						for (auto player : playerview) {
							auto& playerdata{ registry.get<SpatialComponent>(player) };

							for (auto coin : coinview) {
								auto& coindata{ registry.get<CollectableComponent>(coin) };

								SpatialComponent coinspatial{ coindata.x, coindata.y, coindata.w, coindata.h };

								if (collideAt(playerdata, coinspatial)) {
									std::cout << "Coins: " << ++registry.get<AccumulatorComponent>(player).coins << '\n';
									Random::randomizeCoinLocation(registry, worldwidth, worldheight, worldscale * tilesize);
								}
							}
						}
					}
				}

				// Visual System
				{
					auto view{ registry.view<VisualComponent, SpatialComponent>() };

					// *Perceivable and material entities have their visual component alligned with their spatial component*

					for (auto entity : view)
					{
						auto& visual{ registry.get<VisualComponent>(entity) };
						const auto& spatial{ registry.get<SpatialComponent>(entity) };

						visual.dstRect.x = spatial.x;
						visual.dstRect.y = spatial.y;
					}
				}

				// Coin Visual System
				{
					auto view{ registry.view<VisualComponent, CollectableComponent>() };

					// *Perceivable and collectable entities have their visual component alligned with their collectable component*

					for (auto entity : view)
					{
						auto& visual{ registry.get<VisualComponent>(entity) };
						const auto& collectable{ registry.get<CollectableComponent>(entity) };

						visual.dstRect.x = collectable.x;
						visual.dstRect.y = collectable.y;
					}
				}
			}

			// Render System
			{
				SDL_RenderClear(renderer);

				auto view{ registry.view<VisualComponent>() };

				for (auto entity : view) {
					auto& visual{ registry.get<VisualComponent>(entity) };

					SDL_RenderCopyEx(renderer, textures.at(visual.texture), &visual.srcRect, &visual.dstRect, 0, nullptr, visual.flip);
				}



				// Debug (visualizes certain values for certain entities)

				auto viewDebugStatic{ registry.view<SpatialComponent, DebugComponent>(entt::exclude<VelocityComponent>) };

				for (auto entity : viewDebugStatic) {
					auto& debug{ registry.get<DebugComponent>(entity) };
					auto& spatial{ registry.get<SpatialComponent>(entity) };

					if (debug.toggle) {
						SDL_SetRenderDrawColor(renderer, 255, 255, 0, 100);
						SDL_Rect box{ spatial.x, spatial.y, spatial.w, spatial.h };
						SDL_RenderFillRect(renderer, &box);
					}
				}

				auto viewDebugCoin{ registry.view<CollectableComponent, DebugComponent>() };

				for (auto entity : viewDebugCoin) {
					auto& debug{ registry.get<DebugComponent>(entity) };
					auto& collectable{ registry.get<CollectableComponent>(entity) };

					if (debug.toggle) {
						SDL_SetRenderDrawColor(renderer, 255, 0, 255, 100);
						SDL_Rect box{ collectable.x, collectable.y, collectable.w, collectable.h };
						SDL_RenderFillRect(renderer, &box);
					}
				}

				auto viewDebug{ registry.view<SpatialComponent, VelocityComponent, DebugComponent>() };

				for (auto entity : viewDebug) {
					auto& debug{ registry.get<DebugComponent>(entity) };
					auto& spatial{ registry.get<SpatialComponent>(entity) };
					auto& velocity{ registry.get<VelocityComponent>(entity) };

					if (debug.toggle) {
						SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

						// X Axis Position
						SDL_SetRenderDrawColor(renderer, 255, 0, 0, 100);
						SDL_Rect xBox{ spatial.x, 0, spatial.w, 5 };
						SDL_RenderFillRect(renderer, &xBox);

						// Y Axis Position
						SDL_SetRenderDrawColor(renderer, 0, 0, 255, 100);
						SDL_Rect yBox{ 0, spatial.y, 5, spatial.h };
						SDL_RenderFillRect(renderer, &yBox);

						// Collision Box
						SDL_SetRenderDrawColor(renderer, 0, 255, 0, 100);
						SDL_Rect cBox{ spatial.x, spatial.y, spatial.w, spatial.h };
						SDL_RenderFillRect(renderer, &cBox);

						// Velocity Line
						SDL_SetRenderDrawColor(renderer, 0, 255, 255, 255);
						int x1{ spatial.x + static_cast<int>(std::round(spatial.w / 2.0)) };
						int y1{ spatial.y + static_cast<int>(std::round(spatial.h / 2.0)) };
						int x2{ static_cast<int>(std::round(x1 + velocity.x * 3)) };
						int y2{ static_cast<int>(std::round(y1 + velocity.y * 3)) };
						SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
					}
				}

				SDL_RenderPresent(renderer);
			}

			// End Condition (also prints the time since the program started)
			{
				if (registry.get<AccumulatorComponent>(player).coins >= coinsToWin) {
					std::cout << "Time: " << SDL_GetTicks64() / 1000.0 << '\n';
					isRunning = false;
				}
			}

			// pause the game for 25 ticks (completely arbitrary)

			SDL_Delay(25);
		}

		std::cout << linebreak;

		// <CLEANUP>
		std::cout << "<CLEANUP>\n";

		for (auto& [filename, texture] : textures)
		{
			SDL_DestroyTexture(texture);
			std::cout << "Texture('" << filename << "') destroyed...\n";
		}

		if (renderer) {
			std::cout << "Renderer destroyed...\n";
		}

		if (window) {
			std::cout << "Window destroyed...\n";
		}

		std::cout << linebreak;

		// <QUIT>
		std::cout << "<QUIT>\n";

		IMG_Quit();
		std::cout << "SDL_image quit...\n";

		SDL_Quit();
		std::cout << "SDL quit...\n";
	}

	catch (std::runtime_error error)
	{
		std::cout << error.what() << '\n';
	}
	catch (...)
	{
		std::cout << "Unkown error!\n";
	}

	return 0;
}
