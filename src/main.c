#include <stdbool.h>
#include "raylib.h"
#include <stdlib.h>
#include <time.h>

typedef enum State {
    START,
    PLAYING,
    WIN,
    LOSE
} State;

typedef enum CellMark {
    CELL_CLEARED,
    CELL_FLAGGED,
    CELL_QUESTIONED
} CellMark;

typedef enum CellType {
    BLANK_TILE,
    NUMBER,
    MINE,
    MINE_EXPLOSION,
    ANY
} CellType;

typedef struct TILE {
    CellType TYPE;
    int AMOUNT;
    CellMark MARK;
    bool VISIBLE;
} TILE;

typedef struct Status {
    int WIDTH;
    int HEIGHT;
    int W_TILES;
    int H_TILES;
    int TILE;
    int BOMBS;
    int VISIBLE_TILES;
    State STATE;
    CellType FIRST_CELL;
    unsigned int MAX_ITERATIONS;
} Status;

void initializeBoard(TILE **board, int width, int height) {
    for (int i = 0; i < width; i++) {
        for (int j = 0; j < height; j++) {
            board[i][j].TYPE = BLANK_TILE;
            board[i][j].AMOUNT = 0;
            board[i][j].VISIBLE = false;
            board[i][j].MARK = CELL_CLEARED;
        }
    }
}

void freeMem(Status status, TILE **board) {
    for (int i = 0; i < status.W_TILES; i++) {
        free(board[i]);
    }
    free(board);
}

void generateBombs(TILE **board, int count, Status status) {
    int x, y;

    for (int i = 0; i < count; i++) {
        x = rand() % status.W_TILES;
        y = rand() % status.H_TILES;
        board[x][y].TYPE = MINE;
    }
}

void generateNumbers(TILE **board, Status *status) {
    int directions[8][2] = {
            {-1, -1}, {-1, 0}, {-1, 1},
            { 0, -1},          { 0, 1},
            { 1, -1}, { 1, 0}, { 1, 1}
    };
    status->BOMBS = 0;
    for (int x = 0; x < status->W_TILES; x++) {
        for (int y = 0; y < status->H_TILES; y++) {
            if (board[x][y].TYPE != MINE) {
                for (int d = 0; d < 8; d++) {
                    int newX = x + directions[d][0];
                    int newY = y + directions[d][1];

                    if (newX >= 0 && newX < status->W_TILES && newY >= 0 && newY < status->H_TILES) {
                        if (board[newX][newY].TYPE == MINE) {
                            board[x][y].TYPE = NUMBER;
                            board[x][y].AMOUNT += 1;
                        }
                    }
                }
            } else {
                status->BOMBS += 1;
            }
        }
    }
}

void revealEmptyCells(TILE **board, int x, int y, Status *status) {
    int directions[8][2] = {
            {-1, -1}, {-1, 0}, {-1, 1},
            { 0, -1},          { 0, 1},
            { 1, -1}, { 1, 0}, { 1, 1}
    };

    if (x < 0 || x >= status->W_TILES || y < 0 || y >= status->H_TILES || board[x][y].VISIBLE) {
        return;
    }

    if (board[x][y].MARK == CELL_FLAGGED) {
        return;
    }

    board[x][y].VISIBLE = true;
    status->VISIBLE_TILES += 1;

    if (board[x][y].TYPE == MINE) {
        board[x][y].TYPE = MINE_EXPLOSION;
        status->STATE = LOSE;
        return;
    }

    if ((status->VISIBLE_TILES + status->BOMBS) == (status->W_TILES * status->H_TILES)) {
        status->STATE = WIN;
    }

    if (board[x][y].TYPE == NUMBER) {
        return;
    }


    for (int d = 0; d < 8; d++) {
        int newX = x + directions[d][0];
        int newY = y + directions[d][1];
        revealEmptyCells(board, newX, newY, status);
    }
}

