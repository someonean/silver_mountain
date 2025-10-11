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
enum OBJECT_TILE_TYPES {EMPTY, WALL, ORE, STAIRS};

#undef GOLD // raylib defines this as a color, interfering with the enum
enum ORE_TYPES {STONE, SILVER, GOLD};
char *ore_names[] = {"Stone", "Silver", "Gold"}; // same order as in the enum
int ore_values[] = {1, 10, 100};
int ore_durabilities[] = {1, 10, 50}; // more valuable ores are harder to mine

typedef struct
{
	int type;
	int amount;
	int wear; // how worn down the top ore piece is(goes from X to 0)
} Ore;

Ore **ore_map;

// Window dimensions
#define WID 800
#define HEI 600

Rectangle player = {0, 0, 50, 100}; // x, y, w, h

enum player_modes {MOVING, MINING};
int player_mode = MOVING;
int depth = 0;

typedef struct
{
	int x;
	int y;
} int2; // for integer coordinates
int2 mining_target = {-1,-1};

float time_since_last_mined, mining_delay = 1.0;
int mining_power = 10;

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
			ore_map[x][y].type = rand()%3;
			switch(ore_map[x][y].type)
			{
				case STONE: ore_map[x][y].amount = 10000; break;
				case SILVER: ore_map[x][y].amount = 100; break;
				case GOLD: ore_map[x][y].amount = 10; break;
			}
			ore_map[x][y].wear = ore_durabilities[ore_map[x][y].type];
		}
	}

	player.x = player.y = 0;
	object_tiles.tiles[0][0] = EMPTY;
	object_tiles.tiles[0][1] = EMPTY;

	int stairs_x = 1 + rand()%(object_tiles.wid-2);
	int stairs_y = 1 + rand()%(object_tiles.hei-3);
	object_tiles.tiles[stairs_x][stairs_y-1] = WALL;
	object_tiles.tiles[stairs_x-1][stairs_y] = WALL;
	object_tiles.tiles[stairs_x+1][stairs_y] = WALL;
	object_tiles.tiles[stairs_x][stairs_y] = STAIRS;
	depth++;
}

void DrawWearBar(int wear, int max_durability)
{
	DrawRectangle(WID/3, HEI-40, WID/3, 20, GRAY);
	DrawRectangle(WID/3, HEI-40, wear*(WID/3)/max_durability, 20, GREEN);
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

	int stairs_x = 10;
	int stairs_y = 10;
	object_tiles.tiles[stairs_x][stairs_y-1] = WALL;
	object_tiles.tiles[stairs_x-1][stairs_y] = WALL;
	object_tiles.tiles[stairs_x+1][stairs_y] = WALL;
	object_tiles.tiles[stairs_x][stairs_y] = STAIRS;

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

		if(IsKeyPressed(KEY_C))
			coins++;
		if(IsKeyPressed(KEY_X))
			coins--;

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
		EndMode2D();

		DisplayCoins();
		// outside of camera-affected code segment, so that it always
		// displays on the top left corner of the screen

		if(player_mode == MINING)
		{
			int2 t = mining_target;
			if(time_since_last_mined >= mining_delay)
			{
				time_since_last_mined = 0;
				ore_map[t.x][t.y].wear -= mining_power;
				if(ore_map[t.x][t.y].wear <= 0)
				{
					ore_map[t.x][t.y].wear = ore_durabilities[ore_map[t.x][t.y].type];
					ore_map[t.x][t.y].amount--;
					prev_amount = ore_map[t.x][t.y].amount;
					coins += ore_values[ore_map[t.x][t.y].type];
					if(ore_map[t.x][t.y].amount <= 0)
					{
						ore_map[t.x][t.y].type = STONE;
						ore_map[t.x][t.y].amount = 10000;
						ore_map[t.x][t.y].wear = ore_durabilities[STONE];
						player_mode = MOVING;
					}
				}
			}
			DrawText(TextFormat("%s x %d", ore_name, prev_amount), WID/3, HEI-20, 20, YELLOW);
			DrawWearBar(ore_map[t.x][t.y].wear, ore_durabilities[ore_map[t.x][t.y].type]);
			time_since_last_mined += dt;
		}

		if(touches_stairs(player)) descend_stairs();

		EndDrawing();
	}
	CloseWindow();
}
