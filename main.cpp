#include "raylib.h"
#include <cmath>
#include <array>

struct Piece {
    int type;
    int rotation;
    int x;
    int y;
};

static bool PieceCellFilled(int type, int rotation, int x, int y)
{
    static const char* kShapes[7][4] = {
        {"....", "XXXX", "....", "...."}, // I
        {".XX.", ".XX.", "....", "...."}, // O
        {".X..", "XXX.", "....", "...."}, // T
        {".XX.", "XX..", "....", "...."}, // S
        {"XX..", ".XX.", "....", "...."}, // Z
        {"X...", "XXX.", "....", "...."}, // J
        {"..X.", "XXX.", "....", "...."}  // L
    };

    int px = x;
    int py = y;
    rotation &= 3;

    if (rotation == 1) {
        px = y;
        py = 3 - x;
    } else if (rotation == 2) {
        px = 3 - x;
        py = 3 - y;
    } else if (rotation == 3) {
        px = 3 - y;
        py = x;
    }

    return kShapes[type][py][px] == 'X';
}

static bool Collides(const int board[20][10], const Piece& piece)
{
    for (int py = 0; py < 4; py++) {
        for (int px = 0; px < 4; px++) {
            if (!PieceCellFilled(piece.type, piece.rotation, px, py)) continue;

            int bx = piece.x + px;
            int by = piece.y + py;

            if (bx < 0 || bx >= 10 || by >= 20) return true;
            if (by >= 0 && board[by][bx] != 0) return true;
        }
    }
    return false;
}

static void LockPiece(int board[20][10], const Piece& piece, bool& gameOver)
{
    for (int py = 0; py < 4; py++) {
        for (int px = 0; px < 4; px++) {
            if (!PieceCellFilled(piece.type, piece.rotation, px, py)) continue;

            int bx = piece.x + px;
            int by = piece.y + py;

            if (by < 0) {
                gameOver = true;
                return;
            }
            if (by >= 0 && by < 20 && bx >= 0 && bx < 10) {
                board[by][bx] = piece.type + 1;
            }
        }
    }
}

static int ClearLines(int board[20][10])
{
    int cleared = 0;
    for (int y = 19; y >= 0; y--) {
        bool full = true;
        for (int x = 0; x < 10; x++) {
            if (board[y][x] == 0) {
                full = false;
                break;
            }
        }

        if (full) {
            cleared++;
            for (int row = y; row > 0; row--) {
                for (int x = 0; x < 10; x++) {
                    board[row][x] = board[row - 1][x];
                }
            }
            for (int x = 0; x < 10; x++) board[0][x] = 0;
            y++;
        }
    }
    return cleared;
}

