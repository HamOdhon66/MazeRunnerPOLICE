#include "raylib.h"
#include "raymath.h"
#include <vector>
#include <stack>
#include <cstdlib>
#include <ctime>
#include <cmath>

// Maze Settings
const int MAZE_WIDTH = 20;
const int MAZE_HEIGHT = 20;
const float CELL_SIZE = 1.0f;
const float WALL_HEIGHT = 1.5f;
const float WALL_THICKNESS = 0.1f;

// Player Settings
const float PLAYER_HEIGHT = 0.5f;
const float PLAYER_RADIUS = 0.15f;
const float PLAYER_SPEED = 3.0f;
const float MOUSE_SENSITIVITY = 0.003f;
const float CAMERA_HEIGHT = 0.4f;

// Minimap Settings
const int MINIMAP_SIZE = 150;
const int MINIMAP_MARGIN = 10;

struct Cell {
    int x, y;
    bool visited = false;
    bool walls[4] = {true, true, true, true}; // Top, Right, Bottom, Left
    
    Cell(int x = 0, int y = 0) : x(x), y(y) {}
};

// Forward declaration
class MazeGenerator;

// NPC Structure - moved outside of MazeGenerator
struct NPC {
    Vector3 position;
    Vector3 target;
    float speed = 2.0f;  // Slower than player (player is 3.0f)
    float thinkTimer = 0.0f;
    Color color;
    
    enum State { WANDERING, CHASING, FLEEING, PATROLLING };
    State state = WANDERING;
    
    void Think(MazeGenerator& maze, Vector3 playerPos, float deltaTime);
    void Update(MazeGenerator& maze, float deltaTime);
    void Draw();
};

class MazeGenerator {
private:
    Cell grid[MAZE_WIDTH][MAZE_HEIGHT];
    std::stack<Cell*> pathStack;

public:
    void Initialize() {
        for (int x = 0; x < MAZE_WIDTH; x++) {
            for (int y = 0; y < MAZE_HEIGHT; y++) {
                grid[x][y] = Cell(x, y);
            }
        }
    }

    Cell* GetCell(int x, int y) {
        if (x >= 0 && x < MAZE_WIDTH && y >= 0 && y < MAZE_HEIGHT)
            return &grid[x][y];
        return nullptr;
    }

    Cell* GetUnvisitedNeighbour(Cell* current) {
        std::vector<Cell*> neighbours;

        if (current->y + 1 < MAZE_HEIGHT && !grid[current->x][current->y + 1].visited)
            neighbours.push_back(&grid[current->x][current->y + 1]);
        if (current->x + 1 < MAZE_WIDTH && !grid[current->x + 1][current->y].visited)
            neighbours.push_back(&grid[current->x + 1][current->y]);
        if (current->y - 1 >= 0 && !grid[current->x][current->y - 1].visited)
            neighbours.push_back(&grid[current->x][current->y - 1]);
        if (current->x - 1 >= 0 && !grid[current->x - 1][current->y].visited)
            neighbours.push_back(&grid[current->x - 1][current->y]);

        if (!neighbours.empty())
            return neighbours[rand() % neighbours.size()];
        return nullptr;
    }

    void RemoveWall(Cell* current, Cell* next) {
        int dx = current->x - next->x;
        int dy = current->y - next->y;

        if (dx == 1) {
            current->walls[3] = false;
            next->walls[1] = false;
        }
        else if (dx == -1) {
            current->walls[1] = false;
            next->walls[3] = false;
        }

        if (dy == 1) {
            current->walls[2] = false;
            next->walls[0] = false;
        }
        else if (dy == -1) {
            current->walls[0] = false;
            next->walls[2] = false;
        }
    }

    void Generate() {
        Cell* current = &grid[0][0];
        current->visited = true;
        pathStack.push(current);

        while (!pathStack.empty()) {
            Cell* next = GetUnvisitedNeighbour(current);
            if (next != nullptr) {
                RemoveWall(current, next);
                next->visited = true;
                pathStack.push(current);
                current = next;
            }
            else {
                current = pathStack.top();
                pathStack.pop();
            }
        }
    }

    Vector3 GetRandomSpawnPosition() {
        int x = rand() % MAZE_WIDTH;
        int y = rand() % MAZE_HEIGHT;
        return {x * CELL_SIZE, PLAYER_HEIGHT / 2, y * CELL_SIZE};
    }

