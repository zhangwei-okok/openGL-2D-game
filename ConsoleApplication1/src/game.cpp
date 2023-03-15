#include <algorithm>
#include <irrklang/irrKlang.h>
#include "text_renderer.h"
using namespace irrklang;

#include "game.h"
#include "resource_manager.h"
#include "sprite_renderer.h"

#include "game.h"
#include "resource_manager.h"
#include "sprite_renderer.h"
#include "ball_object_collisions.h"
#include "particle_generator.h"
#include "post_processor.h"


Direction VectorDirection(glm::vec2 closest);
GLboolean CheckCollision(GameObject &one, GameObject &two);
Collision CheckCollision(BallObject &one, GameObject &two);
GLboolean ShouldSpawn(GLuint chance);
void ActivatePowerUp(PowerUp &powerUp);
GLboolean IsOtherPowerUpActive(std::vector<PowerUp> &powerUps, std::string type);

// Game-related State data
SpriteRenderer  *Renderer;
GameObject      *Player;
BallObject      *Ball;
ParticleGenerator *Particles;
PostProcessor   *Effects;
TextRenderer      *Text;

ISoundEngine *SoundEngine = createIrrKlangDevice();

Game::Game(GLuint width, GLuint height) 
	: State(GAME_MENU), Keys(), Width(width), Height(height), Level(0), Lives(3)
{ 

}

Game::~Game()
{
	delete Renderer;
	delete Player;
	delete Ball;
	delete Particles;
	delete Effects;
	delete Text;

	SoundEngine->drop();
}

void Game::Init()
{
	// Load shaders
	ResourceManager::LoadShader("shaders/sprite.vs", "shaders/sprite.frag", nullptr, "sprite");
	ResourceManager::LoadShader("shaders/particle.vs", "shaders/particle.frag", nullptr, "particle");
	ResourceManager::LoadShader("shaders/post_processing.vs", "shaders/post_processing.frag", nullptr, "postprocessing");

	// Configure shaders
	glm::mat4 projection = glm::ortho(0.0f, static_cast<GLfloat>(this->Width), static_cast<GLfloat>(this->Height), 0.0f, -1.0f, 1.0f);
	ResourceManager::GetShader("sprite").Use().SetInteger("image", 0);
	ResourceManager::GetShader("sprite").SetMatrix4("projection", projection);

	ResourceManager::GetShader("particle").Use().SetInteger("sprite", 0);
	ResourceManager::GetShader("particle").SetMatrix4("projection", projection);

	// Load textures
	ResourceManager::LoadTexture("textures/background.jpg", GL_FALSE, "background");
	ResourceManager::LoadTexture("textures/awesomeface.png", GL_TRUE, "face");
	ResourceManager::LoadTexture("textures/block.png", GL_FALSE, "block");
	ResourceManager::LoadTexture("textures/block_solid.png", GL_FALSE, "block_solid");
	ResourceManager::LoadTexture("textures/particle.png", GL_TRUE, "particle");
	ResourceManager::LoadTexture("textures/paddle.png", GL_TRUE, "paddle");

	ResourceManager::LoadTexture("textures/powerup_speed.png", GL_TRUE, "powerup_speed");
	ResourceManager::LoadTexture("textures/powerup_sticky.png", GL_TRUE, "powerup_sticky");
	ResourceManager::LoadTexture("textures/powerup_increase.png", GL_TRUE, "powerup_increase");
	ResourceManager::LoadTexture("textures/powerup_confuse.png", GL_TRUE, "powerup_confuse");
	ResourceManager::LoadTexture("textures/powerup_chaos.png", GL_TRUE, "powerup_chaos");
	ResourceManager::LoadTexture("textures/powerup_passthrough.png", GL_TRUE, "powerup_passthrough");


	// ���عؿ�
	GameLevel one; one.Load("levels/one.lvl", this->Width, this->Height * 0.5);
	GameLevel two; two.Load("levels/two.lvl", this->Width, this->Height * 0.5);
	GameLevel three; three.Load("levels/three.lvl", this->Width, this->Height * 0.5);
	GameLevel four; four.Load("levels/four.lvl", this->Width, this->Height * 0.5);
	this->Levels.push_back(one);
	this->Levels.push_back(two);
	this->Levels.push_back(three);
	this->Levels.push_back(four);


	// Set render-specific controls
	Shader sprite = ResourceManager::GetShader("sprite");
	Renderer = new SpriteRenderer(sprite);

	//������ҵ���
	glm::vec2 playerPos = glm::vec2(
		this->Width / 2 - PLAYER_SIZE.x / 2,
		this->Height - PLAYER_SIZE.y
	);
	Texture2D paddle_t = ResourceManager::GetTexture("paddle");
	Player = new GameObject(playerPos, PLAYER_SIZE, paddle_t);

	//�½���
	glm::vec2 ballPos = playerPos + glm::vec2(PLAYER_SIZE.x / 2 - BALL_RADIUS, -BALL_RADIUS * 2);
	Texture2D face_t = ResourceManager::GetTexture("face");
	Ball = new BallObject(ballPos, BALL_RADIUS, INITIAL_BALL_VELOCITY, face_t);

	//����
	Shader particle = ResourceManager::GetShader("particle");
	Texture2D particle_t = ResourceManager::GetTexture("particle");
	Particles = new ParticleGenerator(particle, particle_t, 500);

	//���ڴ���
	Shader postprocessing = ResourceManager::GetShader("postprocessing");
	Effects = new PostProcessor(postprocessing, this->Width, this->Height);

	//������ʾ
	Text = new TextRenderer(this->Width, this->Height);
	Text->Load("fonts/ocraext.TTF", 24);

	//bgm
	SoundEngine->play2D("audio/ClericBeast.mp3", GL_TRUE);
}

