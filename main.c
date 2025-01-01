#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

// 游戏窗口尺寸
#define WINDOW_WIDTH 600  // 游戏窗口的宽度（像素）
#define WINDOW_HEIGHT 600 // 游戏窗口的高度（像素）

// 游戏区域尺寸（以方块为单位）
#define ARENA_WIDTH 12 // 游戏区域（俄罗斯方块下落区域）的宽度（方块数量）
#define ARENA_HEIGHT 20 // 游戏区域的高度（方块数量）

int score = 0;         // 当前游戏分数
Uint32 lastFall = 0; // 记录上次下落时间
Uint32 lastFallInterval = 300; // 方块下落间隔时间, 初始化为中间值 (100 + 500)/2

// 俄罗斯方块结构体
typedef struct {
    int x, y;        // 方块在游戏区域中的位置
    int shape[4][4]; // 方块的4x4形状矩阵
    int type;        // 方块的类型 (0-6对应7种不同形状)
} Tetromino;

// 消除动画结构体
typedef struct {
    int lines[4];     // 正在消除的行号（最多同时消除4行）
    int count;        // 正在消除的行数
    float timer;      // 动画计时器，用于控制闪烁速度
    bool isAnimating; // 是否正在播放消除动画
    bool visible;     // 当前是否可见（用于实现闪烁效果）
} ClearAnimation;

ClearAnimation clearAnim = {0}; // 消除动画状态

// 游戏状态历史记录结构体
typedef struct {
    uint8_t arena[ARENA_HEIGHT][ARENA_WIDTH]; // 游戏区域状态
    Tetromino currentPiece;                   // 当前方块
    Tetromino nextPiece;                      // 下一个方块
    int score;                                // 当前分数
} GameState;

#define HISTORY_SIZE 3           // 保存最近3个游戏状态
GameState history[HISTORY_SIZE]; // 历史状态数组，用于实现撤销功能
int historyIndex = 0; // 当前历史状态索引，用于循环记录

Tetromino currentPiece; // 当前下落的方块
Tetromino nextPiece;    // 存储下一个方块
uint8_t arena[ARENA_HEIGHT][ARENA_WIDTH];

// 所有俄罗斯方块的形状
const int tetrominoes[7][4][4] = {
    // I型
    {{0, 0, 0, 0}, {1, 1, 1, 1}, {0, 0, 0, 0}, {0, 0, 0, 0}},
    // O型
    {{0, 0, 0, 0}, {0, 1, 1, 0}, {0, 1, 1, 0}, {0, 0, 0, 0}},
    // T型
    {{0, 0, 0, 0}, {0, 1, 1, 1}, {0, 0, 1, 0}, {0, 0, 0, 0}},
    // S型
    {{0, 0, 0, 0}, {0, 0, 1, 1}, {0, 1, 1, 0}, {0, 0, 0, 0}},
    // Z型
    {{0, 0, 0, 0}, {0, 1, 1, 0}, {0, 0, 1, 1}, {0, 0, 0, 0}},
    // J型
    {{0, 0, 0, 0}, {0, 1, 0, 0}, {0, 1, 1, 1}, {0, 0, 0, 0}},
    // L型
    {{0, 0, 0, 0}, {0, 0, 0, 1}, {0, 1, 1, 1}, {0, 0, 0, 0}}};

// 检测方块是否发生碰撞
bool checkCollision(Tetromino *piece) {
    // 遍历方块的4x4矩阵
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (piece->shape[i][j]) { // 如果当前位置有方块
                int x = piece->x + j; // 计算在游戏区域中的x坐标
                int y = piece->y + i; // 计算在游戏区域中的y坐标

                // 检查是否超出边界或与已有方块碰撞
                if (x < 0 || x >= ARENA_WIDTH || y >= ARENA_HEIGHT ||
                    (y >= 0 && arena[y][x])) {
                    return true; // 发生碰撞
                }
            }
        }
    }
    return false; // 没有碰撞
}

// 游戏状态标志
bool gameOver = false;         // 游戏是否结束
bool isPaused = false;         // 游戏是否暂停
bool inStartMenu = true;       // 是否在开始菜单界面
bool inHelpMenu = false;       // 是否在帮助说明界面
bool inSettingsMenu = false;   // 在否在游戏设置界面
bool inGameSelectMenu = false; // 是否在新游戏/加载游戏选择界面
bool blindMode = false;        // 是否处于盲打模式

bool lockPiece() {
    // 将当前方块锁定到游戏区域
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (currentPiece.shape[i][j]) {
                int x = currentPiece.x + j;
                int y = currentPiece.y + i;

                // 检查方块是否在游戏区域内
                if (x >= 0 && x < ARENA_WIDTH && y >= 0 && y < ARENA_HEIGHT) {
                    arena[y][x] =
                        currentPiece.type + 1; // 存储方块类型+1（0表示空）
                }
            }
        }
    }

    return false;
}

void undoLastMove() {
    // 计算要恢复的历史状态索引
    int restoreIndex = (historyIndex - 1 + HISTORY_SIZE) % HISTORY_SIZE;

    // 恢复游戏状态
    memcpy(arena, history[restoreIndex].arena, sizeof(arena));
    currentPiece = history[restoreIndex].currentPiece;
    nextPiece = history[restoreIndex].nextPiece;
    score = history[restoreIndex].score;

    // 更新历史索引
    historyIndex = restoreIndex;
}

void newPiece() {
    // 保存当前游戏状态到历史记录
    historyIndex = (historyIndex + 1) % HISTORY_SIZE;
    memcpy(history[historyIndex].arena, arena, sizeof(arena));
    history[historyIndex].currentPiece = currentPiece;
    history[historyIndex].nextPiece = nextPiece;
    history[historyIndex].score = score;

    // 如果游戏已经结束，直接返回
    if (gameOver) {
        return;
    }

    // 检查游戏场地最顶部一行是否有任何非空单元格
    for (int j = 0; j < ARENA_WIDTH; j++) {
        if (arena[0][j]) {
            gameOver = true;
            return;
        }
    }

    // 将下一个方块设为当前方块
    currentPiece = nextPiece;
    currentPiece.x = ARENA_WIDTH / 2 - 2; // 初始位置居中，-2是因为方块宽度为4
    currentPiece.y = -2;

    // 生成新的下一个方块
    nextPiece.type = rand() % 7;
    memcpy(nextPiece.shape, tetrominoes[nextPiece.type],
           sizeof(nextPiece.shape));
}

// 初始化游戏
void initGame() {
    // 清空游戏区域
    memset(arena, 0, sizeof(arena));
    // 初始化随机数种子
    srand(SDL_GetTicks());

    // 尝试加载保存的游戏进度
    FILE *file = fopen("savegame.dat", "rb");
    if (file) {
        // 从文件加载游戏状态
        fread(arena, sizeof(arena), 1, file); // 加载游戏区域
        fread(&currentPiece, sizeof(currentPiece), 1, file); // 加载当前方块
        fread(&nextPiece, sizeof(nextPiece), 1, file); // 加载下一个方块
        fread(&score, sizeof(score), 1, file);         // 加载分数
        fclose(file);
    } else {
        // 如果没有保存的进度，初始化新的游戏
        // 随机生成第一个下一个方块
        nextPiece.type = rand() % 7;
        memcpy(nextPiece.shape, tetrominoes[nextPiece.type],
               sizeof(nextPiece.shape));

        // 生成第一个当前方块
        newPiece();
    }
}

// 定义每种方块类型的颜色
const SDL_Color pieceColors[7] = {
    {0, 255, 255, 255}, // I型：青色
    {255, 255, 0, 255}, // O型：黄色
    {128, 0, 128, 255}, // T型：紫色
    {0, 255, 0, 255},   // S型：绿色
    {255, 0, 0, 255},   // Z型：红色
    {0, 0, 255, 255},   // J型：蓝色
    {255, 165, 0, 255}  // L型：橙色
};

void drawPiece(SDL_Renderer *renderer, Tetromino *piece, bool isPreview) {
    // 绘制当前方块
    SDL_Color color = pieceColors[piece->type];
    int blockSize = 24; // 每个小方块的实际大小
    int gap = 6;        // 方块之间的间隔

    // 如果是预览方块，设置半透明颜色
    if (isPreview) {
        color.a = 192; // 设置75%透明度
    }

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (piece->shape[i][j]) {
                SDL_Rect rect = {(piece->x + j) * (blockSize + gap) + gap,
                                 (piece->y + i) * (blockSize + gap) + gap,
                                 blockSize, blockSize};
                SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b,
                                       color.a);
                SDL_RenderFillRect(renderer, &rect);

                // 如果不是预览方块，绘制边框
                if (!isPreview) {
                    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                    SDL_RenderDrawRect(renderer, &rect);
                }
            }
        }
    }
}

void drawPreview(SDL_Renderer *renderer, Tetromino *piece) {
    // 创建临时方块用于预览
    Tetromino preview = *piece;
    
    // 模拟下落直到碰撞
    while (!checkCollision(&preview)) {
        preview.y++;
    }
    preview.y--; // 回退到最后有效位置

    // 使用当前方块的填充颜色绘制轮廓
    SDL_Color color = pieceColors[piece->type];
    int blockSize = 24; // 每个小方块的实际大小
    int gap = 6;        // 方块之间的间隔

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (preview.shape[i][j]) {
                SDL_Rect rect = {(preview.x + j) * (blockSize + gap) + gap,
                                 (preview.y + i) * (blockSize + gap) + gap,
                                 blockSize, blockSize};
                // 绘制更粗的边框，使用当前方块的填充颜色，但透明度为0
                SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 0);
                // 绘制多个偏移的矩形来创建更粗的边框
                for (int offset = 0; offset < 3; offset++) {
                    SDL_Rect thickRect = {
                        rect.x - offset, rect.y - offset,
                        rect.w + offset * 2, rect.h + offset * 2
                    };
                    SDL_RenderDrawRect(renderer, &thickRect);
                }
            }
        }
    }
}

Mix_Chunk *clearSound = NULL; // 消除音效

void updateAnimation(float deltaTime) {
    if (clearAnim.isAnimating) {
        // 更新计时器
        clearAnim.timer += deltaTime;

        // 每0.1秒切换一次可见状态
        if ((int)(clearAnim.timer * 10) % 2 == 0) {
            clearAnim.visible = true;
        } else {
            clearAnim.visible = false;
        }

        // 动画持续0.5秒后结束
        if (clearAnim.timer >= 0.5f) {
            // 动画结束，实际消除所有标记的行
            // 从下往上消除，避免影响上面的行号
            for (int i = clearAnim.count - 1; i >= 0; i--) {
                int line = clearAnim.lines[i];
                // 将当前行以上的所有行向下移动一行
                for (int k = line; k > 0; k--) {
                    memcpy(arena[k], arena[k - 1], ARENA_WIDTH);
                }
                // 将最顶行清零
                memset(arena[0], 0, ARENA_WIDTH);
            }
            clearAnim.isAnimating = false;
            clearAnim.count = 0;      // 重置消除行数
            clearAnim.timer = 0;      // 重置计时器
            clearAnim.visible = true; // 重置可见状态
        }
    }
}

