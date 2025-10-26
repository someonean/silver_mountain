#include <raylib.h>
#include <raymath.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

typedef struct
{
	int wid, hei; // how many tiles high and wide
	int **tiles; // each number represents a type of tile
} Tilemap;

Tilemap ground_tiles, object_tiles;
// Ground tiles(grass, dirt) are drawn underneath object tiles for decoration,
// and the player interacts with the object tiles(walls, ores, etc.)

enum GROUND_TILE_TYPES {DIRT, GRASS};
enum OBJECT_TILE_TYPES {EMPTY, WALL, ORE, STAIRS, ENTRANCE, N_OBJECTS};

#undef GOLD // raylib defines this as a color, interfering with the enum
enum ORE_TYPES {STONE, BRONZE, IRON, SILVER, GOLD, RUBY, SAPPHIRE, EMERALD, N_ORES};

typedef struct
{
	char *name;
	int value;
	int durability;
	int frequency;
	int amount;
} Oreinfo;
Oreinfo ores[N_ORES] =
{
{"Stone",	1,	1,	0,	10000},
{"Bronze",	15,	10,	0,	500},
{"Iron",	40,	20,	0,	100},
{"Silver",	75,	30,	0,	50},
{"Gold",	180,	100,	0,	10},
{"Ruby",	300,	20,	0,	5},
{"Sapphire",	350,	20,	0,	5},
{"Emerald",	400,	20,	0,	5},
};
// the frequencies are set dynamically later, depending on the tier

int ore_frequencies[N_ORES]; // still around, cause it's more convenient for weighed_rand()

//We have to make the ores work like this:
// {"Ore Name", ore_max_hp, ore_value, ore_type (the type defines the max amount and the frequency), mine tier that it's met in}
//ores[1] = {"Stone", 5, 1, RUBBLE, 1};
//ores[2] = {"Copper", 7, 14, MASS, 1};
//ores[3] = {"Iron", 11, 43, MAIN, 1};
//ores[4] = {"Amethyst", 21, 512, RARE, 1};
//ores[5] = {"Lapis", 32, 2304, SUPER, 1};
//ores[6] = {"Silver", 12, 23, MASS, 2};
//ores[7] = {"Gold", 17, 70, MAIN, 2};
//ores[8] = {"Sapphire", 36, 868, RARE, 2};
//ores[9] = {"Ruby", 54, 3906, SUPER, 2};


// The ores now work with a struct, as suggested(should've done it a while ago).
// The frequency/amount system seems more flexible for now, but we'll see.


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
//Todo: make another rectangle which is the player hitbox {0, 0, 35, 35}

enum player_modes {MOVING, MINING};
int player_mode = MOVING;
int tier = 0; // surface layer is of tier 0
int mine_floor = 1; // with each floor the player descends, this number gets larger

typedef struct
{
	int x, y; // placement of entrance on above floor
	int z; // which floor the above floor is
	char stairs; // whether above entrance is just stairs, or a mine entrance
} MPath; // Mine path

MPath *path = NULL; // where the player currently is
int depth = 0; // how long/deep the current path is

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

char touches_stairs(Rectangle rec, int2 *pos)
{
	for(int x = 0; x < object_tiles.wid; x++)
	for(int y = 0; y < object_tiles.hei; y++)
	{
		if(object_tiles.tiles[x][y] != STAIRS) continue;
		Rectangle tilerec = (Rectangle){x*SCALE, y*SCALE, SCALE, SCALE};
		if(CheckCollisionRecs(rec, tilerec))
		{
			pos->x = x; pos->y = y;
			return 1;
		}
	}
	return 0;
}