void Game::Update(GLfloat dt)
{
	// ���¶���
	Ball->Move(dt, this->Width);
	// �����ײ
	this->DoCollisions();

	// ��������
	Particles->Update(dt, *Ball, 2, glm::vec2(Ball->Radius / 2));

	//���µ���
	this->UpdatePowerUps(dt);

	if (ShakeTime > 0.0f)
	{
		ShakeTime -= dt;
		if (ShakeTime <= 0.0f)
			Effects->Shake = false;
	}
	
	if (Ball->Position.y >= this->Height) // ���Ƿ�Ӵ��ײ��߽磿
	{
		--this->Lives;
		// Did the player lose all his lives? : Game over
		if (this->Lives == 0)
		{
			this->ResetLevel();
			this->State = GAME_MENU;
		}

		this->ResetPlayer();
	}

	if (this->State == GAME_ACTIVE && this->Levels[this->Level].IsCompleted())
	{
		this->ResetLevel();
		this->ResetPlayer();
		Effects->Chaos = GL_TRUE;
		this->State = GAME_WIN;
	}
}


void Game::ProcessInput(GLfloat dt)
{

	if (this->State == GAME_MENU)
	{
		if (this->Keys[GLFW_KEY_ENTER] && !this->KeysProcessed[GLFW_KEY_ENTER])
		{
			this->State = GAME_ACTIVE;
			this->KeysProcessed[GLFW_KEY_ENTER] = GL_TRUE;
		}
		if (this->Keys[GLFW_KEY_W] && !this->KeysProcessed[GLFW_KEY_W])
		{
			this->Level = (this->Level + 1) % 4;
			this->KeysProcessed[GLFW_KEY_W] = GL_TRUE;
		}
		if (this->Keys[GLFW_KEY_S] && !this->KeysProcessed[GLFW_KEY_S])
		{
			if (this->Level > 0)
				--this->Level;
			else
				this->Level = 3;
			this->KeysProcessed[GLFW_KEY_S] = GL_TRUE;
		}
	}
	if (this->State == GAME_WIN)
	{
		if (this->Keys[GLFW_KEY_ENTER])
		{
			this->KeysProcessed[GLFW_KEY_ENTER] = GL_TRUE;
			Effects->Chaos = GL_FALSE;
			this->State = GAME_MENU;
		}
	}

	if (this->State == GAME_ACTIVE)
	{
		GLfloat velocity = PLAYER_VELOCITY * dt;
		// �ƶ�����
		if (this->Keys[GLFW_KEY_A])
		{
			if (Player->Position.x >= 0)
				Player->Position.x -= velocity;

			if (Ball->Stuck)
				Ball->Position.x -= velocity;
		}
		if (this->Keys[GLFW_KEY_D])
		{
			if (Player->Position.x <= this->Width - Player->Size.x)
				Player->Position.x += velocity;

			if (Ball->Stuck)
				Ball->Position.x += velocity;
		}


		//����
		if (this->Keys[GLFW_KEY_SPACE])
			Ball->Stuck = false;
	}
}

