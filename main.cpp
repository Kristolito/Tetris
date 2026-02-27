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
    const int initialWidth = 1280;
    const int initialHeight = 720;

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

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(initialWidth, initialHeight, "raylib - simple tetris");
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
    constexpr int kPreviewCount = 5;
    std::array<int, 7> bag = {0, 1, 2, 3, 4, 5, 6};
    std::array<int, kPreviewCount> nextQueue = {};
    int bagIndex = 7;
    int holdType = -1;
    bool holdUsed = false;
    int score = 0;
    int lines = 0;
    int level = 1;
    bool gameOver = false;
    bool wasGameOver = false;
    float fallTimer = 0.0f;
    float lockTimer = 0.0f;
    bool grounded = false;
    int lockResets = 0;
    const float lockDelay = 0.45f;
    const int maxLockResets = 12;
    int moveDir = 0;
    float moveHoldTimer = 0.0f;
    float moveRepeatTimer = 0.0f;
    const float dasDelay = 0.16f;
    const float arrDelay = 0.045f;
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
        current.type = nextQueue[0];
        current.rotation = 0;
        current.x = 3;
        current.y = -1;
        for (int i = 0; i < kPreviewCount - 1; i++) nextQueue[i] = nextQueue[i + 1];
        nextQueue[kPreviewCount - 1] = popFromBag();
        holdUsed = false;
        grounded = false;
        lockTimer = 0.0f;
        lockResets = 0;
        moveDir = 0;
        moveHoldTimer = 0.0f;
        moveRepeatTimer = 0.0f;
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
        lockTimer = 0.0f;
        grounded = false;
        lockResets = 0;
        moveDir = 0;
        moveHoldTimer = 0.0f;
        moveRepeatTimer = 0.0f;
        shakeTimer = 0.0f;
        lineFlashTimer = 0.0f;
        bagIndex = 7;
        for (int i = 0; i < kPreviewCount; i++) nextQueue[i] = popFromBag();
        spawnPiece();
    };

    auto resetLockDelay = [&]() {
        if (grounded && lockResets < maxLockResets) {
            lockTimer = 0.0f;
            lockResets++;
        }
    };

    auto tryMoveHorizontal = [&](int dir) -> bool {
        Piece test = current;
        test.x += dir;
        if (!Collides(board, test)) {
            current = test;
            resetLockDelay();
            if (hasMoveSfx) PlaySound(sfxMove);
            return true;
        }
        return false;
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
                resetLockDelay();
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

    for (int i = 0; i < kPreviewCount; i++) nextQueue[i] = popFromBag();
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
                moveDir = -1;
                moveHoldTimer = 0.0f;
                moveRepeatTimer = 0.0f;
                tryMoveHorizontal(-1);
            }
            if (IsKeyPressed(KEY_RIGHT)) {
                moveDir = 1;
                moveHoldTimer = 0.0f;
                moveRepeatTimer = 0.0f;
                tryMoveHorizontal(1);
            }

            if (moveDir == -1 && !IsKeyDown(KEY_LEFT)) {
                if (IsKeyDown(KEY_RIGHT)) {
                    moveDir = 1;
                    moveHoldTimer = 0.0f;
                    moveRepeatTimer = 0.0f;
                    tryMoveHorizontal(1);
                } else {
                    moveDir = 0;
                }
            } else if (moveDir == 1 && !IsKeyDown(KEY_RIGHT)) {
                if (IsKeyDown(KEY_LEFT)) {
                    moveDir = -1;
                    moveHoldTimer = 0.0f;
                    moveRepeatTimer = 0.0f;
                    tryMoveHorizontal(-1);
                } else {
                    moveDir = 0;
                }
            }

            if ((moveDir == -1 && IsKeyDown(KEY_LEFT)) || (moveDir == 1 && IsKeyDown(KEY_RIGHT))) {
                moveHoldTimer += dt;
                if (moveHoldTimer >= dasDelay) {
                    moveRepeatTimer += dt;
                    while (moveRepeatTimer >= arrDelay) {
                        tryMoveHorizontal(moveDir);
                        moveRepeatTimer -= arrDelay;
                    }
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
                    grounded = false;
                    lockTimer = 0.0f;
                    lockResets = 0;
                    if (Collides(board, current)) gameOver = true;
                }
                holdUsed = true;
            }

            auto lockAndSpawn = [&]() {
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
            };

            if (IsKeyPressed(KEY_SPACE)) {
                Piece test = current;
                while (!Collides(board, test)) {
                    current = test;
                    test.y++;
                }
                lockAndSpawn();
                fallTimer = 0.0f;
            }

            fallTimer += dt;
            if (fallTimer >= currentFall) {
                fallTimer -= currentFall;
                Piece test = current;
                test.y++;
                if (!Collides(board, test)) {
                    current = test;
                }
            }

            Piece below = current;
            below.y++;
            if (Collides(board, below)) {
                if (!grounded) {
                    grounded = true;
                    lockTimer = 0.0f;
                    lockResets = 0;
                } else {
                    lockTimer += dt;
                }

                if (lockTimer >= lockDelay || lockResets >= maxLockResets) lockAndSpawn();
            } else {
                grounded = false;
                lockTimer = 0.0f;
                lockResets = 0;
            }
        }

        if (!wasGameOver && gameOver && hasGameOverSfx) PlaySound(sfxGameOver);
        wasGameOver = gameOver;

        int screenWidth = GetScreenWidth();
        int screenHeight = GetScreenHeight();

        int maxCellByHeight = (screenHeight - 40) / 20;
        int maxCellByWidth = (screenWidth - 440) / 10;
        int cellSize = maxCellByHeight < maxCellByWidth ? maxCellByHeight : maxCellByWidth;
        if (cellSize < 14) cellSize = 14;
        if (cellSize > 34) cellSize = 34;

        int boardWidth = 10 * cellSize;
        int boardHeight = 20 * cellSize;
        int panelWidth = 180;
        int sideGap = cellSize + 8;
        int totalLayoutWidth = panelWidth + sideGap + boardWidth + sideGap + panelWidth;
        int layoutX = (screenWidth - totalLayoutWidth) / 2;
        if (layoutX < 10) layoutX = 10;
        int leftPanelX = layoutX;
        int boardX = leftPanelX + panelWidth + sideGap;
        int rightPanelX = boardX + boardWidth + sideGap;
        int boardY = (screenHeight - boardHeight) / 2;
        if (boardY < 10) boardY = 10;

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

        int titleSize = cellSize + 16;
        if (titleSize < 28) titleSize = 28;
        if (titleSize > 48) titleSize = 48;
        int statSize = cellSize / 2 + 14;
        if (statSize < 20) statSize = 20;
        if (statSize > 30) statSize = 30;
        int labelSize = cellSize / 2 + 12;
        if (labelSize < 18) labelSize = 18;
        if (labelSize > 28) labelSize = 28;
        int controlSize = cellSize / 3 + 10;
        if (controlSize < 11) controlSize = 11;
        if (controlSize > 16) controlSize = 16;

        DrawText("TETRIS", leftPanelX, boardY + 10, titleSize, RAYWHITE);
        DrawText(TextFormat("Score: %i", score), leftPanelX, boardY + 22 + titleSize, statSize, RAYWHITE);
        DrawText(TextFormat("Lines: %i", lines), leftPanelX, boardY + 30 + titleSize + statSize, statSize, RAYWHITE);
        DrawText(TextFormat("Level: %i", level), leftPanelX, boardY + 38 + titleSize + statSize * 2, statSize, RAYWHITE);

        int holdBoxY = boardY + 58 + titleSize + statSize * 3;
        int holdBoxH = 120;
        DrawText("Hold:", leftPanelX, holdBoxY - 32, labelSize, RAYWHITE);
        DrawRectangle(leftPanelX, holdBoxY, panelWidth, holdBoxH, {28, 28, 38, 255});
        int holdPreviewCell = cellSize + 3;
        if (holdPreviewCell < 16) holdPreviewCell = 16;
        if (holdPreviewCell > 24) holdPreviewCell = 24;
        drawPreviewPiece(holdType, leftPanelX + (panelWidth - holdPreviewCell * 4) / 2, holdBoxY + 16, holdPreviewCell);

        DrawText("Next:", rightPanelX, boardY + 10, labelSize, RAYWHITE);
        int nextPreviewCell = cellSize - 6;
        if (nextPreviewCell < 10) nextPreviewCell = 10;
        if (nextPreviewCell > 16) nextPreviewCell = 16;
        int nextStep = nextPreviewCell * 4 + 8;
        int nextBoxY = boardY + 45;
        int nextBoxH = kPreviewCount * nextStep + 12;
        DrawRectangle(rightPanelX, nextBoxY, panelWidth, nextBoxH, {28, 28, 38, 255});
        for (int i = 0; i < kPreviewCount; i++) {
            drawPreviewPiece(nextQueue[i], rightPanelX + (panelWidth - nextPreviewCell * 4) / 2, nextBoxY + 6 + i * nextStep, nextPreviewCell);
        }

        int controlsY = nextBoxY + nextBoxH + 14;
        DrawText("Controls:", rightPanelX, controlsY, labelSize - 4, RAYWHITE);
        DrawText("Left/Right: Move", rightPanelX, controlsY + 24, controlSize, LIGHTGRAY);
        DrawText("Up/X/Z: Rotate", rightPanelX, controlsY + 24 + controlSize + 3, controlSize, LIGHTGRAY);
        DrawText("Down: Soft Drop", rightPanelX, controlsY + 24 + (controlSize + 3) * 2, controlSize, LIGHTGRAY);
        DrawText("Space: Hard Drop", rightPanelX, controlsY + 24 + (controlSize + 3) * 3, controlSize, LIGHTGRAY);
        DrawText("C: Hold   R: Restart", rightPanelX, controlsY + 24 + (controlSize + 3) * 4, controlSize - 1, LIGHTGRAY);

        if (gameOver) {
            const char* text1 = "GAME OVER";
            const char* text2 = "Press R to restart";
            int goTitleSize = titleSize;
            int goSubSize = statSize;
            int w1 = MeasureText(text1, goTitleSize);
            int w2 = MeasureText(text2, goSubSize);
            float pulse = 0.55f + 0.45f * (0.5f + 0.5f * (float)std::sin((float)GetTime() * 6.0f));
            int boxW = boardWidth + panelWidth;
            if (boxW > screenWidth - 60) boxW = screenWidth - 60;
            int boxH = goTitleSize + goSubSize + 60;
            int boxX = (screenWidth - boxW) / 2;
            int boxY = (screenHeight - boxH) / 2;
            DrawRectangle(boxX, boxY, boxW, boxH, Fade(BLACK, 0.8f));
            DrawText(text1, (screenWidth - w1) / 2, boxY + 16, goTitleSize, Fade(RED, pulse));
            DrawText(text2, (screenWidth - w2) / 2, boxY + 24 + goTitleSize, goSubSize, RAYWHITE);
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
