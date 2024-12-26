#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

int score = 0;  // 分数变量

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
                    arena[y][x] = currentPiece.type + 1; // 存储方块类型+1（0表示空）
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

// 定义每种方块类型的颜色
const SDL_Color pieceColors[7] = {
    {0, 255, 255, 255},   // I型：青色
    {255, 255, 0, 255},   // O型：黄色
    {128, 0, 128, 255},   // T型：紫色
    {0, 255, 0, 255},     // S型：绿色
    {255, 0, 0, 255},     // Z型：红色
    {0, 0, 255, 255},     // J型：蓝色
    {255, 165, 0, 255}    // L型：橙色
};

void drawPiece(SDL_Renderer *renderer, Tetromino *piece) {
    // 绘制当前方块
    SDL_Color color = pieceColors[piece->type];
    int blockSize = 24;  // 每个小方块的实际大小
    int gap = 6;         // 方块之间的间隔
    
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (piece->shape[i][j]) {
                SDL_Rect rect = {
                    (piece->x + j) * (blockSize + gap) + gap, 
                    (piece->y + i) * (blockSize + gap) + gap, 
                    blockSize, 
                    blockSize
                };
                SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
                SDL_RenderFillRect(renderer, &rect);
            }
        }
    }
}

Mix_Chunk *clearSound = NULL;  // 消除音效

void clearLines() {
    int linesCleared = 0;  // 记录消除的行数
    
    // 先检查有多少行要消除
    for (int i = ARENA_HEIGHT - 1; i >= 0; i--) {
        bool full = true;
        for (int j = 0; j < ARENA_WIDTH; j++) {
            if (!arena[i][j]) {
                full = false;
                break;
            }
        }
        if (full) {
            linesCleared++;
        }
    }
    
    // 如果有消除行，先播放音效
    if (linesCleared > 0 && clearSound) {
        Mix_PlayChannel(-1, clearSound, 0);
    }
    
    // 然后执行消除动画
    for (int i = ARENA_HEIGHT - 1; i >= 0; i--) {
        bool full = true;
        for (int j = 0; j < ARENA_WIDTH; j++) {
            if (!arena[i][j]) {
                full = false;
                break;
            }
        }
        
        if (full) {
            for (int k = i; k > 0; k--) {
                memcpy(arena[k], arena[k-1], ARENA_WIDTH);
            }
            memset(arena[0], 0, ARENA_WIDTH);
            i++; 
        }
    }
    
    // 根据消除的行数更新分数
    // 使用俄罗斯方块的标准计分规则：
    // 1行：100分
    // 2行：300分
    // 3行：500分
    // 4行：800分（Tetris）
    switch (linesCleared) {
        case 1: score += 100; break;
        case 2: score += 300; break;
        case 3: score += 500; break;
        case 4: score += 800; break;
    }
}

void drawScore(SDL_Renderer *renderer) {
    // 绘制分割线
    // 设置分割线宽度为7像素
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // 白色
    for (int i = 0; i < 7; i++) {
        SDL_RenderDrawLine(renderer, ARENA_WIDTH * 30 + 2 + i, 0, ARENA_WIDTH * 30 + 2 + i, WINDOW_HEIGHT);
    }

    // 加载字体
    TTF_Font* font = TTF_OpenFont("arial.ttf", 24);
    if (!font) {
        printf("Failed to load font: %s\n", TTF_GetError());
        return;
    }

    // 创建分数文本
    char scoreText[32];
    snprintf(scoreText, sizeof(scoreText), "Score: %d", score);

    // 创建表面并渲染文本
    SDL_Color textColor = {255, 255, 255, 255};
    SDL_Surface* textSurface = TTF_RenderText_Solid(font, scoreText, textColor);
    if (!textSurface) {
        TTF_CloseFont(font);
        return;
    }

    // 创建纹理
    SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
    if (!textTexture) {
        SDL_FreeSurface(textSurface);
        TTF_CloseFont(font);
        return;
    }

    // 设置绘制位置（右半边屏幕中心，顶部下方10像素）
    int rightPanelWidth = WINDOW_WIDTH - ARENA_WIDTH * 30;
    int xPos = ARENA_WIDTH * 30 + (rightPanelWidth - textSurface->w) / 2;
    SDL_Rect destRect = {xPos, 10, textSurface->w, textSurface->h};
    SDL_RenderCopy(renderer, textTexture, NULL, &destRect);

    // 清理资源
    SDL_DestroyTexture(textTexture);
    SDL_FreeSurface(textSurface);
    TTF_CloseFont(font);
}

void drawArena(SDL_Renderer *renderer) {
    // 绘制游戏区域
    int blockSize = 24;  // 每个小方块的实际大小
    int gap = 6;         // 方块之间的间隔
    
    for (int i = 0; i < ARENA_HEIGHT; i++) {
        for (int j = 0; j < ARENA_WIDTH; j++) {
            if (arena[i][j]) {
                SDL_Rect rect = {
                    j * (blockSize + gap) + gap, 
                    i * (blockSize + gap) + gap, 
                    blockSize, 
                    blockSize
                };
                // 使用与方块类型对应的颜色
                SDL_SetRenderDrawColor(renderer, 
                    pieceColors[arena[i][j] - 1].r,
                    pieceColors[arena[i][j] - 1].g,
                    pieceColors[arena[i][j] - 1].b,
                    pieceColors[arena[i][j] - 1].a);
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

    // 初始化SDL_mixer
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        printf("SDL_mixer could not initialize! Mix_Error: %s\n", Mix_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    // 加载背景音乐
    Mix_Music *bgMusic = Mix_LoadMUS("background.wav");
    if (!bgMusic) {
        printf("Failed to load background music! Mix_Error: %s\n", Mix_GetError());
        Mix_CloseAudio();
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    // 加载消除音效
    clearSound = Mix_LoadWAV("clear.wav");
    if (!clearSound) {
        printf("Failed to load clear sound effect! Mix_Error: %s\n", Mix_GetError());
        Mix_FreeMusic(bgMusic);
        Mix_CloseAudio();
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    // 播放背景音乐，循环播放
    if (Mix_PlayMusic(bgMusic, -1) == -1) {
        printf("Failed to play background music! Mix_Error: %s\n", Mix_GetError());
        Mix_FreeMusic(bgMusic);
        Mix_CloseAudio();
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    
    // 设置音量为原来的1/10
    int currentVolume = Mix_VolumeMusic(-1); // 获取当前音量（-1表示获取而不修改）
    Mix_VolumeMusic(currentVolume / 10);     // 设置为当前音量的1/10

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

        // 绘制游戏区域、当前方块和分数
        drawArena(renderer);
        drawPiece(renderer, &currentPiece);
        drawScore(renderer);

        // 更新屏幕
        SDL_RenderPresent(renderer);
    }

    // 清理资源
    // 停止并释放音乐资源
    Mix_HaltMusic();
    Mix_FreeMusic(bgMusic);
    Mix_FreeChunk(clearSound);
    
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    Mix_CloseAudio();
    TTF_Quit();
    SDL_Quit();

    return 0;
}