void Game::Render()
{
	if (this->State == GAME_ACTIVE)
	{
		// Begin rendering to postprocessing quad
		Effects->BeginRender();

		// ���Ʊ���
		Texture2D background = ResourceManager::GetTexture("background");
		Renderer->DrawSprite(background,glm::vec2(0, 0), glm::vec2(this->Width, this->Height), 0.0f);
		// ���ƹؿ�
		this->Levels[this->Level].Draw(*Renderer);

		Player->Draw(*Renderer);

		Particles->Draw();

		Ball->Draw(*Renderer);

		//��Ⱦ����
		for (PowerUp &powerUp : this->PowerUps)
			if (!powerUp.Destroyed)
				powerUp.Draw(*Renderer);


		// End rendering to postprocessing quad
		Effects->EndRender();
		// Render postprocessing quad
		Effects->Render(glfwGetTime());

		Text->RenderText("Lives:" + std::to_string(this->Lives),5.0f, 5.0f, 1.0f);
	}

	if (this->State == GAME_MENU)
	{
		Text->RenderText("Press ENTER to start", 250.0f, this->Height / 2, 1.0f);
		Text->RenderText("Press W or S to select level", 245.0f, this->Height / 2 + 20.0f, 0.75f);
	}
	if (this->State == GAME_WIN)
	{
		Text->RenderText("You WON!!!", 320.0f, this->Height / 2 - 20.0f, 1.0f, glm::vec3(0.0f, 1.0f, 0.0f));
		Text->RenderText("Press ENTER to retry or ESC to quit", 130.0f, this->Height / 2, 1.0f, glm::vec3(1.0f, 1.0f, 0.0f));
	}

	Text->RenderText("Level:" + std::to_string(this->Level + 1), this->Width - 100.0f, 5.0f, 1.0f);
}


void Game::ResetLevel()
{
	if (this->Level == 0)this->Levels[0].Load("levels/one.lvl", this->Width, this->Height * 0.5f);
	else if (this->Level == 1)
		this->Levels[1].Load("levels/two.lvl", this->Width, this->Height * 0.5f);
	else if (this->Level == 2)
		this->Levels[2].Load("levels/three.lvl", this->Width, this->Height * 0.5f);
	else if (this->Level == 3)
		this->Levels[3].Load("levels/four.lvl", this->Width, this->Height * 0.5f);

	this->Lives = 3;
}

void Game::ResetPlayer()
{
	// Reset player/ball stats
	Player->Size = PLAYER_SIZE;
	Player->Position = glm::vec2(this->Width / 2 - PLAYER_SIZE.x / 2, this->Height - PLAYER_SIZE.y);
	Ball->Reset(Player->Position + glm::vec2(PLAYER_SIZE.x / 2 - BALL_RADIUS, -(BALL_RADIUS * 2)), INITIAL_BALL_VELOCITY);
	// Also disable all active powerups
	Effects->Chaos = Effects->Confuse = GL_FALSE;
	Ball->PassThrough = Ball->Sticky = GL_FALSE;
	Player->Color = glm::vec3(1.0f);
	Ball->Color = glm::vec3(1.0f);
}


