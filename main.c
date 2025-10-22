#include <raylib.h>
#include <raymath.h>
#include <stdlib.h>

typedef struct
{
	int wid, hei; // how many tiles high and wide
	int **tiles; // each number represents a type of tile
} Tilemap;

Tilemap ground_tiles, object_tiles;
// Ground tiles(grass, dirt) are drawn underneath object tiles for decoration,
// and the player interacts with the object tiles(walls, ores, etc.)

enum GROUND_TILE_TYPES {DIRT, GRASS};
enum OBJECT_TILE_TYPES {EMPTY, WALL, ORE, STAIRS, ENTRANCE};

#undef GOLD // raylib defines this as a color, interfering with the enum
enum ORE_TYPES {STONE, BRONZE, IRON, SILVER, GOLD, RUBY, SAPPHIRE, EMERALD, N_ORES};
char *ore_names[] = {"Stone", "Bronze", "Iron", "Silver", "Gold", "Ruby", "Sapphire", "Emerald"};
// same order as in the enum

int ore_values[] = {1, 15, 40, 75, 180, 300, 350, 400};
int ore_durabilities[] = {1, 10, 20, 30, 100, 20, 20, 20};
int ore_frequencies[N_ORES];
int ore_amounts[] = {10000, 500, 100, 50, 10, 5, 5, 5};

#define MAX_TIERS 3
int tier_frequencies[MAX_TIERS][N_ORES] = // tier-frequency table
{
{20,	50,	100,	50,	20,	0,	0},
{20,	20,	50,	100,	50,	20,	0},
{20,	20,	20,	50,	100,	50,	20},
};

int weighed_rand(int *prob_distribution, int width)
{
	int sum = 0, i;
	for(i = 0; i < width; i++) sum += prob_distribution[i];
	int n = rand()%sum;
	sum = 0;
	for(i = 0; sum < n; i++) sum += prob_distribution[i];
	return i-1;
}

typedef struct
{
	int type;
	int amount;
	float wear; // how worn down the top ore piece is(goes from X to 0)
} Ore;

Ore **ore_map;

// Window dimensions
#define WID 800
#define HEI 600

Rectangle player = {0, 0, 49, 49}; // x, y, w, h

enum player_modes {MOVING, MINING};
int player_mode = MOVING;
int depth = 0;
int tier = 0; // surface layer is of tier 0
int mine_floor = 1; // with each floor the player descends, this number gets larger

typedef struct
{
	int x;
	int y;
} int2; // for integer coordinates
int2 mining_target = {-1,-1};

float time_since_last_mined, mining_delay = 1.0;
int total_level = 0;
int mining_speed = 0;
int mining_power = 0; float mining_damage = 2.0;
int mining_skill = 0; float ore_value_multiplier = 1.0;

// upgrade costs
int mining_speed_upgrade = 0;
int mining_power_upgrade = 0;
int mining_skill_upgrade = 0;

#define PLAYER_SPEED 500.0 // pixels per second

// Tile dimension(s)
#define SCALE 50 // serves as both tile width and height

// Map dimensions
#define MAP_WID (ground_tiles.wid*SCALE)
#define MAP_HEI (ground_tiles.hei*SCALE)

// Player camera
Camera2D camera = {0};

int coins = 0;

void DrawGroundTiles()
{
	for(int x = 0; x < ground_tiles.wid; x++)
	for(int y = 0; y < ground_tiles.hei; y++)
	{
		Color c;
		switch(ground_tiles.tiles[x][y])
		{
			case DIRT: c = BROWN; break;
			case GRASS: c = GREEN; break;
			default: c = BLACK; break;
		}
		DrawRectangle(x*SCALE, y*SCALE, SCALE, SCALE, c);
	}
}