int main( int argc, char *argv[] )
{

    time_t t;
    srand((unsigned)time(&t));

    Status status = {
            .TILE = 40,
            .W_TILES = 32,
            .H_TILES = 16,
            .BOMBS = 99,
            .STATE = START,
            .VISIBLE_TILES = 0,
            .FIRST_CELL = BLANK_TILE,
            .MAX_ITERATIONS = 10000
    };
    status.WIDTH = status.W_TILES * status.TILE;
    status.HEIGHT = status.H_TILES * status.TILE;

    unsigned int iterationCounter = 0;

    Status defaultStatus = status;

    TILE **board = malloc(status.W_TILES * sizeof(TILE *));
    for (int i = 0; i < status.W_TILES; i++) {
        board[i] = malloc(status.H_TILES * sizeof(TILE));
    }

    initializeBoard(board, status.W_TILES, status.H_TILES);

    InitWindow(status.WIDTH, status.HEIGHT, "Minesweeper");


    SetTargetFPS(60);

    // GENERATE BOMBS And NUMBERS
    generateBombs(board, status.BOMBS, status);
    generateNumbers(board, &status);

#ifndef PLATFORM_ANDROID
    ChangeDirectory("assets");
#endif
    Image atlas = LoadImage("game/atlas.png");
    Image cursorImg = LoadImage("game/pointer.png");

#ifndef PLATFORM_ANDROID
    ChangeDirectory("..");
#endif


    ImageResize(&cursorImg, status.TILE, status.TILE);
    Texture cursor = LoadTextureFromImage(cursorImg);
    UnloadImage(cursorImg);
    Vector2 cursorRect = {0, 0};

    // SPRITE ARRAY
    Texture sprites[16];

    int spriteIndex = 0;
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            Rectangle rect = { col * 16, row * 16, 16, 16 };
            Image img = ImageFromImage(atlas, rect);
            ImageResize(&img, status.TILE, status.TILE);
            sprites[spriteIndex] = LoadTextureFromImage(img);
            UnloadImage(img);
            spriteIndex++;
        }
    }
    UnloadImage(atlas);


    // GENERAL VAR SETTINGS
    bool setVisibleTiles = false;

    // CURSOR RECT ON TILES
    int rectX, rectY;

    while (!WindowShouldClose()) {


        // CURSOR MOVEMENT
        if (IsKeyPressed(KEY_A)) {
            cursorRect.x -= status.TILE;
        }

        if (IsKeyPressed(KEY_D)) {
            cursorRect.x += status.TILE;
        }

        if (IsKeyPressed(KEY_W)) {
            cursorRect.y -= status.TILE;
        }

        if (IsKeyPressed(KEY_S)) {
            cursorRect.y += status.TILE;
        }

        switch (GetGestureDetected()) {
            case GESTURE_SWIPE_RIGHT: cursorRect.x += status.TILE; break;
            case GESTURE_SWIPE_LEFT: cursorRect.x -= status.TILE; break;
            case GESTURE_SWIPE_UP: cursorRect.y -= status.TILE; break;
            case GESTURE_SWIPE_DOWN: cursorRect.y += status.TILE; break;
        }

        // CURSOR DOESN'T GO OUTSIDE THE WINDOW
        if (cursorRect.x == status.WIDTH) {
            cursorRect.x = 0;
        }

        if (cursorRect.x == -status.TILE) {
            cursorRect.x = status.WIDTH - status.TILE;
        }

        if (cursorRect.y == status.HEIGHT) {
            cursorRect.y = 0;
        }

        if (cursorRect.y == -status.TILE) {
            cursorRect.y = status.HEIGHT - status.TILE;
        }

        rectX = cursorRect.x/status.TILE;
        rectY = cursorRect.y/status.TILE;



        // GAMEPLAY
        if (IsKeyPressed(KEY_O) || IsGestureDetected(GESTURE_PINCH_IN)) {
            if (status.STATE != defaultStatus.STATE) {
                status.STATE = defaultStatus.STATE;
                setVisibleTiles = false;
            }
            status.BOMBS = defaultStatus.BOMBS;
            status.VISIBLE_TILES = defaultStatus.VISIBLE_TILES;
            initializeBoard(board, status.W_TILES, status.H_TILES);
            generateBombs(board, status.BOMBS, status);
            generateNumbers(board, &status);
        }

        if (IsKeyPressed(KEY_P) || IsGestureDetected(GESTURE_PINCH_OUT)) {
            setVisibleTiles = !setVisibleTiles;
        }

        if (IsKeyPressed(KEY_K) || IsGestureDetected(GESTURE_DOUBLETAP)) {
            iterationCounter = 0;
            if (status.STATE == START && status.FIRST_CELL != ANY) {
                while (board[rectX][rectY].TYPE != status.FIRST_CELL) {
                    status.BOMBS = defaultStatus.BOMBS;
                    status.VISIBLE_TILES = defaultStatus.VISIBLE_TILES;
                    initializeBoard(board, status.W_TILES, status.H_TILES);
                    generateBombs(board, status.BOMBS, status);
                    generateNumbers(board, &status);

                    iterationCounter++;
                    if (iterationCounter > status.MAX_ITERATIONS) {
                        CloseWindow();
                        break;
                    }

                }
                status.STATE = PLAYING;
            }
            if (board[rectX][rectY].MARK != CELL_FLAGGED && (status.STATE == START || status.STATE == PLAYING)) {
                revealEmptyCells(board, rectX, rectY, &status);
            }
        }

        if (IsKeyPressed(KEY_L) || (IsGestureDetected(GESTURE_HOLD) && GetGestureHoldDuration() > 500)) {
            if (status.STATE == PLAYING && (!board[rectX][rectY].VISIBLE || (setVisibleTiles && !board[rectX][rectY].VISIBLE))) {
                board[rectX][rectY].MARK = (board[rectX][rectY].MARK + 1) % 3;
            } else {
                board[rectX][rectY].MARK = CELL_CLEARED;
            }
        }




        BeginDrawing();

        ClearBackground(RAYWHITE);

        // RENDER TILES
        for (int x = 0; x < status.W_TILES; x++) {
            for (int y = 0; y < status.H_TILES; y++) {
                // SET RECT FOR ALL TILES
                Vector2 rect = {x * status.TILE,
                                y * status.TILE};

                if (board[x][y].VISIBLE || setVisibleTiles || status.STATE == LOSE || status.STATE == WIN) {

                    if (board[x][y].TYPE == BLANK_TILE) {
                        DrawTextureV(sprites[8], rect, WHITE);
                        if (board[x][y].MARK == CELL_FLAGGED && status.STATE == LOSE) {
                            DrawTextureV(sprites[11], rect, WHITE);
                        }
                    }

                    if (board[x][y].TYPE == NUMBER) {
                        DrawTextureV(sprites[board[x][y].AMOUNT-1], rect, WHITE);

                        if (board[x][y].MARK == CELL_FLAGGED && status.STATE == LOSE) {
                            DrawTextureV(sprites[11], rect, WHITE);
                        }
                    }

                    if (board[x][y].TYPE == MINE) {
                        DrawTextureV(sprites[14], rect, WHITE);
                    }

                    if (board[x][y].TYPE == MINE_EXPLOSION) {
                        DrawTextureV(sprites[15], rect, WHITE);
                    }
                } else {

                    DrawTextureV(sprites[9], rect, WHITE);

                    if (board[x][y].MARK == CELL_FLAGGED) {
                        DrawTextureV(sprites[10], rect, WHITE);
                    }

                    if (board[x][y].MARK == CELL_QUESTIONED) {
                        DrawTextureV(sprites[13], rect, WHITE);
                    }
                }
            }
        }

        // RENDER CURSOR
        DrawTextureV(cursor, cursorRect, WHITE);



        EndDrawing();

    }

    CloseWindow();          // Close window and OpenGL context

    freeMem(status, board);


    return 0;

}