void Game::DoCollisions()
{
	for (GameObject &box : this->Levels[this->Level].Bricks)
	{
		if (!box.Destroyed)
		{
			Collision collision = CheckCollision(*Ball, box);
			if (std::get<0>(collision)) // ���collision �� true
			{
				// // ���ש�鲻��ʵ�ľ�����ש��
				// if (!box.IsSolid)
				// 	box.Destroyed = GL_TRUE;
				   // �������ʵ�ĵ�ש����ݻ�
				if (!box.IsSolid)
				{
					box.Destroyed = GL_TRUE;

					//�������
					this->SpawnPowerUps(box);

					SoundEngine->play2D("audio/bleep.mp3", GL_FALSE);
				}
				else
				{   // �����ʵ�ĵ�ש���򼤻�shake��Ч
					ShakeTime = 0.05f;
					Effects->Shake = true;

					SoundEngine->play2D("audio/solid.wav", GL_FALSE);
				}

				// ��ײ����
				Direction dir = std::get<1>(collision);
				glm::vec2 diff_vector = std::get<2>(collision);
				if (!(Ball->PassThrough && !box.IsSolid))  // don't do collision resolution on non-solid bricks if pass-through activated
				{
					if (dir == LEFT || dir == RIGHT) // ˮƽ������ײ
					{
						Ball->Velocity.x = -Ball->Velocity.x; // ��תˮƽ�ٶ�
						// �ض�λ
						GLfloat penetration = Ball->Radius - std::abs(diff_vector.x);
						if (dir == LEFT)
							Ball->Position.x += penetration; // ��������
						else
							Ball->Position.x -= penetration; // ��������
					}
					else // ��ֱ������ײ
					{
						Ball->Velocity.y = -Ball->Velocity.y; // ��ת��ֱ�ٶ�
						// �ض�λ
						GLfloat penetration = Ball->Radius - std::abs(diff_vector.y);
						if (dir == UP)
							Ball->Position.y -= penetration; // ��������
						else
							Ball->Position.y += penetration; // ��������
					}
				}
			}
		}
	}

	//��������ײ���
	Collision result = CheckCollision(*Ball, *Player);
	if (!Ball->Stuck && std::get<0>(result))
	{
		// ��������˵�����ĸ�λ�ã������������ĸ�λ�����ı��ٶ�
		GLfloat centerBoard = Player->Position.x + Player->Size.x / 2;
		GLfloat distance = (Ball->Position.x + Ball->Radius) - centerBoard;
		GLfloat percentage = distance / (Player->Size.x / 2);
		// ���ݽ���ƶ�
		GLfloat strength = 2.0f;
		glm::vec2 oldVelocity = Ball->Velocity;
		Ball->Velocity.x = INITIAL_BALL_VELOCITY.x * percentage * strength;
		Ball->Velocity.y = -Ball->Velocity.y;
		Ball->Velocity = glm::normalize(Ball->Velocity) * glm::length(oldVelocity);
		//ճ������ ���ֵ�ԭ�� ֡���ϵ�ʱ ��ҵ����Խϸߵ��ٶ������򣬵���������Ľ�����ҵ��塣��������û�п������������AABB�ڲ����������Ϸ�������ͼ�����е���ײ������Ӧ��������������ʱ���Ѿ���y���ٶȷ�ת�˶�Σ��������޷�ȷ����������������ϻ��������˶���
		Ball->Velocity.y = -1 * abs(Ball->Velocity.y);

		//��С���뵲����ײ��ݵ�ʵ��StickyЧ��
		Ball->Stuck = Ball->Sticky;

		SoundEngine->play2D("audio/bleep.wav", GL_FALSE);
	}

	//���������ײ���
	for (PowerUp &powerUp : this->PowerUps)
	{
		if (!powerUp.Destroyed)
		{
			if (powerUp.Position.y >= this->Height)
				powerUp.Destroyed = GL_TRUE;
			if (CheckCollision(*Player, powerUp))
			{   // �����뵲��Ӵ�����������
				ActivatePowerUp(powerUp);
				powerUp.Destroyed = GL_TRUE;
				powerUp.Activated = GL_TRUE;

				SoundEngine->play2D("audio/powerup.wav", GL_FALSE);
			}
		}
	}
}