    bool CheckWallCollision(Vector3 newPos) {
        int cellX = (int)((newPos.x + CELL_SIZE / 2) / CELL_SIZE);
        int cellY = (int)((newPos.z + CELL_SIZE / 2) / CELL_SIZE);
        
        float localX = newPos.x - (cellX * CELL_SIZE - CELL_SIZE / 2);
        float localY = newPos.z - (cellY * CELL_SIZE - CELL_SIZE / 2);

        Cell* cell = GetCell(cellX, cellY);
        if (!cell) return true;

        if (cell->walls[0] && localY > CELL_SIZE - PLAYER_RADIUS) return true;
        if (cell->walls[1] && localX > CELL_SIZE - PLAYER_RADIUS) return true;
        if (cell->walls[2] && localY < PLAYER_RADIUS) return true;
        if (cell->walls[3] && localX < PLAYER_RADIUS) return true;

        return false;
    }

    void DrawWall(Vector3 position, bool rotated) {
        Vector3 size;
        if (rotated) {
            size = {WALL_THICKNESS, WALL_HEIGHT, CELL_SIZE + WALL_THICKNESS};
        } else {
            size = {CELL_SIZE + WALL_THICKNESS, WALL_HEIGHT, WALL_THICKNESS};
        }
        
        DrawCubeV(position, size, DARKGRAY);
        DrawCubeWiresV(position, size, BLACK);
    }

    void Draw() {
        for (int x = 0; x < MAZE_WIDTH; x++) {
            for (int y = 0; y < MAZE_HEIGHT; y++) {
                Cell& current = grid[x][y];
                Vector3 pos = {x * CELL_SIZE, WALL_HEIGHT / 2, y * CELL_SIZE};

                if (current.walls[0]) {
                    DrawWall({pos.x, pos.y, pos.z + CELL_SIZE / 2}, false);
                }
                if (current.walls[1]) {
                    DrawWall({pos.x + CELL_SIZE / 2, pos.y, pos.z}, true);
                }
                if (y == 0 && current.walls[2]) {
                    DrawWall({pos.x, pos.y, pos.z - CELL_SIZE / 2}, false);
                }
                if (x == 0 && current.walls[3]) {
                    DrawWall({pos.x - CELL_SIZE / 2, pos.y, pos.z}, true);
                }
            }
        }
    }

    void DrawMinimap(int screenWidth, int screenHeight, Vector3 playerPos, float playerYaw, std::vector<NPC>& npcs) {
        int minimapX = screenWidth - MINIMAP_SIZE - MINIMAP_MARGIN;
        int minimapY = screenHeight - MINIMAP_SIZE - MINIMAP_MARGIN;
        
        // Semi-transparent background
        DrawRectangle(minimapX - 5, minimapY - 5, MINIMAP_SIZE + 10, MINIMAP_SIZE + 10, Fade(BLACK, 0.7f));
        
        float cellPixelSize = (float)MINIMAP_SIZE / fmax(MAZE_WIDTH, MAZE_HEIGHT);
        
        // Draw maze cells and walls
        for (int x = 0; x < MAZE_WIDTH; x++) {
            for (int y = 0; y < MAZE_HEIGHT; y++) {
                Cell& current = grid[x][y];
                
                float pixelX = minimapX + x * cellPixelSize;
                float pixelY = minimapY + y * cellPixelSize;
                
                // Draw cell background
                DrawRectangle((int)pixelX, (int)pixelY, (int)cellPixelSize, (int)cellPixelSize, Fade(DARKGRAY, 0.3f));
                
                // Draw walls
                if (current.walls[0]) {
                    DrawLineEx({pixelX, pixelY + cellPixelSize}, {pixelX + cellPixelSize, pixelY + cellPixelSize}, 2, WHITE);
                }
                if (current.walls[1]) {
                    DrawLineEx({pixelX + cellPixelSize, pixelY}, {pixelX + cellPixelSize, pixelY + cellPixelSize}, 2, WHITE);
                }
                if (y == 0 && current.walls[2]) {
                    DrawLineEx({pixelX, pixelY}, {pixelX + cellPixelSize, pixelY}, 2, WHITE);
                }
                if (x == 0 && current.walls[3]) {
                    DrawLineEx({pixelX, pixelY}, {pixelX, pixelY + cellPixelSize}, 2, WHITE);
                }
            }
        }
        
        // Draw NPCs on minimap
        for (const auto& npc : npcs) {
            float npcPixelX = minimapX + (npc.position.x / CELL_SIZE + 0.5f) * cellPixelSize;
            float npcPixelY = minimapY + (npc.position.z / CELL_SIZE + 0.5f) * cellPixelSize;
            DrawCircle((int)npcPixelX, (int)npcPixelY, 3, npc.color);
        }
        
        // Draw player position and direction
        float playerPixelX = minimapX + (playerPos.x / CELL_SIZE + 0.5f) * cellPixelSize;
        float playerPixelY = minimapY + (playerPos.z / CELL_SIZE + 0.5f) * cellPixelSize;
        
        // Player dot
        DrawCircle((int)playerPixelX, (int)playerPixelY, 4, RED);
        
        // Direction indicator
        float dirLength = cellPixelSize * 0.6f;
        float dirX = playerPixelX + sinf(playerYaw) * dirLength;
        float dirY = playerPixelY + cosf(playerYaw) * dirLength;
        DrawLineEx({playerPixelX, playerPixelY}, {dirX, dirY}, 2, YELLOW);
        
        DrawText("MAP", minimapX + 5, minimapY - 20, 15, WHITE);
    }
};

