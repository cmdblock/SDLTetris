#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define ARENA_WIDTH 10
#define ARENA_HEIGHT 20

typedef struct {
    int x, y;
    int shape[4][4];
} Tetromino;

Tetromino currentPiece;
uint8_t arena[ARENA_HEIGHT][ARENA_WIDTH];

void initGame() {
    // 初始化游戏状态
    memset(arena, 0, sizeof(arena));
    // 初始化当前方块
    currentPiece.x = ARENA_WIDTH / 2 - 2;
    currentPiece.y = 0;
    // 这里可以初始化方块的形状
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
    while (!quit) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = true;
            } else if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                case SDLK_LEFT:
                    // 左移方块
                    break;
                case SDLK_RIGHT:
                    // 右移方块
                    break;
                case SDLK_DOWN:
                    // 加速下落
                    break;
                case SDLK_UP:
                    // 旋转方块
                    break;
                }
            }
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
