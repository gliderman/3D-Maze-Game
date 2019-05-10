/*
 * 3dmaze_game.c
 *
 *  Created on: April 1, 2014
 *      Author: Nate Hoffman
 */
////////////////////////////MMmmbbbb

#include "project_settings.h"
#include "random_int.h"
#include "stddef.h"
#include "strings.h"
#include "game.h"
#include "timing.h"
#include "task.h"
#include "terminal.h"
#include "random_int.h"
#include "math.h"
#include "render_engine.h"
#ifdef USE_MODULE_GAME_CONTROLLER
#include "game_controller_host.h"
#include "game_controller.h"
#endif

#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 24
#define CAMERA_FOV_HORIZONTAL 100
#define CAMERA_FOV_VERTICAL 75
#define CAMERA_MOVE 0.5
#define CAMERA_ROTATE 15
#define NUM_TRIANGLES 120

// World generation
#define WALL_HEIGHT 3
#define TILE_SIZE 4
#define WORLD_BACKGROUND Blue
#define WIN_TILE Green
#define REG_TILE Red
#define POS_X_WALL Yellow
#define NEG_X_WALL White
#define POS_Y_WALL Cyan
#define NEG_Y_WALL Magenta

/// game structure
struct maze_game_t {
    camera_t camera; ///< camera where the player is
    world_t world; ///< game world
    framebuffer_t framebuffer; ///< screen framebuffer
    uint16_t timer; ///< keep track of how long it takes to complete the maze
    uint8_t bufAlloc[SCREEN_WIDTH * SCREEN_HEIGHT]; ///< don't use directly
    triangle_t triangles[NUM_TRIANGLES]; ///< triangle data
    uint8_t id; ///< ID of game
};
static struct maze_game_t game;

// note the user doesn't need to access these functions directly so they are
// defined here instead of in the .h file
// further they are made static so that no other files can access them
// ALSO OTHER MODULES CAN USE THESE SAME NAMES SINCE THEY ARE STATIC
static void Receiver(uint8_t c);

static void Play(void);
static void Help(void);
static void GameOver();

static uint16_t AddTile(uint16_t index, int x, int y, uint8_t posXWall,
        uint8_t negXWall, uint8_t posYWall, uint8_t negYWall);
static void IncrementTimer();
static void RenderWorld();
static void MoveCamera(float dx, float dy);
static void CheckWin();

void MazeGame_Init(void) {
    // Register the module with the game system and give it the name "MAZE"
    game.id = Game_Register("MAZE", "maze navigation", Play, Help);
}

void Help(void) { 
#ifdef USE_MODULE_GAME_CONTROLLER
    Game_Printf("Unsupported at this time.\r\n");
#else
    Game_Printf("'w' to move forward, 's' to move backward, 'a' to move left, "
            "'d' to move right, '<' to rotate left, '>' to rotate right.\r\n");
#endif
}

#ifdef USE_MODULE_GAME_CONTROLLER
static void A_ButtonPressed(controller_buttons_t b, void * ptr) {
    Fire();
}

static void LeftPressed(controller_buttons_t b, void * ptr) {
    MoveLeft();
}

static void RightPressed(controller_buttons_t b, void * ptr) {
    MoveRight();
}
#endif