void DrawOre(int x, int y, int type)
{
	switch(type)
	{
		case STONE:
			DrawRectangle(x*SCALE, y*SCALE, SCALE, SCALE, GRAY);
			break;
		case BRONZE:
			DrawRectangle(x*SCALE, y*SCALE, SCALE, SCALE, GRAY);
			DrawRectangle(x*SCALE+SCALE/4, y*SCALE+SCALE/4, SCALE/2, SCALE/2, BROWN);
			break;
		case IRON:
			DrawRectangle(x*SCALE, y*SCALE, SCALE, SCALE, GRAY);
			DrawRectangle(x*SCALE+SCALE/4, y*SCALE+SCALE/4, SCALE/2, SCALE/2, BLACK);
			break;
		case SILVER:
			DrawRectangle(x*SCALE, y*SCALE, SCALE, SCALE, GRAY);
			// stone base

			DrawRectangle(x*SCALE+SCALE/4, y*SCALE+SCALE/4, SCALE/2, SCALE/2, LIGHTGRAY);
			// draw ores with a half-square in the middle of
			// the stone base(until textures come in)
			break;
		case GOLD:
			DrawRectangle(x*SCALE, y*SCALE, SCALE, SCALE, GRAY);
			DrawRectangle(x*SCALE+SCALE/4, y*SCALE+SCALE/4, SCALE/2, SCALE/2, YELLOW);
			break;
		case RUBY:
			DrawRectangle(x*SCALE, y*SCALE, SCALE, SCALE, GRAY);
			DrawRectangle(x*SCALE+SCALE/4, y*SCALE+SCALE/4, SCALE/2, SCALE/2, RED);
			break;
		case SAPPHIRE:
			DrawRectangle(x*SCALE, y*SCALE, SCALE, SCALE, GRAY);
			DrawRectangle(x*SCALE+SCALE/4, y*SCALE+SCALE/4, SCALE/2, SCALE/2, BLUE);
			break;
		case EMERALD:
			DrawRectangle(x*SCALE, y*SCALE, SCALE, SCALE, GRAY);
			DrawRectangle(x*SCALE+SCALE/4, y*SCALE+SCALE/4, SCALE/2, SCALE/2, GREEN);
			break;
	}
}

void DrawObjectTiles()
{
	for(int x = 0; x < object_tiles.wid; x++)
	for(int y = 0; y < object_tiles.hei; y++)
	{
		Color c;
		switch(object_tiles.tiles[x][y])
		{
			case WALL: c = GRAY; break;
			case STAIRS: c = BLACK; break;
			case ENTRANCE: c = DARKBROWN; break;
			case ORE: DrawOre(x, y, ore_map[x][y].type);
			default: continue;
		}
		if(object_tiles.tiles[x][y] != ORE)
			DrawRectangle(x*SCALE, y*SCALE, SCALE, SCALE, c);
	}
}

#define COIN_WIDGET_SCALE 20
void DisplayCoins()
{
	DrawCircle(COIN_WIDGET_SCALE, COIN_WIDGET_SCALE, COIN_WIDGET_SCALE, YELLOW);
	DrawText(TextFormat("%d", coins), COIN_WIDGET_SCALE*2 + 5, 0, COIN_WIDGET_SCALE*2, WHITE);
}

void collide_with_walls(Rectangle *player, Rectangle oldrec)
{
	for(int y = 0; y < object_tiles.hei; y++)
	for(int x = 0; x < object_tiles.wid; x++)
	{
		if(object_tiles.tiles[x][y] == EMPTY) continue;
		if(object_tiles.tiles[x][y] == STAIRS) continue;
		if(object_tiles.tiles[x][y] == ENTRANCE) continue;

		Rectangle tilerec = (Rectangle){x*SCALE, y*SCALE, SCALE, SCALE};
		if(CheckCollisionRecs(*player, tilerec))
		{
			char skip_x = 0, skip_y = 0;
			if(oldrec.x < tilerec.x+tilerec.width && oldrec.x+oldrec.width > tilerec.x)
				skip_x = 1;
			if(oldrec.y < tilerec.y+tilerec.height && oldrec.y+oldrec.height > tilerec.y)
				skip_y = 1;

			if(!skip_x)
			{
				if(player->x > tilerec.x) // just to determine whether we're coming from the left
					player->x = tilerec.x+tilerec.width; // no
				else
					player->x = tilerec.x-player->width; // yes
			}
			if(!skip_y)
			{
				if(player->y > tilerec.y)
					player->y = tilerec.y+tilerec.height;
				else
					player->y = tilerec.y-player->height;
			}
		}
	}
}

char touches_stairs(Rectangle rec)
{
	for(int x = 0; x < object_tiles.wid; x++)
	for(int y = 0; y < object_tiles.hei; y++)
	{
		if(object_tiles.tiles[x][y] != STAIRS) continue;
		Rectangle tilerec = (Rectangle){x*SCALE, y*SCALE, SCALE, SCALE};
		if(CheckCollisionRecs(rec, tilerec)) return 1;
	}
	return 0;
}