void clearLines() {
    clearAnim.count = 0; // 重置消除行数

    // 第一步：检查有多少行需要消除
    // 从底部开始向上检查每一行
    for (int i = ARENA_HEIGHT - 1; i >= 0; i--) {
        bool full = true;
        // 检查当前行是否被完全填满
        for (int j = 0; j < ARENA_WIDTH; j++) {
            if (!arena[i][j]) {
                full = false;
                break;
            }
        }
        // 如果当前行被填满，记录行号
        if (full && clearAnim.count < 4) {
            clearAnim.lines[clearAnim.count++] = i;
        }
    }

    // 如果有消除行
    if (clearAnim.count > 0) {
        // 第二步：播放消除音效
        if (clearSound) {
            Mix_PlayChannel(-1, clearSound, 0);
        }

        // 第三步：启动动画
        clearAnim.timer = 0;
        clearAnim.visible = true;
        clearAnim.isAnimating = true;

        // 第四步：根据消除的行数更新分数
        switch (clearAnim.count) {
        case 1:
            score += 100;
            break;
        case 2:
            score += 300;
            break;
        case 3:
            score += 500;
            break;
        case 4:
            score += 800;
            break;
        }
    }
}

void drawNextPiece(SDL_Renderer *renderer) {
    // 设置预览区域的位置和大小
    int previewX = ARENA_WIDTH * 30 + 50; // 从分割线向右偏移50像素
    int previewY = 100;                   // 在分数下方
    int blockSize = 20;                   // 预览方块的大小

    // 绘制"Next Piece"文字
    TTF_Font *font = TTF_OpenFont("simhei.ttf", 20);
    
    // 绘制当前模式提示
    TTF_Font *modeFont = TTF_OpenFont("simhei.ttf", 18);
    if (modeFont) {
        const char *modeText = blindMode ? "当前模式: 盲打模式" : "当前模式: 显示模式";
        SDL_Color textColor = blindMode ? (SDL_Color){255, 100, 100, 255} : (SDL_Color){100, 255, 100, 255};
        SDL_Surface *textSurface = TTF_RenderUTF8_Solid(modeFont, modeText, textColor);
        if (textSurface) {
            SDL_Texture *textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
            if (textTexture) {
                SDL_Rect destRect = {previewX, previewY + 150, textSurface->w, textSurface->h};
                SDL_RenderCopy(renderer, textTexture, NULL, &destRect);
                SDL_DestroyTexture(textTexture);
            }
            SDL_FreeSurface(textSurface);
        }
        TTF_CloseFont(modeFont);
    }
    if (font) {
        SDL_Color textColor = {255, 255, 255, 255};
        SDL_Surface *textSurface =
            TTF_RenderUTF8_Solid(font, "下一个方块:", textColor);
        if (textSurface) {
            SDL_Texture *textTexture =
                SDL_CreateTextureFromSurface(renderer, textSurface);
            if (textTexture) {
                SDL_Rect destRect = {previewX, previewY - 30, textSurface->w,
                                     textSurface->h};
                SDL_RenderCopy(renderer, textTexture, NULL, &destRect);
                SDL_DestroyTexture(textTexture);
            }
            SDL_FreeSurface(textSurface);
        }
        TTF_CloseFont(font);
    }

    // 绘制下一个方块的预览
    SDL_Color color = pieceColors[nextPiece.type];
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (nextPiece.shape[i][j]) {
                SDL_Rect rect = {previewX + j * blockSize,
                                 previewY + i * blockSize, blockSize,
                                 blockSize};
                SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b,
                                       color.a);
                SDL_RenderFillRect(renderer, &rect);

                // 绘制边框
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                SDL_RenderDrawRect(renderer, &rect);
            }
        }
    }
}

void drawScore(SDL_Renderer *renderer) {
    // 绘制分割线
    // 设置分割线宽度为3像素，在游戏区域和分数显示区域之间
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // 白色
    for (int i = 0; i < 3; i++) {
        // 绘制垂直线，从窗口顶部到底部
        // ARENA_WIDTH * 30 是游戏区域的宽度（每个方块30像素）
        SDL_RenderDrawLine(renderer, ARENA_WIDTH * 30 + 1 + i, 0,
                           ARENA_WIDTH * 30 + 1 + i, WINDOW_HEIGHT);
    }

    // 加载支持中文的字体文件
    // 使用24号字体大小
    TTF_Font *font = TTF_OpenFont("simhei.ttf", 24);
    if (!font) {
        printf("Failed to load font: %s\n", TTF_GetError());
        return;
    }

    // 创建分数文本
    // 使用UTF-8编码
    char scoreText[32];
    snprintf(scoreText, sizeof(scoreText), "分数: %d", score);

    // 创建表面并渲染文本
    // 使用白色（255,255,255）渲染文本
    SDL_Color textColor = {255, 255, 255, 255};
    SDL_Surface *textSurface = TTF_RenderUTF8_Solid(font, scoreText, textColor);
    if (!textSurface) {
        TTF_CloseFont(font);
        return;
    }

    // 创建纹理
    // 将表面转换为纹理以便渲染
    SDL_Texture *textTexture =
        SDL_CreateTextureFromSurface(renderer, textSurface);
    if (!textTexture) {
        SDL_FreeSurface(textSurface);
        TTF_CloseFont(font);
        return;
    }

    // 设置绘制位置
    // 计算右侧面板的宽度（窗口总宽度减去游戏区域宽度）
    int rightPanelWidth = WINDOW_WIDTH - ARENA_WIDTH * 30;
    // 计算x坐标：游戏区域宽度 + (右侧面板宽度 - 文本宽度)/2，实现水平居中
    int xPos = ARENA_WIDTH * 30 + (rightPanelWidth - textSurface->w) / 2;
    // 设置绘制矩形：x坐标，y坐标（顶部下方10像素），使用文本的宽度和高度
    SDL_Rect destRect = {xPos, 10, textSurface->w, textSurface->h};
    // 将纹理复制到渲染器
    SDL_RenderCopy(renderer, textTexture, NULL, &destRect);

    // 清理资源
    // 释放纹理、表面和字体对象
    SDL_DestroyTexture(textTexture);
    SDL_FreeSurface(textSurface);
    TTF_CloseFont(font);
}