// NPC method implementations
void NPC::Think(MazeGenerator& maze, Vector3 playerPos, float deltaTime) {
    thinkTimer += deltaTime;
    
    if (thinkTimer > 0.5f) {
        thinkTimer = 0.0f;
        
        float distToPlayer = Vector3Distance(position, playerPos);
        
        if (distToPlayer < 3.0f) {
            state = FLEEING;
            Vector3 awayDir = Vector3Subtract(position, playerPos);
            awayDir = Vector3Normalize(awayDir);
            target = Vector3Add(position, Vector3Scale(awayDir, 2.0f));
        }
        else if (distToPlayer < 5.0f) {
            state = CHASING;
            target = playerPos;
        }
        else {
            state = WANDERING;
            if (rand() % 10 < 3) {
                target = maze.GetRandomSpawnPosition();
            }
        }
    }
}

void NPC::Update(MazeGenerator& maze, float deltaTime) {
    Vector3 direction = Vector3Subtract(target, position);
    float distance = Vector3Length(direction);
    
    if (distance > 0.1f) {
        direction = Vector3Normalize(direction);
        Vector3 move = Vector3Scale(direction, speed * deltaTime);
        
        Vector3 newPos = Vector3Add(position, move);
        if (!maze.CheckWallCollision(newPos)) {
            position = newPos;
        }
        else {
            target = maze.GetRandomSpawnPosition();
        }
    }
}

void NPC::Draw() {
    DrawSphere(position, PLAYER_RADIUS * 1.5f, color);
    DrawSphereWires(position, PLAYER_RADIUS * 1.5f, 8, 8, BLACK);
    
    // Draw state indicator above NPC using billboard effect
    const char* stateText = "";
    Color stateColor = WHITE;
    switch(state) {
        case WANDERING: stateText = "?"; stateColor = GRAY; break;
        case CHASING: stateText = "!"; stateColor = YELLOW; break;
        case FLEEING: stateText = "!"; stateColor = RED; break;
        case PATROLLING: stateText = "-"; stateColor = BLUE; break;
    }
    
    // Draw a small sphere above NPC as state indicator instead of text
    Vector3 indicatorPos = Vector3Add(position, (Vector3){0, 0.5f, 0});
    DrawSphere(indicatorPos, 0.1f, stateColor);
}

struct Player {
    Vector3 position;
    float yaw = 0.0f;
    float pitch = 0.0f;

    Vector3 GetForward() {
        return {
            cosf(pitch) * sinf(yaw),
            sinf(pitch),
            cosf(pitch) * cosf(yaw)
        };
    }

    Vector3 GetRight() {
        return {
            cosf(yaw),
            0.0f,
            -sinf(yaw)
        };
    }
};