int main()
{
    const int screenWidth = 800;
    const int screenHeight = 450;
    const int cellSize = 20;
    const int boardX = 280;
    const int boardY = 25;

    const Color pieceColors[8] = {
        {25, 25, 35, 255}, // empty (unused for blocks)
        SKYBLUE,           // I
        YELLOW,            // O
        PURPLE,            // T
        GREEN,             // S
        RED,               // Z
        BLUE,              // J
        ORANGE             // L
    };

    InitWindow(screenWidth, screenHeight, "raylib - simple tetris");
    SetTargetFPS(60);
    InitAudioDevice();

    Sound sfxMove{};
    Sound sfxRotate{};
    Sound sfxLock{};
    Sound sfxLineClear{};
    Sound sfxGameOver{};
    Sound sfxBark{};
    bool hasMoveSfx = false;
    bool hasRotateSfx = false;
    bool hasLockSfx = false;
    bool hasLineClearSfx = false;
    bool hasGameOverSfx = false;
    bool hasBarkSfx = false;

    auto loadSfx = [](const char* path, Sound& target, bool& readyFlag) {
        if (FileExists(path)) {
            target = LoadSound(path);
            readyFlag = target.frameCount > 0;
            if (readyFlag) SetSoundVolume(target, 0.65f);
        }
    };

    loadSfx("assets/sounds/move.wav", sfxMove, hasMoveSfx);
    loadSfx("assets/sounds/rotate.wav", sfxRotate, hasRotateSfx);
    loadSfx("assets/sounds/lock.wav", sfxLock, hasLockSfx);
    loadSfx("assets/sounds/line_clear.wav", sfxLineClear, hasLineClearSfx);
    loadSfx("assets/sounds/game_over.wav", sfxGameOver, hasGameOverSfx);
    loadSfx("assets/sounds/bark.wav", sfxBark, hasBarkSfx);
    if (hasBarkSfx) SetSoundVolume(sfxBark, 0.45f);


    int board[20][10] = {};
    Piece current{};
    std::array<int, 7> bag = {0, 1, 2, 3, 4, 5, 6};
    int bagIndex = 7;
    int nextType = 0;
    int holdType = -1;
    bool holdUsed = false;
    int score = 0;
    int lines = 0;
    int level = 1;
    bool gameOver = false;
    bool wasGameOver = false;
    float fallTimer = 0.0f;
    float shakeTimer = 0.0f;
    float shakeStrength = 0.0f;
    float lineFlashTimer = 0.0f;

    auto refillBag = [&]() {
        for (int i = 0; i < 7; i++) bag[i] = i;
        for (int i = 6; i > 0; i--) {
            int j = GetRandomValue(0, i);
            int t = bag[i];
            bag[i] = bag[j];
            bag[j] = t;
        }
        bagIndex = 0;
    };

    auto popFromBag = [&]() -> int {
        if (bagIndex >= 7) refillBag();
        return bag[bagIndex++];
    };

    auto spawnPiece = [&]() {
        current.type = nextType;
        current.rotation = 0;
        current.x = 3;
        current.y = -1;
        nextType = popFromBag();
        holdUsed = false;
        if (Collides(board, current)) gameOver = true;
        if (!gameOver && hasBarkSfx) PlaySound(sfxBark);
    };

    auto resetGame = [&]() {
        for (int y = 0; y < 20; y++) {
            for (int x = 0; x < 10; x++) board[y][x] = 0;
        }
        score = 0;
        lines = 0;
        level = 1;
        gameOver = false;
        wasGameOver = false;
        holdType = -1;
        holdUsed = false;
        fallTimer = 0.0f;
        shakeTimer = 0.0f;
        lineFlashTimer = 0.0f;
        bagIndex = 7;
        nextType = popFromBag();
        spawnPiece();
    };

    auto tryRotateWithKicks = [&](int direction) -> bool {
        Piece rotated = current;
        rotated.rotation = (rotated.rotation + direction + 4) & 3;
        const int kicks[6][2] = {{0, 0}, {-1, 0}, {1, 0}, {-2, 0}, {2, 0}, {0, -1}};

        for (int i = 0; i < 6; i++) {
            Piece candidate = rotated;
            candidate.x += kicks[i][0];
            candidate.y += kicks[i][1];
            if (!Collides(board, candidate)) {
                current = candidate;
                if (hasRotateSfx) PlaySound(sfxRotate);
                return true;
            }
        }
        return false;
    };

    auto drawPreviewPiece = [&](int type, int drawX, int drawY, int previewCell) {
        if (type < 0) return;
        for (int py = 0; py < 4; py++) {
            for (int px = 0; px < 4; px++) {
                if (!PieceCellFilled(type, 0, px, py)) continue;
                DrawRectangle(drawX + px * previewCell, drawY + py * previewCell, previewCell - 2, previewCell - 2, pieceColors[type + 1]);
            }
        }
    };

    nextType = popFromBag();
    spawnPiece();

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        if (shakeTimer > 0.0f) shakeTimer -= dt;
        if (lineFlashTimer > 0.0f) lineFlashTimer -= dt;
        if (shakeTimer < 0.0f) shakeTimer = 0.0f;
        if (lineFlashTimer < 0.0f) lineFlashTimer = 0.0f;

        if (IsKeyPressed(KEY_R)) resetGame();

        if (!gameOver) {
            level = lines / 10 + 1;
            float baseFall = 0.55f - (level - 1) * 0.045f;
            if (baseFall < 0.08f) baseFall = 0.08f;
            float currentFall = IsKeyDown(KEY_DOWN) ? 0.05f : baseFall;

            if (IsKeyPressed(KEY_LEFT)) {
                Piece test = current;
                test.x--;
                if (!Collides(board, test)) {
                    current = test;
                    if (hasMoveSfx) PlaySound(sfxMove);
                }
            }
            if (IsKeyPressed(KEY_RIGHT)) {
                Piece test = current;
                test.x++;
                if (!Collides(board, test)) {
                    current = test;
                    if (hasMoveSfx) PlaySound(sfxMove);
                }
            }
            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_X)) tryRotateWithKicks(1);
            if (IsKeyPressed(KEY_Z)) tryRotateWithKicks(-1);

            if (IsKeyPressed(KEY_C) && !holdUsed) {
                if (holdType < 0) {
                    holdType = current.type;
                    spawnPiece();
                } else {
                    int swapType = holdType;
                    holdType = current.type;
                    current.type = swapType;
                    current.rotation = 0;
                    current.x = 3;
                    current.y = -1;
                    if (Collides(board, current)) gameOver = true;
                }
                holdUsed = true;
            }

            if (IsKeyPressed(KEY_SPACE)) {
                Piece test = current;
                while (!Collides(board, test)) {
                    current = test;
                    test.y++;
                }
                fallTimer = currentFall;
            }

            fallTimer += dt;
            if (fallTimer >= currentFall) {
                fallTimer = 0.0f;
                Piece test = current;
                test.y++;
                if (!Collides(board, test)) {
                    current = test;
                } else {
                    LockPiece(board, current, gameOver);
                    if (!gameOver) {
                        shakeTimer = 0.05f;
                        shakeStrength = 2.5f;
                        if (hasLockSfx) PlaySound(sfxLock);
                    }
                    if (!gameOver) {
                        int cleared = ClearLines(board);
                        lines += cleared;
                        if (cleared > 0) {
                            lineFlashTimer = 0.12f;
                            shakeTimer = 0.09f;
                            shakeStrength = 3.5f + cleared * 0.7f;
                            if (hasLineClearSfx) PlaySound(sfxLineClear);
                        }
                        if (cleared == 1) score += 100 * level;
                        if (cleared == 2) score += 300 * level;
                        if (cleared == 3) score += 500 * level;
                        if (cleared == 4) score += 800 * level;
                        spawnPiece();
                    }
                }
            }
        }

        if (!wasGameOver && gameOver && hasGameOverSfx) PlaySound(sfxGameOver);
        wasGameOver = gameOver;

        int boardOffsetX = 0;
        int boardOffsetY = 0;
        if (shakeTimer > 0.0f) {
            boardOffsetX = (int)(GetRandomValue(-100, 100) * 0.01f * shakeStrength);
            boardOffsetY = (int)(GetRandomValue(-100, 100) * 0.01f * shakeStrength);
        }

        BeginDrawing();
        ClearBackground({18, 18, 24, 255});

        DrawRectangle(boardX - 2 + boardOffsetX, boardY - 2 + boardOffsetY, 10 * cellSize + 4, 20 * cellSize + 4, GRAY);
        DrawRectangle(boardX + boardOffsetX, boardY + boardOffsetY, 10 * cellSize, 20 * cellSize, BLACK);

        for (int y = 0; y < 20; y++) {
            for (int x = 0; x < 10; x++) {
                if (board[y][x] == 0) {
                    DrawRectangleLines(boardX + x * cellSize + boardOffsetX, boardY + y * cellSize + boardOffsetY, cellSize, cellSize, {35, 35, 45, 255});
                } else {
                    Color c = pieceColors[board[y][x]];
                    DrawRectangle(boardX + x * cellSize + 1 + boardOffsetX, boardY + y * cellSize + 1 + boardOffsetY, cellSize - 2, cellSize - 2, c);
                }
            }
        }

        if (!gameOver) {
            Piece ghost = current;
            Piece ghostTest = ghost;
            ghostTest.y++;
            while (!Collides(board, ghostTest)) {
                ghost = ghostTest;
                ghostTest.y++;
            }

            if (ghost.y != current.y) {
                Color ghostColor = Fade(pieceColors[current.type + 1], 0.35f);
                for (int py = 0; py < 4; py++) {
                    for (int px = 0; px < 4; px++) {
                        if (!PieceCellFilled(ghost.type, ghost.rotation, px, py)) continue;
                        int bx = ghost.x + px;
                        int by = ghost.y + py;
                        if (by < 0) continue;
                        DrawRectangleLines(boardX + bx * cellSize + 1 + boardOffsetX, boardY + by * cellSize + 1 + boardOffsetY, cellSize - 2, cellSize - 2, ghostColor);
                    }
                }
            }

            for (int py = 0; py < 4; py++) {
                for (int px = 0; px < 4; px++) {
                    if (!PieceCellFilled(current.type, current.rotation, px, py)) continue;
                    int bx = current.x + px;
                    int by = current.y + py;
                    if (by < 0) continue;
                    DrawRectangle(boardX + bx * cellSize + 1 + boardOffsetX, boardY + by * cellSize + 1 + boardOffsetY, cellSize - 2, cellSize - 2, pieceColors[current.type + 1]);
                }
            }
        }

        if (lineFlashTimer > 0.0f) {
            float flash = lineFlashTimer / 0.12f;
            DrawRectangle(boardX + boardOffsetX, boardY + boardOffsetY, 10 * cellSize, 20 * cellSize, Fade(RAYWHITE, 0.28f * flash));
        }

        DrawText("TETRIS", 40, 30, 40, RAYWHITE);
        DrawText(TextFormat("Score: %i", score), 40, 95, 26, RAYWHITE);
        DrawText(TextFormat("Lines: %i", lines), 40, 130, 26, RAYWHITE);
        DrawText(TextFormat("Level: %i", level), 40, 165, 26, RAYWHITE);

        DrawText("Hold:", 40, 205, 28, RAYWHITE);
        DrawRectangle(40, 240, 170, 120, {28, 28, 38, 255});
        drawPreviewPiece(holdType, 78, 260, 24);

        DrawText("Next:", 560, 55, 28, RAYWHITE);
        DrawRectangle(560, 90, 150, 120, {28, 28, 38, 255});
        drawPreviewPiece(nextType, 590, 110, 24);

        DrawText("Controls:", 540, 235, 24, RAYWHITE);
        DrawText("Left/Right: Move", 540, 268, 20, LIGHTGRAY);
        DrawText("Up/X/Z: Rotate", 540, 295, 20, LIGHTGRAY);
        DrawText("Down: Soft Drop", 540, 322, 20, LIGHTGRAY);
        DrawText("Space: Hard Drop", 540, 349, 20, LIGHTGRAY);
        DrawText("C: Hold Piece", 540, 376, 20, LIGHTGRAY);
        DrawText("R: Restart", 540, 403, 20, LIGHTGRAY);

        if (gameOver) {
            const char* text1 = "GAME OVER";
            const char* text2 = "Press R to restart";
            int w1 = MeasureText(text1, 50);
            int w2 = MeasureText(text2, 26);
            float pulse = 0.55f + 0.45f * (0.5f + 0.5f * (float)std::sin((float)GetTime() * 6.0f));
            DrawRectangle(150, 145, 500, 150, Fade(BLACK, 0.8f));
            DrawText(text1, (screenWidth - w1) / 2, 170, 50, Fade(RED, pulse));
            DrawText(text2, (screenWidth - w2) / 2, 230, 26, RAYWHITE);
        }

        EndDrawing();
    }

    if (hasMoveSfx) UnloadSound(sfxMove);
    if (hasRotateSfx) UnloadSound(sfxRotate);
    if (hasLockSfx) UnloadSound(sfxLock);
    if (hasLineClearSfx) UnloadSound(sfxLineClear);
    if (hasGameOverSfx) UnloadSound(sfxGameOver);
    if (hasBarkSfx) UnloadSound(sfxBark);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
