#include <raylib.h>
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
enum OBJECT_TILE_TYPES {EMPTY, WALL};

// Window dimensions
#define WID 800
#define HEI 600

Rectangle player = {0, 0, 50, 100}; // x, y, w, h

#define PLAYER_SPEED 500.0 // pixels per second

// Tile dimension(s)
#define SCALE 50 // serves as both tile width and height

// Map dimensions
#define MAP_WID (ground_tiles.wid*SCALE)
#define MAP_HEI (ground_tiles.hei*SCALE)

// Player camera
Camera2D camera = {0};

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

void DrawObjectTiles()
{
	for(int x = 0; x < object_tiles.wid; x++)
	for(int y = 0; y < object_tiles.hei; y++)
	{
		Color c;
		switch(object_tiles.tiles[x][y])
		{
			case WALL: c = GRAY; break;
			default: continue;
		}
		DrawRectangle(x*SCALE, y*SCALE, SCALE, SCALE, c);
	}
}

char collides_with_walls(Rectangle rec)
{
	for(int y = 0; y < object_tiles.hei; y++)
	for(int x = 0; x < object_tiles.wid; x++)
	{
		if(object_tiles.tiles[x][y] != WALL) continue;

		Rectangle tilerec = (Rectangle){x*SCALE, y*SCALE, SCALE, SCALE};
		if(CheckCollisionRecs(rec, tilerec)) return 1;
	}
	return 0;
}

int main()
{
	InitWindow(WID, HEI, "Silver Mountain");
	SetTargetFPS(60);

	ground_tiles.wid = ground_tiles.hei = 100;
	ground_tiles.tiles = malloc(sizeof(int*)*ground_tiles.wid);
	for(int x = 0; x < ground_tiles.wid; x++)
	{
		ground_tiles.tiles[x] = malloc(sizeof(int)*ground_tiles.hei);
		for(int y = 0; y < ground_tiles.hei; y++)
			ground_tiles.tiles[x][y] = rand()%2;
	}

	object_tiles.wid = object_tiles.hei = 100;
	object_tiles.tiles = malloc(sizeof(int*)*object_tiles.wid);
	for(int x = 0; x < object_tiles.wid; x++)
	{
		object_tiles.tiles[x] = malloc(sizeof(int)*object_tiles.hei);
		for(int y = 0; y < object_tiles.hei; y++)
			object_tiles.tiles[x][y] = rand()%100?EMPTY:WALL;
	}

	camera.offset = (Vector2){WID/2, HEI/2};
	camera.rotation = 0;
	camera.zoom = 1.0;

	while(!WindowShouldClose())
	{
		BeginDrawing();
		ClearBackground(BLACK);
		float dt = GetFrameTime();

		Rectangle prev_player_pos = player;
		if(IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))
			player.y -= dt*PLAYER_SPEED;
		if(IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))
			player.x -= dt*PLAYER_SPEED;
		if(IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))
			player.y += dt*PLAYER_SPEED;
		if(IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT))
			player.x += dt*PLAYER_SPEED;

		if(player.x < 0) player.x = 0;
		if(player.y < 0) player.y = 0;
		if(player.x+player.width > MAP_WID) player.x = MAP_WID-player.width;
		if(player.y+player.height > MAP_HEI) player.y = MAP_HEI-player.height;

		if(collides_with_walls(player) && !collides_with_walls(prev_player_pos))
			player = prev_player_pos;
		// roll player position back on collision with walls, but only
		// if the player wasn't somehow already in a wall before

		camera.target = (Vector2){player.x+player.width/2, player.y+player.height/2};

		BeginMode2D(camera);
		DrawGroundTiles();
		DrawObjectTiles();
		DrawRectangleRec(player, RED);
		EndMode2D();

		EndDrawing();
	}
	CloseWindow();
}
