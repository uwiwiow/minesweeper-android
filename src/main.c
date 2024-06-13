#include <stdbool.h>
#include "raylib.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>

#define handle_error(msg) \
           do { perror(msg); } while (0);

#define MAX_CLIENTS 100

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

typedef enum CellAction {
    NONE = -1,
    OPENED,
    CLEAR,
    FLAGGED,
    QUESTIONED
} CellAction;

typedef struct Packet {
    Vector2 pos_cursor;
    CellAction action_tile;
    unsigned int seed;
    State state;
} Packet;

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
    SetTraceLogLevel(LOG_WARNING);

    int socket_fd;
    struct addrinfo  *result, *rp;

    struct addrinfo hints = {
            .ai_family = AF_INET,
            .ai_socktype = SOCK_STREAM,
            .ai_protocol = 0,
            .ai_flags = 0,
    };

    if (getaddrinfo("127.0.0.1", "12345", &hints, &result) < 0)
        handle_error("getaddrinfo")

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        socket_fd = socket(rp->ai_family, rp->ai_socktype,
                           rp->ai_protocol);
        if (socket_fd == -1)
            continue;

        if (connect(socket_fd, rp->ai_addr, rp->ai_addrlen) != -1)
            break;                  /* Success */

        close(socket_fd);
    }

    freeaddrinfo(result);           /* No longer needed */

    if (rp == NULL) {               /* No address succeeded */
        fprintf(stderr, "Could not connect\n");
        exit(EXIT_FAILURE);
    }

    unsigned int seed = 0;
    srand(seed);

    Status status = {
            .TILE = 108,
            .W_TILES = 10,
            .H_TILES = 18,
            .BOMBS = 35,
            .STATE = START,
            .VISIBLE_TILES = 0,
            .FIRST_CELL = BLANK_TILE,
            .MAX_ITERATIONS = 10000
    };
    status.WIDTH = 1080;
    status.HEIGHT = 2292;

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
    Image aImg = LoadImage("buttons/A.png");
    Image bImg = LoadImage("buttons/B.png");
    Image rImg = LoadImage("buttons/R.png");
#ifndef PLATFORM_ANDROID
    ChangeDirectory("..");
