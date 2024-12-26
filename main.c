#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define WINDOW_WIDTH 800    // 游戏窗口的宽度（像素）
#define WINDOW_HEIGHT 600   // 游戏窗口的高度（像素）
#define ARENA_WIDTH 12      // 游戏区域（俄罗斯方块下落区域）的宽度（方块数量）
#define ARENA_HEIGHT 20     // 游戏区域的高度（方块数量）

typedef struct {
    int x, y;
    int shape[4][4];
    int type;
} Tetromino;

Tetromino currentPiece;
uint8_t arena[ARENA_HEIGHT][ARENA_WIDTH];

// 所有俄罗斯方块的形状
const int tetrominoes[7][4][4] = {
    // I型
    {
        {0,0,0,0},
        {1,1,1,1},
        {0,0,0,0},
        {0,0,0,0}
    },
    // O型
    {
        {0,0,0,0},
        {0,1,1,0},
        {0,1,1,0},
        {0,0,0,0}
    },
    // T型
    {
        {0,0,0,0},
        {0,1,1,1},
        {0,0,1,0},
        {0,0,0,0}
    },
    // S型
    {
        {0,0,0,0},
        {0,0,1,1},
        {0,1,1,0},
        {0,0,0,0}
    },
    // Z型
    {
        {0,0,0,0},
        {0,1,1,0},
        {0,0,1,1},
        {0,0,0,0}
    },
    // J型
    {
        {0,0,0,0},
        {0,1,0,0},
        {0,1,1,1},
        {0,0,0,0}
    },
    // L型
    {
        {0,0,0,0},
        {0,0,0,1},
        {0,1,1,1},
        {0,0,0,0}
    }
};

bool checkCollision(Tetromino *piece) {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (piece->shape[i][j]) {
                int x = piece->x + j;
                int y = piece->y + i;
                if (x < 0 || x >= ARENA_WIDTH || y >= ARENA_HEIGHT || 
                    (y >= 0 && arena[y][x])) {
                    return true;
                }
            }
        }
    }
    return false;
}

void lockPiece() {
    // 将当前方块锁定到游戏区域
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (currentPiece.shape[i][j]) {
                int x = currentPiece.x + j;
                int y = currentPiece.y + i;
                if (y >= 0) {
                    arena[y][x] = 1;
                }
            }
        }
    }
}

void newPiece() {
    // 随机生成新方块
    currentPiece.type = rand() % 7;
    memcpy(currentPiece.shape, tetrominoes[currentPiece.type], sizeof(currentPiece.shape));
    currentPiece.x = ARENA_WIDTH / 2 - 2;  // 初始位置居中，-2是因为方块宽度为4
    currentPiece.y = -2;
    
    if (checkCollision(&currentPiece)) {
        // 游戏结束
        memset(arena, 0, sizeof(arena));
    }
}

void initGame() {
    // 初始化游戏状态
    memset(arena, 0, sizeof(arena));
    srand(SDL_GetTicks());
    newPiece();
}

void drawPiece(SDL_Renderer *renderer, Tetromino *piece) {
    // 绘制当前方块
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (piece->shape[i][j]) {
                SDL_Rect rect = {(piece->x + j) * 30, (piece->y + i) * 30, 30,
                                 30};
                SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
                SDL_RenderFillRect(renderer, &rect);
            }
        }
    }
}

void clearLines() {
    for (int i = ARENA_HEIGHT - 1; i >= 0; i--) {
        bool full = true;
        for (int j = 0; j < ARENA_WIDTH; j++) {
            if (!arena[i][j]) {
                full = false;
                break;
            }
        }
        if (full) {
            // 将上面的行下移
            for (int k = i; k > 0; k--) {
                memcpy(arena[k], arena[k-1], ARENA_WIDTH);
            }
            memset(arena[0], 0, ARENA_WIDTH);
            i++; // 重新检查当前行
        }
    }
}

void drawArena(SDL_Renderer *renderer) {
    // 绘制游戏区域
    for (int i = 0; i < ARENA_HEIGHT; i++) {
        for (int j = 0; j < ARENA_WIDTH; j++) {
            if (arena[i][j]) {
                SDL_Rect rect = {j * 30, i * 30, 30, 30};
                SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
                SDL_RenderFillRect(renderer, &rect);
            }
        }
    }
}

int main(int argv, char *args[]) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    if (TTF_Init() == -1) {
        printf("TTF could not initialize! TTF_Error: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("Tetris", SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH,
                                          WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    if (window == NULL) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer =
        SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL) {
        printf("Renderer could not be created! SDL_Error: %s\n",
               SDL_GetError());
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    initGame();

    // 游戏主循环
    bool quit = false;
    SDL_Event e;
    Uint32 lastFall = SDL_GetTicks();
    while (!quit) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = true;
            } else if (e.type == SDL_KEYDOWN) {
                Tetromino temp = currentPiece;
                switch (e.key.keysym.sym) {
                case SDLK_LEFT:
                    temp.x--;
                    if (!checkCollision(&temp)) currentPiece.x--;
                    break;
                case SDLK_RIGHT:
                    temp.x++;
                    if (!checkCollision(&temp)) currentPiece.x++;
                    break;
                case SDLK_DOWN:
                    temp.y++;
                    if (!checkCollision(&temp)) currentPiece.y++;
                    break;
                case SDLK_UP:
                    // 旋转方块
                    Tetromino rotated = currentPiece;
                    for (int i = 0; i < 4; i++) {
                        for (int j = 0; j < 4; j++) {
                            rotated.shape[i][j] = currentPiece.shape[3 - j][i];
                        }
                    }
                    if (!checkCollision(&rotated)) {
                        currentPiece = rotated;
                    }
                    break;
                }
            }
        }

        // 自动下落
        if (SDL_GetTicks() - lastFall > 500) {
            Tetromino temp = currentPiece;
            temp.y++;
            if (!checkCollision(&temp)) {
                currentPiece.y++;
            } else {
                lockPiece();
                clearLines();
                newPiece();
            }
            lastFall = SDL_GetTicks();
        }

        // 清屏
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // 绘制游戏区域和当前方
        drawArena(renderer);
        drawPiece(renderer, &currentPiece);

        // 更新屏幕
        SDL_RenderPresent(renderer);
    }

    // 清理资源
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    return 0;
}