void Play(void) {
#ifdef USE_MODULE_GAME_CONTROLLER
    // Not supported
#endif
    Game_HideCursor();
    Game_ClearScreen();
    
    // Create the world data
    game.camera.fovHorizontal = CAMERA_FOV_HORIZONTAL;
    game.camera.fovVertical = CAMERA_FOV_VERTICAL;
    game.camera.location.x = 0;
    game.camera.location.y = -2 * TILE_SIZE;
    game.camera.location.z = 1.8;
    game.camera.rotation.x = 0;
    game.camera.rotation.y = 0;
    game.camera.rotation.z = 90;
    game.framebuffer.buffer = game.bufAlloc;
    game.framebuffer.width = SCREEN_WIDTH;
    game.framebuffer.height = SCREEN_HEIGHT;
    game.world.backgroundColor = WORLD_BACKGROUND;
    game.world.numTriangles = NUM_TRIANGLES;
    game.world.triangles = game.triangles;
    
    // Create the world
    volatile int i = 0;
    i += AddTile(i, 0, 0, 0, 1, 1, 1);
    
    i += AddTile(i, 1, 0, 1, 0, 0, 0);
    i += AddTile(i, 1, -1, 1, 0, 0, 1);
    i += AddTile(i, 0, -1, 0, 0, 0, 1);
    i += AddTile(i, -1, -1, 0, 1, 0, 1);
    i += AddTile(i, -1, 0, 0, 1, 0, 0);
    i += AddTile(i, -1, 1, 0, 1, 1, 0);
    i += AddTile(i, 0, 1, 0, 0, 0, 0);
    i += AddTile(i, 1, 1, 1, 0, 1, 0);
    
    i += AddTile(i, 0, 2, 0, 1, 1, 0);
    i += AddTile(i, 1, 2, 0, 0, 1, 0);
    i += AddTile(i, 2, 2, 1, 0, 1, 0);
    i += AddTile(i, 2, 1, 1, 0, 0, 0);
    i += AddTile(i, 2, 0, 1, 0, 0, 0);
    i += AddTile(i, 2, -1, 1, 0, 0, 0);
    i += AddTile(i, 2, -2, 1, 0, 0, 1);
    i += AddTile(i, 1, -2, 0, 0, 0, 1);
    i += AddTile(i, 0, -2, 0, 0, 0, 1);
    i += AddTile(i, -1, -2, 0, 0, 0, 1);
    i += AddTile(i, -2, -2, 0, 1, 0, 1);
    i += AddTile(i, -2, -1, 0, 1, 0, 0);
    i += AddTile(i, -2, 0, 0, 1, 0, 0);
    i += AddTile(i, -2, 1, 0, 1, 0, 0);
    i += AddTile(i, -2, 2, 0, 1, 1, 0);
    i += AddTile(i, -1, 2, 0, 0, 1, 0);
    
//    i += AddTile(i, 0, 3, 0, 0, 1, 0);
//    i += AddTile(i, 1, 3, 0, 0, 1, 0);
//    i += AddTile(i, 2, 3, 0, 0, 1, 0);
//    i += AddTile(i, 3, 3, 1, 0, 1, 0);
//    i += AddTile(i, 3, 2, 1, 0, 0, 0);
//    i += AddTile(i, 3, 1, 1, 0, 0, 0);
//    i += AddTile(i, 3, 0, 1, 0, 0, 0);
//    i += AddTile(i, 3, -1, 1, 0, 0, 0);
//    i += AddTile(i, 3, -2, 1, 0, 0, 0);
//    i += AddTile(i, 3, -3, 1, 0, 0, 1);
//    i += AddTile(i, 2, -3, 0, 0, 0, 1);
//    i += AddTile(i, 1, -3, 0, 0, 0, 1);
//    i += AddTile(i, 0, -3, 0, 0, 0, 1);
//    i += AddTile(i, -1, -3, 0, 0, 0, 1);
//    i += AddTile(i, -2, -3, 0, 0, 0, 1);
//    i += AddTile(i, -3, -3, 0, 1, 0, 1);
//    i += AddTile(i, -3, -2, 0, 1, 1, 0);
//    i += AddTile(i, -3, -1, 0, 1, 0, 0);
//    i += AddTile(i, -3, 0, 0, 1, 0, 0);
//    i += AddTile(i, -3, 1, 0, 1, 0, 0);
//    i += AddTile(i, -3, 2, 0, 1, 0, 0);
//    i += AddTile(i, -3, 3, 0, 1, 1, 0);
//    i += AddTile(i, -2, 3, 0, 0, 1, 0);
//    i += AddTile(i, -1, 3, 0, 0, 1, 0);
    
    // initialize game variables
    game.timer = 0;
    
    // Render the world
    RenderWorld();
    
    // Add a receiver for player commands
    Game_RegisterPlayer1Receiver(Receiver);
    
    // Keep track of how long it takes to complete the maze
    Task_Schedule(IncrementTimer, 0, 100, 100);
}

