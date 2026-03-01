#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>

// SDL type definitions for compatibility
typedef struct {
    int type;
    union {
        struct {
            int sym;
            int repeat;
        } key;
        struct {
            int button;
            int x;
            int y;
        } button;
    };
} SDL_Event;

// Forward declaration for SDL_Texture (defined in SpriteManager.h)
struct SDL_Texture;

typedef uint8_t u8;

// SDL constants
#define SDL_KEYDOWN 1
#define SDL_KEYUP 2
#define SDL_MOUSEBUTTONDOWN 3
#define SDL_MOUSEBUTTONUP 4
#define SDL_BUTTON_LEFT 1
#define SDL_BUTTON_RIGHT 2

// SDL key constants
#define SDLK_LEFT 1
#define SDLK_RIGHT 2
#define SDLK_UP 3
#define SDLK_DOWN 4
#define SDLK_a 5
#define SDLK_d 6
#define SDLK_w 7
#define SDLK_s 8
#define SDLK_ESCAPE 9
#define SDLK_RETURN 10
#define SDLK_t 11
#define SDLK_y 12
#define SDLK_m 13
#define SDLK_i 14
#define SDLK_1 15
#define SDLK_2 16
#define SDLK_3 17
#define SDLK_4 18
#define SDLK_5 19
#define SDLK_6 20
#define SDLK_7 21
#define SDLK_8 22
#define SDLK_r 23
#define SDLK_BACKSPACE 24
#define SDLK_x 25
#define SDLK_q 26
#define SDLK_0 27
#define SDLK_9 28
#define SDLK_COMMA 29
#define SDLK_o 30
#define SDLK_e 31
#define SDLK_SPACE 32
#define SDLK_p 33
#define SDLK_l 34
#define SDLK_c 35
#define SDLK_TAB 36
#define SDLK_KP_ENTER 37

class BattleState;
class Item;
class Player;
class PokeMap;
class Battler;
class Trainer;
class Npc;
class WorldObject;
class MartItem;
class Move;

class Gui {
public:
    static bool showingText;
    static bool battling;
    static bool canMap;
    bool choosingFromLongList;
    static bool usingBattleItem;
    static bool busedItem;
    static bool rightClicked;
    static bool inMenu;
    static bool buySell;
    static bool buying;
    static bool selling;
    bool checkingPokes;
    bool checkingMoves;
    static bool checkingTms;
    static bool teachingMove;
    static bool replacingMove;
    static bool depWith;
    static bool depositing;
    static bool withdraw;
    static bool flying;
    static bool inside;
    static bool showingDex;
    static bool rareCandy;
    static bool pickingStarter;
    static bool surfing;
    static bool spacebar;
    int currentMenu; // 0=Inventory, 1=TMs, 2=Pokemon, 3=Pokedex
    bool autoBattle;

    static int clickedChoice;
    static std::string currentText;
    static std::string currentLoc;
    std::string longListHeaderOverride;
    std::vector<std::string> strArr;
    static std::vector<Item*> usableItems;
    static bool usingItemList;
    static BattleState *playerState;
    static BattleState *enemyState;

    enum class Direction {
        SOUTH = 0,
        NORTH = 1,
        WEST = 2,
        EAST = 3
    };

    enum class StepPhase { NONE, MOVING, LANDING };

    void print(const std::string &s);
    static std::string promptText(const std::string &prompt);
    static std::string promptNumber(const std::string &prompt);
    int waitForChoice(int maxChoice);
    static void setBattleStates(BattleState *a, BattleState *b);
    static void autoAdvanceBattleText();
    static void runGui();

    explicit Gui(Player *player);
    Gui(Player *player, const std::string &saveInfo);

    void setWildItems();
    void setTrainerItems();
    void pumpEvents();
    void handleEvent(const SDL_Event &event);
    void update();
    void render();
    void closeMenus();

    // Public variables for battle inventory access
    std::vector<std::string> longArr;
    int psi;

    Player *player;

private:
    static void advanceText();
    void stopMovement();
    void applySaveInfo(const std::string &saveInfo);
    void setup();
    void loadMap(PokeMap *map);
    void loadTileImages();
    void loadPlayerSprites();
    void clickMouse(int mouseX, int mouseY, bool rightClick);
    void save();
    void startFlying();
    std::string printable(const std::string &s);
    bool canMove(Direction d);
    int elevate(const std::string &s, bool B, int m);
    char randomSpooky();
    void printSpooky();
    void advanceStep();
    void blackout();
    void useRevive(Battler *b, int n);
    void healHp(Battler *b, int n);
    void useElixir(Battler *b, int n);
    void healStatus(Battler *b, const std::string &s);
    void useRepel(int steps);
    void catchMon(double bm);
    void notifyUseItem(bool b);
    void drawTextBox(const std::string &text);
    void drawWrappedText(const std::string &text, int x, int y, int maxWidth);
    void drawRainbowText(u8* framebuffer, std::map<char, std::pair<const unsigned char*, unsigned int>>& fontMapData, const std::string &text, int x, int y, int fb_width, int fb_height);
    void drawConnection(int dirIndex, PokeMap *conn, int offset, int camX, int camY);
    int getCurrentFrame() const;
    void renderBattle();

    PokeMap *pm = nullptr;
    PokeMap *lastHeal = nullptr;
    std::string mapName = "RedsHouse2F";
    std::vector<std::vector<int>> currentMap;
    std::vector<std::vector<int>> tileTypes;
    PokeMap *connections[4]{nullptr, nullptr, nullptr, nullptr};
    int connOffsets[4]{0, 0, 0, 0};

    int playerX = 0;
    int playerY = 6;
    int repelSteps = 0;
    int boxNum = 0;
    int frames = 0;
    int phaseFrame = 0;
    int currentStepFrames = 8;
    int timesMoved = 0;
    int bet = 1;
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    Direction facing = Direction::SOUTH;
    Direction heldDirection = Direction::SOUTH;
    bool hasHeldDirection = false;
    StepPhase stepPhase = StepPhase::NONE;
    bool switchingMaps = false;
    bool spinning = false;
    bool jumping = false;

    bool blackjackActive = false;
    int blackjackBet = 0;
    std::vector<int> blackjackDeck;
    std::vector<int> blackjackPlayer;
    std::vector<int> blackjackDealer;
    bool blackjackHideFirst = true;
    bool blackjackFinished = false;
    bool blackjackPlayerWon = false;
    int blackjackTieFrames = 0;
    bool blackjackShowingResults = false;

    std::vector<Direction> pressedKeys;
    Battler *chosenMon = nullptr;
    Battler *wildMon = nullptr;
    Item *usedItem = nullptr;
    std::vector<MartItem> martItems;
    std::vector<Move*> tms;
    Move *tm = nullptr;

    std::vector<SDL_Texture*> tileImages;
    std::vector<SDL_Texture*> playerFrames;
    std::vector<SDL_Texture*> seelFrames;
    std::vector<SDL_Texture*> pBattlers;
    std::vector<SDL_Texture*> eBattlers;
    std::vector<const unsigned char*> danceFrames;
    std::vector<const unsigned char*> oakFrames;
    std::vector<const unsigned char*> monsterFrames;
    Npc *mattNpc = nullptr;
    Npc *oakNpc = nullptr;
    Trainer *monsterTrainer = nullptr;

    // Static tile data array for 3DS rendering
    static std::pair<const unsigned char*, unsigned int>* tileDataArray;
    static bool tileDataInitialized;
    static Gui *instance;
};

// Global GUI instance declaration
extern Gui* g_gui;