char touches_entrance(Rectangle rec)
{
	for(int x = 0; x < object_tiles.wid; x++)
	for(int y = 0; y < object_tiles.hei; y++)
	{
		if(object_tiles.tiles[x][y] != ENTRANCE) continue;
		Rectangle tilerec = (Rectangle){x*SCALE, y*SCALE, SCALE, SCALE};
		if(CheckCollisionRecs(rec, tilerec)) return 1;
	}
	return 0;
}

char is_9by9_obstructed(int center_x, int center_y)
{
	for(int i = -1; i <= 1; i++)
	for(int j = -1; j <= 1; j++)
	{
		int nx = center_x + i;
		int ny = center_y + j;
		if(nx < 0 || nx >= object_tiles.wid)
			return 1;
		if(ny < 0 || ny >= object_tiles.hei)
			return 1;
		if(object_tiles.tiles[nx][ny] == WALL) return 1;
		if(object_tiles.tiles[nx][ny] == STAIRS) return 1;
		if(object_tiles.tiles[nx][ny] == ENTRANCE) return 1;
	}
	return 0;
}

void place_random_entrance()
{
	int ent_x = 0, ent_y = 0;
	while(is_9by9_obstructed(ent_x, ent_y))
	{
		ent_x = 1 + rand()%(object_tiles.wid-2);
		ent_y = 1 + rand()%(object_tiles.hei-2);
	}
	for(int i = -1; i <= 1; i++)
	for(int j = -1; j <= 1; j++)
			object_tiles.tiles[ent_x+i][ent_y+j] = WALL;
	object_tiles.tiles[ent_x][ent_y] = ENTRANCE;
	object_tiles.tiles[ent_x][ent_y+1] = EMPTY;
}

void place_random_stairs()
{
	int stairs_x = 0, stairs_y = 0;
	while(is_9by9_obstructed(stairs_x, stairs_y))
	{
		stairs_x = 1 + rand()%(object_tiles.wid-2);
		stairs_y = 1 + rand()%(object_tiles.hei-2);
	}
	object_tiles.tiles[stairs_x-1][stairs_y-1] = WALL;
	object_tiles.tiles[stairs_x-1][stairs_y+1] = WALL;
	object_tiles.tiles[stairs_x+1][stairs_y-1] = WALL;
	object_tiles.tiles[stairs_x+1][stairs_y+1] = WALL;
	object_tiles.tiles[stairs_x][stairs_y] = STAIRS;
}

void descend_stairs()
{
	for(int x = 0; x < ground_tiles.wid; x++)
	{
		for(int y = 0; y < ground_tiles.hei; y++)
			ground_tiles.tiles[x][y] = DIRT;
	}
	for(int x = 0; x < object_tiles.wid; x++)
	{
		for(int y = 0; y < object_tiles.hei; y++)
			object_tiles.tiles[x][y] = rand()%100?EMPTY:ORE;
	}
	for(int x = 0; x < object_tiles.wid; x++)
	{
		for(int y = 0; y < object_tiles.hei; y++)
		{
			ore_map[x][y].type = weighed_rand(ore_frequencies, N_ORES);
			ore_map[x][y].amount = ore_amounts[ore_map[x][y].type];
			ore_map[x][y].wear = ore_durabilities[ore_map[x][y].type];
		}
	}

	depth++;
	mine_floor++;

	int next_tier_chance = 0;
	// measured in promils(1/1000) instead of percents for greater precision

	if(mine_floor > 10)
	{
		next_tier_chance = (mine_floor-10)*10;
	}

	place_random_stairs();
	if(rand()%1000 < next_tier_chance)
		place_random_entrance();

	player.x = MAP_WID / 2;
	player.y = MAP_HEI / 2;
	object_tiles.tiles[0][0] = EMPTY;
}

void DrawWearBar(float wear, int max_durability)
{
	DrawRectangle(WID/3, HEI-40, WID/3, 20, GRAY);
	DrawRectangle(WID/3, HEI-40, wear*(WID/3)/max_durability, 20, GREEN);
}