void Game::SpawnPowerUps(GameObject &block)
{
	if (ShouldSpawn(75)) //  1/75�ļ���
		this->PowerUps.push_back(PowerUp("speed", glm::vec3(0.5f, 0.5f, 1.0f), 0.0f, block.Position, ResourceManager::GetTexture("powerup_speed")));
	if (ShouldSpawn(75))
		this->PowerUps.push_back(PowerUp("sticky", glm::vec3(1.0f, 0.5f, 1.0f), 20.0f, block.Position, ResourceManager::GetTexture("powerup_sticky")));
	if (ShouldSpawn(75))
		this->PowerUps.push_back(PowerUp("pass-through", glm::vec3(0.5f, 1.0f, 0.5f), 10.0f, block.Position, ResourceManager::GetTexture("powerup_passthrough")));
	if (ShouldSpawn(75))
		this->PowerUps.push_back(PowerUp("pad-size-increase", glm::vec3(1.0f, 0.6f, 0.4), 0.0f, block.Position, ResourceManager::GetTexture("powerup_increase")));
	if (ShouldSpawn(15)) // ������߱���Ƶ��������
		this->PowerUps.push_back(PowerUp("confuse", glm::vec3(1.0f, 0.3f, 0.3f), 15.0f, block.Position, ResourceManager::GetTexture("powerup_confuse")));
	if (ShouldSpawn(15))
		this->PowerUps.push_back(PowerUp("chaos", glm::vec3(0.9f, 0.25f, 0.25f), 15.0f, block.Position, ResourceManager::GetTexture("powerup_chaos")));

	// Speed: ����С��20%���ٶ�
	// Sticky : ��С������ҵ���Ӵ�ʱ��С��ᱣ��ճ�ڵ����ϵ�״ֱ̬���ٴΰ��¿ո�����������������ͷ�С��ǰ�ҵ������ʵ�λ��
	// Pass - Through : ��ʵ��ש�����ײ�������ã�ʹС����Դ������ݻٶ��ש��
	// Pad - Size - Increase : ������ҵ���50���صĿ��
	// Confuse : ��ʱ���ڼ���confuse������Ч���Ի����
	// Chaos : ��ʱ���ڼ���chaos������Ч��ʹ�����ʧ����
}

void Game::UpdatePowerUps(GLfloat dt)
{
	for (PowerUp &powerUp : this->PowerUps)
	{
		powerUp.Position += powerUp.Velocity * dt;
		if (powerUp.Activated)
		{
			powerUp.Duration -= dt;

			if (powerUp.Duration <= 0.0f)
			{
				// ֮��Ὣ��������Ƴ�
				powerUp.Activated = GL_FALSE;
				// ͣ��Ч��
				if (powerUp.Type == "sticky")
				{
					if (!IsOtherPowerUpActive(this->PowerUps, "sticky")) // ����û������stickyЧ�����ڼ���״̬ʱ���ã�����ͬ��
					{
						Ball->Sticky = GL_FALSE;
						Player->Color = glm::vec3(1.0f);
					}
				}
				else if (powerUp.Type == "pass-through")
				{
					if (!IsOtherPowerUpActive(this->PowerUps, "pass-through"))
					{
						Ball->PassThrough = GL_FALSE;
						Ball->Color = glm::vec3(1.0f);
					}
				}
				else if (powerUp.Type == "confuse")
				{
					if (!IsOtherPowerUpActive(this->PowerUps, "confuse"))
					{
						Effects->Confuse = GL_FALSE;
					}
				}
				else if (powerUp.Type == "chaos")
				{
					if (!IsOtherPowerUpActive(this->PowerUps, "chaos"))
					{
						Effects->Chaos = GL_FALSE;
					}
				}
			}
		}
	}
	this->PowerUps.erase(std::remove_if(this->PowerUps.begin(), this->PowerUps.end(),
		[](const PowerUp &powerUp) { return powerUp.Destroyed && !powerUp.Activated; }
	), this->PowerUps.end());
}