void drawArena(SDL_Renderer *renderer) {
    // 绘制游戏区域
    int blockSize = 24; // 每个小方块的实际大小
    int gap = 6;        // 方块之间的间隔

    for (int i = 0; i < ARENA_HEIGHT; i++) {
        for (int j = 0; j < ARENA_WIDTH; j++) {
            if (arena[i][j]) {
                SDL_Rect rect = {j * (blockSize + gap) + gap,
                                 i * (blockSize + gap) + gap, blockSize,
                                 blockSize};

                // 检查当前行是否在动画中
                bool isAnimating = false;
                for (int k = 0; k < clearAnim.count; k++) {
                    if (i == clearAnim.lines[k]) {
                        isAnimating = true;
                        break;
                    }
                }

                // 使用与方块类型对应的颜色
                SDL_Color color = pieceColors[arena[i][j] - 1];
                if (isAnimating && !clearAnim.visible) {
                    // 如果是动画中的行且当前不可见，绘制黑色
                    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                } else {
                    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b,
                                           color.a);
                }
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
        printf("SDL_mixer could not initialize! Mix_Error: %s\n",
               Mix_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    // 加载背景音乐
    Mix_Music *bgMusic = Mix_LoadMUS("background.wav");
    if (!bgMusic) {
        printf("Failed to load background music! Mix_Error: %s\n",
               Mix_GetError());
        Mix_CloseAudio();
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    // 加载消除音效
    clearSound = Mix_LoadWAV("clear.wav");
    if (!clearSound) {
        printf("Failed to load clear sound effect! Mix_Error: %s\n",
               Mix_GetError());
        Mix_FreeMusic(bgMusic);
        Mix_CloseAudio();
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    // 播放背景音乐，循环播放
    if (Mix_PlayMusic(bgMusic, -1) == -1) {
        printf("Failed to play background music! Mix_Error: %s\n",
               Mix_GetError());
        Mix_FreeMusic(bgMusic);
        Mix_CloseAudio();
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    // 设置音量为原来的1/10
    int currentVolume =
        Mix_VolumeMusic(-1); // 获取当前音量（-1表示获取而不修改）
    Mix_VolumeMusic(currentVolume / 10); // 设置为当前音量的1/10

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
    Uint32 lastTime = SDL_GetTicks();
    while (!quit) {
        // 帮助界面
        if (inHelpMenu) {
            // 清屏
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);

            // 绘制帮助说明
            TTF_Font *font = TTF_OpenFont("simhei.ttf", 24);
            if (font) {
                // 游戏玩法说明文本
                const char *helpText[] = {
                    "俄罗斯方块玩法说明：",     "1. 使用 A 键向左移动方块",
                    "2. 使用 D 键向右移动方块", "3. 使用 S 键加速下落",
                    "4. 使用 W 键旋转方块",     "5. 填满一行即可消除得分",
                    "6. 按 Esc 键暂停游戏"};

                // 初始绘制位置
                int yPos = 100;

                // 逐行绘制说明文本
                for (int i = 0; i < sizeof(helpText) / sizeof(helpText[0]);
                     i++) {
                    SDL_Color textColor = {255, 255, 255, 255}; // 白色文字
                    SDL_Surface *textSurface =
                        TTF_RenderUTF8_Solid(font, helpText[i], textColor);
                    if (textSurface) {
                        SDL_Texture *textTexture =
                            SDL_CreateTextureFromSurface(renderer, textSurface);
                        if (textTexture) {
                            // 计算文本位置（居中显示）
                            int textWidth = textSurface->w;
                            int textHeight = textSurface->h;
                            SDL_Rect textRect = {(WINDOW_WIDTH - textWidth) /
                                                     2, // 水平居中
                                                 yPos,  // 垂直位置
                                                 textWidth, textHeight};

                            // 绘制文本
                            SDL_RenderCopy(renderer, textTexture, NULL,
                                           &textRect);
                            SDL_DestroyTexture(textTexture);
                        }
                        SDL_FreeSurface(textSurface);
                    }
                    yPos += 40; // 每行间隔40像素
                }

                // 绘制"返回开始界面"按钮
                SDL_Color textColor = {255, 255, 255, 255};
                SDL_Surface *textSurface =
                    TTF_RenderUTF8_Solid(font, "返回开始界面", textColor);
                if (textSurface) {
                    SDL_Texture *textTexture =
                        SDL_CreateTextureFromSurface(renderer, textSurface);
                    if (textTexture) {
                        // 计算按钮位置
                        int buttonWidth = textSurface->w + 40;
                        int buttonHeight = textSurface->h + 20;
                        int buttonX = (WINDOW_WIDTH - buttonWidth) / 2;
                        int buttonY = yPos + 50;

                        // 获取鼠标位置
                        int mouseX, mouseY;
                        SDL_GetMouseState(&mouseX, &mouseY);

                        // 检查鼠标是否在按钮上
                        bool isHovered = (mouseX >= buttonX &&
                                          mouseX <= buttonX + buttonWidth &&
                                          mouseY >= buttonY &&
                                          mouseY <= buttonY + buttonHeight);

                        // 绘制按钮阴影
                        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 64);
                        SDL_Rect shadowRect = {buttonX + 4, buttonY + 4,
                                               buttonWidth, buttonHeight};
                        SDL_RenderFillRect(renderer, &shadowRect);

                        // 根据鼠标悬停状态设置按钮颜色
                        if (isHovered) {
                            SDL_SetRenderDrawColor(renderer, 50, 150, 255, 255);
                        } else {
                            SDL_SetRenderDrawColor(renderer, 30, 80, 150, 255);
                        }
                        SDL_Rect buttonRect = {buttonX, buttonY, buttonWidth,
                                               buttonHeight};

                        // 绘制圆角矩形
                        for (int i = 0; i < 10; i++) {
                            SDL_Rect roundRect = {
                                buttonRect.x + i, buttonRect.y + i,
                                buttonRect.w - i * 2, buttonRect.h - i * 2};
                            SDL_RenderDrawRect(renderer, &roundRect);
                        }
                        SDL_RenderFillRect(renderer, &buttonRect);

                        // 绘制按钮边框
                        if (isHovered) {
                            SDL_SetRenderDrawColor(renderer, 100, 200, 255,
                                                   255);
                        } else {
                            SDL_SetRenderDrawColor(renderer, 255, 255, 255,
                                                   255);
                        }
                        for (int i = 0; i < 2; i++) {
                            SDL_Rect borderRect = {
                                buttonRect.x + i, buttonRect.y + i,
                                buttonRect.w - i * 2, buttonRect.h - i * 2};
                            SDL_RenderDrawRect(renderer, &borderRect);
                        }

                        // 绘制按钮文字
                        SDL_Rect textRect = {buttonX + 20, buttonY + 10,
                                             textSurface->w, textSurface->h};
                        SDL_RenderCopy(renderer, textTexture, NULL, &textRect);

                        // 检测鼠标点击
                        if (SDL_GetMouseState(&mouseX, &mouseY) &
                            SDL_BUTTON(SDL_BUTTON_LEFT)) {
                            if (mouseX >= buttonX &&
                                mouseX <= buttonX + buttonWidth &&
                                mouseY >= buttonY &&
                                mouseY <= buttonY + buttonHeight) {
                                inHelpMenu = false;
                                inStartMenu = true;
                            }
                        }

                        SDL_DestroyTexture(textTexture);
                    }
                    SDL_FreeSurface(textSurface);
                }
                TTF_CloseFont(font);
            }

            // 更新屏幕
            SDL_RenderPresent(renderer);

            // 处理事件
            while (SDL_PollEvent(&e) != 0) {
                if (e.type == SDL_QUIT) {
                    quit = true;
                }
            }

            continue; // 跳过游戏主逻辑
        }

        // 处理新游戏/加载游戏选择界面
        if (inGameSelectMenu) {
            // 清屏
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);

            // 绘制标题
            TTF_Font *titleFont = TTF_OpenFont("simhei.ttf", 48);
            if (titleFont) {
                SDL_Color textColor = {255, 255, 255, 255};
                SDL_Surface *textSurface =
                    TTF_RenderUTF8_Solid(titleFont, "选择游戏模式", textColor);
                if (textSurface) {
                    SDL_Texture *textTexture =
                        SDL_CreateTextureFromSurface(renderer, textSurface);
                    if (textTexture) {
                        int textWidth = textSurface->w;
                        int textHeight = textSurface->h;
                        SDL_Rect textRect = {(WINDOW_WIDTH - textWidth) / 2,
                                             100, // 标题距离顶部100像素
                                             textWidth, textHeight};
                        SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
                        SDL_DestroyTexture(textTexture);
                    }
                    SDL_FreeSurface(textSurface);
                }
                TTF_CloseFont(titleFont);
            }

            // 绘制"新游戏"按钮
            TTF_Font *buttonFont = TTF_OpenFont("simhei.ttf", 36);
            if (buttonFont) {
                SDL_Color textColor = {255, 255, 255, 255};
                SDL_Surface *textSurface =
                    TTF_RenderUTF8_Solid(buttonFont, "新游戏", textColor);
                if (textSurface) {
                    SDL_Texture *textTexture =
                        SDL_CreateTextureFromSurface(renderer, textSurface);
                    if (textTexture) {
                        // 计算按钮位置
                        int buttonWidth = textSurface->w + 40;
                        int buttonHeight = textSurface->h + 20;
                        int buttonX = (WINDOW_WIDTH - buttonWidth) / 2;
                        int buttonY = 200; // 在标题下方

                        // 获取鼠标位置
                        int mouseX, mouseY;
                        SDL_GetMouseState(&mouseX, &mouseY);

                        // 检查鼠标是否在按钮上
                        bool isHovered = (mouseX >= buttonX &&
                                          mouseX <= buttonX + buttonWidth &&
                                          mouseY >= buttonY &&
                                          mouseY <= buttonY + buttonHeight);

                        // 绘制按钮阴影
                        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 64);
                        SDL_Rect shadowRect = {buttonX + 4, buttonY + 4,
                                               buttonWidth, buttonHeight};
                        SDL_RenderFillRect(renderer, &shadowRect);

                        // 根据鼠标悬停状态设置按钮颜色
                        if (isHovered) {
                            SDL_SetRenderDrawColor(renderer, 50, 150, 255, 255);
                        } else {
                            SDL_SetRenderDrawColor(renderer, 30, 80, 150, 255);
                        }
                        SDL_Rect buttonRect = {buttonX, buttonY, buttonWidth,
                                               buttonHeight};

                        // 绘制圆角矩形
                        for (int i = 0; i < 10; i++) {
                            SDL_Rect roundRect = {
                                buttonRect.x + i, buttonRect.y + i,
                                buttonRect.w - i * 2, buttonRect.h - i * 2};
                            SDL_RenderDrawRect(renderer, &roundRect);
                        }
                        SDL_RenderFillRect(renderer, &buttonRect);

                        // 绘制按钮边框
                        if (isHovered) {
                            SDL_SetRenderDrawColor(renderer, 100, 200, 255,
                                                   255);
                        } else {
                            SDL_SetRenderDrawColor(renderer, 255, 255, 255,
                                                   255);
                        }
                        for (int i = 0; i < 2; i++) {
                            SDL_Rect borderRect = {
                                buttonRect.x + i, buttonRect.y + i,
                                buttonRect.w - i * 2, buttonRect.h - i * 2};
                            SDL_RenderDrawRect(renderer, &borderRect);
                        }

                        // 绘制按钮文字
                        SDL_Rect textRect = {buttonX + 20, buttonY + 10,
                                             textSurface->w, textSurface->h};
                        SDL_RenderCopy(renderer, textTexture, NULL, &textRect);

                        // 检测鼠标点击
                        if (SDL_GetMouseState(&mouseX, &mouseY) &
                            SDL_BUTTON(SDL_BUTTON_LEFT)) {
                            if (mouseX >= buttonX &&
                                mouseX <= buttonX + buttonWidth &&
                                mouseY >= buttonY &&
                                mouseY <= buttonY + buttonHeight) {
                                // 开始新游戏
                                inGameSelectMenu = false;
                                // 清空游戏区域
                                memset(arena, 0, sizeof(arena));
                                score = 0;
                                // 初始化随机数种子
                                srand(SDL_GetTicks());
                                // 随机生成第一个下一个方块
                                nextPiece.type = rand() % 7;
                                memcpy(nextPiece.shape, tetrominoes[nextPiece.type],
                                       sizeof(nextPiece.shape));
                                // 生成第一个当前方块
                                newPiece();
                            }
                        }

                        SDL_DestroyTexture(textTexture);
                    }
                    SDL_FreeSurface(textSurface);
                }
                TTF_CloseFont(buttonFont);
            }

            // 绘制"加载游戏"按钮
            buttonFont = TTF_OpenFont("simhei.ttf", 36);
            if (buttonFont) {
                SDL_Color textColor = {255, 255, 255, 255};
                SDL_Surface *textSurface =
                    TTF_RenderUTF8_Solid(buttonFont, "加载游戏", textColor);
                if (textSurface) {
                    SDL_Texture *textTexture =
                        SDL_CreateTextureFromSurface(renderer, textSurface);
                    if (textTexture) {
                        // 计算按钮位置，放在"新游戏"按钮下方
                        int buttonWidth = textSurface->w + 40;
                        int buttonHeight = textSurface->h + 20;
                        int buttonX = (WINDOW_WIDTH - buttonWidth) / 2;
                        int buttonY = 300; // 在新游戏按钮下方

                        // 获取鼠标位置
                        int mouseX, mouseY;
                        SDL_GetMouseState(&mouseX, &mouseY);

                        // 检查鼠标是否在按钮上
                        bool isHovered = (mouseX >= buttonX &&
                                          mouseX <= buttonX + buttonWidth &&
                                          mouseY >= buttonY &&
                                          mouseY <= buttonY + buttonHeight);

                        // 绘制按钮阴影
                        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 64);
                        SDL_Rect shadowRect = {buttonX + 4, buttonY + 4,
                                               buttonWidth, buttonHeight};
                        SDL_RenderFillRect(renderer, &shadowRect);

                        // 根据鼠标悬停状态设置按钮颜色
                        if (isHovered) {
                            SDL_SetRenderDrawColor(renderer, 50, 255, 50, 255);
                        } else {
                            SDL_SetRenderDrawColor(renderer, 30, 150, 30, 255);
                        }
                        SDL_Rect buttonRect = {buttonX, buttonY, buttonWidth,
                                               buttonHeight};

                        // 绘制圆角矩形
                        for (int i = 0; i < 10; i++) {
                            SDL_Rect roundRect = {
                                buttonRect.x + i, buttonRect.y + i,
                                buttonRect.w - i * 2, buttonRect.h - i * 2};
                            SDL_RenderDrawRect(renderer, &roundRect);
                        }
                        SDL_RenderFillRect(renderer, &buttonRect);

                        // 绘制按钮边框
                        if (isHovered) {
                            SDL_SetRenderDrawColor(renderer, 100, 255, 100,
                                                   255);
                        } else {
                            SDL_SetRenderDrawColor(renderer, 255, 255, 255,
                                                   255);
                        }
                        for (int i = 0; i < 2; i++) {
                            SDL_Rect borderRect = {
                                buttonRect.x + i, buttonRect.y + i,
                                buttonRect.w - i * 2, buttonRect.h - i * 2};
                            SDL_RenderDrawRect(renderer, &borderRect);
                        }

                        // 绘制按钮文字
                        SDL_Rect textRect = {buttonX + 20, buttonY + 10,
                                             textSurface->w, textSurface->h};
                        SDL_RenderCopy(renderer, textTexture, NULL, &textRect);

                        // 检测鼠标点击
                        if (SDL_GetMouseState(&mouseX, &mouseY) &
                            SDL_BUTTON(SDL_BUTTON_LEFT)) {
                            if (mouseX >= buttonX &&
                                mouseX <= buttonX + buttonWidth &&
                                mouseY >= buttonY &&
                                mouseY <= buttonY + buttonHeight) {
                                // 加载游戏
                                inGameSelectMenu = false;
                                initGame();
                            }
                        }

                        SDL_DestroyTexture(textTexture);
                    }
                    SDL_FreeSurface(textSurface);
                }
                TTF_CloseFont(buttonFont);
            }

            // 更新屏幕
            SDL_RenderPresent(renderer);

            // 处理事件
            while (SDL_PollEvent(&e) != 0) {
                if (e.type == SDL_QUIT) {
                    quit = true;
                }
            }

            continue; // 跳过游戏主逻辑
        }

        // 设置界面
        if (inSettingsMenu) {
            // 清屏
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);

            // 绘制标题
            TTF_Font *titleFont = TTF_OpenFont("simhei.ttf", 48);
            if (titleFont) {
                SDL_Color textColor = {255, 255, 255, 255};
                SDL_Surface *textSurface =
                    TTF_RenderUTF8_Solid(titleFont, "游戏设置", textColor);
                if (textSurface) {
                    SDL_Texture *textTexture =
                        SDL_CreateTextureFromSurface(renderer, textSurface);
                    if (textTexture) {
                        int textWidth = textSurface->w;
                        int textHeight = textSurface->h;
                        SDL_Rect textRect = {(WINDOW_WIDTH - textWidth) / 2,
                                             20, // 标题距离顶部20像素
                                             textWidth, textHeight};
                        SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
                        SDL_DestroyTexture(textTexture);
                    }
                    SDL_FreeSurface(textSurface);
                }
                TTF_CloseFont(titleFont);
            }

            // 绘制"返回开始界面"按钮
            TTF_Font *buttonFont = TTF_OpenFont("simhei.ttf", 36);
            if (buttonFont) {
                SDL_Color textColor = {255, 255, 255, 255};
                SDL_Surface *textSurface =
                    TTF_RenderUTF8_Solid(buttonFont, "返回开始界面", textColor);

                // 在按钮下方添加"调整音量"提示
                TTF_Font *hintFont = TTF_OpenFont("simhei.ttf", 24);
                if (hintFont) {
                    SDL_Surface *hintSurface =
                        TTF_RenderUTF8_Solid(hintFont, "调整音量", textColor);
                    if (hintSurface) {
                        SDL_Texture *hintTexture =
                            SDL_CreateTextureFromSurface(renderer, hintSurface);
                        if (hintTexture) {
                            int hintX = (WINDOW_WIDTH - hintSurface->w) / 2;
                            int hintY = 200; // 在返回按钮下方
                            SDL_Rect hintRect = {hintX, hintY, hintSurface->w,
                                                 hintSurface->h};
                            SDL_RenderCopy(renderer, hintTexture, NULL,
                                           &hintRect);
                            SDL_DestroyTexture(hintTexture);
                        }
                        SDL_FreeSurface(hintSurface);
                    }
                    TTF_CloseFont(hintFont);
                }
                if (textSurface) {
                    SDL_Texture *textTexture =
                        SDL_CreateTextureFromSurface(renderer, textSurface);
                    if (textTexture) {
                        // 计算按钮位置
                        int buttonWidth = textSurface->w + 40;
                        int buttonHeight = textSurface->h + 20;
                        int buttonX = (WINDOW_WIDTH - buttonWidth) / 2;
                        int buttonY =
                            100; // 按钮在标题下方20像素（标题高度48 + 20 =
                                 // 68，标题位置20 + 68 = 88，取整100）

                        // 获取鼠标位置
                        int mouseX, mouseY;
                        SDL_GetMouseState(&mouseX, &mouseY);

                        // 检查鼠标是否在按钮上
                        bool isHovered = (mouseX >= buttonX &&
                                          mouseX <= buttonX + buttonWidth &&
                                          mouseY >= buttonY &&
                                          mouseY <= buttonY + buttonHeight);

                        // 绘制按钮阴影
                        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 64);
                        SDL_Rect shadowRect = {buttonX + 4, buttonY + 4,
                                               buttonWidth, buttonHeight};
                        SDL_RenderFillRect(renderer, &shadowRect);

                        // 根据鼠标悬停状态设置按钮颜色
                        if (isHovered) {
                            SDL_SetRenderDrawColor(renderer, 50, 150, 255, 255);
                        } else {
                            SDL_SetRenderDrawColor(renderer, 30, 80, 150, 255);
                        }
                        SDL_Rect buttonRect = {buttonX, buttonY, buttonWidth,
                                               buttonHeight};

                        // 绘制圆角矩形
                        for (int i = 0; i < 10; i++) {
                            SDL_Rect roundRect = {
                                buttonRect.x + i, buttonRect.y + i,
                                buttonRect.w - i * 2, buttonRect.h - i * 2};
                            SDL_RenderDrawRect(renderer, &roundRect);
                        }
                        SDL_RenderFillRect(renderer, &buttonRect);

                        // 绘制按钮边框
                        if (isHovered) {
                            SDL_SetRenderDrawColor(renderer, 100, 200, 255,
                                                   255);
                        } else {
                            SDL_SetRenderDrawColor(renderer, 255, 255, 255,
                                                   255);
                        }
                        for (int i = 0; i < 2; i++) {
                            SDL_Rect borderRect = {
                                buttonRect.x + i, buttonRect.y + i,
                                buttonRect.w - i * 2, buttonRect.h - i * 2};
                            SDL_RenderDrawRect(renderer, &borderRect);
                        }

                        // 绘制按钮文字
                        SDL_Rect textRect = {buttonX + 20, buttonY + 10,
                                             textSurface->w, textSurface->h};
                        SDL_RenderCopy(renderer, textTexture, NULL, &textRect);

                        // 检测鼠标点击
                        if (SDL_GetMouseState(&mouseX, &mouseY) &
                            SDL_BUTTON(SDL_BUTTON_LEFT)) {
                            if (mouseX >= buttonX &&
                                mouseX <= buttonX + buttonWidth &&
                                mouseY >= buttonY &&
                                mouseY <= buttonY + buttonHeight) {
                                inSettingsMenu = false;
                                inStartMenu = true;
                            }
                        }

                        SDL_DestroyTexture(textTexture);
                    }
                    SDL_FreeSurface(textSurface);
                }
                TTF_CloseFont(buttonFont);
            }

            // 绘制音量调整提示和滑动条
            TTF_Font *volumeFont = TTF_OpenFont("simhei.ttf", 24);
            if (volumeFont) {
                // 绘制"调整音量"文字
                SDL_Color textColor = {255, 255, 255, 255};
                SDL_Surface *textSurface =
                    TTF_RenderUTF8_Solid(volumeFont, "调整音量", textColor);
                if (textSurface) {
                    SDL_Texture *textTexture =
                        SDL_CreateTextureFromSurface(renderer, textSurface);
                    if (textTexture) {
                        int textWidth = textSurface->w;
                        int textHeight = textSurface->h;
                        SDL_Rect textRect = {(WINDOW_WIDTH - textWidth) / 2,
                                             200, // 在返回按钮下方
                                             textWidth, textHeight};
                        SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
                        SDL_DestroyTexture(textTexture);
                    }
                    SDL_FreeSurface(textSurface);
                }

                // 绘制音量滑动条
                int sliderWidth = 300;
                int sliderHeight = 20;
                int sliderX = (WINDOW_WIDTH - sliderWidth) / 2;
                int sliderY = 250; // 在文字下方

                // 获取当前音量
                int currentVolume = Mix_VolumeMusic(-1);
                int maxVolume = 128; // SDL_mixer最大音量
                int sliderPos = (currentVolume * sliderWidth) / maxVolume;

                // 绘制滑动条背景
                SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
                SDL_Rect sliderBgRect = {sliderX, sliderY, sliderWidth,
                                         sliderHeight};
                SDL_RenderFillRect(renderer, &sliderBgRect);

                // 绘制滑动条
                SDL_SetRenderDrawColor(renderer, 50, 150, 255, 255);
                SDL_Rect sliderRect = {sliderX, sliderY, sliderPos,
                                       sliderHeight};
                SDL_RenderFillRect(renderer, &sliderRect);

                // 绘制滑动条边框
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                SDL_RenderDrawRect(renderer, &sliderBgRect);

                // 处理滑动条交互
                int mouseX, mouseY;
                Uint32 mouseState = SDL_GetMouseState(&mouseX, &mouseY);
                if (mouseState & SDL_BUTTON(SDL_BUTTON_LEFT)) {
                    if (mouseY >= sliderY && mouseY <= sliderY + sliderHeight &&
                        mouseX >= sliderX && mouseX <= sliderX + sliderWidth) {
                        // 计算新音量
                        int newVolume =
                            ((mouseX - sliderX) * maxVolume) / sliderWidth;
                        Mix_VolumeMusic(newVolume);
                    }
                }

                TTF_CloseFont(volumeFont);
            }

            // 绘制"方块下落速度"提示
            TTF_Font *speedFont = TTF_OpenFont("simhei.ttf", 24);
            if (speedFont) {
                SDL_Color textColor = {255, 255, 255, 255};
                SDL_Surface *textSurface =
                    TTF_RenderUTF8_Solid(speedFont, "方块下落速度", textColor);
                if (textSurface) {
                    SDL_Texture *textTexture =
                        SDL_CreateTextureFromSurface(renderer, textSurface);
                    if (textTexture) {
                        int textWidth = textSurface->w;
                        int textHeight = textSurface->h;
                        SDL_Rect textRect = {(WINDOW_WIDTH - textWidth) / 2,
                                             300, // 在音量滑动条下方
                                             textWidth, textHeight};
                        SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
                        SDL_DestroyTexture(textTexture);
                    }
                    SDL_FreeSurface(textSurface);
                }
                TTF_CloseFont(speedFont);
            }

            // 绘制方块下落速度滑动条
            int speedSliderWidth = 300;
            int speedSliderHeight = 20;
            int speedSliderX = (WINDOW_WIDTH - speedSliderWidth) / 2;
            int speedSliderY = 350; // 在文字下方

            // 计算当前速度对应的滑块位置 (左边慢500ms，右边快100ms)
            int minFallTime = 100; // 最快速度（右边）
            int maxFallTime = 500; // 最慢速度（左边）
            // 初始位置在中间
            int speedSliderPos = (speedSliderWidth * (maxFallTime - lastFallInterval)) / 
                                (maxFallTime - minFallTime);

            // 绘制滑动条背景
            SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
            SDL_Rect speedSliderBgRect = {speedSliderX, speedSliderY,
                                          speedSliderWidth, speedSliderHeight};
            SDL_RenderFillRect(renderer, &speedSliderBgRect);

            // 绘制滑动条
            SDL_SetRenderDrawColor(renderer, 50, 150, 255, 255);
            SDL_Rect speedSliderRect = {speedSliderX, speedSliderY,
                                        speedSliderPos, speedSliderHeight};
            SDL_RenderFillRect(renderer, &speedSliderRect);

            // 绘制滑动条边框
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderDrawRect(renderer, &speedSliderBgRect);

            // 处理滑动条交互
            int mouseX, mouseY;
            Uint32 mouseState = SDL_GetMouseState(&mouseX, &mouseY);
            if (mouseState & SDL_BUTTON(SDL_BUTTON_LEFT)) {
                if (mouseY >= speedSliderY &&
                    mouseY <= speedSliderY + speedSliderHeight &&
                    mouseX >= speedSliderX &&
                    mouseX <= speedSliderX + speedSliderWidth) {
                    // 计算新的下落间隔时间（左边慢，右边快）
                    // 鼠标越靠右，下落速度越快
                    int newFallTime =
                        maxFallTime - ((mouseX - speedSliderX) *
                                       (maxFallTime - minFallTime)) /
                                          speedSliderWidth;
                    // 更新全局下落间隔时间
                    lastFallInterval = newFallTime;
                }
            }

            // 更新屏幕
            SDL_RenderPresent(renderer);

            // 处理事件
            while (SDL_PollEvent(&e) != 0) {
                if (e.type == SDL_QUIT) {
                    quit = true;
                }
            }

            continue; // 跳过游戏主逻辑
        }

        // 处理开始界面
        if (inStartMenu) {
            // 清屏
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);

            // 绘制标题
            TTF_Font *titleFont = TTF_OpenFont("simhei.ttf", 64);
            if (titleFont) {
                SDL_Color textColor = {255, 255, 255, 255};
                SDL_Surface *textSurface =
                    TTF_RenderUTF8_Solid(titleFont, "俄罗斯方块", textColor);
                if (textSurface) {
                    SDL_Texture *textTexture =
                        SDL_CreateTextureFromSurface(renderer, textSurface);
                    if (textTexture) {
                        int textWidth = textSurface->w;
                        int textHeight = textSurface->h;
                        SDL_Rect textRect = {(WINDOW_WIDTH - textWidth) / 2,
                                             100, // 标题距离顶部100像素
                                             textWidth, textHeight};
                        SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
                        SDL_DestroyTexture(textTexture);
                    }
                    SDL_FreeSurface(textSurface);
                }
                TTF_CloseFont(titleFont);
            }

            // 绘制开始游戏按钮
            TTF_Font *buttonFont = TTF_OpenFont("simhei.ttf", 36);
            if (buttonFont) {
                SDL_Color textColor = {255, 255, 255, 255};
                SDL_Surface *textSurface =
                    TTF_RenderUTF8_Solid(buttonFont, "开始游戏", textColor);
                if (textSurface) {
                    SDL_Texture *textTexture =
                        SDL_CreateTextureFromSurface(renderer, textSurface);
                    if (textTexture) {
                        // 计算按钮位置
                        int buttonWidth = textSurface->w + 40;
                        int buttonHeight = textSurface->h + 20;
                        int buttonX = (WINDOW_WIDTH - buttonWidth) / 2;
                        int buttonY =
                            234; // 按钮在标题下方70像素（标题高度64 + 70 =
                                 // 134，标题位置100 + 134 = 234）

                        // 获取鼠标位置
                        int mouseX, mouseY;
                        SDL_GetMouseState(&mouseX, &mouseY);

                        // 检查鼠标是否在按钮上
                        bool isHovered = (mouseX >= buttonX &&
                                          mouseX <= buttonX + buttonWidth &&
                                          mouseY >= buttonY &&
                                          mouseY <= buttonY + buttonHeight);

                        // 绘制按钮阴影
                        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 64);
                        SDL_Rect shadowRect = {buttonX + 4, buttonY + 4,
                                               buttonWidth, buttonHeight};
                        SDL_RenderFillRect(renderer, &shadowRect);

                        // 根据鼠标悬停状态设置按钮颜色
                        if (isHovered) {
                            SDL_SetRenderDrawColor(renderer, 50, 150, 255, 255);
                        } else {
                            SDL_SetRenderDrawColor(renderer, 30, 80, 150, 255);
                        }
                        SDL_Rect buttonRect = {buttonX, buttonY, buttonWidth,
                                               buttonHeight};

                        // 绘制圆角矩形
                        for (int i = 0; i < 10; i++) {
                            SDL_Rect roundRect = {
                                buttonRect.x + i, buttonRect.y + i,
                                buttonRect.w - i * 2, buttonRect.h - i * 2};
                            SDL_RenderDrawRect(renderer, &roundRect);
                        }
                        SDL_RenderFillRect(renderer, &buttonRect);

                        // 绘制按钮边框
                        if (isHovered) {
                            SDL_SetRenderDrawColor(renderer, 100, 200, 255,
                                                   255);
                        } else {
                            SDL_SetRenderDrawColor(renderer, 255, 255, 255,
                                                   255);
                        }
                        for (int i = 0; i < 2; i++) {
                            SDL_Rect borderRect = {
                                buttonRect.x + i, buttonRect.y + i,
                                buttonRect.w - i * 2, buttonRect.h - i * 2};
                            SDL_RenderDrawRect(renderer, &borderRect);
                        }

                        // 绘制按钮文字
                        SDL_Rect textRect = {buttonX + 20, buttonY + 10,
                                             textSurface->w, textSurface->h};
                        SDL_RenderCopy(renderer, textTexture, NULL, &textRect);

                        // 检测鼠标点击
                        if (SDL_GetMouseState(&mouseX, &mouseY) &
                            SDL_BUTTON(SDL_BUTTON_LEFT)) {
                            if (mouseX >= buttonX &&
                                mouseX <= buttonX + buttonWidth &&
                                mouseY >= buttonY &&
                                mouseY <= buttonY + buttonHeight) {
                                inStartMenu = false;
                                inGameSelectMenu = true; // 进入游戏选择界面
                            }
                        }

                        SDL_DestroyTexture(textTexture);
                    }
                    SDL_FreeSurface(textSurface);
                }
                TTF_CloseFont(buttonFont);
            }

            // 绘制"游戏设置"按钮
            buttonFont = TTF_OpenFont("simhei.ttf", 36);
            if (buttonFont) {
                SDL_Color textColor = {255, 255, 255, 255};
                SDL_Surface *textSurface =
                    TTF_RenderUTF8_Solid(buttonFont, "游戏设置", textColor);
                if (textSurface) {
                    SDL_Texture *textTexture =
                        SDL_CreateTextureFromSurface(renderer, textSurface);
                    if (textTexture) {
                        // 计算按钮位置，放在"开始游戏"按钮下方
                        int buttonWidth = textSurface->w + 40;
                        int buttonHeight = textSurface->h + 20;
                        int buttonX = (WINDOW_WIDTH - buttonWidth) / 2;
                        int buttonY =
                            367; // 在开始游戏按钮(234)和游戏帮助按钮(500)中间

                        // 获取鼠标位置
                        int mouseX, mouseY;
                        SDL_GetMouseState(&mouseX, &mouseY);

                        // 检查鼠标是否在按钮上
                        bool isHovered = (mouseX >= buttonX &&
                                          mouseX <= buttonX + buttonWidth &&
                                          mouseY >= buttonY &&
                                          mouseY <= buttonY + buttonHeight);

                        // 绘制按钮阴影
                        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 64);
                        SDL_Rect shadowRect = {buttonX + 4, buttonY + 4,
                                               buttonWidth, buttonHeight};
                        SDL_RenderFillRect(renderer, &shadowRect);

                        // 根据鼠标悬停状态设置按钮颜色
                        if (isHovered) {
                            SDL_SetRenderDrawColor(renderer, 150, 100, 255,
                                                   255);
                        } else {
                            SDL_SetRenderDrawColor(renderer, 100, 50, 200, 255);
                        }
                        SDL_Rect buttonRect = {buttonX, buttonY, buttonWidth,
                                               buttonHeight};

                        // 绘制圆角矩形
                        for (int i = 0; i < 10; i++) {
                            SDL_Rect roundRect = {
                                buttonRect.x + i, buttonRect.y + i,
                                buttonRect.w - i * 2, buttonRect.h - i * 2};
                            SDL_RenderDrawRect(renderer, &roundRect);
                        }
                        SDL_RenderFillRect(renderer, &buttonRect);

                        // 绘制按钮边框
                        if (isHovered) {
                            SDL_SetRenderDrawColor(renderer, 200, 150, 255,
                                                   255);
                        } else {
                            SDL_SetRenderDrawColor(renderer, 255, 255, 255,
                                                   255);
                        }
                        for (int i = 0; i < 2; i++) {
                            SDL_Rect borderRect = {
                                buttonRect.x + i, buttonRect.y + i,
                                buttonRect.w - i * 2, buttonRect.h - i * 2};
                            SDL_RenderDrawRect(renderer, &borderRect);
                        }

                        // 绘制按钮文字
                        SDL_Rect textRect = {buttonX + 20, buttonY + 10,
                                             textSurface->w, textSurface->h};
                        SDL_RenderCopy(renderer, textTexture, NULL, &textRect);

                        // 检测鼠标点击
                        if (SDL_GetMouseState(&mouseX, &mouseY) &
                            SDL_BUTTON(SDL_BUTTON_LEFT)) {
                            if (mouseX >= buttonX &&
                                mouseX <= buttonX + buttonWidth &&
                                mouseY >= buttonY &&
                                mouseY <= buttonY + buttonHeight) {
                                // 进入游戏设置界面
                                inSettingsMenu = true;
                                inStartMenu = false;
                            }
                        }

                        SDL_DestroyTexture(textTexture);
                    }
                    SDL_FreeSurface(textSurface);
                }
                TTF_CloseFont(buttonFont);
            }

            // 绘制"游戏帮助"按钮
            buttonFont = TTF_OpenFont("simhei.ttf", 36);
            if (buttonFont) {
                SDL_Color textColor = {255, 255, 255, 255};
                SDL_Surface *textSurface =
                    TTF_RenderUTF8_Solid(buttonFont, "游戏帮助", textColor);
                if (textSurface) {
                    SDL_Texture *textTexture =
                        SDL_CreateTextureFromSurface(renderer, textSurface);
                    if (textTexture) {
                        // 计算按钮位置，放在"游戏设置"按钮下方
                        int buttonWidth = textSurface->w + 40;
                        int buttonHeight = textSurface->h + 20;
                        int buttonX = (WINDOW_WIDTH - buttonWidth) / 2;
                        int buttonY = 500; // 在游戏设置按钮下方100像素

                        // 获取鼠标位置
                        int mouseX, mouseY;
                        SDL_GetMouseState(&mouseX, &mouseY);

                        // 检查鼠标是否在按钮上
                        bool isHovered = (mouseX >= buttonX &&
                                          mouseX <= buttonX + buttonWidth &&
                                          mouseY >= buttonY &&
                                          mouseY <= buttonY + buttonHeight);

                        // 绘制按钮阴影
                        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 64);
                        SDL_Rect shadowRect = {buttonX + 4, buttonY + 4,
                                               buttonWidth, buttonHeight};
                        SDL_RenderFillRect(renderer, &shadowRect);

                        // 根据鼠标悬停状态设置按钮颜色
                        if (isHovered) {
                            SDL_SetRenderDrawColor(renderer, 255, 100, 100,
                                                   255);
                        } else {
                            SDL_SetRenderDrawColor(renderer, 200, 50, 50, 255);
                        }
                        SDL_Rect buttonRect = {buttonX, buttonY, buttonWidth,
                                               buttonHeight};

                        // 绘制圆角矩形
                        for (int i = 0; i < 10; i++) {
                            SDL_Rect roundRect = {
                                buttonRect.x + i, buttonRect.y + i,
                                buttonRect.w - i * 2, buttonRect.h - i * 2};
                            SDL_RenderDrawRect(renderer, &roundRect);
                        }
                        SDL_RenderFillRect(renderer, &buttonRect);

                        // 绘制按钮边框
                        if (isHovered) {
                            SDL_SetRenderDrawColor(renderer, 255, 150, 150,
                                                   255);
                        } else {
                            SDL_SetRenderDrawColor(renderer, 255, 255, 255,
                                                   255);
                        }
                        for (int i = 0; i < 2; i++) {
                            SDL_Rect borderRect = {
                                buttonRect.x + i, buttonRect.y + i,
                                buttonRect.w - i * 2, buttonRect.h - i * 2};
                            SDL_RenderDrawRect(renderer, &borderRect);
                        }

                        // 绘制按钮文字
                        SDL_Rect textRect = {buttonX + 20, buttonY + 10,
                                             textSurface->w, textSurface->h};
                        SDL_RenderCopy(renderer, textTexture, NULL, &textRect);

                        // 检测鼠标点击
                        if (SDL_GetMouseState(&mouseX, &mouseY) &
                            SDL_BUTTON(SDL_BUTTON_LEFT)) {
                            if (mouseX >= buttonX &&
                                mouseX <= buttonX + buttonWidth &&
                                mouseY >= buttonY &&
                                mouseY <= buttonY + buttonHeight) {
                                inHelpMenu = true;
                            }
                        }

                        SDL_DestroyTexture(textTexture);
                    }
                    SDL_FreeSurface(textSurface);
                }
                TTF_CloseFont(buttonFont);
            }

            // 更新屏幕
            SDL_RenderPresent(renderer);

            // 处理事件
            while (SDL_PollEvent(&e) != 0) {
                if (e.type == SDL_QUIT) {
                    quit = true;
                }
            }

            continue; // 跳过游戏主逻辑
        }

        // 计算时间差
        Uint32 currentTime = SDL_GetTicks();
        float deltaTime = (currentTime - lastTime) / 1000.0f;
        lastTime = currentTime;

        // 更新动画
        updateAnimation(deltaTime);
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = true;
            } else if (e.type == SDL_KEYDOWN) {
                Tetromino temp = currentPiece;
                switch (e.key.keysym.sym) {
                case SDLK_a: // A键左移
                    temp.x--;
                    if (!checkCollision(&temp))
                        currentPiece.x--;
                    break;
                case SDLK_d: // D键右移
                    temp.x++;
                    if (!checkCollision(&temp))
                        currentPiece.x++;
                    break;
                case SDLK_s: // S键加速下落
                    temp.y++;
                    if (!checkCollision(&temp))
                        currentPiece.y++;
                    break;
                case SDLK_w: // W键旋转
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
                case SDLK_ESCAPE: // Esc键暂停/继续
                    isPaused = !isPaused;
                    break;
                case SDLK_TAB:    // Tab键切换盲打模式
                    blindMode = !blindMode;
                    break;
                }
            }
        }

        // 自动下落（仅在未暂停时）
        if (!isPaused && SDL_GetTicks() - lastFall > lastFallInterval) {
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

        // 绘制游戏区域、分数
        drawArena(renderer);
        drawScore(renderer);
        
        // 如果不是盲打模式，绘制当前方块和预览
        if (!blindMode) {
            drawPreview(renderer, &currentPiece); // 先绘制预览
            drawPiece(renderer, &currentPiece, false); // 再绘制当前方块
            drawNextPiece(renderer);
        }

        // 如果游戏暂停，绘制暂停界面
        if (isPaused && !gameOver) {
            // 绘制半透明黑色背景
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 128);
            SDL_Rect overlay = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};
            SDL_RenderFillRect(renderer, &overlay);

            // 绘制"游戏暂停"文字
            TTF_Font *font = TTF_OpenFont("simhei.ttf", 48);
            if (font) {
                SDL_Color textColor = {255, 255, 255, 255};
                SDL_Surface *textSurface =
                    TTF_RenderUTF8_Solid(font, "游戏暂停", textColor);
                if (textSurface) {
                    SDL_Texture *textTexture =
                        SDL_CreateTextureFromSurface(renderer, textSurface);
                    if (textTexture) {
                        int textWidth = textSurface->w;
                        int textHeight = textSurface->h;
                        SDL_Rect textRect = {
                            (WINDOW_WIDTH - textWidth) / 2,
                            (WINDOW_HEIGHT - textHeight) / 2 -
                                200, // 再向上移动50像素（总共150像素）
                            textWidth, textHeight};
                        SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
                        SDL_DestroyTexture(textTexture);
                    }
                    SDL_FreeSurface(textSurface);
                }
                TTF_CloseFont(font);
            }

            // 绘制"保存游戏进度"按钮
            font = TTF_OpenFont("simhei.ttf", 36);
            if (font) {
                SDL_Color textColor = {255, 255, 255, 255};
                SDL_Surface *textSurface =
                    TTF_RenderUTF8_Solid(font, "保存游戏", textColor);
                if (textSurface) {
                    SDL_Texture *textTexture =
                        SDL_CreateTextureFromSurface(renderer, textSurface);
                    if (textTexture) {
                        // 计算按钮位置，放在"继续游戏"按钮下方
                        int buttonWidth = textSurface->w + 40;
                        int buttonHeight = textSurface->h + 20;
                        int buttonX = (WINDOW_WIDTH - buttonWidth) / 2;
                        int buttonY = (WINDOW_HEIGHT - buttonHeight) / 2 -
                                      30; // 向上移动80像素

                        // 获取鼠标位置
                        int mouseX, mouseY;
                        SDL_GetMouseState(&mouseX, &mouseY);

                        // 检查鼠标是否在按钮上
                        bool isHovered = (mouseX >= buttonX &&
                                          mouseX <= buttonX + buttonWidth &&
                                          mouseY >= buttonY &&
                                          mouseY <= buttonY + buttonHeight);

                        // 绘制按钮阴影
                        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 64);
                        SDL_Rect shadowRect = {buttonX + 4, buttonY + 4,
                                               buttonWidth, buttonHeight};
                        SDL_RenderFillRect(renderer, &shadowRect);

                        // 根据鼠标悬停状态设置按钮颜色
                        if (isHovered) {
                            // 悬停时使用更亮的橙色
                            SDL_SetRenderDrawColor(renderer, 255, 165, 0, 255);
                        } else {
                            // 正常状态使用深橙色
                            SDL_SetRenderDrawColor(renderer, 200, 100, 0, 255);
                        }
                        SDL_Rect buttonRect = {buttonX, buttonY, buttonWidth,
                                               buttonHeight};

                        // 绘制圆角矩形
                        for (int i = 0; i < 10; i++) {
                            SDL_Rect roundRect = {
                                buttonRect.x + i, buttonRect.y + i,
                                buttonRect.w - i * 2, buttonRect.h - i * 2};
                            SDL_RenderDrawRect(renderer, &roundRect);
                        }
                        SDL_RenderFillRect(renderer, &buttonRect);

                        // 绘制按钮边框
                        if (isHovered) {
                            // 悬停时使用更亮的边框
                            SDL_SetRenderDrawColor(renderer, 255, 200, 100,
                                                   255);
                        } else {
                            // 正常状态使用白色边框
                            SDL_SetRenderDrawColor(renderer, 255, 255, 255,
                                                   255);
                        }
                        for (int i = 0; i < 2; i++) {
                            SDL_Rect borderRect = {
                                buttonRect.x + i, buttonRect.y + i,
                                buttonRect.w - i * 2, buttonRect.h - i * 2};
                            SDL_RenderDrawRect(renderer, &borderRect);
                        }

                        // 添加按钮内发光效果
                        if (isHovered) {
                            SDL_SetRenderDrawColor(renderer, 255, 200, 100, 50);
                            for (int i = 0; i < 5; i++) {
                                SDL_Rect glowRect = {
                                    buttonRect.x + i, buttonRect.y + i,
                                    buttonRect.w - i * 2, buttonRect.h - i * 2};
                                SDL_RenderDrawRect(renderer, &glowRect);
                            }
                        }

                        // 绘制按钮文字
                        SDL_Rect textRect = {buttonX + 20, buttonY + 10,
                                             textSurface->w, textSurface->h};
                        SDL_RenderCopy(renderer, textTexture, NULL, &textRect);

                        // 检测鼠标点击
                        static Uint32 mouseDownTime = 0;
                        static bool isMouseDown = false;

                        if (SDL_GetMouseState(&mouseX, &mouseY) &
                            SDL_BUTTON(SDL_BUTTON_LEFT)) {
                            if (!isMouseDown && mouseX >= buttonX &&
                                mouseX <= buttonX + buttonWidth &&
                                mouseY >= buttonY &&
                                mouseY <= buttonY + buttonHeight) {
                                // 记录鼠标按下时间和状态
                                mouseDownTime = SDL_GetTicks();
                                isMouseDown = true;
                            }
                        } else if (isMouseDown) {
                            // 鼠标抬起，检查是否在按钮区域内且时间超过100ms
                            if (mouseX >= buttonX &&
                                mouseX <= buttonX + buttonWidth &&
                                mouseY >= buttonY &&
                                mouseY <= buttonY + buttonHeight &&
                                SDL_GetTicks() - mouseDownTime >= 100) {
                                // 保存游戏进度
                                FILE *file = fopen("savegame.dat", "wb");
                                if (file) {
                                    // 保存游戏区域
                                    fwrite(arena, sizeof(arena), 1, file);
                                    // 保存当前方块
                                    fwrite(&currentPiece, sizeof(currentPiece),
                                           1, file);
                                    // 保存下一个方块
                                    fwrite(&nextPiece, sizeof(nextPiece), 1,
                                           file);
                                    // 保存分数
                                    fwrite(&score, sizeof(score), 1, file);
                                    fclose(file);
                                }
                            }
                            isMouseDown = false;
                        }

                        SDL_DestroyTexture(textTexture);
                    }
                    SDL_FreeSurface(textSurface);
                }
                TTF_CloseFont(font);
            }

            // 绘制"返回上个方块"按钮
            font = TTF_OpenFont("simhei.ttf", 36);
            if (font) {
                SDL_Color textColor = {255, 255, 255, 255};
                SDL_Surface *textSurface =
                    TTF_RenderUTF8_Solid(font, "方块回退", textColor);
                if (textSurface) {
                    SDL_Texture *textTexture =
                        SDL_CreateTextureFromSurface(renderer, textSurface);
                    if (textTexture) {
                        // 计算按钮位置，放在"保存游戏进度"按钮下方
                        int buttonWidth = textSurface->w + 40;
                        int buttonHeight = textSurface->h + 20;
                        int buttonX = (WINDOW_WIDTH - buttonWidth) / 2;
                        int buttonY = (WINDOW_HEIGHT - buttonHeight) / 2 +
                                      130; // 向下偏移130像素

                        // 获取鼠标位置
                        int mouseX, mouseY;
                        SDL_GetMouseState(&mouseX, &mouseY);

                        // 检查鼠标是否在按钮上
                        bool isHovered = (mouseX >= buttonX &&
                                          mouseX <= buttonX + buttonWidth &&
                                          mouseY >= buttonY &&
                                          mouseY <= buttonY + buttonHeight);

                        // 绘制按钮阴影
                        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 64);
                        SDL_Rect shadowRect = {buttonX + 4, buttonY + 4,
                                               buttonWidth, buttonHeight};
                        SDL_RenderFillRect(renderer, &shadowRect);

                        // 根据鼠标悬停状态设置按钮颜色
                        if (isHovered) {
                            // 悬停时使用更亮的紫色
                            SDL_SetRenderDrawColor(renderer, 200, 100, 200,
                                                   255);
                        } else {
                            // 正常状态使用深紫色
                            SDL_SetRenderDrawColor(renderer, 100, 50, 100, 255);
                        }
                        SDL_Rect buttonRect = {buttonX, buttonY, buttonWidth,
                                               buttonHeight};

                        // 绘制圆角矩形
                        for (int i = 0; i < 10; i++) {
                            SDL_Rect roundRect = {
                                buttonRect.x + i, buttonRect.y + i,
                                buttonRect.w - i * 2, buttonRect.h - i * 2};
                            SDL_RenderDrawRect(renderer, &roundRect);
                        }
                        SDL_RenderFillRect(renderer, &buttonRect);

                        // 绘制按钮边框
                        if (isHovered) {
                            // 悬停时使用更亮的边框
                            SDL_SetRenderDrawColor(renderer, 255, 150, 255,
                                                   255);
                        } else {
                            // 正常状态使用白色边框
                            SDL_SetRenderDrawColor(renderer, 255, 255, 255,
                                                   255);
                        }
                        for (int i = 0; i < 2; i++) {
                            SDL_Rect borderRect = {
                                buttonRect.x + i, buttonRect.y + i,
                                buttonRect.w - i * 2, buttonRect.h - i * 2};
                            SDL_RenderDrawRect(renderer, &borderRect);
                        }

                        // 添加按钮内发光效果
                        if (isHovered) {
                            SDL_SetRenderDrawColor(renderer, 255, 150, 255, 50);
                            for (int i = 0; i < 5; i++) {
                                SDL_Rect glowRect = {
                                    buttonRect.x + i, buttonRect.y + i,
                                    buttonRect.w - i * 2, buttonRect.h - i * 2};
                                SDL_RenderDrawRect(renderer, &glowRect);
                            }
                        }

                        // 绘制按钮文字
                        SDL_Rect textRect = {buttonX + 20, buttonY + 10,
                                             textSurface->w, textSurface->h};
                        SDL_RenderCopy(renderer, textTexture, NULL, &textRect);

                        // 检测鼠标点击
                        static Uint32 mouseDownTime = 0;
                        static bool isMouseDown = false;

                        if (SDL_GetMouseState(&mouseX, &mouseY) &
                            SDL_BUTTON(SDL_BUTTON_LEFT)) {
                            if (!isMouseDown && mouseX >= buttonX &&
                                mouseX <= buttonX + buttonWidth &&
                                mouseY >= buttonY &&
                                mouseY <= buttonY + buttonHeight) {
                                // 记录鼠标按下时间和状态
                                mouseDownTime = SDL_GetTicks();
                                isMouseDown = true;
                            }
                        } else if (isMouseDown) {
                            // 鼠标抬起，检查是否在按钮区域内且时间超过100ms
                            if (mouseX >= buttonX &&
                                mouseX <= buttonX + buttonWidth &&
                                mouseY >= buttonY &&
                                mouseY <= buttonY + buttonHeight &&
                                SDL_GetTicks() - mouseDownTime >= 100) {
                                // 执行撤销操作
                                undoLastMove();
                            }
                            isMouseDown = false;
                        }

                        SDL_DestroyTexture(textTexture);
                    }
                    SDL_FreeSurface(textSurface);
                }
                TTF_CloseFont(font);
            }

            // 绘制"重新开始"按钮
            font = TTF_OpenFont("simhei.ttf", 36);
            if (font) {
                SDL_Color textColor = {255, 255, 255, 255};
                SDL_Surface *textSurface =
                    TTF_RenderUTF8_Solid(font, "重新开始", textColor);
                if (textSurface) {
                    SDL_Texture *textTexture =
                        SDL_CreateTextureFromSurface(renderer, textSurface);
                    if (textTexture) {
                        // 计算按钮位置，放在"方块回退"按钮下方
                        int buttonWidth = textSurface->w + 40;
                        int buttonHeight = textSurface->h + 20;
                        int buttonX = (WINDOW_WIDTH - buttonWidth) / 2;
                        int buttonY = (WINDOW_HEIGHT - buttonHeight) / 2 +
                                      210; // 向下偏移210像素

                        // 获取鼠标位置
                        int mouseX, mouseY;
                        SDL_GetMouseState(&mouseX, &mouseY);

                        // 检查鼠标是否在按钮上
                        bool isHovered = (mouseX >= buttonX &&
                                          mouseX <= buttonX + buttonWidth &&
                                          mouseY >= buttonY &&
                                          mouseY <= buttonY + buttonHeight);

                        // 绘制按钮阴影
                        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 64);
                        SDL_Rect shadowRect = {buttonX + 4, buttonY + 4,
                                               buttonWidth, buttonHeight};
                        SDL_RenderFillRect(renderer, &shadowRect);

                        // 根据鼠标悬停状态设置按钮颜色
                        if (isHovered) {
                            // 悬停时使用更亮的绿色
                            SDL_SetRenderDrawColor(renderer, 50, 255, 50, 255);
                        } else {
                            // 正常状态使用深绿色
                            SDL_SetRenderDrawColor(renderer, 30, 150, 30, 255);
                        }
                        SDL_Rect buttonRect = {buttonX, buttonY, buttonWidth,
                                               buttonHeight};

                        // 绘制圆角矩形
                        for (int i = 0; i < 10; i++) {
                            SDL_Rect roundRect = {
                                buttonRect.x + i, buttonRect.y + i,
                                buttonRect.w - i * 2, buttonRect.h - i * 2};
                            SDL_RenderDrawRect(renderer, &roundRect);
                        }
                        SDL_RenderFillRect(renderer, &buttonRect);

                        // 绘制按钮边框
                        if (isHovered) {
                            // 悬停时使用更亮的边框
                            SDL_SetRenderDrawColor(renderer, 100, 255, 100,
                                                   255);
                        } else {
                            // 正常状态使用白色边框
                            SDL_SetRenderDrawColor(renderer, 255, 255, 255,
                                                   255);
                        }
                        for (int i = 0; i < 2; i++) {
                            SDL_Rect borderRect = {
                                buttonRect.x + i, buttonRect.y + i,
                                buttonRect.w - i * 2, buttonRect.h - i * 2};
                            SDL_RenderDrawRect(renderer, &borderRect);
                        }

                        // 添加按钮内发光效果
                        if (isHovered) {
                            SDL_SetRenderDrawColor(renderer, 100, 255, 100, 50);
                            for (int i = 0; i < 5; i++) {
                                SDL_Rect glowRect = {
                                    buttonRect.x + i, buttonRect.y + i,
                                    buttonRect.w - i * 2, buttonRect.h - i * 2};
                                SDL_RenderDrawRect(renderer, &glowRect);
                            }
                        }

                        // 绘制按钮文字
                        SDL_Rect textRect = {buttonX + 20, buttonY + 10,
                                             textSurface->w, textSurface->h};
                        SDL_RenderCopy(renderer, textTexture, NULL, &textRect);

                        // 检测鼠标点击
                        if (SDL_GetMouseState(&mouseX, &mouseY) &
                            SDL_BUTTON(SDL_BUTTON_LEFT)) {
                            if (mouseX >= buttonX &&
                                mouseX <= buttonX + buttonWidth &&
                                mouseY >= buttonY &&
                                mouseY <= buttonY + buttonHeight) {
                                // 返回开始界面
                                inStartMenu = true;
                                gameOver = false;
                                isPaused = false;
                            }
                        }

                        SDL_DestroyTexture(textTexture);
                    }
                    SDL_FreeSurface(textSurface);
                }
                TTF_CloseFont(font);
            }

            // 绘制"退出游戏"按钮
            font = TTF_OpenFont("simhei.ttf", 36);
            if (font) {
                SDL_Color textColor = {255, 255, 255, 255};
                SDL_Surface *textSurface =
                    TTF_RenderUTF8_Solid(font, "退出游戏", textColor);
                if (textSurface) {
                    SDL_Texture *textTexture =
                        SDL_CreateTextureFromSurface(renderer, textSurface);
                    if (textTexture) {
                        // 计算按钮位置，放在"保存游戏进度"按钮下方
                        int buttonWidth = textSurface->w + 40;
                        int buttonHeight = textSurface->h + 20;
                        int buttonX = (WINDOW_WIDTH - buttonWidth) / 2;
                        int buttonY = (WINDOW_HEIGHT - buttonHeight) / 2 +
                                      50; // 向下偏移80像素

                        // 获取鼠标位置
                        int mouseX, mouseY;
                        SDL_GetMouseState(&mouseX, &mouseY);

                        // 检查鼠标是否在按钮上
                        bool isHovered = (mouseX >= buttonX &&
                                          mouseX <= buttonX + buttonWidth &&
                                          mouseY >= buttonY &&
                                          mouseY <= buttonY + buttonHeight);

                        // 绘制按钮阴影
                        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 64);
                        SDL_Rect shadowRect = {buttonX + 4, buttonY + 4,
                                               buttonWidth, buttonHeight};
                        SDL_RenderFillRect(renderer, &shadowRect);

                        // 根据鼠标悬停状态设置按钮颜色
                        if (isHovered) {
                            // 悬停时使用更亮的红色
                            SDL_SetRenderDrawColor(renderer, 255, 50, 50, 255);
                        } else {
                            // 正常状态使用深红色
                            SDL_SetRenderDrawColor(renderer, 150, 30, 30, 255);
                        }
                        SDL_Rect buttonRect = {buttonX, buttonY, buttonWidth,
                                               buttonHeight};

                        // 绘制圆角矩形
                        for (int i = 0; i < 10; i++) {
                            SDL_Rect roundRect = {
                                buttonRect.x + i, buttonRect.y + i,
                                buttonRect.w - i * 2, buttonRect.h - i * 2};
                            SDL_RenderDrawRect(renderer, &roundRect);
                        }
                        SDL_RenderFillRect(renderer, &buttonRect);

                        // 绘制按钮边框
                        if (isHovered) {
                            // 悬停时使用更亮的边框
                            SDL_SetRenderDrawColor(renderer, 255, 100, 100,
                                                   255);
                        } else {
                            // 正常状态使用白色边框
                            SDL_SetRenderDrawColor(renderer, 255, 255, 255,
                                                   255);
                        }
                        for (int i = 0; i < 2; i++) {
                            SDL_Rect borderRect = {
                                buttonRect.x + i, buttonRect.y + i,
                                buttonRect.w - i * 2, buttonRect.h - i * 2};
                            SDL_RenderDrawRect(renderer, &borderRect);
                        }

                        // 添加按钮内发光效果
                        if (isHovered) {
                            SDL_SetRenderDrawColor(renderer, 255, 100, 100, 50);
                            for (int i = 0; i < 5; i++) {
                                SDL_Rect glowRect = {
                                    buttonRect.x + i, buttonRect.y + i,
                                    buttonRect.w - i * 2, buttonRect.h - i * 2};
                                SDL_RenderDrawRect(renderer, &glowRect);
                            }
                        }

                        // 绘制按钮文字
                        SDL_Rect textRect = {buttonX + 20, buttonY + 10,
                                             textSurface->w, textSurface->h};
                        SDL_RenderCopy(renderer, textTexture, NULL, &textRect);

                        // 检测鼠标点击
                        if (SDL_GetMouseState(&mouseX, &mouseY) &
                            SDL_BUTTON(SDL_BUTTON_LEFT)) {
                            if (mouseX >= buttonX &&
                                mouseX <= buttonX + buttonWidth &&
                                mouseY >= buttonY &&
                                mouseY <= buttonY + buttonHeight) {
                                quit = true;
                            }
                        }

                        SDL_DestroyTexture(textTexture);
                    }
                    SDL_FreeSurface(textSurface);
                }
                TTF_CloseFont(font);
            }

            // 绘制"按Esc继续"提示
            font = TTF_OpenFont("simhei.ttf", 24);
            if (font) {
                SDL_Color textColor = {255, 255, 255, 255};
                SDL_Surface *textSurface =
                    TTF_RenderUTF8_Solid(font, "按 Esc 键继续游戏", textColor);
                if (textSurface) {
                    SDL_Texture *textTexture =
                        SDL_CreateTextureFromSurface(renderer, textSurface);
                    if (textTexture) {
                        int textWidth = textSurface->w;
                        int textHeight = textSurface->h;
                        SDL_Rect textRect = {
                            (WINDOW_WIDTH - textWidth) / 2,
                            (WINDOW_HEIGHT - textHeight) / 2 -
                                130, // 向上移动170像素，与"游戏暂停"间距30像素
                            textWidth, textHeight};
                        SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
                        SDL_DestroyTexture(textTexture);
                    }
                    SDL_FreeSurface(textSurface);
                }
                TTF_CloseFont(font);
            }
        }

        // 如果游戏结束，绘制退出按钮
        if (gameOver) {
            // 绘制半透明黑色背景
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 128);
            SDL_Rect overlay = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};
            SDL_RenderFillRect(renderer, &overlay);

            // 绘制退出按钮
            TTF_Font *font = TTF_OpenFont("simhei.ttf", 36);
            if (font) {
                SDL_Color textColor = {255, 255, 255, 255};
                SDL_Surface *textSurface =
                    TTF_RenderUTF8_Solid(font, "退出游戏", textColor);
                if (textSurface) {
                    SDL_Texture *textTexture =
                        SDL_CreateTextureFromSurface(renderer, textSurface);
                    if (textTexture) {
                        // 计算按钮位置，向上移动100像素
                        int buttonWidth = textSurface->w + 40;
                        int buttonHeight = textSurface->h + 20;
                        int buttonX = (WINDOW_WIDTH - buttonWidth) / 2;
                        int buttonY = (WINDOW_HEIGHT - buttonHeight) / 2 - 100;

                        // 获取鼠标位置
                        int mouseX, mouseY;
                        SDL_GetMouseState(&mouseX, &mouseY);

                        // 检查鼠标是否在按钮上
                        bool isHovered = (mouseX >= buttonX &&
                                          mouseX <= buttonX + buttonWidth &&
                                          mouseY >= buttonY &&
                                          mouseY <= buttonY + buttonHeight);

                        // 绘制按钮阴影
                        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 64);
                        SDL_Rect shadowRect = {buttonX + 4, buttonY + 4,
                                               buttonWidth, buttonHeight};
                        SDL_RenderFillRect(renderer, &shadowRect);

                        // 根据鼠标悬停状态设置按钮颜色
                        if (isHovered) {
                            // 悬停时使用更亮的蓝色
                            SDL_SetRenderDrawColor(renderer, 50, 150, 255, 255);
                        } else {
                            // 正常状态使用深蓝色
                            SDL_SetRenderDrawColor(renderer, 30, 80, 150, 255);
                        }
                        SDL_Rect buttonRect = {buttonX, buttonY, buttonWidth,
                                               buttonHeight};

                        // 绘制圆角矩形
                        for (int i = 0; i < 10; i++) {
                            SDL_Rect roundRect = {
                                buttonRect.x + i, buttonRect.y + i,
                                buttonRect.w - i * 2, buttonRect.h - i * 2};
                            SDL_RenderDrawRect(renderer, &roundRect);
                        }
                        SDL_RenderFillRect(renderer, &buttonRect);

                        // 绘制按钮边框
                        if (isHovered) {
                            // 悬停时使用更亮的边框
                            SDL_SetRenderDrawColor(renderer, 100, 200, 255,
                                                   255);
                        } else {
                            // 正常状态使用白色边框
                            SDL_SetRenderDrawColor(renderer, 255, 255, 255,
                                                   255);
                        }
                        for (int i = 0; i < 2; i++) {
                            SDL_Rect borderRect = {
                                buttonRect.x + i, buttonRect.y + i,
                                buttonRect.w - i * 2, buttonRect.h - i * 2};
                            SDL_RenderDrawRect(renderer, &borderRect);
                        }

                        // 添加按钮内发光效果
                        if (isHovered) {
                            SDL_SetRenderDrawColor(renderer, 100, 200, 255, 50);
                            for (int i = 0; i < 5; i++) {
                                SDL_Rect glowRect = {
                                    buttonRect.x + i, buttonRect.y + i,
                                    buttonRect.w - i * 2, buttonRect.h - i * 2};
                                SDL_RenderDrawRect(renderer, &glowRect);
                            }
                        }

                        // 绘制按钮文字
                        SDL_Rect textRect = {buttonX + 20, buttonY + 10,
                                             textSurface->w, textSurface->h};
                        SDL_RenderCopy(renderer, textTexture, NULL, &textRect);

                        // 检测鼠标点击
                        if (SDL_GetMouseState(&mouseX, &mouseY) &
                            SDL_BUTTON(SDL_BUTTON_LEFT)) {
                            if (mouseX >= buttonX &&
                                mouseX <= buttonX + buttonWidth &&
                                mouseY >= buttonY &&
                                mouseY <= buttonY + buttonHeight) {
                                quit = true;
                            }
                        }

                        SDL_DestroyTexture(textTexture);
                    }
                    SDL_FreeSurface(textSurface);
                }
                TTF_CloseFont(font);
            }

            // 绘制"返回开始界面"按钮
            font = TTF_OpenFont("simhei.ttf", 36);
            if (font) {
                SDL_Color textColor = {255, 255, 255, 255};
                SDL_Surface *textSurface =
                    TTF_RenderUTF8_Solid(font, "返回开始界面", textColor);
                if (textSurface) {
                    SDL_Texture *textTexture =
                        SDL_CreateTextureFromSurface(renderer, textSurface);
                    if (textTexture) {
                        // 计算按钮位置，放在"退出游戏"按钮下方
                        int buttonWidth = textSurface->w + 40;
                        int buttonHeight = textSurface->h + 20;
                        int buttonX = (WINDOW_WIDTH - buttonWidth) / 2;
                        int buttonY = (WINDOW_HEIGHT - buttonHeight) / 2 +
                                      50; // 向下偏移50像素

                        // 获取鼠标位置
                        int mouseX, mouseY;
                        SDL_GetMouseState(&mouseX, &mouseY);

                        // 检查鼠标是否在按钮上
                        bool isHovered = (mouseX >= buttonX &&
                                          mouseX <= buttonX + buttonWidth &&
                                          mouseY >= buttonY &&
                                          mouseY <= buttonY + buttonHeight);

                        // 绘制按钮阴影
                        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 64);
                        SDL_Rect shadowRect = {buttonX + 4, buttonY + 4,
                                               buttonWidth, buttonHeight};
                        SDL_RenderFillRect(renderer, &shadowRect);

                        // 根据鼠标悬停状态设置按钮颜色
                        if (isHovered) {
                            // 悬停时使用更亮的绿色
                            SDL_SetRenderDrawColor(renderer, 50, 255, 50, 255);
                        } else {
                            // 正常状态使用深绿色
                            SDL_SetRenderDrawColor(renderer, 30, 150, 30, 255);
                        }
                        SDL_Rect buttonRect = {buttonX, buttonY, buttonWidth,
                                               buttonHeight};

                        // 绘制圆角矩形
                        for (int i = 0; i < 10; i++) {
                            SDL_Rect roundRect = {
                                buttonRect.x + i, buttonRect.y + i,
                                buttonRect.w - i * 2, buttonRect.h - i * 2};
                            SDL_RenderDrawRect(renderer, &roundRect);
                        }
                        SDL_RenderFillRect(renderer, &buttonRect);

                        // 绘制按钮边框
                        if (isHovered) {
                            // 悬停时使用更亮的边框
                            SDL_SetRenderDrawColor(renderer, 100, 255, 100,
                                                   255);
                        } else {
                            // 正常状态使用白色边框
                            SDL_SetRenderDrawColor(renderer, 255, 255, 255,
                                                   255);
                        }
                        for (int i = 0; i < 2; i++) {
                            SDL_Rect borderRect = {
                                buttonRect.x + i, buttonRect.y + i,
                                buttonRect.w - i * 2, buttonRect.h - i * 2};
                            SDL_RenderDrawRect(renderer, &borderRect);
                        }

                        // 添加按钮内发光效果
                        if (isHovered) {
                            SDL_SetRenderDrawColor(renderer, 100, 255, 100, 50);
                            for (int i = 0; i < 5; i++) {
                                SDL_Rect glowRect = {
                                    buttonRect.x + i, buttonRect.y + i,
                                    buttonRect.w - i * 2, buttonRect.h - i * 2};
                                SDL_RenderDrawRect(renderer, &glowRect);
                            }
                        }

                        // 绘制按钮文字
                        SDL_Rect textRect = {buttonX + 20, buttonY + 10,
                                             textSurface->w, textSurface->h};
                        SDL_RenderCopy(renderer, textTexture, NULL, &textRect);

                        // 检测鼠标点击
                        if (SDL_GetMouseState(&mouseX, &mouseY) &
                            SDL_BUTTON(SDL_BUTTON_LEFT)) {
                            if (mouseX >= buttonX &&
                                mouseX <= buttonX + buttonWidth &&
                                mouseY >= buttonY &&
                                mouseY <= buttonY + buttonHeight) {
                                // 返回开始界面
                                inStartMenu = true;
                                gameOver = false;
                            }
                        }

                        SDL_DestroyTexture(textTexture);
                    }
                    SDL_FreeSurface(textSurface);
                }
                TTF_CloseFont(font);
            }
        }

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