void DisplayUpgradeCosts()
{
	Color c = (coins >= mining_speed_upgrade)? WHITE : RED;
	DrawText(TextFormat("F to upgrade mining speed from %d HPS for %d coins", mining_speed, mining_speed_upgrade), 0, COIN_WIDGET_SCALE*2, 20, c);
	c = (coins >= mining_power_upgrade)? WHITE : RED;
	DrawText(TextFormat("P to upgrade mining power from %d for %d coins", mining_power, mining_power_upgrade), 0, COIN_WIDGET_SCALE*2+20, 20, c);
	c = (coins >= mining_skill_upgrade)? WHITE : RED;
	DrawText(TextFormat("X to upgrade mining skill from %d for %d coins", mining_skill, mining_skill_upgrade), 0, COIN_WIDGET_SCALE*2+40, 20, c);
}

void UpgradeMiningSpeed()
{
	if(coins < mining_speed_upgrade) return;
	coins -= mining_speed_upgrade;

	total_level++;
	mining_speed++; mining_delay = 1.0 / (1.0+mining_speed*0.05); //+5% to the boost of speed per upgrade
}

void UpgradeMiningPower()
{
	if(coins < mining_power_upgrade) return;
	coins -= mining_power_upgrade;

	total_level++;
	mining_power++; mining_damage = 2.0 * (1.0 + mining_power * 0.05);
}

void UpgradeMiningSkill()
{
	if(coins < mining_skill_upgrade) return;
	coins -= mining_skill_upgrade;

	total_level++;
	mining_skill++; ore_value_multiplier = 1.0 + mining_skill * 0.05;
}

void DrawCompass() // for now, to make testing easier
{
	Vector2 player_pos = (Vector2){player.x+player.width/2, player.y+player.height/2};
	Vector2 stairs_pos = (Vector2){0, 0};
	for(int x = 0; x < object_tiles.wid; x++)
	for(int y = 0; y < object_tiles.wid; y++)
	if(object_tiles.tiles[x][y] == STAIRS)
		stairs_pos = (Vector2){x*SCALE, y*SCALE};

	Vector2 compass_arrow = Vector2Scale(Vector2Normalize(Vector2Subtract(stairs_pos, player_pos)), 20);
	DrawLineV(player_pos, Vector2Add(player_pos, compass_arrow), BLUE);
}