int main() {
    srand(static_cast<unsigned>(time(nullptr)));

    const int screenWidth = 800;
    const int screenHeight = 600;

    InitWindow(screenWidth, screenHeight, "Maze Explorer - Enhanced");
    DisableCursor();

    MazeGenerator maze;
    maze.Initialize();
    maze.Generate();

    Player player;
    player.position = maze.GetRandomSpawnPosition();

    // Create NPCs
    std::vector<NPC> npcs;
    for (int i = 0; i < 10; i++) {
        NPC npc;
        npc.position = maze.GetRandomSpawnPosition();
        npc.target = maze.GetRandomSpawnPosition();
        npc.color = (Color){(unsigned char)(rand() % 200 + 55), 
                           (unsigned char)(rand() % 200 + 55), 
                           (unsigned char)(rand() % 200 + 55), 255};
        npcs.push_back(npc);
    }

    Camera3D camera = {0};
    camera.up = {0.0f, 1.0f, 0.0f};
    camera.fovy = 60.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        float deltaTime = GetFrameTime();

        // Mouse look
        Vector2 mouseDelta = GetMouseDelta();
        player.yaw -= mouseDelta.x * MOUSE_SENSITIVITY;
        player.pitch -= mouseDelta.y * MOUSE_SENSITIVITY;
        
        if (player.pitch > 1.5f) player.pitch = 1.5f;
        if (player.pitch < -1.5f) player.pitch = -1.5f;

        // Movement
        Vector3 forward = player.GetForward();
        Vector3 right = player.GetRight();
        
        Vector3 moveForward = {forward.x, 0, forward.z};
        moveForward = Vector3Normalize(moveForward);

        Vector3 velocity = {0, 0, 0};

        if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) {
            velocity.x += moveForward.x * PLAYER_SPEED * deltaTime;
            velocity.z += moveForward.z * PLAYER_SPEED * deltaTime;
        }
        if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) {
            velocity.x -= moveForward.x * PLAYER_SPEED * deltaTime;
            velocity.z -= moveForward.z * PLAYER_SPEED * deltaTime;
        }
        if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) {
            velocity.x += right.x * PLAYER_SPEED * deltaTime;
            velocity.z += right.z * PLAYER_SPEED * deltaTime;
        }
        if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) {
            velocity.x -= right.x * PLAYER_SPEED * deltaTime;
            velocity.z -= right.z * PLAYER_SPEED * deltaTime;
        }

        // Apply movement with collision
        Vector3 newPosX = {player.position.x + velocity.x, player.position.y, player.position.z};
        Vector3 newPosZ = {player.position.x, player.position.y, player.position.z + velocity.z};

        if (!maze.CheckWallCollision(newPosX)) {
            player.position.x = newPosX.x;
        }
        if (!maze.CheckWallCollision(newPosZ)) {
            player.position.z = newPosZ.z;
        }

        // Update NPCs
        for (auto& npc : npcs) {
            npc.Think(maze, player.position, deltaTime);
            npc.Update(maze, deltaTime);
        }

        // Regenerate maze on R key
        if (IsKeyPressed(KEY_R)) {
            maze.Initialize();
            maze.Generate();
            player.position = maze.GetRandomSpawnPosition();
            
            // Respawn NPCs
            for (auto& npc : npcs) {
                npc.position = maze.GetRandomSpawnPosition();
                npc.target = maze.GetRandomSpawnPosition();
            }
        }

        // Update camera
        camera.position = {player.position.x, player.position.y + CAMERA_HEIGHT, player.position.z};
        camera.target = Vector3Add(camera.position, player.GetForward());

        BeginDrawing();
            ClearBackground(SKYBLUE);

            BeginMode3D(camera);
                // Draw maze
                maze.Draw();
                
                // Draw floor
                DrawPlane({(float)MAZE_WIDTH / 2 - 0.5f, 0, (float)MAZE_HEIGHT / 2 - 0.5f}, 
                          {(float)MAZE_WIDTH, (float)MAZE_HEIGHT}, DARKGREEN);
                
                // Draw NPCs
                for (auto& npc : npcs) {
                    npc.Draw();
                }
            EndMode3D();

            // Crosshair
            DrawLine(screenWidth/2 - 10, screenHeight/2, screenWidth/2 + 10, screenHeight/2, WHITE);
            DrawLine(screenWidth/2, screenHeight/2 - 10, screenWidth/2, screenHeight/2 + 10, WHITE);

            // Draw minimap with NPCs
            maze.DrawMinimap(screenWidth, screenHeight, player.position, player.yaw, npcs);

            // Controls
            DrawFPS(screenWidth - 100, 10);

        EndDrawing();
    }

    // Cleanup
    CloseWindow();
    return 0;
}