char touches_entrance(Rectangle rec, int2 *pos)
{
	for(int x = 0; x < object_tiles.wid; x++)
	for(int y = 0; y < object_tiles.hei; y++)
	{
		if(object_tiles.tiles[x][y] != ENTRANCE) continue;
		Rectangle tilerec = (Rectangle){x*SCALE, y*SCALE, SCALE, SCALE};
		if(CheckCollisionRecs(rec, tilerec))
		{
			pos->x = x; pos->y = y;
			return 1;
		}
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

void generate_floor()
{
	if(depth <= 0) // surface
	{
		int ent_x = 10, ent_y = 10;
		for(int i = -1; i <= 1; i++)
		for(int j = -1; j <= 1; j++)
				object_tiles.tiles[ent_x+i][ent_y+j] = WALL;
		object_tiles.tiles[ent_x][ent_y] = ENTRANCE;
		object_tiles.tiles[ent_x][ent_y+1] = EMPTY;

		player.x = 5*SCALE; player.y = 5*SCALE; // place player close to the entrance
		return;
	}

	for(int i = 0; i < N_ORES; i++)
		ores[i].frequency = ore_frequencies[i] = tier_frequencies[tier-1][i];
	int seed = 0;
	for(int i = 0; i < depth; i++)
		seed ^= path[i].x ^ path[i].y ^ path[i].z ^ path[i].stairs;
	srand(seed);

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
			ore_map[x][y].amount = ores[ore_map[x][y].type].amount;
			ore_map[x][y].wear = ores[ore_map[x][y].type].durability;
		}
	}
	int next_tier_chance = 0;
	// measured in promils(1/1000) instead of percents for greater precision

	if(mine_floor > 10)
	{
		next_tier_chance = (mine_floor-10)*10;
	}

	for(int i = 0; i < 5; i++)
		if(rand()%1000 < next_tier_chance)
			place_random_entrance();
		else
			place_random_stairs();
}

int get_entrance_id(int ex, int ey) // which entrance in the given floor it is
{
	int counter = 0;

	for(int x = 0; x < object_tiles.wid; x++)
	for(int y = 0; y < object_tiles.hei; y++)
	{
		if(object_tiles.tiles[x][y] == STAIRS || object_tiles.tiles[x][y] == ENTRANCE)
			counter++;
		if(x == ex && y == ey)
			return counter;
			// first encountered entrance will be 1, second 2, etc.
	}
	return -1; // invalid coordinates
}

char save_floor()
{
	FILE *f = fopen("floor.dat", "wb");
	if(!f)
		return 0; // failure

	fputc(object_tiles.wid, f);
	fputc(object_tiles.hei, f);
	// first 2 bytes of the file are the tile width and height,
	// (meaning they can both be only from 0 to 255),
	// but this saves us the hassle of worrying about endianness

	for(int x = 0; x < object_tiles.wid; x++)
	for(int y = 0; y < object_tiles.hei; y++)
	{
		if(object_tiles.tiles[x][y] == ORE)
		{
			fputc(N_OBJECTS + ore_map[x][y].type, f);
			// ores take up the space after the objects in the
			// encoding

			fwrite(&ore_map[x][y].amount, sizeof(ore_map[x][y].amount), 1, f); // save ore amount
			fwrite(&ore_map[x][y].wear, sizeof(ore_map[x][y].wear), 1, f); // save ore wear
		}
		else fputc(object_tiles.tiles[x][y], f);
	}
	fclose(f);
	return 1; // success
}

char load_floor()
{
	FILE *f = fopen("floor.dat", "rb");
	if(!f)
		return 0;

	for(int x = 0; x < object_tiles.wid; x++)
		free(object_tiles.tiles[x]);
	free(object_tiles.tiles);

	object_tiles.wid = getc(f);
	object_tiles.hei = getc(f);

	object_tiles.tiles = malloc(sizeof(int*)*object_tiles.wid);
	for(int x = 0; x < object_tiles.wid; x++)
	{
		object_tiles.tiles[x] = malloc(sizeof(int)*object_tiles.hei);
		for(int y = 0; y < object_tiles.hei; y++)
		{
			int ch = getc(f);
			if(ch >= N_OBJECTS)
			{
				object_tiles.tiles[x][y] = ORE;
				ore_map[x][y].type = ch - N_OBJECTS;
				fread(&ore_map[x][y].amount, sizeof(ore_map[x][y].amount), 1, f); // load ore amount
				fread(&ore_map[x][y].wear, sizeof(ore_map[x][y].wear), 1, f); // load ore wear
			}
			else
				object_tiles.tiles[x][y] = ch;
		}
	}
	fclose(f);

	if(depth == 0)
	{
		for(int x = 0; x < ground_tiles.wid; x++)
		for(int y = 0; y < ground_tiles.hei; y++)
			ground_tiles.tiles[x][y] = GRASS;
	}
	else
	{
		for(int x = 0; x < ground_tiles.wid; x++)
		for(int y = 0; y < ground_tiles.hei; y++)
			ground_tiles.tiles[x][y] = DIRT;
	}
}

