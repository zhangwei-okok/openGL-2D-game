#pragma once
#ifndef GAME_H
#define GAME_H

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <vector>
#include "game_level.h"
#include "power_up.h"


// Represents the current state of the game
enum GameState {
	GAME_ACTIVE,
	GAME_MENU,
	GAME_WIN
};

// Represents the four possible (collision) directions
enum Direction {
	UP,
	RIGHT,
	DOWN,
	LEFT
};

// �����ƶ��Ĵ�С
const glm::vec2 PLAYER_SIZE(100, 20);
// �����ƶ����ٶ�
const GLfloat PLAYER_VELOCITY(500.0f);

// ��ʼ������ٶ�
const glm::vec2 INITIAL_BALL_VELOCITY(100.0f, -350.0f);
// ��İ뾶
const GLfloat BALL_RADIUS = 12.5f;

//��ײ�����Ϣ
typedef std::tuple<GLboolean, Direction, glm::vec2> Collision;



// Game holds all game-related state and functionality.
// Combines all game-related data into a single class for
// easy access to each of the components and manageability.
class Game
{
public:
	// Game state
	GameState              State;
	GLboolean              Keys[1024];
	GLboolean              KeysProcessed[1024];
	GLuint                 Width, Height;

	//�ζ�Ч������ʱ��
	GLfloat ShakeTime = 0.0f;


	std::vector<PowerUp>  PowerUps;

	std::vector<GameLevel> Levels;
	GLuint                 Level;
	GLuint                 Lives;

	// Constructor/Destructor
	Game(GLuint width, GLuint height);
	~Game();
	// Initialize game state (load all shaders/textures/levels)
	void Init();
	// GameLoop
	void ProcessInput(GLfloat dt);
	void Update(GLfloat dt);
	void Render();

	void ResetLevel();

	void ResetPlayer();

	//��ײ���
	void DoCollisions();

	void SpawnPowerUps(GameObject &block);
	void UpdatePowerUps(GLfloat dt);
};

#endif