uint16_t AddTile(uint16_t index, int x, int y, uint8_t posXWall,
        uint8_t negXWall, uint8_t posYWall, uint8_t negYWall) {
    float offsetX = x * TILE_SIZE;
    float offsetY = y * TILE_SIZE;
    
    // Paint the base
    uint16_t addedTriangles = 2;
    vector_t b0_0 = {offsetX - (TILE_SIZE / 2), offsetY - (TILE_SIZE / 2), 0};
    vector_t b0_1 = {offsetX + (TILE_SIZE / 2), offsetY - (TILE_SIZE / 2), 0};
    vector_t b0_2 = {offsetX - (TILE_SIZE / 2), offsetY + (TILE_SIZE / 2), 0};
    game.world.triangles[index].p1 = b0_0;
    game.world.triangles[index].p2 = b0_1;
    game.world.triangles[index].p3 = b0_2;
    
    vector_t b1_0 = {offsetX + (TILE_SIZE / 2), offsetY + (TILE_SIZE / 2), 0};
    vector_t b1_1 = {offsetX + (TILE_SIZE / 2), offsetY - (TILE_SIZE / 2), 0};
    vector_t b1_2 = {offsetX - (TILE_SIZE / 2), offsetY + (TILE_SIZE / 2), 0};
    game.world.triangles[index + 1].p1 = b1_0;
    game.world.triangles[index + 1].p2 = b1_1;
    game.world.triangles[index + 1].p3 = b1_2;
    
    if ((x == 0) && (y == 0)) {
        game.world.triangles[index].color = WIN_TILE;
        game.world.triangles[index + 1].color = WIN_TILE;
    } else {
        game.world.triangles[index].color = REG_TILE;
        game.world.triangles[index + 1].color = REG_TILE;
    }
    
    // Positive X wall
    if (posXWall) {
        vector_t px0_0 = {offsetX + (TILE_SIZE / 2), offsetY + (TILE_SIZE / 2), 0};
        vector_t px0_1 = {offsetX + (TILE_SIZE / 2), offsetY + (TILE_SIZE / 2), WALL_HEIGHT};
        vector_t px0_2 = {offsetX + (TILE_SIZE / 2), offsetY - (TILE_SIZE / 2), 0};
        game.world.triangles[index + addedTriangles].p1 = px0_0;
        game.world.triangles[index + addedTriangles].p2 = px0_1;
        game.world.triangles[index + addedTriangles].p3 = px0_2;
        game.world.triangles[index + addedTriangles].color = POS_X_WALL;
        
        vector_t px1_0 = {offsetX + (TILE_SIZE / 2), offsetY + (TILE_SIZE / 2), WALL_HEIGHT};
        vector_t px1_1 = {offsetX + (TILE_SIZE / 2), offsetY - (TILE_SIZE / 2), WALL_HEIGHT};
        vector_t px1_2 = {offsetX + (TILE_SIZE / 2), offsetY - (TILE_SIZE / 2), 0};
        game.world.triangles[index + addedTriangles + 1].p1 = px1_0;
        game.world.triangles[index + addedTriangles + 1].p2 = px1_1;
        game.world.triangles[index + addedTriangles + 1].p3 = px1_2;
        game.world.triangles[index + addedTriangles + 1].color = POS_X_WALL;
                
        addedTriangles += 2;
    }
    
    // Negative X wall
    if (negXWall) {
        vector_t nx0_0 = {offsetX - (TILE_SIZE / 2), offsetY + (TILE_SIZE / 2), 0};
        vector_t nx0_1 = {offsetX - (TILE_SIZE / 2), offsetY + (TILE_SIZE / 2), WALL_HEIGHT};
        vector_t nx0_2 = {offsetX - (TILE_SIZE / 2), offsetY - (TILE_SIZE / 2), 0};
        game.world.triangles[index + addedTriangles].p1 = nx0_0;
        game.world.triangles[index + addedTriangles].p2 = nx0_1;
        game.world.triangles[index + addedTriangles].p3 = nx0_2;
        game.world.triangles[index + addedTriangles].color = NEG_X_WALL;
        
        vector_t nx1_0 = {offsetX - (TILE_SIZE / 2), offsetY + (TILE_SIZE / 2), WALL_HEIGHT};
        vector_t nx1_1 = {offsetX - (TILE_SIZE / 2), offsetY - (TILE_SIZE / 2), WALL_HEIGHT};
        vector_t nx1_2 = {offsetX - (TILE_SIZE / 2), offsetY - (TILE_SIZE / 2), 0};
        game.world.triangles[index + addedTriangles + 1].p1 = nx1_0;
        game.world.triangles[index + addedTriangles + 1].p2 = nx1_1;
        game.world.triangles[index + addedTriangles + 1].p3 = nx1_2;
        game.world.triangles[index + addedTriangles + 1].color = NEG_X_WALL;
        
        addedTriangles += 2;
    }
    
    // Positive Y wall
    if (posYWall) {
        vector_t py0_0 = {offsetX + (TILE_SIZE / 2), offsetY + (TILE_SIZE / 2), 0};
        vector_t py0_1 = {offsetX + (TILE_SIZE / 2), offsetY + (TILE_SIZE / 2), WALL_HEIGHT};
        vector_t py0_2 = {offsetX - (TILE_SIZE / 2), offsetY + (TILE_SIZE / 2), 0};
        game.world.triangles[index + addedTriangles].p1 = py0_0;
        game.world.triangles[index + addedTriangles].p2 = py0_1;
        game.world.triangles[index + addedTriangles].p3 = py0_2;
        game.world.triangles[index + addedTriangles].color = POS_Y_WALL;
        
        vector_t py1_0 = {offsetX + (TILE_SIZE / 2), offsetY + (TILE_SIZE / 2), WALL_HEIGHT};
        vector_t py1_1 = {offsetX - (TILE_SIZE / 2), offsetY + (TILE_SIZE / 2), WALL_HEIGHT};
        vector_t py1_2 = {offsetX - (TILE_SIZE / 2), offsetY + (TILE_SIZE / 2), 0};
        game.world.triangles[index + addedTriangles + 1].p1 = py1_0;
        game.world.triangles[index + addedTriangles + 1].p2 = py1_1;
        game.world.triangles[index + addedTriangles + 1].p3 = py1_2;
        game.world.triangles[index + addedTriangles + 1].color = POS_Y_WALL;
        
        addedTriangles += 2;
    }
    
    // Negative Y wall
    if (negYWall) {
        vector_t ny0_0 = {offsetX + (TILE_SIZE / 2), offsetY - (TILE_SIZE / 2), 0};
        vector_t ny0_1 = {offsetX + (TILE_SIZE / 2), offsetY - (TILE_SIZE / 2), WALL_HEIGHT};
        vector_t ny0_2 = {offsetX - (TILE_SIZE / 2), offsetY - (TILE_SIZE / 2), 0};
        game.world.triangles[index + addedTriangles].p1 = ny0_0;
        game.world.triangles[index + addedTriangles].p2 = ny0_1;
        game.world.triangles[index + addedTriangles].p3 = ny0_2;
        game.world.triangles[index + addedTriangles].color = NEG_Y_WALL;
        
        vector_t ny1_0 = {offsetX + (TILE_SIZE / 2), offsetY - (TILE_SIZE / 2), WALL_HEIGHT};
        vector_t ny1_1 = {offsetX - (TILE_SIZE / 2), offsetY - (TILE_SIZE / 2), WALL_HEIGHT};
        vector_t ny1_2 = {offsetX - (TILE_SIZE / 2), offsetY - (TILE_SIZE / 2), 0};
        game.world.triangles[index + addedTriangles + 1].p1 = ny1_0;
        game.world.triangles[index + addedTriangles + 1].p2 = ny1_1;
        game.world.triangles[index + addedTriangles + 1].p3 = ny1_2;
        game.world.triangles[index + addedTriangles + 1].color = NEG_Y_WALL;
        
        addedTriangles += 2;
    }
    
    return addedTriangles;
}