Direction VectorDirection(glm::vec2 target)
{
	glm::vec2 compass[] = {
		glm::vec2(0.0f, 1.0f),  // ��
		glm::vec2(1.0f, 0.0f),  // ��
		glm::vec2(0.0f, -1.0f), // ��
		glm::vec2(-1.0f, 0.0f)  // ��
	};
	GLfloat max = 0.0f;
	GLuint best_match = -1;
	for (GLuint i = 0; i < 4; i++)
	{
		GLfloat dot_product = glm::dot(glm::normalize(target), compass[i]);
		if (dot_product > max)
		{
			max = dot_product;
			best_match = i;
		}
	}
	return (Direction)best_match;
}

//��������ײ
GLboolean   CheckCollision(GameObject &one, GameObject &two) // AABB - AABB collision
{
	// x�᷽����ײ��
	bool collisionX = one.Position.x + one.Size.x >= two.Position.x &&
		two.Position.x + two.Size.x >= one.Position.x;
	// y�᷽����ײ��
	bool collisionY = one.Position.y + one.Size.y >= two.Position.y &&
		two.Position.y + two.Size.y >= one.Position.y;
	// ֻ��������������ײʱ����ײ
	return collisionX && collisionY;
}

//��ש����ײ
Collision  CheckCollision(BallObject &one, GameObject &two) // AABB - Circle collision
{
	// ��ȡԲ������ 
	glm::vec2 center(one.Position + one.Radius);
	// ����AABB����Ϣ�����ġ���߳���
	glm::vec2 aabb_half_extents(two.Size.x / 2, two.Size.y / 2);
	glm::vec2 aabb_center(
		two.Position.x + aabb_half_extents.x,
		two.Position.y + aabb_half_extents.y
	);
	// ��ȡ�������ĵĲ�ʸ��
	glm::vec2 difference = center - aabb_center;
	glm::vec2 clamped = glm::clamp(difference, -aabb_half_extents, aabb_half_extents);
	// AABB_center����clamped�����͵õ�����ײ���Ͼ���Բ����ĵ�closest
	glm::vec2 closest = aabb_center + clamped;
	// ���Բ��center�������closest��ʸ�����ж��Ƿ� length <= radius
	difference = closest - center;
	// return glm::length(difference) < one.Radius;

	if (glm::length(difference) <= one.Radius)
		return std::make_tuple(GL_TRUE, VectorDirection(difference), difference);
	else
		return std::make_tuple(GL_FALSE, UP, glm::vec2(0, 0));

}

GLboolean ShouldSpawn(GLuint chance)
{
	GLuint random = rand() % chance;
	return random == 0;
}

void ActivatePowerUp(PowerUp &powerUp)
{
	// ���ݵ������ͷ�������
	if (powerUp.Type == "speed")
	{
		Ball->Velocity *= 1.2;
	}
	else if (powerUp.Type == "sticky")
	{
		Ball->Sticky = GL_TRUE;
		Player->Color = glm::vec3(1.0f, 0.5f, 1.0f);
	}
	else if (powerUp.Type == "pass-through")
	{
		Ball->PassThrough = GL_TRUE;
		Ball->Color = glm::vec3(1.0f, 0.5f, 0.5f);
	}
	else if (powerUp.Type == "pad-size-increase")
	{
		Player->Size.x += 50;
	}
	else if (powerUp.Type == "confuse")
	{
		if (!Effects->Chaos)
			Effects->Confuse = GL_TRUE; // ֻ��chaosδ����ʱ��Ч��chaosͬ��
	}
	else if (powerUp.Type == "chaos")
	{
		if (!Effects->Confuse)
			Effects->Chaos = GL_TRUE;
	}
}

GLboolean IsOtherPowerUpActive(std::vector<PowerUp> &powerUps, std::string type)
{
	// Check if another PowerUp of the same type is still active
	// in which case we don't disable its effect (yet)
	for (const PowerUp &powerUp : powerUps)
	{
		if (powerUp.Activated)
			if (powerUp.Type == type)
				return GL_TRUE;
	}
	return GL_FALSE;
}