void descend_stairs(int x, int y, char stairs)
{
	depth++;
	path = realloc(path, sizeof(MPath)*depth);
	path[depth-1].x = x;
	path[depth-1].y = y;
	path[depth-1].z = mine_floor;
	path[depth-1].stairs = stairs;

	if(stairs)
		mine_floor++;
	else
	{
		mine_floor = 1;
		tier++;
		if(tier > MAX_TIERS) tier = MAX_TIERS;
	}

	save_floor(); // save the floor we are leaving

	int ent_id = get_entrance_id(x, y);
	if(!chdir(TextFormat("%d", ent_id))) // successful, therefore exists
		load_floor(); // load below floor, stored in entrance directory
	else
	{
		system(TextFormat("mkdir %d", ent_id));
		// the mkdir command works everywhere

		chdir(TextFormat("%d", ent_id));

		generate_floor();
	}

	player.x = MAP_WID / 2;
	player.y = MAP_HEI / 2;
	object_tiles.tiles[0][0] = EMPTY;
	save_floor();
}

void ascend_floor()
{
	if(depth <= 0) return;

	char stairs = path[depth-1].stairs;
	int floor = path[depth-1].z;
	int x = path[depth-1].x;
	int y = path[depth-1].y;

	depth--;
	path = realloc(path, sizeof(MPath)*depth);
	if(stairs)
		mine_floor--;
	else
	{
		mine_floor = floor;
		tier--;
	}

	save_floor(); // save current floor
	chdir(".."); // go to above directory
	load_floor(); // load the above floor

	player.x = x*SCALE;
	player.y = (y+1)*SCALE;

	// place player exactly below above entrance when ascending a floor
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
	Vector2 stairs_pos = (Vector2){0, 0}, closest = (Vector2){0, 0};
	float min_dst = -1;
	for(int x = 0; x < object_tiles.wid; x++)
	for(int y = 0; y < object_tiles.wid; y++)
	{
		if(object_tiles.tiles[x][y] == STAIRS || object_tiles.tiles[x][y] == ENTRANCE)
		{
			stairs_pos = (Vector2){x*SCALE, y*SCALE};
			float dst = Vector2Distance(player_pos, stairs_pos);
			if(min_dst == -1 || dst < min_dst)
			{
				min_dst = dst;
				closest = stairs_pos;
			}
		}
	}

	Vector2 compass_arrow = Vector2Scale(Vector2Normalize(Vector2Subtract(closest, player_pos)), 20);
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

	generate_floor(); // Because the depth is 0, it will generate the surface "floor"

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
				ore_name = ores[ore_map[tilex][tiley].type].name;
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
					ore_map[t.x][t.y].wear = ores[ore_map[t.x][t.y].type].durability;
					ore_map[t.x][t.y].amount--;
					prev_amount = ore_map[t.x][t.y].amount;
					coins += ore_value_multiplier*ores[ore_map[t.x][t.y].type].value;
					if(ore_map[t.x][t.y].amount <= 0)
					{
						ore_map[t.x][t.y].type = STONE;
						ore_map[t.x][t.y].amount = 10000;
						ore_map[t.x][t.y].wear = ores[STONE].durability;
						player_mode = MOVING;
						time_since_last_mined = 0;
						break;
					}
				}
			}
			DrawText(TextFormat("%s x %d", ore_name, prev_amount), WID/3, HEI-20, 20, YELLOW);
			DrawWearBar(ore_map[t.x][t.y].wear, ores[ore_map[t.x][t.y].type].durability);
			time_since_last_mined += dt;
		}

		int2 pos;
		if(touches_stairs(player, &pos)) descend_stairs(pos.x, pos.y, 1);
		else if(touches_entrance(player, &pos)) descend_stairs(pos.x, pos.y, 0);

		if(IsKeyPressed(KEY_U)) // up
			ascend_floor();

		DrawText(TextFormat("Floor: %d", mine_floor), 0, HEI-20, 20, WHITE);

		EndDrawing();
	}
	CloseWindow();
}