void IncrementTimer() {
    game.timer += 1;
}

void RenderWorld() {
    Render_Engine_RenderFrame(&game.world, &game.camera, &game.framebuffer);
    Render_Engine_DisplayFrame(SUBSYSTEM_UART, &game.framebuffer);
}

void MoveCamera(float dx, float dy) {
    float c = cos(game.camera.rotation.z * (3.14159 / 180.0));
    float s = sin(game.camera.rotation.z * (3.14159 / 180.0));
    
    game.camera.location.x += dx * c;
    game.camera.location.y += dx * s;
    
    game.camera.location.x += dy * -s;
    game.camera.location.y += dy * c;
}

void CheckWin() {
    if ((game.camera.location.x > -2) && (game.camera.location.x < 2) &&
            (game.camera.location.y > -2) && (game.camera.location.y < 2)) {
        GameOver();
    }
}

void Receiver(uint8_t c) {
    switch (c) {
        case 'w':
        case 'W':
            MoveCamera(CAMERA_MOVE, 0);
            RenderWorld();
            CheckWin();
            break;
        case 's':
        case 'S':
            MoveCamera(-CAMERA_MOVE, 0);
            RenderWorld();
            CheckWin();
            break;
        case 'a':
        case 'A':
            MoveCamera(0, CAMERA_MOVE);
            RenderWorld();
            CheckWin();
            break;
        case 'd':
        case 'D':
            MoveCamera(0, -CAMERA_MOVE);
            RenderWorld();
            CheckWin();
            break;
        case '<':
        case ',':
            game.camera.rotation.z += CAMERA_ROTATE;
            RenderWorld();
            break;
        case '>':
        case '.':
            game.camera.rotation.z -= CAMERA_ROTATE;
            RenderWorld();
            break;
        case '\r':
            //GameOver();
            break;
        default:
            break;
    }
}

void GameOver() {
    // clean up all scheduled tasks
    Task_Remove(IncrementTimer, 0);
    // if a controller was used then remove the callbacks
#ifdef USE_MODULE_GAME_CONTROLLER
    // Not supported
#endif
    Terminal_SetColor(SUBSYSTEM_UART, Black);
    Terminal_CursorXY(SUBSYSTEM_UART, 0, 0);
    // show score
    Game_Printf("Game Over! Final time: %d.%d seconds\r\n", game.timer / 10, game.timer % 10);
    // unregister the receiver used to run the game
    Game_UnregisterPlayer1Receiver(Receiver);
    // show cursor (it was hidden at the beginning)
    Game_ShowCursor();
    Game_GameOver();
}
