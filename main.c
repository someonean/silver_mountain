#include <raylib.h>

int main()
{
	InitWindow(800, 600, "Silver Mountain");
	SetTargetFPS(60);
	while(!WindowShouldClose())
	{
		BeginDrawing();
		EndDrawing();
	}
	CloseWindow();
}