int main()
{
	char *ore_name; int prev_amount;
	// both vars are for displaying the "Ore x amount" message at the bottom
	// when mining

	InitWindow(WID, HEI, "Silver Mountain");
	SetTargetFPS(60);

	ground_tiles.wid = ground_tiles.hei = 100;
	ground_tiles.tiles = malloc(sizeof(int*)*ground_tiles.wid);
	for(int x = 0; x < ground_tiles.wid; x++)
	{
		ground_tiles.tiles[x] = malloc(sizeof(int)*ground_tiles.hei);
		for(int y = 0; y < ground_tiles.hei; y++)
			ground_tiles.tiles[x][y] = GRASS;
	}

	object_tiles.wid = object_tiles.hei = 100;
	object_tiles.tiles = malloc(sizeof(int*)*object_tiles.wid);
	for(int x = 0; x < object_tiles.wid; x++)
	{
		object_tiles.tiles[x] = malloc(sizeof(int)*object_tiles.hei);
		for(int y = 0; y < object_tiles.hei; y++)
			object_tiles.tiles[x][y] = EMPTY;
	}

	ore_map = malloc(sizeof(Ore*)*object_tiles.wid);
	for(int x = 0; x < object_tiles.wid; x++)
		ore_map[x] = malloc(sizeof(Ore)*object_tiles.hei);

	int ent_x = 10, ent_y = 10;
	for(int i = -1; i <= 1; i++)
	for(int j = -1; j <= 1; j++)
			object_tiles.tiles[ent_x+i][ent_y+j] = WALL;
	object_tiles.tiles[ent_x][ent_y] = ENTRANCE;
	object_tiles.tiles[ent_x][ent_y+1] = EMPTY;

	camera.offset = (Vector2){WID/2, HEI/2};
	camera.rotation = 0;
	camera.zoom = 1.0;

	while(!WindowShouldClose())
	{
		BeginDrawing();
		if(depth == 0)
			ClearBackground(SKYBLUE);
		else
			ClearBackground(BLACK);
		float dt = GetFrameTime();

		mining_speed_upgrade = 2 * mining_speed * mining_speed + total_level;
		mining_power_upgrade = 2 * mining_power * mining_power + total_level;
		mining_skill_upgrade = 2 * mining_skill * mining_skill + total_level;

		if(IsKeyPressed(KEY_F))
			UpgradeMiningSpeed();
		if(IsKeyPressed(KEY_P))
			UpgradeMiningPower();
		if(IsKeyPressed(KEY_X))
			UpgradeMiningSkill();

		Rectangle prev_player_pos = player;
		if(IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))
			player.y -= dt*PLAYER_SPEED;
		if(IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))
			player.x -= dt*PLAYER_SPEED;
		if(IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))
			player.y += dt*PLAYER_SPEED;
		if(IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT))
			player.x += dt*PLAYER_SPEED;

		collide_with_walls(&player, prev_player_pos);

		if(player.x < 0) player.x = 0;
		if(player.y < 0) player.y = 0;
		if(player.x+player.width > MAP_WID) player.x = MAP_WID-player.width;
		if(player.y+player.height > MAP_HEI) player.y = MAP_HEI-player.height;

		camera.target = (Vector2){player.x+player.width/2, player.y+player.height/2};

		if(player.x != prev_player_pos.x || player.y != prev_player_pos.y)
			player_mode = MOVING;
		if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
		{
			Vector2 mpos = GetMousePosition();
			mpos = Vector2Add(mpos, camera.target);
			mpos = Vector2Subtract(mpos, camera.offset);
			int tilex, tiley;
			tilex = (int)(mpos.x/SCALE);
			tiley = (int)(mpos.y/SCALE);

			if(Vector2Distance(mpos, camera.target) <= 2*SCALE)
			if(object_tiles.tiles[tilex][tiley] == ORE)
			if(player_mode != MINING)
			{
				player_mode = MINING;
				mining_target = (int2){tilex, tiley};
				time_since_last_mined = mining_delay;
				prev_amount = ore_map[tilex][tiley].amount;
				ore_name = ore_names[ore_map[tilex][tiley].type];
			}
		}

		BeginMode2D(camera);
		DrawGroundTiles();
		DrawObjectTiles();
		DrawRectangleRec(player, RED);
		DrawCompass();
		EndMode2D();

		DisplayCoins();
		DisplayUpgradeCosts();

		if(player_mode == MINING)
		{
			int2 t = mining_target;
			while(time_since_last_mined >= mining_delay)
			{
				time_since_last_mined -= mining_delay;
				ore_map[t.x][t.y].wear -= mining_damage;
				if(ore_map[t.x][t.y].wear <= 0)
				{
					ore_map[t.x][t.y].wear = ore_durabilities[ore_map[t.x][t.y].type];
					ore_map[t.x][t.y].amount--;
					prev_amount = ore_map[t.x][t.y].amount;
					coins += ore_value_multiplier*ore_values[ore_map[t.x][t.y].type];
					if(ore_map[t.x][t.y].amount <= 0)
					{
						ore_map[t.x][t.y].type = STONE;
						ore_map[t.x][t.y].amount = 10000;
						ore_map[t.x][t.y].wear = ore_durabilities[STONE];
						player_mode = MOVING;
						time_since_last_mined = 0;
						break;
					}
				}
			}
			DrawText(TextFormat("%s x %d", ore_name, prev_amount), WID/3, HEI-20, 20, YELLOW);
			DrawWearBar(ore_map[t.x][t.y].wear, ore_durabilities[ore_map[t.x][t.y].type]);
			time_since_last_mined += dt;
		}

		if(touches_stairs(player)) descend_stairs();
		if(touches_entrance(player))
		{
			mine_floor = 0;
			tier++;
			if(tier > MAX_TIERS) tier = MAX_TIERS;
			else
				for(int i = 0; i < N_ORES; i++)
					ore_frequencies[i] = tier_frequencies[tier-1][i];
			descend_stairs();
		}

		DrawText(TextFormat("Mining Damage: %f", mining_damage), 0, HEI - 40, 20, WHITE);
		DrawText(TextFormat("Floor: %d", mine_floor), 0, HEI-20, 20, WHITE);

		EndDrawing();
	}
	CloseWindow();
}