#endif

    ImageResize(&aImg, aImg.width*2, aImg.height*2);
    ImageResize(&bImg, bImg.width*2, bImg.height*2);
    ImageResize(&rImg, rImg.width*2, rImg.height*2);
    Texture aBtn = LoadTextureFromImage(aImg);
    Texture bBtn = LoadTextureFromImage(bImg);
    Texture rBtn = LoadTextureFromImage(rImg);
    UnloadImage(aImg);
    UnloadImage(bImg);
    UnloadImage(rImg);
    Rectangle aBtnLimit = {status.WIDTH/3 - aBtn.width - 20, status.TILE * status.H_TILES + 10, aBtn.width, aBtn.height};
    Rectangle bBtnLimit = {status.WIDTH/3*2 - bBtn.width - 20, status.TILE * status.H_TILES + 10, bBtn.width, bBtn.height};
    Rectangle rBtnLimit = {status.WIDTH - rBtn.width - 20, status.TILE * status.H_TILES + 10, rBtn.width, rBtn.height};


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
    Vector2 touchPosition = {0, 0};
    Vector2 lastTouchPosition = {0, 0};
    Rectangle touchLimit = {0, 0, status.TILE * status.W_TILES, status.TILE * status.H_TILES};


    Vector2 cursors[MAX_CLIENTS];

    Packet packet = {
            cursorRect,
            NONE
    };
    Packet allPackets[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) {
        allPackets[i] = (Packet) {
                cursorRect,
                NONE
        };
    }

    while (!WindowShouldClose()) {

        lastTouchPosition = touchPosition;
        touchPosition = GetTouchPosition(0);

        if (CheckCollisionPointRec(touchPosition, touchLimit) && (lastTouchPosition.x != touchPosition.x || lastTouchPosition.y != touchPosition.y)) {
            cursorRect.x = touchPosition.x - ( (int) touchPosition.x % status.TILE);
            cursorRect.y = touchPosition.y - ( (int) touchPosition.y % status.TILE);
        }

        rectX = cursorRect.x / status.TILE;
        rectY = cursorRect.y / status.TILE;

        packet.pos_cursor = cursorRect;
        packet.action_tile = NONE;

        // GAMEPLAY
        if (CheckCollisionPointRec(touchPosition, rBtnLimit) && (lastTouchPosition.x != touchPosition.x || lastTouchPosition.y != touchPosition.y)) {
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

        if (IsGestureDetected(GESTURE_PINCH_OUT)) {
            setVisibleTiles = !setVisibleTiles;
        }

        if ((CheckCollisionPointRec(touchPosition, aBtnLimit) && (lastTouchPosition.x != touchPosition.x || lastTouchPosition.y != touchPosition.y)) || (CheckCollisionPointRec(touchPosition, touchLimit) && (IsGestureDetected(GESTURE_DOUBLETAP)))) {
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
                packet.action_tile = OPENED;
            }
        }

        if (CheckCollisionPointRec(touchPosition, bBtnLimit) && (lastTouchPosition.x != touchPosition.x || lastTouchPosition.y != touchPosition.y)) {
            if (status.STATE == PLAYING && (!board[rectX][rectY].VISIBLE || (setVisibleTiles && !board[rectX][rectY].VISIBLE))) {
                board[rectX][rectY].MARK = (board[rectX][rectY].MARK + 1) % 3;
                packet.action_tile = (board[rectX][rectY].MARK) + 1;
            } else {
                board[rectX][rectY].MARK = CELL_CLEARED;
            }
        }


        packet.state = status.STATE;


        if (send(socket_fd, &packet, sizeof(packet), 0) == -1)
            handle_error("send")

        if (packet.action_tile >= 0 ) {
            printf("Sent Packet: Cursor(%f, %f),Action(%d)\n",
                   packet.pos_cursor.x, packet.pos_cursor.y, packet.action_tile);
        }

        ssize_t nbytes_read = recv(socket_fd, allPackets, sizeof(allPackets), 0);
        if (nbytes_read == -1)
            handle_error("recv");

        int num_packets_received = nbytes_read / sizeof(Packet);
//        printf("Received %d Packets from server:\n", num_packets_received);
        for (int i = 0; i < num_packets_received; i++) {
            if (allPackets[i].action_tile != -1 ) {
                printf("Packet %d: Cursor(%f, %f),Action(%d)\n",
                       i, allPackets[i].pos_cursor.x, allPackets[i].pos_cursor.y, allPackets[i].action_tile);
            }

            if (allPackets[i].seed != seed) {
                seed = allPackets[i].seed;
                printf("seed %du\n", seed);
                srand(seed);
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

            int pkRectX = allPackets[i].pos_cursor.x/status.TILE;
            int pkRectY = allPackets[i].pos_cursor.y/status.TILE;

            switch (allPackets[i].action_tile) {
                case NONE:
                    break;
                case OPENED: {
                    iterationCounter = 0;
                    if (status.STATE == START && status.FIRST_CELL != ANY) {
                        while (board[pkRectX][pkRectY].TYPE != status.FIRST_CELL) {
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
                        revealEmptyCells(board, pkRectX, pkRectY, &status);
                    }
                }
                    break;
                case CLEAR: board[pkRectX][pkRectY].MARK = CELL_CLEARED;
                    break;
                case FLAGGED: board[pkRectX][pkRectY].MARK = CELL_FLAGGED;
                    break;
                case QUESTIONED: board[pkRectX][pkRectY].MARK = CELL_QUESTIONED;
                    break;
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
        for (int p = 0; p < num_packets_received; p++) {
            DrawTextureV(cursor, allPackets[p].pos_cursor, WHITE);
        }

        DrawTextureV(aBtn, (Vector2) {aBtnLimit.x, aBtnLimit.y}, WHITE);
        DrawTextureV(bBtn, (Vector2) {bBtnLimit.x, bBtnLimit.y}, WHITE);
        DrawTextureV(rBtn, (Vector2) {rBtnLimit.x, rBtnLimit.y}, WHITE);

        EndDrawing();

    }

    UnloadTexture(aBtn);
    UnloadTexture(bBtn);
    UnloadTexture(cursor);

    for (int i = 0; i < 16; i++) {
        UnloadTexture(sprites[i]);
    }

    CloseWindow();          // Close window and OpenGL context

    freeMem(status, board);


    return 0;

}


