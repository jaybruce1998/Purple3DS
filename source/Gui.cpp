#include "Gui.h"
#include "BattleState.h"
#include "Blocker.h"
#include "FlyLocation.h"
#include "GameConfig3DS.h"
#include "Item.h"
#include "MartItem.h"
#include "Monster.h"
#include "Move.h"
#include "Npc.h"
#include "NpcStrings.h"
#include "PokeMap.h"
#include "Player.h"
#include "Trainer.h"
#include "Warp.h"
#include "WorldObject.h"
#include "Utils.h"
#include "map_3ds_data.h"

// External declarations for 3DS debugging
extern void debugCheckpoint(const char* message, bool bottom = false);

// Initialize static members
std::pair<const unsigned char*, unsigned int>* Gui::tileDataArray = nullptr;
bool Gui::tileDataInitialized = false;

#include "Gui.h"
#include "Player.h"
#include "GameConfig3DS.h"
#include "MAP_3DS_bgr.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <3ds.h>
#include <3ds/gfx.h>
#include "all_sprites.hpp"
#include "all_battlers.h"
#include "map_data.h"
#include "map_3ds.h"

// External declarations for font map data
extern std::map<char, std::pair<const unsigned char*, unsigned int>> fontMapData;

// 3DS Graphics Functions (moved from main_3ds.cpp)
void setPixel(u8* framebuffer, int x, int y, u8 r, u8 g, u8 b, int width) {
    if (x < 0 || x >= width || y < 0 || y >= 240) return;
    
    // Follow drawBGRAsset pattern EXACTLY:
    // 1. Always flip X: (width - 1) - x
    // 2. Don't flip Y (since we're not doing flip_horizontal): y
    
    int dst_x_pos = (width - 1) - x;
    int dst_y_pos = y;
    
    int pos = (dst_y_pos * width + dst_x_pos) * 3;
    framebuffer[pos] = b;     // B
    framebuffer[pos + 1] = g; // G
    framebuffer[pos + 2] = r; // R
}

void drawBGRAsset(u8* framebuffer, const unsigned char* bgr_data, unsigned int bgr_size, int dst_x, int dst_y, int fb_width, int fb_height, bool flip_horizontal) {
    if (bgr_size < 4) return;
    
    // Read dimensions from header
    int width = bgr_data[0] | (bgr_data[1] << 8);
    int height = bgr_data[2] | (bgr_data[3] << 8);
    
    // Copy BGR data to framebuffer
    if (flip_horizontal) {
        for (int y = height - 1; y >= 0; y--) {
            for (int x = 0; x < width; x++) {
                int src_pos = 4 + (y * width + x) * 3;
                
                u8 b = bgr_data[src_pos];
                u8 g = bgr_data[src_pos + 1];
                u8 r = bgr_data[src_pos + 2];
                
                // Skip transparent magenta pixels
                if (b >= 0x60 && r >= 0x60 && g <= 0x90) {
                    continue;
                }
                
                int dst_x_pos = (fb_width - 1) - dst_x - x;
                int dst_y_pos = dst_y + (height - 1 - y);
                
                if (dst_x_pos >= 0 && dst_x_pos < fb_width && dst_y_pos >= 0 && dst_y_pos < fb_height) {
                    int dst_pos = (dst_y_pos * fb_width + dst_x_pos) * 3;
                    framebuffer[dst_pos] = b;
                    framebuffer[dst_pos + 1] = g;
                    framebuffer[dst_pos + 2] = r;
                }
            }
        }
    } else {
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                int src_pos = 4 + (y * width + x) * 3;
                
                // Check for transparent pixels (more tolerant magenta detection)
                u8 b = bgr_data[src_pos];
                u8 g = bgr_data[src_pos + 1];
                u8 r = bgr_data[src_pos + 2];
                
                // Treat as transparent if it's magenta-like (high red/blue, low green)
                // MAXIMUM AGGRESSIVE - catch ANY purple/magenta tint whatsoever
                if (b >= 0x60 && r >= 0x60 && g <= 0x90) {
                    continue;
                }
                
                int dst_x_pos = (fb_width - 1) - dst_x - x;
                int dst_y_pos = dst_y + y;
                
                if (dst_x_pos >= 0 && dst_x_pos < fb_width && dst_y_pos >= 0 && dst_y_pos < fb_height) {
                    int dst_pos = (dst_y_pos * fb_width + dst_x_pos) * 3;
                    framebuffer[dst_pos] = b;
                    framebuffer[dst_pos + 1] = g;
                    framebuffer[dst_pos + 2] = r;
                }
            }
        }
    }
}

void fillRectangle(u8* framebuffer, int b, int g, int r, int dst_x, int dst_y, int width, int height, int fb_width, int fb_height) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int dst_x_pos = (fb_width - 1) - dst_x - x;
            int dst_y_pos = dst_y + y;
            int dst_pos = (dst_y_pos * fb_width + dst_x_pos) * 3;
            framebuffer[dst_pos] = b;
            framebuffer[dst_pos + 1] = g;
            framebuffer[dst_pos + 2] = r;
        }
    }
}

void displayText(u8* framebuffer, const std::map<char, std::pair<const unsigned char*, unsigned int>>& fontMapData, 
                 const char* text, int start_x, int start_y, int screen_width, int screen_height) {
    int x = start_x;
    int y = start_y;
    int glyph_size = 12; // All glyphs are 12x12 pixels
    
    for (const char* c = text; *c != '\0'; c++) {
        if (*c == '\n') {
            x = start_x;
            y += glyph_size;
            continue;
        }
        
        auto it = fontMapData.find(*c);
        if (it != fontMapData.end()) {
            auto glyphData = it->second;
            drawBGRAsset(framebuffer, glyphData.first, glyphData.second, y, x, screen_width, screen_height, true);
            x += glyph_size;
        } else {
            x += glyph_size; // Skip space for missing character
        }
        
        if (x + glyph_size > screen_height) {  // Use screen_height instead of screen_width due to 3DS rotation
            x = start_x;
            y += glyph_size;
        }
    }
}

void drawBGRAssetWithColor(u8* framebuffer, const unsigned char* bgr_data, unsigned int bgr_size, int dst_y, int dst_x, int fb_width, int fb_height, u8 color_r, u8 color_g, u8 color_b) {

    // Read dimensions from header
    int width = bgr_data[0] | (bgr_data[1] << 8);
    int height = bgr_data[2] | (bgr_data[3] << 8);
    
    // Copy BGR data to framebuffer with color replacement
    for (int y = height - 1; y >= 0; y--) {
        for (int x = 0; x < width; x++) {
            int src_pos = 4 + (y * width + x) * 3;
            
            u8 b = bgr_data[src_pos];
            u8 g = bgr_data[src_pos + 1];
            u8 r = bgr_data[src_pos + 2];
            
            // Skip transparent magenta pixels
            if (b >= 0x60 && r >= 0x60 && g <= 0x90) {
                continue;
            }
            int dst_x_pos = (fb_width - 1) - dst_x - x;
            int dst_y_pos = dst_y + (height - 1 - y);
            int pos = (dst_y_pos * fb_width + dst_x_pos) * 3;
            
            // Replace with specified color
            framebuffer[pos] = color_b;     // B
            framebuffer[pos + 1] = color_g; // G
            framebuffer[pos + 2] = color_r; // R
        }
    }
}

// Draw Missingno with random colors at specified position
void drawMissingno(u8* framebuffer, int dst_x, int dst_y, int width, int height, int fb_width, int fb_height) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int dst_x_pos = (fb_width - 1) - dst_x - x;
            int dst_y_pos = dst_y + y;
            int dst_pos = (dst_y_pos * fb_width + dst_x_pos) * 3;
            
            // Generate random colors for each pixel
            uint8_t r = static_cast<uint8_t>(utils::randInt(0, 256));
            uint8_t g = static_cast<uint8_t>(utils::randInt(0, 256));
            uint8_t b = static_cast<uint8_t>(utils::randInt(0, 256));
            
            framebuffer[dst_pos] = b;
            framebuffer[dst_pos + 1] = g;
            framebuffer[dst_pos + 2] = r;
        }
    }
}

// External declarations for RED sprites
extern const unsigned char RED_0_bgr[];
extern const unsigned int RED_0_bgr_size;
extern const unsigned char RED_1_bgr[];
extern const unsigned int RED_1_bgr_size;
extern const unsigned char RED_2_bgr[];
extern const unsigned int RED_2_bgr_size;
extern const unsigned char RED_3_bgr[];
extern const unsigned int RED_3_bgr_size;
extern const unsigned char RED_4_bgr[];
extern const unsigned int RED_4_bgr_size;
extern const unsigned char RED_5_bgr[];
extern const unsigned int RED_5_bgr_size;
extern const unsigned char RED_6_bgr[];
extern const unsigned int RED_6_bgr_size;
extern const unsigned char RED_7_bgr[];
extern const unsigned int RED_7_bgr_size;
extern const unsigned char RED_8_bgr[];
extern const unsigned int RED_8_bgr_size;
extern const unsigned char RED_9_bgr[];
extern const unsigned int RED_9_bgr_size;

static Item *FAKE_ID = nullptr;
static Item *LIFT_KEY = nullptr;
static Item *AMULET_COIN = nullptr;
static Move *CUT = nullptr;
static Move *FLY = nullptr;
static Move *SURF = nullptr;
static Move *STRENGTH = nullptr;

static const int NUM_STEP_FRAMES = 8;
static const int BUMP_STEP_FRAMES = NUM_STEP_FRAMES * 2;

static bool s_inTextPrompt = false;

static const std::vector<int> HIDEOUT_Y = {0, 18, 18, 0, 14};
static const std::vector<int> SILPH_X = {0, 20, 20, 20, 20, 20, 18, 18, 18, 18, 12, 13};
static const std::vector<int> CATCH_RATES = {
    1,  45, 45, 45, 45, 45, 45, 45, 45, 45, 255, 120, 45, 255, 120, 45, 255, 120, 45, 255, 127, 255, 90, 255, 90,
    190, 75, 255, 90, 235, 120, 45, 235, 120, 45, 150, 25, 190, 75, 170, 50, 255, 90, 255, 120, 45, 190, 75, 190, 75,
    255, 50, 255, 90, 190, 75, 190, 75, 190, 75, 255, 120, 45, 200, 100, 50, 180, 90, 45, 255, 120, 45, 190, 60, 255, 120,
    45, 190, 60, 190, 75, 190, 60, 45, 190, 45, 190, 75, 190, 75, 190, 60, 190, 90, 45, 45, 190, 75, 225, 60, 190, 60, 90,
    45, 190, 75, 45, 45, 45, 190, 60, 120, 60, 30, 45, 45, 225, 75, 225, 60, 225, 60, 45, 45, 45, 45, 45, 45, 45, 255, 45,
    45, 35, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 25, 3,  3,  3,  45, 27, 9,  3,  2};

bool Gui::showingText = false;
bool Gui::battling = false;
bool Gui::canMap = true;
bool Gui::usingBattleItem = false;
bool Gui::busedItem = false;
bool Gui::rightClicked = false;
bool Gui::inMenu = false;
bool Gui::buySell = false;
bool Gui::buying = false;
bool Gui::selling = false;
bool Gui::checkingTms = false;
bool Gui::teachingMove = false;
bool Gui::replacingMove = false;
bool Gui::depWith = false;
bool Gui::depositing = false;
bool Gui::withdraw = false;
bool Gui::flying = false;
bool Gui::inside = false;
bool Gui::showingDex = false;
bool Gui::rareCandy = false;
bool Gui::surfing = false;
bool Gui::spacebar = false;

int Gui::clickedChoice = -1;
std::string Gui::currentText;
std::string Gui::currentLoc;
std::vector<Item*> Gui::usableItems;
bool Gui::usingItemList = false;
BattleState *Gui::playerState = nullptr;
BattleState *Gui::enemyState = nullptr;
Gui *Gui::instance = nullptr;

void Gui::advanceText() {
    if (showingText) {
        showingText = false;
        currentText.clear();
    }
}

void Gui::autoAdvanceBattleText() {
    if (!battling || !g_gui->autoBattle) {
        return;
    }
    if (!showingText) {
        return;
    }
    if (instance && (instance->choosingFromLongList || usingBattleItem || s_inTextPrompt)) {
        return;
    }

    static uint32_t lastAdvance = 0;
    static uint32_t frameCounter = 0;
    frameCounter++; // Use local counter instead of member frames
    if (frameCounter - lastAdvance < 6) { // 6 frames ~ 100ms at 60fps
        return;
    }
    lastAdvance = frameCounter;
    advanceText();
}

void Gui::stopMovement() {
    pressedKeys.clear();
    hasHeldDirection = false;
    stepPhase = StepPhase::NONE;
    phaseFrame = 0;
    offsetX = 0;
    offsetY = 0;
}

static int menuChoiceFromY(int y) {
    int menuBoxY = GameConfig3DS::WINDOW_HEIGHT - GameConfig3DS::MENU_BOX_HEIGHT - GameConfig3DS::MENU_BOX_MARGIN;
    int menuStartY = menuBoxY + GameConfig3DS::MENU_OPTION_START_OFFSET;
    int menuEndY = menuStartY + GameConfig3DS::MENU_OPTION_COUNT * GameConfig3DS::MENU_OPTION_HEIGHT;
    if (y < menuStartY || y >= menuEndY) {
        return -1;
    }
    return (y - menuStartY) / GameConfig3DS::MENU_OPTION_HEIGHT;
}

static int longListChoiceFromY(int y) {
    // Menu data is now on bottom screen starting at y=0, with 12 pixels per line
    if (y < 0) {
        return -1;
    }
    return y / 12; // 12 pixels per line as specified
}

static int clampPageStart(int start, int total, int pageSize) {
    if (pageSize <= 0) {
        return 0;
    }
    if (total <= pageSize) {
        return 0;
    }
    int maxStart = ((total - 1) / pageSize) * pageSize;
    start = std::max(0, std::min(start, maxStart));
    start = (start / pageSize) * pageSize;
    return start;
}

Gui::Gui(Player *player) : player(player) {
    instance = this;
    strArr = {"", "", "", ""};
    autoBattle = false;
    currentMenu = 0; // Start with Inventory
    choosingFromLongList = false;
    checkingPokes = false;
    checkingMoves = false;
    longListHeaderOverride = "";
    setup();
}

Gui::Gui(Player *player, const std::string &saveInfo)
    : player(player) {
    instance = this;
    strArr = {"", "", "", ""};
    autoBattle = false;
    currentMenu = 0; // Start with Inventory
    choosingFromLongList = false;
    checkingPokes = false;
    checkingMoves = false;
    longListHeaderOverride = "";
    applySaveInfo(saveInfo);
    setup();
}

void Gui::applySaveInfo(const std::string &saveInfo) {
    if (saveInfo.empty()) {
        return;
    }
    std::vector<std::string> parts = utils::split(saveInfo, ',');
    if (parts.size() < 10) {
        return;
    }
    mapName = parts[0];
    try {
        playerX = std::stoi(parts[1]);
        playerY = std::stoi(parts[2]);
        facing = static_cast<Gui::Direction>(std::stoi(parts[3]));
        repelSteps = std::stoi(parts[4]);
    } catch (...) {
    }
    if (parts.size() >= 6) {
        auto it = PokeMap::POKEMAPS.find(parts[5]);
        if (it != PokeMap::POKEMAPS.end()) {
            lastHeal = it->second;
        }
    }
    if (player) {
        if (parts.size() >= 7) {
            player->ballin = (parts[6] == "1");
        }
        if (parts.size() >= 8) {
            try {
                player->numCaught = std::stoi(parts[7]);
            } catch (...) {
            }
        }
        if (parts.size() >= 9) {
            try {
                player->money = std::stoi(parts[8]);
            } catch (...) {
            }
        }
        if (parts.size() >= 10) {
            player->name = parts[9];
        }
    }
}

void Gui::setup() {
    // Initialize tile data array once for all instances
    if (!tileDataInitialized) {
        tileDataArray = initTileArrayData();
        tileDataInitialized = true;
    }
    
    if (!FAKE_ID) {
        FAKE_ID = Item::ITEM_MAP["Fake ID"];
        LIFT_KEY = Item::ITEM_MAP["Lift Key"];
        AMULET_COIN = Item::ITEM_MAP["Amulet Coin"];
        CUT = Move::MOVE_MAP["Cut"];
        FLY = Move::MOVE_MAP["Fly"];
        SURF = Move::MOVE_MAP["Surf"];
        STRENGTH = Move::MOVE_MAP["Strength"];
    }
    pm = PokeMap::POKEMAPS[mapName];
    if (!lastHeal) {
        lastHeal = PokeMap::POKEMAPS["RedsHouse1F"];
    }
    
    // Skip SDL texture loading - assets are already compiled as C code
    // loadTileImages(); // REMOVED - tiles are in all_tiles_data.c
    // loadPlayerSprites(); // REMOVED - sprites are in all_sprites_complete.cpp
    loadMap(pm);
    
    // Use existing C asset data for frames
    // danceFrames - use COOLTRAINER_F sprites from ALL_SPRITES_MAP
    danceFrames.resize(10, nullptr);
    for (int i = 0; i < 10; i++) {
        auto it = ALL_SPRITES_MAP.find("COOLTRAINER_F_" + std::to_string(i));
        if (it != ALL_SPRITES_MAP.end()) {
            // Convert sprite data to SDL_Texture format for compatibility
            // For now, set to nullptr since we're not using SDL textures
            danceFrames[i] = nullptr;
        }
    }
    
    // oakFrames - use OAK sprites from ALL_SPRITES_MAP  
    oakFrames.resize(10, nullptr);
    for (int i = 0; i < 10; i++) {
        auto it = ALL_SPRITES_MAP.find("OAK_" + std::to_string(i));
        if (it != ALL_SPRITES_MAP.end()) {
            oakFrames[i] = nullptr; // Set to nullptr for 3DS compatibility
        }
    }
    
    // monsterFrames - use MONSTER sprites from ALL_SPRITES_MAP
    monsterFrames.resize(3, nullptr);
    for (int i = 0; i < 3; i++) {
        auto it = ALL_SPRITES_MAP.find("MONSTER_" + std::to_string(i));
        if (it != ALL_SPRITES_MAP.end()) {
            monsterFrames[i] = nullptr; // Set to nullptr for 3DS compatibility
        }
    }

    // Skip SDL-specific code since renderer is nullptr for 3DS
    mattNpc = PokeMap::POKEMAPS["Route24"]->npcs[0][19];
    oakNpc = PokeMap::POKEMAPS["FuchsiaCity"]->npcs[6][13];
    monsterTrainer = nullptr;
    auto it = PokeMap::POKEMAPS.find("FuchsiaCity");
    if (it != PokeMap::POKEMAPS.end() && it->second) {
        PokeMap *fm = it->second;
        if (fm->trainers.size() > 5 && fm->trainers[5].size() > 13) {
            monsterTrainer = fm->trainers[5][13];
        }
    }

    if (player) {
        for (auto &kv : PokeMap::POKEMAPS) {
            PokeMap *m = kv.second;
            if (!m) {
                continue;
            }
            for (size_t y = 0; y < m->npcs.size(); y++) {
                for (size_t x = 0; x < m->npcs[y].size(); x++) {
                    Blocker *b = dynamic_cast<Blocker *>(m->npcs[y][x]);
                    if (!b) {
                        continue;
                    }
                    bool missingItem = false;
                    if (b->item != nullptr) {
                        if (b->item->name == "Master Ball") {
                            missingItem = player->ballin;
                        } else {
                            missingItem = !player->hasItem(b->item);
                        }
                    }
                    bool missingMove = (b->move != nullptr && !player->hasMove(b->move));
                    bool missingBadge = (b->numBadges >= 0 && (b->numBadges >= static_cast<int>(player->leadersBeaten.size()) ||
                                                             !player->leadersBeaten[b->numBadges]));

                    if (!missingItem && !missingMove && !missingBadge) {
                        m->npcs[y][x] = nullptr;
                        delete b;
                    }
                }
            }
        }
    }
}
void Gui::renderBattle() {
    if (battling && playerState && enemyState) {
        // 3DS Battle rendering
        // Get top screen framebuffer
        u16 topWidth, topHeight;
        u8* top_fb = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &topWidth, &topHeight);
        auto drawFilledRainbowStar = [&](int cx, int cy, int outerRadius, int points) {
            if (points < 2) return;
            int innerRadius = std::max(1, outerRadius * 2 / 5);
            
            // Draw rainbow colored lines from center to star points
            for (int i = 0; i < points * 2; i++) {
                double angle = (static_cast<double>(i) * M_PI / static_cast<double>(points)) - (M_PI / 2.0);
                int r = (i % 2 == 0) ? outerRadius : innerRadius;
                int x = cx + static_cast<int>(std::round(std::cos(angle) * r));
                int y = cy + static_cast<int>(std::round(std::sin(angle) * r));
                
                // Calculate rainbow color
                float t = static_cast<float>(i) / static_cast<float>(points * 2);
                float h = 360.0f * t;
                float s = 1.0f;
                float v = 1.0f;
                
                // HSV to RGB conversion
                h = std::fmod(h, 360.0f);
                if (h < 0) h += 360.0f;
                float c = v * s;
                float x_val = c * (1.0f - std::fabs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
                float m = v - c;
                float r_val = 0.0f, g_val = 0.0f, b_val = 0.0f;
                
                if (h < 60.0f) { r_val = c; g_val = x_val; }
                else if (h < 120.0f) { r_val = x_val; g_val = c; }
                else if (h < 180.0f) { g_val = c; b_val = x_val; }
                else if (h < 240.0f) { g_val = x_val; b_val = c; }
                else if (h < 300.0f) { r_val = x_val; b_val = c; }
                else { r_val = c; b_val = x_val; }
                
                // Draw line from center to point
                for (int j = 0; j < r; j++) {
                    float t_line = static_cast<float>(j) / static_cast<float>(r);
                    int px = cx + static_cast<int>((x - cx) * t_line);
                    int py = cy + static_cast<int>((y - cy) * t_line);
                    
                    u8 red = static_cast<u8>(std::round((r_val + m) * 255.0f));
                    u8 green = static_cast<u8>(std::round((g_val + m) * 255.0f));
                    u8 blue = static_cast<u8>(std::round((b_val + m) * 255.0f));
                    
                    setPixel(top_fb, px, py, red, green, blue, topWidth);
                }
            }
        };
        
        auto drawBattlerPanel = [&](int x, int y, Battler *b) {
            fillRectangle(top_fb, 255, 255, 255, y, x, 120, 180, topWidth, topHeight);
            // drawRectOutline(top_fb, x, y, y + 124, x + 150, 0, 0, 0, topWidth);
            // Draw nickname (rainbow if shiny)
            if (b->shiny) {
                //TODO: verify
                drawRainbowText(top_fb, fontMapData, b->nickname, x + 10, y + 8, topWidth, topHeight);
            } else {
                // Use external displayText function for white text
                extern void displayText(u8* framebuffer, const std::map<char, std::pair<const unsigned char*, unsigned int>>& fontMapData, const char* text, int x, int y, int screen_width, int screen_height);
                extern std::map<char, std::pair<const unsigned char*, unsigned int>> fontMapData;
                displayText(top_fb, fontMapData, b->nickname.c_str(), x + 10, y + 8, topWidth, topHeight);
            }
            
            // Draw level
            std::string levelText = "Level " + std::to_string(b->level);
            displayText(top_fb, fontMapData, levelText.c_str(), x + 10, y + 32, topWidth, topHeight);
            
            // Draw status if not empty
            if (!b->status.empty()) {
                displayText(top_fb, fontMapData, b->status.c_str(), x + 10, y + 52, topWidth, topHeight);
            }
            
            // Draw HP
            std::string hpText = "HP: " + std::to_string(b->hp) + "/" + std::to_string(b->mhp);
            displayText(top_fb, fontMapData, hpText.c_str(), x + 10, y + 69, topWidth, topHeight);
            
            // Draw HP bar
            int percent = b->mhp <= 0 ? 0 : static_cast<int>((b->hp / static_cast<float>(b->mhp)) * 100.0f);
            percent = std::max(0, std::min(100, percent));
            
            // Bar background - using fillRectangle with working y, x, height, width pattern
            fillRectangle(top_fb, 60, 60, 60, y + 96, x + 10, 16, 150, topWidth, topHeight);
            
            // Bar foreground (color based on HP percentage)
            u8 barR, barG, barB;
            if (percent <= 33) { barR = 200; barG = 0; barB = 0; }
            else if (percent <= 66) { barR = 200; barG = 200; barB = 0; }
            else { barR = 0; barG = 200; barB = 0; }
            fillRectangle(top_fb, barB, barG, barR, y + 96, x + 10, 16, 150 * percent / 100, topWidth, topHeight);
        };
        
        // Draw battler panels
        drawBattlerPanel(0, 20, playerState->monster);
        drawBattlerPanel(220, 20, enemyState->monster);
        //fillRectangle(top_fb, 40, 40, 40, 200, 0, 40, 400, topWidth, topHeight);
        
        // Draw battler sprites
        int pX = 64;
        int eX = GameConfig3DS::TOP_SCREEN_WIDTH - 154;
        int y = 128;
        int pdn=playerState->monster->dexNum;
        int edn=enemyState->monster->dexNum;
        // Player battler
        if(pdn==0)
            drawMissingno(top_fb, y, pX, 96, 96, topWidth, topHeight);
        else
            drawBGRAsset(top_fb, battlers_bgr[pdn], battlers_bgr_sizes[pdn], y, pX, topWidth, topHeight, false);
        if (playerState->monster->shiny) {
            drawFilledRainbowStar(pX + 32, y + 32, 44, 5);
            drawFilledRainbowStar(pX + 96, y + 32, 36, 5);
        }
        
        // Enemy battler
        if(edn==0)
            drawMissingno(top_fb, y, eX, 96, 96, topWidth, topHeight);
        else
            drawBGRAsset(top_fb, battlers_bgr[edn], battlers_bgr_sizes[edn], y, eX, topWidth, topHeight, true);
        if (enemyState->monster->shiny) {
            drawFilledRainbowStar(eX + 96, y + 32, 44, 5);
            drawFilledRainbowStar(eX + 160, y + 32, 36, 5);
        }
    }
}
void Gui::print(const std::string &s) {
    // Call displayText (defined in main_3ds) at 0,0 on the bottom screen
    u16 bottomWidth, bottomHeight;
    u8* bottom_fb = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &bottomWidth, &bottomHeight);
    
    // Clear bottom screen
    memset(bottom_fb, 255, bottomWidth * bottomHeight * 3);
    
    // Display the text
    displayText(bottom_fb, fontMapData, s.c_str(), 0, 0, bottomWidth, bottomHeight);
    renderBattle();
    // Swap/whatever once, only need to render once no need in the loop
    gfxFlushBuffers();
    gfxSwapBuffers();
    
    while(true) {
        // Listen for A, B, X, Y, L button, R button, or tap anywhere on screen, break from the loop
        hidScanInput();
        u32 kDown = hidKeysDown();
        
        if (kDown & (KEY_A | KEY_B | KEY_X | KEY_Y | KEY_L | KEY_R | KEY_TOUCH)) {
            break;
        }
        
        // Vsync
        gspWaitForVBlank();
        
        // If battling && autobattling && frames%3==0, break
        static int localFrames = 0;
        if (battling && autoBattle && (localFrames++ % 3 == 0)) {
            break;
        }
    }
}

std::string Gui::promptText(const std::string &prompt) {
    std::string input = "";
    char lastChar = 'A';
    bool uppercase = true;
    
    while (aptMainLoop()) {
        // Clear screens
        u16 topWidth, topHeight;
        u8* top_fb = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &topWidth, &topHeight);
        memset(top_fb, 255, topWidth * topHeight * 3);
        
        u16 bottomWidth, bottomHeight;
        u8* bottom_fb = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &bottomWidth, &bottomHeight);
        memset(bottom_fb, 255, bottomWidth * bottomHeight * 3);
        
        // Build top screen text: prompt + instructions + input
        std::string topText = prompt + "\n\nSELECT: Shift case\nSTART: Confirm\nB: Backspace\nA: Repeat last\n\nInput: " + input;
        
        // Draw all top screen text at once
        displayText(top_fb, fontMapData, topText.c_str(), 10, 10, topWidth, topHeight);
        
        // Build keyboard string for bottom screen with formatting
        const char* keyboard = uppercase ? 
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%^&*()_+{}|:<>?[]\\;'\",./" :
            "abcdefghijklmnopqrstuvwxyz1234567890-=~`";
        
        // Format keyboard with spaces and newlines every 10 characters
        std::string keyboardText = "";
        for (size_t i = 0; i < strlen(keyboard); i++) {
            keyboardText += keyboard[i];
            keyboardText += " "; // Add space between characters
            if ((i + 1) % 10 == 0) {
                keyboardText += "\n"; // Newline every 10 characters
            }
        }
        
        // Draw keyboard on bottom screen
        displayText(bottom_fb, fontMapData, keyboardText.c_str(), 10, 20, bottomWidth, bottomHeight);
        
        // Swap buffers
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
        
        // Handle input
        hidScanInput();
        u32 kDown = hidKeysDown();
        
        if (kDown & KEY_START) {
            break;  // Return current input
        }
        else if (kDown & KEY_SELECT) {
            uppercase = !uppercase;  // Toggle case
        }
        else if (kDown & KEY_B) {
            if (!input.empty()) {
                input.pop_back();  // Backspace
                lastChar = input.empty() ? 'A' : input.back();
            }
        }
        else if (kDown & KEY_A) {
            if (input.length() < 10) {
                input += lastChar;  // Repeat last character or A if none
            }
        }
        else if (kDown & KEY_TOUCH) {
            touchPosition touch;
            hidTouchRead(&touch);
            
            // Calculate which key was touched
            // Each character takes 24 pixels (12 for char + 12 for space)
            // 10 characters per line = 240 pixels total
            // Line height is 14 pixels (12 glyph + 2 spacing)
            int charsPerLine = 10;
            int charWidth = 24; // 12 for character + 12 for space
            int charHeight = 14; // 12 glyph height + 2 line spacing
            
            int line = (touch.py - 20) / charHeight;
            int col = touch.px / charWidth;
            size_t keyIndex = line * charsPerLine + col;
            
            if (keyIndex < strlen(keyboard) && input.length() < 10) {
                input += keyboard[keyIndex];
                lastChar = keyboard[keyIndex];
            }
        }
    }
    
    return input;
}

std::string Gui::promptNumber(const std::string &prompt) {
    std::string input = "";
    char lastChar = '0';
    
    while (aptMainLoop()) {
        // Clear screens
        u16 topWidth, topHeight;
        u8* top_fb = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &topWidth, &topHeight);
        memset(top_fb, 255, topWidth * topHeight * 3);
        
        u16 bottomWidth, bottomHeight;
        u8* bottom_fb = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &bottomWidth, &bottomHeight);
        memset(bottom_fb, 255, bottomWidth * bottomHeight * 3);
        
        // Build top screen text: prompt + instructions + input
        std::string topText = prompt + "\n\nSTART: Confirm\nB: Backspace\nA: Repeat last\n\nInput: " + input;
        
        // Draw all top screen text at once
        displayText(top_fb, fontMapData, topText.c_str(), 10, 10, topWidth, topHeight);
        
        // Build number pad for bottom screen with formatting
        const char* numpad = "1234567890";
        
        // Format numpad with spaces and newlines every 5 characters
        std::string numpadText = "";
        for (size_t i = 0; i < strlen(numpad); i++) {
            numpadText += numpad[i];
            numpadText += " "; // Add space between characters
            if ((i + 1) % 5 == 0) {
                numpadText += "\n"; // Newline every 5 characters
            }
        }
        
        // Draw numpad on bottom screen
        displayText(bottom_fb, fontMapData, numpadText.c_str(), 10, 20, bottomWidth, bottomHeight);
        
        // Swap buffers
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
        
        // Handle input
        hidScanInput();
        u32 kDown = hidKeysDown();
        
        if (kDown & KEY_START) {
            break;  // Return current input
        }
        else if (kDown & KEY_B) {
            if (!input.empty()) {
                input.pop_back();  // Backspace
                lastChar = input.empty() ? '0' : input.back();
            }
        }
        else if (kDown & KEY_A) {
            if (input.length() < 10) {
                input += lastChar;  // Repeat last character or 0 if none
            }
        }
        else if (kDown & KEY_TOUCH) {
            touchPosition touch;
            hidTouchRead(&touch);
            
            // Calculate which key was touched
            // Each character takes 24 pixels (12 for char + 12 for space)
            // 5 characters per line = 120 pixels total
            // Line height is 14 pixels (12 glyph + 2 spacing)
            int charsPerLine = 5;
            int charWidth = 24; // 12 for character + 12 for space
            int charHeight = 14; // 12 glyph height + 2 line spacing
            
            int line = (touch.py - 20) / charHeight;
            int col = touch.px / charWidth;
            size_t keyIndex = line * charsPerLine + col;
            
            if (keyIndex < strlen(numpad) && input.length() < 10) {
                input += numpad[keyIndex];
                lastChar = numpad[keyIndex];
            }
        }
    }
    
    return input;
}

int Gui::waitForChoice(int maxChoice) {
    renderBattle();
    u16 topWidth, topHeight;
    u8* top_fb = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &topWidth, &topHeight);
    
    // Render header on top screen if there's an override
    if (!longListHeaderOverride.empty()) {
        int headerX = 0;
        int headerY = 200;
        int headerWidth = 400;
        int headerHeight = 40;
        fillRectangle(top_fb, 255, 255, 255, headerY, headerX, headerHeight, headerWidth, topWidth, topWidth);
        displayText(top_fb, fontMapData, longListHeaderOverride.c_str(), 
                   headerX + 10, headerY + 12,
                   topWidth, topHeight);
    }
    
    u16 bottomWidth, bottomHeight;
    u8* bottom_fb = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &bottomWidth, &bottomHeight);
    memset(bottom_fb, 255, bottomWidth * bottomHeight * 3);
    
    // Build choice text from longArr
    std::string choiceText = "";
    for (size_t i = 0; i < longArr.size() && i < 20; i++) { // Limit to 20 options to fit screen
        if (!longArr[i].empty()) {
            if (!choiceText.empty()) choiceText += "\n";
            choiceText += longArr[i];
        }
    }
    
    displayText(bottom_fb, fontMapData, choiceText.c_str(), 0, 0, bottomWidth, bottomHeight);
    gfxFlushBuffers();
    gfxSwapBuffers();
    while (true) {
        // Scan 3DS input
        hidScanInput();
        u32 kDown = hidKeysDown();
        
        // Check if they pressed A - return 0
        if (kDown & KEY_A) {
            return 0;
        }
        
        // Check if they pressed B - return -1
        if (kDown & KEY_B) {
            return -1;
        }
        
        // Check if they tapped - divide the y they tapped by 12, that's the choice
        if (kDown & KEY_TOUCH) {
            touchPosition touch;
            hidTouchRead(&touch);
            int choice = touch.py / 12;
            // Validate choice is within range
            if (choice >= 0 && choice <= maxChoice) {
                return choice;
            }
        }
        
        // Vsync so the frame passes
        gspWaitForVBlank();
    }
}

void Gui::setBattleStates(BattleState *a, BattleState *b) {
    playerState = a;
    enemyState = b;
    battling = true;
}

void Gui::runGui() {
    // Placeholder for future CLI launch behavior.
}

void Gui::loadMap(PokeMap *map) {
    if (!map) {
        return;
    }
    pm = map;
    currentMap = pm->grid;
    tileTypes = pm->types;
    connections[0] = pm->north;
    connOffsets[0] = pm->nOff;
    connections[1] = pm->south;
    connOffsets[1] = pm->sOff;
    connections[2] = pm->west;
    connOffsets[2] = pm->wOff;
    connections[3] = pm->east;
    connOffsets[3] = pm->eOff;
}

        void Gui::closeMenus() {
    inMenu = false;
    buySell = false;
    buying = false;
    selling = false;
    checkingPokes = false;
    checkingMoves = false;
    checkingTms = false;
    teachingMove = false;
    replacingMove = false;
    longListHeaderOverride.clear();
    depWith = false;
    depositing = false;
    withdraw = false;
    choosingFromLongList = false;
    showingDex = false;
    usingItemList = false;
}

void Gui::clickMouse(int mouseX, int mouseY, bool rightClick) {
    if (showingText) {
        advanceText();
        return;
    }

    // Handle flying map touch input
    if (flying && canMap && !showingText && !battling && !choosingFromLongList && !buySell && !blackjackActive) {
        if (rightClick) {
            flying = false;
            inMenu = false;
            inside = false;
            currentLoc.clear();
            return;
        }
        
        // Map is centered: mapX = 44, mapY = 4, map size = 231x231, cell size = 21x21
        int mapX = (320 - 231) / 2;  // 44
        int mapY = (240 - 231) / 2;  // 4
        int cellW = 21;  // 231 / 11 = 21 pixels per cell
        int cellH = 21;  // 231 / 11 = 21 pixels per cell
        
        // Check if click is within map bounds
        if (mouseX >= mapX && mouseX < mapX + 231 && mouseY >= mapY && mouseY < mapY + 231) {
            // Convert to map coordinates
            int mapLocalX = mouseX - mapX;
            int mapLocalY = mouseY - mapY;
            
            // Convert to grid coordinates (11x11 grid)
            int j = mapLocalX / cellW;
            int i = mapLocalY / cellH;
            
            if (i >= 0 && i < 11 && j >= 0 && j < 11) {
                std::string n = FlyLocation::NAME_MAP[i][j];
                if (n.empty()) {
                    return;
                }
                std::string p = printable(n);
                if (inside || FlyLocation::isRed(i, j)) {
                    currentLoc = p;
                    return;
                }
                auto it = FlyLocation::FLY_LOCATIONS.find(n);
                if (it == FlyLocation::FLY_LOCATIONS.end() || !it->second || !it->second->visited) {
                    return;
                }
                FlyLocation *f = it->second;
                loadMap(f->dest);
                playerX = f->x;
                playerY = f->y;
                offsetX = 0;
                offsetY = 0;
                inMenu = false;
                flying = false;
                return;
            }
        }
    }

    if (blackjackShowingResults) {
        longArr.clear();
        choosingFromLongList = false;
        blackjackShowingResults = false;
        inMenu = false;
        buySell = false;
        buying = false;
        selling = false;
        closeMenus();
        clickedChoice = 0;
        return;
    }

    if (blackjackActive) {
        if (blackjackFinished) {
            return;
        }

        static std::mt19937 s_blackjackRng(std::random_device{}());

        auto cardValue = [](int c) {
            int r = c % 13;
            if (r == 0) {
                return 11;
            }
            if (r >= 9) {
                return 10;
            }
            return r + 1;
        };
        auto handValue = [&](const std::vector<int> &hand) {
            int sum = 0;
            int aces = 0;
            for (int c : hand) {
                int v = cardValue(c);
                sum += v;
                if (c % 13 == 0) {
                    aces++;
                }
            }
            while (sum > 21 && aces-- > 0) {
                sum -= 10;
            }
            return sum;
        };
        auto isSoft = [&](const std::vector<int> &hand) {
            int sum = 0;
            int aces = 0;
            for (int c : hand) {
                sum += cardValue(c);
                if (c % 13 == 0) {
                    aces++;
                }
            }
            return aces > 0 && sum <= 21;
        };
        auto dealerShouldHit = [&](const std::vector<int> &hand) {
            int v = handValue(hand);
            return v < 17 || (v == 17 && isSoft(hand));
        };
        auto drawCard = [&]() {
            if (blackjackDeck.empty()) {
                blackjackDeck.clear();
                blackjackDeck.reserve(52);
                for (int i = 0; i < 52; i++) {
                    blackjackDeck.push_back(i);
                }
                std::shuffle(blackjackDeck.begin(), blackjackDeck.end(), s_blackjackRng);
            }
            int c = blackjackDeck.back();
            blackjackDeck.pop_back();
            return c;
        };

        if (!rightClick) {
            blackjackPlayer.push_back(drawCard());
            if (handValue(blackjackPlayer) > 21) {
                blackjackFinished = true;
                blackjackPlayerWon = false;
            }
        } else {
            blackjackHideFirst = false;
            while (dealerShouldHit(blackjackDealer)) {
                blackjackDealer.push_back(drawCard());
            }
            int p = handValue(blackjackPlayer);
            int d = handValue(blackjackDealer);
            if (p > 21) {
                blackjackFinished = true;
                blackjackPlayerWon = false;
            } else if (d > 21) {
                blackjackFinished = true;
                blackjackPlayerWon = true;
            } else if (p > d) {
                blackjackFinished = true;
                blackjackPlayerWon = true;
            } else if (p < d) {
                blackjackFinished = true;
                blackjackPlayerWon = false;
            } else {
                blackjackTieFrames = 60;
            }
        }

        if (blackjackFinished) {
            blackjackHideFirst = false;

            auto fmtCard = [](int c) {
                static const char *ranks[] = {"A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K"};
                static const char *suits[] = {"H", "D", "C", "S"};
                int r = c % 13;
                int s = c / 13;
                std::string out = ranks[r];
                out += suits[s];
                return out;
            };
            auto fmtHand = [&](const std::vector<int> &hand) {
                std::string out;
                for (size_t i = 0; i < hand.size(); i++) {
                    if (!out.empty()) {
                        out += " ";
                    }
                    out += fmtCard(hand[i]);
                }
                return out;
            };

            int p = handValue(blackjackPlayer);
            int d = handValue(blackjackDealer);

            std::string result;
            result += blackjackPlayerWon ? "YOU WIN!\n\n" : "YOU LOSE\n\n";
            result += "Dealer: " + fmtHand(blackjackDealer) + " (" + std::to_string(d) + ")\n";
            result += "Player: " + fmtHand(blackjackPlayer) + " (" + std::to_string(p) + ")\n\n";

            std::string moneyLine;

            if (player) {
                if (blackjackPlayerWon) {
                    player->money += blackjackBet;
                    moneyLine = "You won $" + std::to_string(blackjackBet) + "!";
                } else {
                    player->money -= blackjackBet;
                    moneyLine = "You lost $" + std::to_string(blackjackBet) + "!";
                }
                bet = std::max(1, blackjackBet * 2);
            }

            longArr.clear();
            longArr.push_back(blackjackPlayerWon ? "YOU WIN!" : "YOU LOSE");
            longArr.push_back("");
            longArr.push_back("Dealer:");
            longArr.push_back(fmtHand(blackjackDealer) + " (" + std::to_string(d) + ")");
            longArr.push_back("");
            longArr.push_back("Player:");
            longArr.push_back(fmtHand(blackjackPlayer) + " (" + std::to_string(p) + ")");
            longArr.push_back("");
            if (!moneyLine.empty()) {
                longArr.push_back(moneyLine);
                longArr.push_back("");
            }
            psi = 0;
            choosingFromLongList = true;
            showingText = false;
            blackjackShowingResults = true;
            blackjackActive = false;
            blackjackBet = 0;
        }
        return;
    }

    if (rightClick) {
        if (battling) {
            if (usingBattleItem || usingItemList) {
                usedItem = nullptr;
                chosenMon = nullptr;
                tm = nullptr;
                tms.clear();
                longArr.clear();
                usableItems.clear();
                choosingFromLongList = false;
                usingItemList = false;
                busedItem = false;
                usingBattleItem = false;
                inMenu = false;
                checkingPokes = false;
                checkingMoves = false;
                checkingTms = false;
                teachingMove = false;
                replacingMove = false;
                closeMenus();
                advanceText();
                rightClicked = true;
                clickedChoice = 0;
                return;
            }

            if (!choosingFromLongList) {
                clickedChoice = 3;
                return;
            }

            rightClicked = true;
            clickedChoice = 0;
            return;
        }

        if (buySell || buying || selling) {
            canMap = true;
        }
        usedItem = nullptr;
        chosenMon = nullptr;
        tm = nullptr;
        tms.clear();
        longArr.clear();
        choosingFromLongList = false;
        usingItemList = false;
        busedItem = false;
        usingBattleItem = false;
        inMenu = false;
        buySell = false;
        buying = false;
        selling = false;
        checkingPokes = false;
        checkingMoves = false;
        checkingTms = false;
        teachingMove = false;
        replacingMove = false;
        closeMenus();
        advanceText();
        rightClicked = true;
        clickedChoice = 0;
        return;
    }
    if (buySell) {
        int choice = longListChoiceFromY(mouseY);
        if (choice == 0) {
            inMenu = true;
            buying = true;
            buySell = false;
            canMap = false;
            choosingFromLongList = true;
            //usableItems.clear();
            longArr.clear();
            for (MartItem &mi : martItems) {
                //if (mi.item) {
                    //usableItems.push_back(mi.item);
                    longArr.push_back(mi.toString());
            }
            psi = 0;
            clickedChoice = -1;
            return;
        } else if (choice == 1) {
            selling = true;
            buySell = false;
            canMap = false;
            choosingFromLongList = true;
            usableItems.clear();
            longArr.clear();
            for (Item *si : player->items) {
                if (si && si->price > 0) {
                    usableItems.push_back(si);
                    longArr.push_back(si->name + " x" + std::to_string(si->quantity) + "   $" + std::to_string(si->price / 2));
                }
            }
            psi = 0;
            clickedChoice = -1;
            return;
        }
    }
    if (buying && choosingFromLongList) {
        int lineIdx = longListChoiceFromY(mouseY);
        if (lineIdx < 0) {
            return;
        }
        int idx = psi + lineIdx;
        if (idx < 0 || idx >= static_cast<int>(martItems.size())) {
            return;
        }
        const MartItem &m = martItems[idx];
        if (m.price <= 0) {
            return;
        }

        if (m.move) {
            if (player && player->hasMove(m.move)) {
                print("You already have that TM!");
            } else {
                if (!player || player->money < m.price) {
                    print("You can't afford that!");
                    buying = true;
                    buySell = false;
                    canMap = false;
                    choosingFromLongList = true;
                    inMenu = true;
                    usableItems.clear();
                    longArr.clear();
                    for (const MartItem &mi : martItems) {
                        longArr.push_back(mi.toString());
                    }
                    psi = 0;
                    return;
                }
                player->money -= m.price;
                player->give(m.move);
                print("Thank you!");
            }
        } else {
            std::string qStr = promptNumber("How many?");
            int q = 0;
            try {
                long long parsed = std::stoll(qStr);
                if (parsed <= 0 || parsed > std::numeric_limits<int>::max()) {
                    print("Keep those obscenities to yourself, boy!");
                    buying = true;
                    buySell = false;
                    canMap = false;
                    choosingFromLongList = true;
                    inMenu = true;
                    usableItems.clear();
                    longArr.clear();
                    for (const MartItem &mi : martItems) {
                        longArr.push_back(mi.toString());
                    }
                    psi = 0;
                    return;
                }
                q = static_cast<int>(parsed);
            } catch (...) {
                print("Keep those obscenities to yourself, boy!");
                buying = true;
                buySell = false;
                canMap = false;
                choosingFromLongList = true;
                inMenu = true;
                usableItems.clear();
                longArr.clear();
                for (const MartItem &mi : martItems) {
                    longArr.push_back(mi.toString());
                }
                strArr[0] = "Money: $" + std::to_string(player ? player->money : 0);
                psi = 0;
                return;
            }
            long long cost = static_cast<long long>(m.price) * q;
            if (!player || player->money < cost) {
                print("You can't afford that!");
                buying = true;
                buySell = false;
                canMap = false;
                choosingFromLongList = true;
                inMenu = true;
                usableItems.clear();
                longArr.clear();
                for (const MartItem &mi : martItems) {
                    longArr.push_back(mi.toString());
                }
                strArr[0] = "Money: $" + std::to_string(player ? player->money : 0);
                psi = 0;
                return;
            }
            player->money -= static_cast<int>(cost);
            if (m.item) {
                player->give(m.item, q);
            } else if (m.mon) {
                for (int i = 0; i < q; i++) {
                    player->give(new Battler(5, m.mon));
                }
            }
            print("Thank you!");
        }
        buying = false;
        buySell = true;
        canMap = false;
        choosingFromLongList = true;
        inMenu = true;
        longArr.clear();
        longArr.push_back("Buy");
        longArr.push_back("Sell");
        return;
    }
    if (selling && choosingFromLongList) {
        int lineIdx = longListChoiceFromY(mouseY);
        if (lineIdx < 0) {
            return;
        }
        int idx = psi + lineIdx;
        if (idx < 0 || idx >= static_cast<int>(usableItems.size())) {
            return;
        }
        Item *it = usableItems[idx];
        if (!it) {
            return;
        }
        std::string qStr = promptNumber("How many?");
        int q = 0;
        try {
            long long parsed = std::stoll(qStr);
            if (parsed <= 0 || parsed > std::numeric_limits<int>::max()) {
                print("Keep those obscenities to yourself, boy!");
                selling = true;
                buySell = false;
                canMap = false;
                choosingFromLongList = true;
                inMenu = true;
                usableItems.clear();
                longArr.clear();
                for (Item *si : player->items) {
                    if (si && si->price > 0) {
                        usableItems.push_back(si);
                        longArr.push_back(si->name + " x" + std::to_string(si->quantity) + "   $" + std::to_string(si->price / 2));
                    }
                }
                strArr[0] = "Sell";
                psi = 0;
                return;
            }
            q = static_cast<int>(parsed);
        } catch (...) {
            print("Keep those obscenities to yourself, boy!");
            selling = true;
            buySell = false;
            canMap = false;
            choosingFromLongList = true;
            inMenu = true;
            usableItems.clear();
            longArr.clear();
            for (Item *si : player->items) {
                if (si && si->price > 0) {
                    usableItems.push_back(si);
                    longArr.push_back(si->name + " x" + std::to_string(si->quantity) + "   $" + std::to_string(si->price / 2));
                }
            }
            strArr[0] = "Sell";
            psi = 0;
            return;
        }
        if (it->quantity <= 0) {
            return;
        }
        q = std::min(q, it->quantity);
        player->sell(it, q);
        print("Thank you!");
        selling = false;
        buySell = true;
        canMap = false;
        choosingFromLongList = true;
        inMenu = true;
        longArr.clear();
        longArr.push_back("Buy");
        longArr.push_back("Sell");
        return;
    }
    if (flying) {
        if (rightClick) {
            flying = false;
            inMenu = false;
            inside = false;
            currentLoc.clear();
            return;
        }
        if (mouseY < GameConfig3DS::WORLD_HEIGHT) {
            return;
        }
        int mapX = mouseX;
        int mapY = mouseY - GameConfig3DS::WORLD_HEIGHT;
        int cellW = GameConfig3DS::BOTTOM_SCREEN_WIDTH / 11;
        int cellH = GameConfig3DS::UI_HEIGHT / 11;
        int j = mapX / std::max(1, cellW);
        int i = mapY / std::max(1, cellH);
        if (i < 0 || j < 0 || i >= 11 || j >= 11) {
            return;
        }
        std::string n = FlyLocation::NAME_MAP[i][j];
        if (n.empty()) {
            return;
        }
        std::string p = printable(n);
        if (inside || FlyLocation::isRed(i, j)) {
            currentLoc = p;
            return;
        }
        auto it = FlyLocation::FLY_LOCATIONS.find(n);
        if (it == FlyLocation::FLY_LOCATIONS.end() || !it->second || !it->second->visited) {
            return;
        }
        FlyLocation *f = it->second;
        loadMap(f->dest);
        playerX = f->x;
        playerY = f->y;
        offsetX = 0;
        offsetY = 0;
        inMenu = false;
        flying = false;
        return;
    }
    if (showingText) {
        advanceText();
        return;
    }

    if (depWith) {
        int choice = longListChoiceFromY(mouseY);
        if (choice == 0) {
            depositing = true;
            depWith = false;
            longArr = Battler::partyStrings(player->team);
            choosingFromLongList = true;
            inMenu = true;
            psi = 0;
            return;
        }
        if (choice == 1) {
            withdraw = true;
            depWith = false;
            choosingFromLongList = true;
            inMenu = true;
            psi = 0;
            boxNum = 1;
            longArr.clear();
            for (int i = 0; i < 20; i++) {
                int ind = psi + i;
                longArr.push_back((player && ind < static_cast<int>(player->pc.size()) && player->pc[ind]) ? player->pc[ind]->toString() : "");
            }
            return;
        }
    }

    if (depositing) {
        int lineIdx = longListChoiceFromY(mouseY);
        if (lineIdx < 0) {
            return;
        }
        int idx = psi + lineIdx;
        if (!player || idx < 0 || idx >= static_cast<int>(player->team.size()) || player->team[idx] == nullptr) {
            return;
        }
        if (player->team.size() > 1 && player->team[1] == nullptr) {
            print("No, you'll never survive in this world without pokemon!");
            depositing = false;
            withdraw = false;
            depWith = true;
            inMenu = true;
            choosingFromLongList = true;
            psi = 0;
            longArr.clear();
            longArr.push_back("Deposit");
            longArr.push_back("Withdraw");
            return;
        }
        print("You deposited " + player->team[idx]->nickname + "!");
        player->pc.push_back(player->team[idx]);
        for (size_t i = static_cast<size_t>(idx) + 1; i < player->team.size(); i++) {
            player->team[i - 1] = player->team[i];
        }
        if (!player->team.empty()) {
            player->team[player->team.size() - 1] = nullptr;
        }
        longArr = Battler::partyStrings(player->team);
        inMenu = true;
        choosingFromLongList = true;
        return;
    }

    if (withdraw) {
        int lineIdx = longListChoiceFromY(mouseY);
        if (lineIdx < 0 || lineIdx > 19) {
            return;
        }
        int ind = psi + lineIdx;
        if (!player || ind < 0 || ind >= static_cast<int>(player->pc.size())) {
            return;
        }
        for (size_t i = 1; i < player->team.size(); i++) {
            if (player->team[i] == nullptr) {
                player->team[i] = player->pc[ind];
                player->pc.erase(player->pc.begin() + ind);
                std::string nick = player->team[i]->nickname;
                print("You withdrew " + nick + "!");
                inMenu = true;
                withdraw = true;
                depositing = false;
                depWith = false;
                choosingFromLongList = true;
                usingItemList = false;
                psi = clampPageStart(psi, static_cast<int>(player->pc.size()), 20);
                longArr.clear();
                for (int j = 0; j < 20; j++) {
                    int refillInd = psi + j;
                    longArr.push_back((refillInd < static_cast<int>(player->pc.size()) && player->pc[refillInd]) ? player->pc[refillInd]->toString() : "");
                }
                return;
            }
        }
    }

    if (checkingPokes && choosingFromLongList) {
        int lineIdx = longListChoiceFromY(mouseY);
        if (lineIdx < 0) {
            return;
        }
        int idx = psi + lineIdx;
        if (!player || idx < 0 || idx >= static_cast<int>(player->team.size())) {
            return;
        }
        if (player->team[idx] != nullptr) {
            clickedChoice = idx;
            longArr = player->team[idx]->allInformation();
            checkingPokes = false;
            checkingMoves = true;
        }
        return;
    }

    if (checkingMoves) {
        longArr = Battler::partyStrings(player->team);
        checkingPokes = true;
        checkingMoves = false;
        return;
    }

    if (checkingTms) {
        int lineIdx = longListChoiceFromY(mouseY);
        if (lineIdx < 0) {
            return;
        }
        int idx = psi + lineIdx;
        if (idx < 0 || idx >= static_cast<int>(tms.size())) {
            return;
        }
        tm = tms[idx];
        if (!tm) {
            return;
        }
        checkingTms = false;
        teachingMove = true;
        longArr = Battler::partyStrings(player->team);
        return;
    }

    if (teachingMove) {
        int lineIdx = longListChoiceFromY(mouseY);
        if (lineIdx < 0) {
            return;
        }
        int idx = psi + lineIdx;
        if (!player || idx < 0 || idx >= static_cast<int>(player->team.size()) || player->team[idx] == nullptr) {
            return;
        }
        chosenMon = player->team[idx];
        if (!chosenMon || !tm) {
            return;
        }

        bool learnable = false;
        if (chosenMon->dexNum >= 0 && chosenMon->dexNum < static_cast<int>(Monster::MONSTERS.size())) {
            const Monster &mon = Monster::MONSTERS[chosenMon->dexNum];
            learnable = std::find(mon.learnable.begin(), mon.learnable.end(), tm) != mon.learnable.end();
        }
        if (!learnable) {
            print(chosenMon->nickname + " cannot learn " + tm->name + ".");
            chosenMon = nullptr;
            return;
        }
        for (size_t i = 0; i < chosenMon->moves.size(); i++) {
            if (chosenMon->moves[i] == nullptr) {
                chosenMon->moves[i] = tm;
                chosenMon->pp[i] = tm->pp;
                chosenMon->mpp[i] = tm->pp;
                print(chosenMon->nickname + " learned " + tm->name + "!");
                chosenMon = nullptr;
                tm = nullptr;
                longArr.clear();
                choosingFromLongList = false;
                psi = 0;
                closeMenus();
                return;
            }
            if (chosenMon->moves[i] == tm) {
                print(chosenMon->nickname + " already knows " + tm->name + "!");
                chosenMon = nullptr;
                tm = nullptr;
                longArr.clear();
                choosingFromLongList = false;
                psi = 0;
                closeMenus();
                return;
            }
        }

        print(chosenMon->nickname + " is trying to learn " + tm->name + "! Select a move to replace it with.");
        longArr = chosenMon->moveStrings();
        psi = 0;
        choosingFromLongList = true;
        inMenu = true;
        teachingMove = false;
        replacingMove = true;
        return;
    }

    if (replacingMove) {
        int lineIdx = longListChoiceFromY(mouseY);
        if (lineIdx < 0) {
            return;
        }
        int idx = psi + lineIdx;
        if (!chosenMon || !tm || idx < 0 || idx >= static_cast<int>(chosenMon->moves.size())) {
            return;
        }
        if (chosenMon->moves[idx] != nullptr) {
            print(chosenMon->nickname + " forgot how to use " + chosenMon->moves[idx]->name + ", but...");
        }
        chosenMon->moves[idx] = tm;
        chosenMon->pp[idx] = std::min(chosenMon->pp[idx], tm->pp);
        chosenMon->mpp[idx] = tm->pp;
        print(chosenMon->nickname + " learned " + tm->name + "!");
        tm = nullptr;
        chosenMon = nullptr;
        replacingMove = false;
        longArr.clear();
        choosingFromLongList = false;
        psi = 0;
        closeMenus();
        return;
    }

    if (choosingFromLongList && usingItemList && !usingBattleItem) {
        int lineIdx = longListChoiceFromY(mouseY);
        if (lineIdx < 0) {
            return;
        }
        int idx = psi + lineIdx;
        if (idx < 0 || idx >= static_cast<int>(usableItems.size())) {
            return;
        }

        if (usedItem == nullptr) {
            usedItem = usableItems[idx];
            if (!usedItem) {
                return;
            }

            if (usedItem->name == "Rare Candy") {
                rareCandy = true;
                longArr = Battler::partyStrings(player->team);
                chosenMon = nullptr;
                strArr[0] = "Pokemon";
                psi = 0;
                choosingFromLongList = true;
                return;
            }

            if (usedItem->name == "Repel") {
                useRepel(101);
                return;
            }
            if (usedItem->name == "Super Repel") {
                useRepel(201);
                return;
            }
            if (usedItem->name == "Max Repel") {
                useRepel(251);
                return;
            }

            if (usedItem->name == "Old Rod" || usedItem->name == "Good Rod" || usedItem->name == "Super Rod") {
                inMenu = true;
                std::string n = usedItem->name;
                usedItem = nullptr;

                int nx = playerX;
                int ny = playerY;
                switch (facing) {
                    case Direction::SOUTH:
                        ny++;
                        break;
                    case Direction::NORTH:
                        ny--;
                        break;
                    case Direction::WEST:
                        nx--;
                        break;
                    case Direction::EAST:
                        nx++;
                        break;
                }
                if (nx < 0 || ny < 0 || ny >= static_cast<int>(tileTypes.size()) || nx >= static_cast<int>(tileTypes[0].size())) {
                    closeMenus();
                    choosingFromLongList = false;
                    usingItemList = false;
                    inMenu = false;
                    print("What exactly do you expect to find in the void?");
                    return;
                }
                if (tileTypes[ny][nx] != 4) {
                    closeMenus();
                    choosingFromLongList = false;
                    usingItemList = false;
                    inMenu = false;
                    print("Maybe I should fish in the water...");
                    return;
                }
                wildMon = pm->getRandomEncounter(n);
                if (wildMon == nullptr) {
                    closeMenus();
                    choosingFromLongList = false;
                    usingItemList = false;
                    inMenu = false;
                    print("Huh, no fish... I should try somewhere else!");
                    return;
                }
                closeMenus();
                int result = BattleState::wildBattle(player->team, wildMon);
                if (result < 0) {
                    blackout();
                } else {
                    player->money += result * (player->hasItem(AMULET_COIN) ? 2 : 1);
                }
                battling = false;
                wildMon = nullptr;
                offsetX = 0;
                offsetY = 0;
                return;
            }

            if (usedItem->name == "Escape Rope") {
                loadMap(lastHeal);
                playerX = lastHeal->healX;
                playerY = lastHeal->healY;
                closeMenus();
                print("You climbed back to the last heal spot!");
                player->use(usedItem);
                usedItem = nullptr;
                return;
            }

            if (!usedItem->world && !usedItem->wild && !usedItem->battle && !usedItem->heal) {
                print("This can't be used now.");
                notifyUseItem(false);
                return;
            }

            longArr = Battler::partyStrings(player->team);
            chosenMon = nullptr;
            strArr[0] = "Pokemon";
            psi = 0;
            choosingFromLongList = true;
            return;
        }

        if (chosenMon == nullptr) {
            if (!player || idx < 0 || idx >= static_cast<int>(player->team.size())) {
                return;
            }
            Battler *b = player->team[idx];
            if (!b) {
                return;
            }

            choosingFromLongList = false;
            inMenu = false;

            if (usedItem->name == "Antidote") {
                healStatus(b, "POISONED");
            } else if (usedItem->name == "Awakening") {
                healStatus(b, "SLEEPING");
            } else if (usedItem->name == "Burn Heal") {
                healStatus(b, "BURNED");
            } else if (usedItem->name == "Ice Heal") {
                healStatus(b, "FROZEN");
            } else if (usedItem->name == "Paralyze Heal") {
                healStatus(b, "PARALYZED");
            } else if (usedItem->name == "Full Heal") {
                if (b->status.empty()) {
                    print("Quit messing around!");
                    notifyUseItem(false);
                } else {
                    if (playerState && playerState->monster == b) {
                        playerState->poisonDamage = 0;
                    }
                    b->status.clear();
                    print(b->nickname + "'s status was healed!");
                    notifyUseItem(true);
                }
            } else if (usedItem->name == "Full Restore") {
                if (b->hp == 0) {
                    print("No, you'll need something stronger to heal this one...");
                    notifyUseItem(false);
                } else if (b->hp == b->mhp && b->status.empty()) {
                    print("Quit messing around!");
                    notifyUseItem(false);
                } else {
                    b->hp = b->mhp;
                    b->status.clear();
                    print(b->nickname + " was fully restored!");
                    notifyUseItem(true);
                }
            } else if (usedItem->name == "Elixir") {
                useElixir(b, 10);
            } else if (usedItem->name == "Max Elixir") {
                useElixir(b, 99);
            } else if (usedItem->name == "Potion") {
                healHp(b, 20);
            } else if (usedItem->name == "Fresh Water") {
                healHp(b, 50);
            } else if (usedItem->name == "Super Potion") {
                healHp(b, 50);
            } else if (usedItem->name == "Soda Pop") {
                healHp(b, 60);
            } else if (usedItem->name == "Hyper Potion") {
                healHp(b, 200);
            } else if (usedItem->name == "Lemonade") {
                healHp(b, 80);
            } else if (usedItem->name == "Max Potion") {
                healHp(b, 999);
            } else if (usedItem->name == "Revive") {
                useRevive(b, b->mhp / 2);
            } else if (usedItem->name == "Max Revive") {
                useRevive(b, b->mhp);
            } else if (usedItem->name == "Ether" || usedItem->name == "Max Ether" || usedItem->name == "PP Up") {
                chosenMon = b;
                longArr = chosenMon->moveStrings();
                choosingFromLongList = true;
                inMenu = true;
                return;
            } else if (rareCandy && usedItem && usedItem->name == "Rare Candy") {
                player->use(usedItem);
                usedItem = nullptr;
                int delta = std::max(0, b->mxp - b->xp);
                b->gainXp(player, b->moves, delta, 0, 0, 0, 0, 0, 0);
                rareCandy = false;
                choosingFromLongList = false;
                inMenu = false;
                return;
            } else if (usedItem->name == "Moon Stone" || usedItem->name == "Fire Stone" || usedItem->name == "Water Stone" ||
                       usedItem->name == "Thunder Stone" || usedItem->name == "Leaf Stone") {
                bool ok = b->useStone(usedItem->name);
                if (ok && player) {
                    player->registerBattler(b);
                }
                notifyUseItem(ok);
            } else if (usedItem->name == "Calcium") {
                b->spatkXp += 10;
                b->refreshStats();
                print(b->nickname + " gained special attack experience!");
                notifyUseItem(true);
            } else if (usedItem->name == "Carbos") {
                b->spdXp += 10;
                b->refreshStats();
                print(b->nickname + " gained speed experience!");
                notifyUseItem(true);
            } else if (usedItem->name == "HP Up") {
                b->hpXp += 10;
                b->refreshStats();
                print(b->nickname + " gained HP experience!");
                notifyUseItem(true);
            } else if (usedItem->name == "Iron") {
                b->defXp += 10;
                b->refreshStats();
                print(b->nickname + " gained defense experience!");
                notifyUseItem(true);
            } else if (usedItem->name == "Protein") {
                b->atkXp += 10;
                b->refreshStats();
                print(b->nickname + " gained attack experience!");
                notifyUseItem(true);
            } else if (usedItem->name == "Zinc") {
                b->spdefXp += 10;
                b->refreshStats();
                print(b->nickname + " gained special defense experience!");
                notifyUseItem(true);
            } else {
                print("This can't be used now.");
                notifyUseItem(false);
            }
            return;
        }

        if (usedItem != nullptr && chosenMon != nullptr) {
            if (idx < 0 || chosenMon->moves.empty() || idx >= static_cast<int>(chosenMon->moves.size())) {
                return;
            }
            Move *move = chosenMon->moves[idx];
            if (!move) {
                return;
            }
            if (usedItem->name == "Ether") {
                if (chosenMon->pp[idx] == chosenMon->mpp[idx]) {
                    print("DO NOT WASTE THAT!");
                    notifyUseItem(false);
                } else {
                    chosenMon->pp[idx] = std::min(chosenMon->mpp[idx], chosenMon->pp[idx] + 10);
                    print(move->name + "'s PP was restored by 10!");
                    notifyUseItem(true);
                }
            } else if (usedItem->name == "Max Ether") {
                if (chosenMon->pp[idx] == chosenMon->mpp[idx]) {
                    print("DO NOT WASTE THAT!");
                    notifyUseItem(false);
                } else {
                    chosenMon->pp[idx] = chosenMon->mpp[idx];
                    print(move->name + "'s PP was fully restored!");
                    notifyUseItem(true);
                }
            } else if (usedItem->name == "PP Up") {
                chosenMon->mpp[idx] = static_cast<int>(chosenMon->mpp[idx] * 1.2);
                print(move->name + "'s PP was permanently increased!");
                player->use(usedItem);
                usedItem = nullptr;
                choosingFromLongList = false;
                busedItem = true;
                usingBattleItem = false;
            }
            chosenMon = nullptr;
            inMenu = false;
            choosingFromLongList = false;
            return;
        }
    }
    if (choosingFromLongList && usingBattleItem && usingItemList) {
        int lineIdx = longListChoiceFromY(mouseY);
        if (lineIdx < 0) {
            return;
        }
        int idx = psi + lineIdx;

        if (usedItem == nullptr) {
            if (idx < 0 || idx >= static_cast<int>(usableItems.size())) {
                return;
            }
            usedItem = usableItems[idx];
            if (!usedItem) {
                notifyUseItem(false);
                return;
            }
            if (!usedItem->battle && !usedItem->wild) {
                print("This can't be used now.");
                notifyUseItem(false);
                return;
            }

            if (usedItem->name == "X Accuracy") {
            playerState->accStage++;
            print(playerState->monster->nickname + "'s accuracy rose by 1 stage!");
            if (playerState->accStage == 7) {
                playerState->accStage = 6;
                print("Although it maxed out at 6!");
            }
            notifyUseItem(true);
            } else if (usedItem->name == "X Attack") {
            playerState->atkStage++;
            print(playerState->monster->nickname + "'s attack rose by 1 stage!");
            if (playerState->atkStage == 7) {
                playerState->atkStage = 6;
                print("Although it maxed out at 6!");
            }
            notifyUseItem(true);
            } else if (usedItem->name == "X Defend") {
            playerState->defStage++;
            print(playerState->monster->nickname + "'s defense rose by 1 stage!");
            if (playerState->defStage == 7) {
                playerState->defStage = 6;
                print("Although it maxed out at 6!");
            }
            notifyUseItem(true);
            } else if (usedItem->name == "X Special Attack") {
            playerState->spatkStage++;
            print(playerState->monster->nickname + "'s special attack rose by 1 stage!");
            if (playerState->spatkStage == 7) {
                playerState->spatkStage = 6;
                print("Although it maxed out at 6!");
            }
            notifyUseItem(true);
            } else if (usedItem->name == "X Special Defend") {
            playerState->spdefStage++;
            print(playerState->monster->nickname + "'s special defense rose by 1 stage!");
            if (playerState->spdefStage == 7) {
                playerState->spdefStage = 6;
                print("Although it maxed out at 6!");
            }
            notifyUseItem(true);
            } else if (usedItem->name == "X Speed") {
            playerState->spdStage++;
            print(playerState->monster->nickname + "'s speed rose by 1 stage!");
            if (playerState->spdStage == 7) {
                playerState->spdStage = 6;
                print("Although it maxed out at 6!");
            }
            notifyUseItem(true);
            } else if (usedItem->name == "Dire Hit") {
            playerState->critMul *= 4;
            print(playerState->monster->nickname + " is now 4 times more likely to get a critical hit!");
            notifyUseItem(true);
            } else if (usedItem->name == "Guard Spec.") {
            if (playerState->canLower) {
                playerState->canLower = false;
                print(playerState->monster->nickname + "'s stats cannot be lowered!");
            } else {
                print("Although they already could not be lowered!");
            }
            notifyUseItem(true);
            } else if (usedItem->name == "Poke Doll") {
            battling = false;
            print("You got away successfully!");
            notifyUseItem(true);
            } else if (usedItem->name == "PokeBall") {
            if (!wildMon) {
                print("This can't be used now.");
                notifyUseItem(false);
                return;
            }
            if (enemyState && enemyState->monster && enemyState->monster->nickname.rfind("Enemy ", 0) == 0) {
                print("You can't catch a trainer's pokemon!");
                notifyUseItem(false);
                return;
            }
            catchMon(1.0);
            } else if (usedItem->name == "Great Ball") {
            if (!wildMon) {
                print("This can't be used now.");
                notifyUseItem(false);
                return;
            }
            if (enemyState && enemyState->monster && enemyState->monster->nickname.rfind("Enemy ", 0) == 0) {
                print("You can't catch a trainer's pokemon!");
                notifyUseItem(false);
                return;
            }
            catchMon(1.5);
            } else if (usedItem->name == "Ultra Ball") {
            if (!wildMon) {
                print("This can't be used now.");
                notifyUseItem(false);
                return;
            }
            if (enemyState && enemyState->monster && enemyState->monster->nickname.rfind("Enemy ", 0) == 0) {
                print("You can't catch a trainer's pokemon!");
                notifyUseItem(false);
                return;
            }
            catchMon(2.0);
            } else if (usedItem->name == "Master Ball") {
            if (!wildMon) {
                print("This can't be used now.");
                notifyUseItem(false);
                return;
            }
            if (enemyState && enemyState->monster && enemyState->monster->nickname.rfind("Enemy ", 0) == 0) {
                print("You can't catch a trainer's pokemon!");
                notifyUseItem(false);
                return;
            }
            catchMon(100000);
            } else if (usedItem->name == "Repel") {
            useRepel(101);
            } else if (usedItem->name == "Super Repel") {
            useRepel(201);
            } else if (usedItem->name == "Max Repel") {
            useRepel(251);
            } else {
                longArr = Battler::partyStrings(player->team);
                psi = 0;
                chosenMon = nullptr;
            }
            return;
        }

        if (chosenMon == nullptr) {
            if (!player || idx < 0 || idx >= static_cast<int>(player->team.size()) || player->team[idx] == nullptr) {
                return;
            }
            Battler *b = player->team[idx];
            if (!b) {
                return;
            }
            if (usedItem->name == "Ether" || usedItem->name == "Max Ether" || usedItem->name == "PP Up") {
                chosenMon = b;
                longArr = chosenMon->moveStrings();
                psi = 0;
                return;
            }

            if (usedItem->name == "Antidote") {
                healStatus(b, "POISONED");
            } else if (usedItem->name == "Awakening") {
                healStatus(b, "SLEEPING");
            } else if (usedItem->name == "Burn Heal") {
                healStatus(b, "BURNED");
            } else if (usedItem->name == "Ice Heal") {
                healStatus(b, "FROZEN");
            } else if (usedItem->name == "Paralyze Heal") {
                healStatus(b, "PARALYZED");
            } else if (usedItem->name == "Full Heal") {
                if (b->status.empty()) {
                    print("Quit messing around!");
                    notifyUseItem(false);
                } else {
                    if (playerState && playerState->monster == b) {
                        playerState->poisonDamage = 0;
                    }
                    b->status.clear();
                    print(b->nickname + "'s status was healed!");
                    notifyUseItem(true);
                }
            } else if (usedItem->name == "Full Restore") {
                if (b->hp == 0) {
                    print("No, you'll need something stronger to heal this one...");
                    notifyUseItem(false);
                } else if (b->hp == b->mhp && b->status.empty()) {
                    print("Quit messing around!");
                    notifyUseItem(false);
                } else {
                    b->hp = b->mhp;
                    b->status.clear();
                    print(b->nickname + " was fully restored!");
                    notifyUseItem(true);
                }
            } else if (usedItem->name == "Elixir") {
                useElixir(b, 10);
            } else if (usedItem->name == "Max Elixir") {
                useElixir(b, 99);
            } else if (usedItem->name == "Potion") {
                healHp(b, 20);
            } else if (usedItem->name == "Fresh Water") {
                healHp(b, 50);
            } else if (usedItem->name == "Super Potion") {
                healHp(b, 50);
            } else if (usedItem->name == "Soda Pop") {
                healHp(b, 60);
            } else if (usedItem->name == "Hyper Potion") {
                healHp(b, 200);
            } else if (usedItem->name == "Lemonade") {
                healHp(b, 80);
            } else if (usedItem->name == "Max Potion") {
                healHp(b, 999);
            } else if (usedItem->name == "Revive") {
                useRevive(b, b->mhp / 2);
            } else if (usedItem->name == "Max Revive") {
                useRevive(b, b->mhp);
            } else {
                print("This can't be used now.");
                notifyUseItem(false);
            }
            return;
        }

        if (chosenMon != nullptr) {
            if (idx < 0 || idx >= static_cast<int>(chosenMon->moves.size()) || chosenMon->moves[idx] == nullptr) {
                return;
            }
            Move *move = chosenMon->moves[idx];
            if (usedItem->name == "Ether") {
                if (chosenMon->pp[idx] == chosenMon->mpp[idx]) {
                    print("DO NOT WASTE THAT!");
                    notifyUseItem(false);
                } else {
                    chosenMon->pp[idx] = std::min(chosenMon->mpp[idx], chosenMon->pp[idx] + 10);
                    print(move->name + "'s PP was restored by 10!");
                    notifyUseItem(true);
                }
            } else if (usedItem->name == "Max Ether") {
                if (chosenMon->pp[idx] == chosenMon->mpp[idx]) {
                    print("DO NOT WASTE THAT!");
                    notifyUseItem(false);
                } else {
                    chosenMon->pp[idx] = chosenMon->mpp[idx];
                    print(move->name + "'s PP was fully restored!");
                    notifyUseItem(true);
                }
            } else if (usedItem->name == "PP Up") {
                chosenMon->mpp[idx] = static_cast<int>(chosenMon->mpp[idx] * 1.2);
                print(move->name + "'s PP was permanently increased!");
                notifyUseItem(true);
            } else {
                notifyUseItem(false);
            }
            chosenMon = nullptr;
            return;
        }
    }
    if (choosingFromLongList) {
        int lineIdx = longListChoiceFromY(mouseY);
        if (lineIdx < 0) {
            return;
        }
        clickedChoice = lineIdx;
        return;
    }
    clickedChoice = menuChoiceFromY(mouseY);
}

void Gui::setWildItems() {
    if (!player) {
        return;
    }
    longArr.clear();
    usableItems.clear();
    for (Item *item : player->items) {
        usableItems.push_back(item);
        longArr.push_back(item ? item->toString() : "");
    }
    psi = 0;
    usingBattleItem = true;
    busedItem = false;
    choosingFromLongList = true;
    usingItemList = true;
    if (player->items.empty()) {
        print("No items!");
        usingBattleItem = false;
        choosingFromLongList = false;
        usingItemList = false;
    }
}

void Gui::setTrainerItems() {
    if (!player) {
        return;
    }
    longArr.clear();
    usableItems.clear();
    for (Item *item : player->items) {
        usableItems.push_back(item);
        longArr.push_back(item ? item->toString() : "");
    }
    psi = 0;
    usingBattleItem = true;
    busedItem = false;
    choosingFromLongList = true;
    usingItemList = true;
    if (player->items.empty()) {
        print("No items!");
        usingBattleItem = false;
        choosingFromLongList = false;
        usingItemList = false;
    }
}

void Gui::pumpEvents() {
}

// TODO: Re-analyze this function later - currently converts 3DS input to SDL events then back to game actions
// If performance issues arise, refactor to direct 3DS input handling instead of SDL event conversion
void Gui::handleEvent(const SDL_Event &event) {
    // Debug: Log all SDL events
    /*char debugMsg[256];
    if (event.type == SDL_KEYDOWN) {
        snprintf(debugMsg, sizeof(debugMsg), "SDL_KEYDOWN received: keysym=%d", event.key.sym);
        svcOutputDebugString(debugMsg, strlen(debugMsg));
    } else if (event.type == SDL_KEYUP) {
        snprintf(debugMsg, sizeof(debugMsg), "SDL_KEYUP received: keysym=%d", event.key.sym);
        svcOutputDebugString(debugMsg, strlen(debugMsg));
    }*/
    
    if (s_inTextPrompt) {
        return;
    }
    if (event.type == SDL_KEYDOWN && event.key.sym == SDLK_SPACE && !event.key.repeat) {
        autoBattle = !autoBattle;
        spacebar = false;
        if (showingText) {
            advanceText();
        }
    }
    if (event.type == SDL_MOUSEBUTTONDOWN) {
        clickMouse(event.button.x, event.button.y, event.button.button == SDL_BUTTON_RIGHT);
        return;
    }
    auto pressDirection = [&](Direction dir, bool pressed) {
        
        auto it = std::find(pressedKeys.begin(), pressedKeys.end(), dir);
        if (pressed) {
            if (it != pressedKeys.end()) {
                pressedKeys.erase(it);
            }
            pressedKeys.push_back(dir);
            heldDirection = dir;
            hasHeldDirection = true;
        } else {
            if (it != pressedKeys.end()) {
                pressedKeys.erase(it);
            }
            if (hasHeldDirection && heldDirection == dir) {
                if (pressedKeys.empty()) {
                    hasHeldDirection = false;
                } else {
                    heldDirection = pressedKeys.back();
                }
            }
        }
    };
    if (event.type == SDL_KEYDOWN && !event.key.repeat) {
        if (event.key.sym == SDLK_p || event.key.sym == SDLK_l || event.key.sym == SDLK_i || event.key.sym == SDLK_c ||
            event.key.sym == SDLK_t || event.key.sym == SDLK_y || event.key.sym == SDLK_x || event.key.sym == SDLK_q ||
            event.key.sym == SDLK_m) {
            if (inMenu || choosingFromLongList || buySell || flying || usingItemList || usingBattleItem || depWith || depositing || withdraw ||
                checkingPokes || checkingMoves || checkingTms || teachingMove || replacingMove) {
                clickMouse(0, 0, true);
                return;
            }
        }
        switch (event.key.sym) {
            case SDLK_ESCAPE:
                // B button - close menus, blackjack stand, or exit flying mode
                if (blackjackActive && !blackjackFinished) {
                    clickMouse(0, 0, true); // Trigger stand logic
                } else if (flying) {
                    flying = false;
                    inMenu = false;
                    inside = false;
                    currentLoc.clear();
                } else if (inMenu || choosingFromLongList || buySell || depWith || depositing || withdraw || 
                    checkingPokes || checkingMoves || checkingTms || teachingMove || replacingMove || usingBattleItem) {
                    closeMenus();
                }
                break;
            case SDLK_TAB:
            case SDLK_BACKSPACE:
            case SDLK_r:
                // Handle L/R button menu switching: Inventory -> TMs -> Pokemon -> Pokedex
                if (!showingText && !battling) {
                    // Determine direction: SDLK_BACKSPACE=previous, SDLK_r=next, TAB=next
                    if (event.key.sym == SDLK_BACKSPACE) {
                        currentMenu = (currentMenu - 1 + 4) % 4; // Previous menu
                    } else {
                        currentMenu = (currentMenu + 1) % 4; // Next menu
                    }
                    
                    // Close any existing menus first
                    if (inMenu || choosingFromLongList || buySell) {
                        closeMenus();
                    }
                    
                    switch (currentMenu) {
                        case 0: // Inventory
                            usableItems.clear();
                            longArr.clear();
                            for (Item *it : player->items) {
                                usableItems.push_back(it);
                                longArr.push_back(it ? it->toString() : "");
                            }
                            strArr[0] = "Items";
                            psi = 0;
                            choosingFromLongList = true;
                            inMenu = true;
                            usingItemList = true;
                            break;
                            
                        case 1: // TMs
                            longArr.clear();
                            tms.clear();
                            for (Move *m : player->tmHms) {
                                if (!m) continue;
                                tms.push_back(m);
                                longArr.push_back(m->name);
                            }
                            strArr[0] = "TMs";
                            psi = 0;
                            choosingFromLongList = true;
                            inMenu = true;
                            checkingTms = true;
                            break;
                            
                        case 2: // Pokemon
                            usableItems.clear();
                            longArr = Battler::partyStrings(player->team);
                            strArr[0] = "Pokemon";
                            psi = 0;
                            choosingFromLongList = true;
                            inMenu = true;
                            checkingPokes = true;
                            break;
                            
                        case 3: // Pokedex
                            closeMenus();
                            strArr[0] = "Pokedex (Caught: " + std::to_string(player->numCaught) + ")";
                            int dexSize = static_cast<int>(Monster::MONSTERS.size());
                            longArr.assign(std::max(20, dexSize), "");
                            for (int i = 0; i < static_cast<int>(longArr.size()); i++) {
                                std::string n = std::to_string(i);
                                std::string num = std::string(3 - static_cast<int>(n.size()), '0') + n;
                                std::string monName = "?";
                                if (i < static_cast<int>(player->pokedex.size()) && player->pokedex[i] && i < static_cast<int>(Monster::MONSTERS.size())) {
                                    monName = Monster::MONSTERS[i].name;
                                }
                                if (i == 0) {
                                    monName = (!player->pokedex.empty() && player->pokedex[0]) ? "Missingo" : "";
                                }
                                if ((i == 151 || i == 152) && (i >= static_cast<int>(player->pokedex.size()) || !player->pokedex[i])) {
                                    monName.clear();
                                }
                                longArr[i] = monName.empty() ? "" : (num + ": " + monName);
                            }
                            psi = 0;
                            choosingFromLongList = true;
                            inMenu = true;
                            showingDex = true;
                            break;
                    }
                }
                break;
            case SDLK_w:
            case SDLK_COMMA:
                pressDirection(Direction::NORTH, true);
                break;
            case SDLK_s:
            case SDLK_o:
                pressDirection(Direction::SOUTH, true);
                break;
            case SDLK_a:
                pressDirection(Direction::WEST, true);
                break;
            case SDLK_d:
            case SDLK_e:
                pressDirection(Direction::EAST, true);
                break;
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                if (!showingText && !battling) {
                    save();
                }
                break;
            case SDLK_m:
                if (!showingText && !battling) {
                    if (flying) {
                        flying = false;
                        inMenu = false;
                        inside = false;
                        currentLoc.clear();
                    } else {
                        startFlying();
                    }
                }
                break;
            case SDLK_p:
            case SDLK_l:
                if (battling && !showingText) {
                    clickedChoice = 1;
                } else if (!showingText) {
                    if (inMenu || choosingFromLongList || buySell) {
                        closeMenus();
                    }
                    usableItems.clear();
                    longArr = Battler::partyStrings(player->team);
                    strArr[0] = "Pokemon";
                    psi = 0;
                    choosingFromLongList = true;
                    inMenu = true;
                    checkingPokes = true;
                }
                break;
            case SDLK_i:
            case SDLK_c:
                if (battling && !showingText) {
                    clickedChoice = 2;
                } else if (!showingText) {
                    if (inMenu || choosingFromLongList || buySell) {
                        closeMenus();
                    }
                    
                    // Use the same currentMenu variable as L/R switching
                    
                    switch (currentMenu) {
                        case 0: // Inventory
                            usableItems.clear();
                            longArr.clear();
                            for (Item *it : player->items) {
                                usableItems.push_back(it);
                                longArr.push_back(it ? it->toString() : "");
                            }
                            strArr[0] = "Items";
                            psi = 0;
                            choosingFromLongList = true;
                            inMenu = true;
                            usingItemList = true;
                            break;
                            
                        case 1: // TMs
                            longArr.clear();
                            tms.clear();
                            for (Move *m : player->tmHms) {
                                if (!m) continue;
                                tms.push_back(m);
                                longArr.push_back(m->name);
                            }
                            strArr[0] = "TMs";
                            psi = 0;
                            choosingFromLongList = true;
                            inMenu = true;
                            checkingTms = true;
                            break;
                            
                        case 2: // Pokemon
                            longArr = Battler::partyStrings(player->team);
                            strArr[0] = "Pokemon";
                            psi = 0;
                            choosingFromLongList = true;
                            inMenu = true;
                            checkingPokes = true;
                            break;
                            
                        case 3: // Pokedex
                            closeMenus();
                            strArr[0] = "Pokedex (Caught: " + std::to_string(player->numCaught) + ")";
                            int dexSize = static_cast<int>(Monster::MONSTERS.size());
                            longArr.assign(std::max(20, dexSize), "");
                            for (int i = 0; i < static_cast<int>(longArr.size()); i++) {
                                std::string n = std::to_string(i);
                                std::string num = std::string(3 - static_cast<int>(n.size()), '0') + n;
                                std::string monName = "?";
                                if (i < static_cast<int>(player->pokedex.size()) && player->pokedex[i] && i < static_cast<int>(Monster::MONSTERS.size())) {
                                    monName = Monster::MONSTERS[i].name;
                                }
                                if (i == 0) {
                                    longArr[i] = "#--- " + monName;
                                } else {
                                    longArr[i] = "#" + num + " " + monName;
                                }
                            }
                            psi = 0;
                            choosingFromLongList = true;
                            inMenu = true;
                            showingDex = true;
                            break;
                    }
                }
                break;
            case SDLK_t:
            case SDLK_y:
                if (!showingText && !battling) {
                    if (flying) {
                        // If already flying, exit flying mode
                        flying = false;
                        inMenu = false;
                        inside = false;
                        currentLoc.clear();
                    } else {
                        // Otherwise, handle TM menu
                        if (inMenu || choosingFromLongList || buySell) {
                            closeMenus();
                        }
                        longArr.clear();
                        tms.clear();
                        for (Move *m : player->tmHms) {
                            if (!m) {
                                continue;
                            }
                            tms.push_back(m);
                            longArr.push_back(m->name);
                        }
                        strArr[0] = "TMs";
                        psi = 0;
                        choosingFromLongList = true;
                        inMenu = true;
                        checkingTms = true;
                    }
                }
                break;
            case SDLK_x:
            case SDLK_q:
                if (!showingText && !battling) {
                    if (inMenu) {
                        closeMenus();
                        break;
                    }
                    if (choosingFromLongList || buySell) {
                        break;
                    }
                    closeMenus();
                    strArr[0] = "Pokedex (Caught: " + std::to_string(player->numCaught) + ")";
                    int dexSize = static_cast<int>(Monster::MONSTERS.size());
                    longArr.assign(std::max(20, dexSize), "");
                    for (int i = 0; i < static_cast<int>(longArr.size()); i++) {
                        std::string n = std::to_string(i);
                        std::string num = std::string(3 - static_cast<int>(n.size()), '0') + n;
                        std::string monName = "?";
                        if (i < static_cast<int>(player->pokedex.size()) && player->pokedex[i] && i < static_cast<int>(Monster::MONSTERS.size())) {
                            monName = Monster::MONSTERS[i].name;
                        }
                        if (i == 0) {
                            monName = (!player->pokedex.empty() && player->pokedex[0]) ? "Missingo" : "";
                        }
                        if ((i == 151 || i == 152) && (i >= static_cast<int>(player->pokedex.size()) || !player->pokedex[i])) {
                            monName.clear();
                        }
                        longArr[i] = monName.empty() ? "" : (num + ": " + monName);
                    }
                    psi = 0;
                    choosingFromLongList = true;
                    showingDex = true;
                    inMenu = true;
                }
                break;
            case SDLK_UP:
                if (inMenu) {
                    if (checkingPokes || depositing) {
                        int n = 0;
                        while (n < static_cast<int>(player->team.size()) && player->team[n] != nullptr) {
                            n++;
                        }
                        if (n >= 2) {
                            Battler *b = player->team[0];
                            for (int i = 1; i < n; i++) {
                                player->team[i - 1] = player->team[i];
                                if (i - 1 < static_cast<int>(longArr.size()) && i < static_cast<int>(longArr.size())) {
                                    longArr[i - 1] = longArr[i];
                                }
                            }
                            player->team[n - 1] = b;
                            if (n - 1 < static_cast<int>(longArr.size())) {
                                longArr[n - 1] = b ? b->toString() : "";
                            }
                        }
                    } else if (checkingTms && !tms.empty()) {
                        if (tms.size() >= 2 && !longArr.empty()) {
                            Move *m = tms[0];
                            for (size_t i = 1; i < tms.size(); i++) {
                                tms[i - 1] = tms[i];
                                if (i - 1 < longArr.size() && i < longArr.size()) {
                                    longArr[i - 1] = longArr[i];
                                }
                            }
                            tms[tms.size() - 1] = m;
                            if (tms.size() - 1 < longArr.size()) {
                                longArr[tms.size() - 1] = m ? m->name : "";
                            }
                        }
                    } else if (checkingMoves && player && clickedChoice >= 0 && clickedChoice < static_cast<int>(player->team.size()) && player->team[clickedChoice]) {
                        Battler *b = player->team[clickedChoice];
                        for (int i = 1; i < 4; i++) {
                            if (b->moves[i] == nullptr) {
                                continue;
                            }
                            std::swap(b->moves[i - 1], b->moves[i]);
                            std::swap(b->pp[i - 1], b->pp[i]);
                            std::swap(b->mpp[i - 1], b->mpp[i]);
                        }
                        longArr = b->allInformation();
                    } else if (withdraw) {
                        if (!player->pc.empty()) {
                            psi = clampPageStart(psi, static_cast<int>(player->pc.size()), 20);
                            if (psi < static_cast<int>(player->pc.size())) {
                                player->pc.push_back(player->pc[psi]);
                                player->pc.erase(player->pc.begin() + psi);
                                for (int i = 0; i < 20 && i < static_cast<int>(longArr.size()); i++) {
                                    longArr[i] = (psi + i < static_cast<int>(player->pc.size()) && player->pc[psi + i]) ? player->pc[psi + i]->toString() : "";
                                }
                            }
                        }
                    } else if (usingItemList && !usableItems.empty()) {
                        if (usableItems.size() >= 2 && !longArr.empty()) {
                            Item *it = usableItems[0];
                            for (size_t i = 1; i < usableItems.size(); i++) {
                                usableItems[i - 1] = usableItems[i];
                                if (i - 1 < longArr.size() && i < longArr.size()) {
                                    longArr[i - 1] = longArr[i];
                                }
                            }
                            usableItems[usableItems.size() - 1] = it;
                            if (!longArr.empty()) {
                                longArr[longArr.size() - 1] = it ? it->toString() : "";
                            }

                            if (player && player->items.size() == usableItems.size()) {
                                Item *pit = player->items[0];
                                for (size_t i = 1; i < player->items.size(); i++) {
                                    player->items[i - 1] = player->items[i];
                                }
                                player->items[player->items.size() - 1] = pit;
                            }
                        }
                    }
                }
                break;
            case SDLK_DOWN:
                if (inMenu) {
                    if ((checkingPokes || depositing) && player && player->team.size() >= 2 && player->team[0] && player->team[1]) {
                        std::swap(player->team[0], player->team[1]);
                        if (longArr.size() >= 2) {
                            longArr[0] = player->team[0]->toString();
                            longArr[1] = player->team[1]->toString();
                        }
                    } else if (checkingMoves && player && clickedChoice >= 0 && clickedChoice < static_cast<int>(player->team.size()) && player->team[clickedChoice]) {
                        Battler *b = player->team[clickedChoice];
                        for (int i = 2; i >= 0; i--) {
                            if (b->moves[i] == nullptr || b->moves[i + 1] == nullptr) {
                                continue;
                            }
                            std::swap(b->moves[i], b->moves[i + 1]);
                            std::swap(b->pp[i], b->pp[i + 1]);
                            std::swap(b->mpp[i], b->mpp[i + 1]);
                        }
                        longArr = b->allInformation();
                    } else if (checkingTms && tms.size() >= 2) {
                        std::swap(tms[0], tms[1]);
                        if (longArr.size() >= 2) {
                            longArr[0] = tms[0] ? tms[0]->name : "";
                            longArr[1] = tms[1] ? tms[1]->name : "";
                        }
                    } else if (usingItemList && usableItems.size() >= 2) {
                        std::swap(usableItems[0], usableItems[1]);
                        if (longArr.size() >= 2) {
                            longArr[0] = usableItems[0] ? usableItems[0]->toString() : "";
                            longArr[1] = usableItems[1] ? usableItems[1]->toString() : "";
                        }

                        if (player && player->items.size() == usableItems.size()) {
                            std::swap(player->items[0], player->items[1]);
                        }
                    }
                }
                break;
            case SDLK_LEFT:
                if (withdraw) {
                    if (psi == 0) {
                        break;
                    }
                    boxNum = std::max(1, boxNum - 1);
                    psi = std::max(0, psi - 20);
                    longArr.clear();
                    for (int i = 0; i < 20; i++) {
                        int ind = psi + i;
                        longArr.push_back((player && ind < static_cast<int>(player->pc.size()) && player->pc[ind]) ? player->pc[ind]->toString() : "");
                    }
                    break;
                }
                if (choosingFromLongList) {
                    psi = clampPageStart(psi - 20, static_cast<int>(longArr.size()), 20);
                }
                break;
            case SDLK_RIGHT:
                if (withdraw) {
                    if (!player || psi + 20 >= static_cast<int>(player->pc.size())) {
                        break;
                    }
                    boxNum++;
                    psi += 20;
                    longArr.clear();
                    for (int i = 0; i < 20; i++) {
                        int ind = psi + i;
                        longArr.push_back((player && ind < static_cast<int>(player->pc.size()) && player->pc[ind]) ? player->pc[ind]->toString() : "");
                    }
                    break;
                }
                if (choosingFromLongList) {
                    psi = clampPageStart(psi + 20, static_cast<int>(longArr.size()), 20);
                }
                break;
            case SDLK_1:
            case SDLK_2:
            case SDLK_3:
            case SDLK_4:
            case SDLK_5:
            case SDLK_6:
            case SDLK_7:
            case SDLK_8:
            case SDLK_9:
            case SDLK_0: {
                if (showingText) {
                    advanceText();
                    break;
                }
                int v = (event.key.sym == SDLK_0) ? 9 : (static_cast<int>(event.key.sym - SDLK_1));

                if (choosingFromLongList && v >= 0 && v < 20) {
                    int startY = GameConfig3DS::WORLD_HEIGHT + GameConfig3DS::LONG_LIST_START_Y;
                    int clickY = startY + v * GameConfig3DS::MENU_OPTION_HEIGHT + 1;
                    clickMouse(80, clickY, false);
                    return;
                }

                if (depWith) {
                    int boxHeight = GameConfig3DS::MENU_BOX_HEIGHT;
                    int boxY = GameConfig3DS::WINDOW_HEIGHT - boxHeight - GameConfig3DS::MENU_BOX_MARGIN;
                    int startY = boxY + GameConfig3DS::MENU_OPTION_START_OFFSET;
                    int clickY = startY + v * GameConfig3DS::MENU_OPTION_HEIGHT + 1;
                    clickMouse(80, clickY, false);
                    return;
                }

                if (buySell && !choosingFromLongList) {
                    if (v == 0) {
                        buying = true;
                        buySell = false;
                        choosingFromLongList = true;
                        usableItems.clear();
                        longArr.clear();
                        for (const MartItem &m : martItems) {
                            longArr.push_back(m.toString());
                        }
                        strArr[0] = "Money: $" + std::to_string(player ? player->money : 0);
                        psi = 0;
                        return;
                    }
                    if (v == 1) {
                        selling = true;
                        buySell = false;
                        choosingFromLongList = true;
                        usableItems.clear();
                        longArr.clear();
                        for (Item *it : player->items) {
                            if (it && it->price > 0) {
                                usableItems.push_back(it);
                                longArr.push_back(it->name + " x" + std::to_string(it->quantity) + "  " + std::to_string(it->price / 2));
                            }
                        }
                        strArr[0] = "Sell";
                        psi = 0;
                        return;
                    }
                }
                if ((buying || selling) && choosingFromLongList) {
                    clickedChoice = v;
                    return;
                }
                clickedChoice = v;
                break;
            }
            default:
                break;
        }
    } else if (event.type == SDL_KEYUP) {
        switch (event.key.sym) {
            case SDLK_w:
            case SDLK_COMMA:
                pressDirection(Direction::NORTH, false);
                break;
            case SDLK_s:
            case SDLK_o:
                pressDirection(Direction::SOUTH, false);
                break;
            case SDLK_a:
                pressDirection(Direction::WEST, false);
                break;
            case SDLK_d:
            case SDLK_e:
                pressDirection(Direction::EAST, false);
                break;
            default:
                break;
        }
    }
}

void Gui::update() {
    if (showingText || battling || buySell || buying || selling || inMenu) {
        return;
    }
    static std::mt19937 s_blackjackRng(std::random_device{}());

    frames++;
    if (frames % 6 == 0) {
        if (spacebar) {
            clickMouse(0, 0, false);
        }
        if (frames % 30 == 0) {
            if (mattNpc && !danceFrames.empty()) {
                mattNpc->bi = danceFrames[utils::randInt(0, static_cast<int>(danceFrames.size()) - 1)];
            }
        }
        if (monsterTrainer != nullptr) {
            if (oakNpc && !oakFrames.empty()) {
                oakNpc->bi = oakFrames[utils::randInt(0, static_cast<int>(oakFrames.size()) - 1)];
            }
            if (!monsterFrames.empty()) {
                if (frames == 24) {
                    monsterTrainer->bi = monsterFrames[0];
                } else if (frames == 48 && monsterFrames.size() > 2) {
                    monsterTrainer->bi = monsterFrames[2];
                } else if (monsterFrames.size() > 1) {
                    frames %= 60;
                    monsterTrainer->bi = monsterFrames[1];
                }
            }
        }
    }

    if (blackjackActive) {
        if (blackjackTieFrames > 0) {
            blackjackTieFrames--;
            if (blackjackTieFrames == 0) {
                blackjackPlayer.clear();
                blackjackDealer.clear();
                blackjackHideFirst = true;
                blackjackFinished = false;
                blackjackPlayerWon = false;

                if (blackjackDeck.size() < 10) {
                    blackjackDeck.clear();
                    blackjackDeck.reserve(52);
                    for (int i = 0; i < 52; i++) {
                        blackjackDeck.push_back(i);
                    }
                    std::shuffle(blackjackDeck.begin(), blackjackDeck.end(), s_blackjackRng);
                }
                auto drawCard = [&]() {
                    int c = blackjackDeck.back();
                    blackjackDeck.pop_back();
                    return c;
                };
                blackjackPlayer.push_back(drawCard());
                blackjackPlayer.push_back(drawCard());
                blackjackDealer.push_back(drawCard());
                blackjackDealer.push_back(drawCard());
            }
        }
        return;
    }

    if (stepPhase == StepPhase::NONE && hasHeldDirection) {
        facing = heldDirection;
        currentStepFrames = canMove(heldDirection) ? NUM_STEP_FRAMES : BUMP_STEP_FRAMES;
        stepPhase = StepPhase::MOVING;
        phaseFrame = 0;
    }
    if (stepPhase != StepPhase::NONE) {
        advanceStep();
    }
}

void Gui::render() {
    // Get framebuffers for top and bottom screens
    u16 topWidth, topHeight;
    u8* top_fb = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &topWidth, &topHeight);
    
    u16 bottomWidth, bottomHeight;
    u8* bottom_fb = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &bottomWidth, &bottomHeight);
    
    memset(top_fb, 0, topWidth * topHeight * 3);
    
    int viewW = GameConfig3DS::TOP_SCREEN_WIDTH;
    int viewH = GameConfig3DS::TOP_SCREEN_HEIGHT;
    int camX = static_cast<int>(playerX * GameConfig3DS::TILE_SIZE + offsetX - viewW / 2 + GameConfig3DS::TILE_SIZE / 2);
    int camY = static_cast<int>(playerY * GameConfig3DS::TILE_SIZE + offsetY - viewH / 2 + GameConfig3DS::TILE_SIZE / 2);

    int startX = std::max(0, camX / GameConfig3DS::TILE_SIZE - 1);
    int startY = std::max(0, camY / GameConfig3DS::TILE_SIZE - 1);
    int endX = std::min(static_cast<int>(currentMap[0].size()), startX + viewW / GameConfig3DS::TILE_SIZE + 3);
    int endY = std::min(static_cast<int>(currentMap.size()), startY + viewH / GameConfig3DS::TILE_SIZE + 3);
    
    int tilesRendered = 0;
    for (int y = startY; y < endY; y++) {
        for (int x = startX; x < endX; x++) {
            int tileId = currentMap[y][x];
            int screenX = x * GameConfig3DS::TILE_SIZE - camX;
            int screenY = y * GameConfig3DS::TILE_SIZE - camY;
            
            // Draw tile using drawBGRAsset (no bounds check needed - only valid IDs)
            auto tileData = tileDataArray[tileId];
            if (tileData.first != nullptr) {
                drawBGRAsset(top_fb, tileData.first, tileData.second, screenY, screenX, topWidth, topHeight, false);
                tilesRendered++;
            }
            Trainer *t = pm->trainers[y][x];
            if (t) {
                 drawBGRAsset(top_fb, t->bi, RED_0_bgr_size, screenY, screenX, topWidth, topHeight, true);
            } else if (pm->wob[y][x]) {
                 drawBGRAsset(top_fb, pm->wob[y][x]->bi, RED_0_bgr_size, screenY, screenX, topWidth, topHeight, true);
            } else if (pm->npcs[y][x]) {
                 drawBGRAsset(top_fb, pm->npcs[y][x]->bi, RED_0_bgr_size, screenY, screenX, topWidth, topHeight, true);
            }
            
        }
    }

    for (int i = 0; i < 4; i++) {
        if (connections[i]) {
            drawConnection(i, connections[i], connOffsets[i], camX, camY);
        }
    }

    int drawX = viewW / 2 - GameConfig3DS::TILE_SIZE / 2;
    int drawY = viewH / 2 - GameConfig3DS::TILE_SIZE / 2;
    
    // Draw player sprite using RED sprites
    int currentFrame = getCurrentFrame();
    
    if (surfing) {
        // Use SEEL sprite frames when surfing
        switch (currentFrame) {
            case 0: drawBGRAsset(top_fb, SEEL_0_bgr, SEEL_0_bgr_size, drawY, drawX, topWidth, topHeight, true); break;
            case 1: drawBGRAsset(top_fb, SEEL_1_bgr, SEEL_1_bgr_size, drawY, drawX, topWidth, topHeight, true); break;
            case 2: drawBGRAsset(top_fb, SEEL_2_bgr, SEEL_2_bgr_size, drawY, drawX, topWidth, topHeight, true); break;
            case 3: drawBGRAsset(top_fb, SEEL_3_bgr, SEEL_3_bgr_size, drawY, drawX, topWidth, topHeight, true); break;
            case 4: drawBGRAsset(top_fb, SEEL_4_bgr, SEEL_4_bgr_size, drawY, drawX, topWidth, topHeight, true); break;
            case 5: drawBGRAsset(top_fb, SEEL_5_bgr, SEEL_5_bgr_size, drawY, drawX, topWidth, topHeight, true); break;
            case 6: drawBGRAsset(top_fb, SEEL_6_bgr, SEEL_6_bgr_size, drawY, drawX, topWidth, topHeight, true); break;
            case 7: drawBGRAsset(top_fb, SEEL_7_bgr, SEEL_7_bgr_size, drawY, drawX, topWidth, topHeight, true); break;
            case 8: drawBGRAsset(top_fb, SEEL_8_bgr, SEEL_8_bgr_size, drawY, drawX, topWidth, topHeight, true); break;
            case 9: drawBGRAsset(top_fb, SEEL_9_bgr, SEEL_9_bgr_size, drawY, drawX, topWidth, topHeight, true); break;
            default: drawBGRAsset(top_fb, SEEL_0_bgr, SEEL_0_bgr_size, drawY, drawX, topWidth, topHeight, true); break;
        }
    } else {
        // Use appropriate RED sprite based on current frame
        switch (currentFrame) {
            case 0: drawBGRAsset(top_fb, RED_0_bgr, RED_0_bgr_size, drawY, drawX, topWidth, topHeight, true); break;
            case 1: drawBGRAsset(top_fb, RED_1_bgr, RED_1_bgr_size, drawY, drawX, topWidth, topHeight, true); break;
            case 2: drawBGRAsset(top_fb, RED_2_bgr, RED_2_bgr_size, drawY, drawX, topWidth, topHeight, true); break;
            case 3: drawBGRAsset(top_fb, RED_3_bgr, RED_3_bgr_size, drawY, drawX, topWidth, topHeight, true); break;
            case 4: drawBGRAsset(top_fb, RED_4_bgr, RED_4_bgr_size, drawY, drawX, topWidth, topHeight, true); break;
            case 5: drawBGRAsset(top_fb, RED_5_bgr, RED_5_bgr_size, drawY, drawX, topWidth, topHeight, true); break;
            case 6: drawBGRAsset(top_fb, RED_6_bgr, RED_6_bgr_size, drawY, drawX, topWidth, topHeight, true); break;
            case 7: drawBGRAsset(top_fb, RED_7_bgr, RED_7_bgr_size, drawY, drawX, topWidth, topHeight, true); break;
            case 8: drawBGRAsset(top_fb, RED_8_bgr, RED_8_bgr_size, drawY, drawX, topWidth, topHeight, true); break;
            case 9: drawBGRAsset(top_fb, RED_9_bgr, RED_9_bgr_size, drawY, drawX, topWidth, topHeight, true); break;
            default: drawBGRAsset(top_fb, RED_0_bgr, RED_0_bgr_size, drawY, drawX, topWidth, topHeight, true); break;
        }
    }

    // Set bottom screen color based on auto-battle status
    memset(bottom_fb, 40, bottomWidth * bottomHeight * 3); // 40, 40, 40 = dark gray
    if (autoBattle) {
        fillRectangle(bottom_fb, 0, 255, 0, 0, 0, 20, 20, bottomWidth, bottomHeight); // Green square for auto-battling
    }

    bool canDrawMap = !showingText && !battling && !choosingFromLongList && !buySell && !blackjackActive &&!inMenu;
    if (canDrawMap||flying) {
        // Draw MAP_3DS on bottom screen, centered
        extern const unsigned char MAP_3DS_bgr[];
        extern const unsigned int MAP_3DS_bgr_size;
        
        // Center the 231x231 map on the 320x240 bottom screen
        int mapX = (bottomWidth - 231) / 2;
        int mapY = (bottomHeight - 231) / 2;
        drawBGRAsset(bottom_fb, MAP_3DS_bgr, MAP_3DS_bgr_size, mapX, mapY, bottomWidth, bottomHeight, true);

        if (flying) {
            // Draw red overlay for flyable locations
            for (int i = 0; i < 11; i++) {
                for (int j = 0; j < 11; j++) {
                    if (FlyLocation::isRed(i, j)) {
                        fillRectangle(bottom_fb, 0, 0, 255, 4+i*21, 44+j*21, 21, 21, bottomWidth, bottomHeight);
                    }
                }
            }
        }

        // Draw player sprite on map
        int currentFrame = getCurrentFrame();
        
        // Find and highlight current position on map
        if (!inside) {
            for (int i = 0; i < 11; i++) {
                for (int j = 0; j < 11; j++) {
                    if (pm->name == FlyLocation::NAME_MAP[i][j]) {
                        // Update player position on map
                        int playerMapX = 44+j*21;
                        int playerMapY = 4+i*21;
                        
                        // Redraw player sprite at correct position
                        if (surfing) {
                            drawBGRAsset(bottom_fb, RED_0_bgr, RED_0_bgr_size, playerMapY, playerMapX, bottomWidth, bottomHeight, true);
                        } else {
                            switch (currentFrame) {
                                case 0: drawBGRAsset(bottom_fb, RED_0_bgr, RED_0_bgr_size, playerMapY, playerMapX, bottomWidth, bottomHeight, true); break;
                                case 1: drawBGRAsset(bottom_fb, RED_1_bgr, RED_1_bgr_size, playerMapY, playerMapX, bottomWidth, bottomHeight, true); break;
                                case 2: drawBGRAsset(bottom_fb, RED_2_bgr, RED_2_bgr_size, playerMapY, playerMapX, bottomWidth, bottomHeight, true); break;
                                case 3: drawBGRAsset(bottom_fb, RED_3_bgr, RED_3_bgr_size, playerMapY, playerMapX, bottomWidth, bottomHeight, true); break;
                                case 4: drawBGRAsset(bottom_fb, RED_4_bgr, RED_4_bgr_size, playerMapY, playerMapX, bottomWidth, bottomHeight, true); break;
                                case 5: drawBGRAsset(bottom_fb, RED_5_bgr, RED_5_bgr_size, playerMapY, playerMapX, bottomWidth, bottomHeight, true); break;
                                case 6: drawBGRAsset(bottom_fb, RED_6_bgr, RED_6_bgr_size, playerMapY, playerMapX, bottomWidth, bottomHeight, true); break;
                                case 7: drawBGRAsset(bottom_fb, RED_7_bgr, RED_7_bgr_size, playerMapY, playerMapX, bottomWidth, bottomHeight, true); break;
                                case 8: drawBGRAsset(bottom_fb, RED_8_bgr, RED_8_bgr_size, playerMapY, playerMapX, bottomWidth, bottomHeight, true); break;
                                case 9: drawBGRAsset(bottom_fb, RED_9_bgr, RED_9_bgr_size, playerMapY, playerMapX, bottomWidth, bottomHeight, true); break;
                                default: drawBGRAsset(bottom_fb, RED_0_bgr, RED_0_bgr_size, playerMapY, playerMapX, bottomWidth, bottomHeight, true); break;
                            }
                        }
                        break;
                    }
                }
            }
        }

        if (flying && !currentLoc.empty()) {
            // Draw current location text in green
            extern std::map<char, std::pair<const unsigned char*, unsigned int>> fontMapData;
            drawRainbowText(bottom_fb, fontMapData, currentLoc.c_str(), 10, 10, bottomWidth, bottomHeight);
        }
    }

    if (blackjackActive) {
        auto cardValue = [](int c) {
            int r = c % 13;
            if (r == 0) {
                return 11;
            }
            if (r >= 9) {
                return 10;
            }
            return r + 1;
        };

        auto handValue = [&](const std::vector<int> &hand) {
            int sum = 0;
            int aces = 0;
            for (int c : hand) {
                sum += cardValue(c);
                if (c % 13 == 0) {
                    aces++;
                }
            }
            while (sum > 21 && aces-- > 0) {
                sum -= 10;
            }
            return sum;
        };

        auto fmtCard = [](int c) {
            static const char *ranks[] = {"A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K"};
            static const char *suits[] = {"H", "D", "C", "S"};
            int r = c % 13;
            int s = c / 13;
            std::string out = ranks[r];
            out += suits[s];
            return out;
        };

        auto fmtHand = [&](const std::vector<int> &hand, bool hideFirst) {
            std::string out;
            for (size_t i = 0; i < hand.size(); i++) {
                if (!out.empty()) {
                    out += " ";
                }
                if (i == 0 && hideFirst) {
                    out += "??";
                } else {
                    out += fmtCard(hand[i]);
                }
            }
            return out;
        };

        // Draw white background on bottom screen
        memset(bottom_fb, 255, bottomWidth * bottomHeight * 3);

        // Draw "Blackjack" title on bottom screen
        displayText(bottom_fb, fontMapData, "Blackjack", 
                   (bottomWidth - 120) / 2, 30, 
                   bottomWidth, bottomHeight);

        std::vector<std::string> lines;
        lines.push_back("");
        lines.push_back("Dealer:");
        lines.push_back(fmtHand(blackjackDealer, blackjackHideFirst));
        lines.push_back("");
        lines.push_back("Player:");
        lines.push_back(fmtHand(blackjackPlayer, false));
        lines.push_back("Points: " + std::to_string(handValue(blackjackPlayer)));
        lines.push_back("");
        if (blackjackTieFrames > 0) {
            lines.push_back("TIE! DEALING AGAIN");
        } else {
            lines.push_back("Tap: Hit");
            lines.push_back("B button: Stand");
        }

        int x = 60;
        int y = 60;
        int maxLines = 20;
        for (int lineIndex = 0; lineIndex < maxLines; lineIndex++) {
            if (lineIndex >= static_cast<int>(lines.size())) {
                break;
            }
            // Use displayText for bottom screen
            displayText(bottom_fb, fontMapData, lines[lineIndex].c_str(), 
                       x, y + lineIndex * 12, 
                       bottomWidth, bottomHeight);
        }
    } else if (showingText) {
        // Draw white background on UI area using 3DS framebuffer
        memset(top_fb + (GameConfig3DS::WORLD_HEIGHT * topWidth * 3), 255, 
               GameConfig3DS::UI_HEIGHT * topWidth * 3);
        // Display text directly using displayText
        displayText(top_fb, fontMapData, currentText.c_str(), 
                   60, GameConfig3DS::WORLD_HEIGHT + 20, 
                   topWidth, topHeight);
    } else if (choosingFromLongList) {
        // Draw dark rectangle background for header on top screen near bottom
        int headerX = 0;
        int headerY = 200;
        int headerWidth = 400;
        int headerHeight = 40;
        fillRectangle(top_fb, 255, 255, 255, headerY, headerX, headerHeight, headerWidth, topWidth, topHeight);
        auto headerText = [&]() -> std::string {
            if (!longListHeaderOverride.empty()) {
                return longListHeaderOverride;
            }
            if (blackjackShowingResults) {
                return "Blackjack";
            }
            if (teachingMove && tm) {
                return "Teach " + tm->name + " to which pokemon?";
            }
            if (replacingMove && tm) {
                return "Replace which move with " + tm->name + "?";
            }
            if (buying || selling) {
                return "Money: $" + std::to_string(player ? player->money : 0);
            }
            if (buySell) {
                return "Mart";
            }
            if (depWith) {
                return "Pokemon PC";
            }
            if (checkingPokes || depositing) {
                return "Party";
            }
            if (withdraw) {
                return "Box " + std::to_string(boxNum);
            }
            if (checkingTms) {
                return "TM/HMs";
            }
            if (checkingMoves) {
                return "Stats";
            }
            if (showingDex) {
                return "Number Caught: " + std::to_string(player ? player->numCaught : 0);
            }
            return "Inventory";
        };

        std::string header = headerText();
        
        // Draw header text on top screen (centered in dark rectangle)
        displayText(top_fb, fontMapData, header.c_str(), 
                   headerX, headerY,
                   topWidth, topHeight);

        // Clear bottom screen for menu data
        memset(bottom_fb, 255, bottomWidth * bottomHeight * 3); // White background

        // Render longArr data on bottom screen every 12 pixels starting at 0,0
        int maxLines = 20;
        int drawStart = 0;
        if (!withdraw) {
            psi = clampPageStart(psi, static_cast<int>(longArr.size()), maxLines);
            drawStart = psi;
        }
        for (int lineIndex = 0; lineIndex < maxLines; lineIndex++) {
            int i = drawStart + lineIndex;
            if (i < 0 || i >= static_cast<int>(longArr.size())) {
                break;
            }
            // Display on bottom screen every 12 pixels starting at 0,0
            displayText(bottom_fb, fontMapData, longArr[i].c_str(), 
                       0, lineIndex * 12, 
                       bottomWidth, bottomHeight);
        }
    } else if (battling) {
        bool hasBottomUi = false;
        for (int i = 0; i < 4; i++) {
            if (!strArr[i].empty()) {
                hasBottomUi = true;
                break;
            }
        }

        if (!hasBottomUi) {
            // Draw white background on UI area using 3DS framebuffer
            memset(top_fb + (GameConfig3DS::WORLD_HEIGHT * topWidth * 3), 255, 
                   GameConfig3DS::UI_HEIGHT * topWidth * 3);
        } else {
            // Draw white background on UI area using 3DS framebuffer
            memset(top_fb + (GameConfig3DS::WORLD_HEIGHT * topWidth * 3), 255, 
                   GameConfig3DS::UI_HEIGHT * topWidth * 3);
            // Display text directly using displayText
            displayText(top_fb, fontMapData, strArr[0].c_str(), 
                       60, GameConfig3DS::WORLD_HEIGHT + 20, 
                       topWidth, topHeight);
            int boxHeight = GameConfig3DS::MENU_BOX_HEIGHT;
            int boxY = GameConfig3DS::WINDOW_HEIGHT - boxHeight - GameConfig3DS::MENU_BOX_MARGIN;
            int startY = boxY + GameConfig3DS::MENU_OPTION_START_OFFSET;
            displayText(top_fb, fontMapData, strArr[1].c_str(), 
                       60, startY + GameConfig3DS::MENU_OPTION_HEIGHT * 1, 
                       topWidth, topHeight);
            displayText(top_fb, fontMapData, strArr[2].c_str(), 
                       60, startY + GameConfig3DS::MENU_OPTION_HEIGHT * 2, 
                       topWidth, topHeight);
            displayText(top_fb, fontMapData, strArr[3].c_str(), 
                       60, startY + GameConfig3DS::MENU_OPTION_HEIGHT * 3, 
                       topWidth, topHeight);
        }
    } else if (flying) {
    } else if (inMenu) {
        if (choosingFromLongList) {
            // Use same rendering logic as main choosingFromLongList section
            // Draw dark rectangle background for header on top screen near bottom
            int headerX = 0;
            int headerY = 200;
            int headerWidth = 400;
            int headerHeight = 40;
            fillRectangle(top_fb, 255, 255, 255, headerY, headerX, headerHeight, headerWidth, topWidth, topHeight);

            auto headerText = [&]() -> std::string {
                if (!longListHeaderOverride.empty()) {
                    return longListHeaderOverride;
                }
                if (usingBattleItem) {
                    return "Battle Items";
                }
                if (replacingMove) {
                    Move* tm = &Move::MOVES[clickedChoice];
                    return "Replace which move with " + tm->name + "?";
                }
                if (buying || selling) {
                    return "Money: $" + std::to_string(player ? player->money : 0);
                }
                if (buySell) {
                    return "Mart";
                }
                if (depWith) {
                    return "Pokemon PC";
                }
                if (checkingPokes || depositing) {
                    return "Party";
                }
                if (withdraw) {
                    return "Box " + std::to_string(boxNum);
                }
                if (checkingTms) {
                    return "TM/HMs";
                }
                if (checkingMoves) {
                    return "Stats";
                }
                if (showingDex) {
                    return "Number Caught: " + std::to_string(player ? player->numCaught : 0);
                }
                return "Inventory";
            };

            std::string header = headerText();
            
            // Draw header text on top screen (centered in dark rectangle)
            displayText(top_fb, fontMapData, header.c_str(), 
                       headerX, headerY,
                       topWidth, topHeight);

            // Clear bottom screen for menu data
            memset(bottom_fb, 255, bottomWidth * bottomHeight * 3); // White background

            // Render longArr data on bottom screen every 12 pixels starting at 0,0
            int maxLines = 20;
            int drawStart = 0;
            if (!withdraw) {
                psi = clampPageStart(psi, static_cast<int>(longArr.size()), maxLines);
                drawStart = psi;
            }
            for (int lineIndex = 0; lineIndex < maxLines; lineIndex++) {
                int i = drawStart + lineIndex;
                if (i < 0 || i >= static_cast<int>(longArr.size())) {
                    break;
                }
                // Display on bottom screen every 12 pixels starting at 0,0
                displayText(bottom_fb, fontMapData, longArr[i].c_str(), 
                           0, lineIndex * 12, 
                           bottomWidth, bottomHeight);
            }
        } else {
            // Draw white background on UI area using 3DS framebuffer
            memset(top_fb + (GameConfig3DS::WORLD_HEIGHT * topWidth * 3), 255, 
                   GameConfig3DS::UI_HEIGHT * topWidth * 3);
            // Display text directly using displayText
            displayText(top_fb, fontMapData, strArr[0].c_str(), 
                       60, GameConfig3DS::WORLD_HEIGHT + 20, 
                       topWidth, topHeight);
            int boxHeight = GameConfig3DS::MENU_BOX_HEIGHT;
            int boxY = GameConfig3DS::WINDOW_HEIGHT - boxHeight - GameConfig3DS::MENU_BOX_MARGIN;
            int startY = boxY + GameConfig3DS::MENU_OPTION_START_OFFSET;
            displayText(top_fb, fontMapData, strArr[1].c_str(), 
                       60, startY + GameConfig3DS::MENU_OPTION_HEIGHT * 1, 
                       topWidth, topHeight);
            displayText(top_fb, fontMapData, strArr[2].c_str(), 
                       60, startY + GameConfig3DS::MENU_OPTION_HEIGHT * 2, 
                       topWidth, topHeight);
            displayText(top_fb, fontMapData, strArr[3].c_str(), 
                       60, startY + GameConfig3DS::MENU_OPTION_HEIGHT * 3, 
                       topWidth, topHeight);
        }
    }
    
    // Flush buffers and swap screens for 3DS
    gfxFlushBuffers();
    gfxSwapBuffers();
    gspWaitForVBlank();
}

void Gui::drawConnection(int dirIndex, PokeMap *conn, int offset, int camX, int camY) {
    // Get framebuffers for top screen
    u16 topWidth, topHeight;
    u8* top_fb = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &topWidth, &topHeight);
    
    int baseX = 0;
    int baseY = 0;
    switch (dirIndex) {
        case 0:
            baseX = offset * GameConfig3DS::TILE_SIZE;
            baseY = -static_cast<int>(conn->grid.size()) * GameConfig3DS::TILE_SIZE;
            break;
        case 1:
            baseX = offset * GameConfig3DS::TILE_SIZE;
            baseY = static_cast<int>(currentMap.size()) * GameConfig3DS::TILE_SIZE;
            break;
        case 2:
            baseX = -static_cast<int>(conn->grid[0].size()) * GameConfig3DS::TILE_SIZE;
            baseY = offset * GameConfig3DS::TILE_SIZE;
            break;
        case 3:
            baseX = static_cast<int>(currentMap[0].size()) * GameConfig3DS::TILE_SIZE;
            baseY = offset * GameConfig3DS::TILE_SIZE;
            break;
    }
    int px = baseX - camX;
    int py = baseY - camY;

    auto floorDiv = [](int a, int b) {
        int q = a / b;
        int r = a % b;
        if (r != 0 && ((r < 0) != (b < 0))) {
            q--;
        }
        return q;
    };

    int tilesW = GameConfig3DS::TOP_SCREEN_WIDTH / GameConfig3DS::TILE_SIZE + 3;
    int tilesH = GameConfig3DS::WORLD_HEIGHT / GameConfig3DS::TILE_SIZE + 3;

    int sx = floorDiv(camX - baseX, GameConfig3DS::TILE_SIZE) - 1;
    int sy = floorDiv(camY - baseY, GameConfig3DS::TILE_SIZE) - 1;
    int ex = std::min(sx + tilesW, static_cast<int>(conn->grid[0].size()));
    int ey = std::min(sy + tilesH, static_cast<int>(conn->grid.size()));
    sx = std::max(sx, 0);
    sy = std::max(sy, 0);
    for (int y = sy; y < ey; y++) {
        for (int x = sx; x < ex; x++) {
            int screenX = x * GameConfig3DS::TILE_SIZE + px;
            int screenY = y * GameConfig3DS::TILE_SIZE + py;
            
            // Draw tile using drawBGRAsset
            int tileId = conn->grid[y][x];
            auto tileData = tileDataArray[tileId];
            if (tileData.first != nullptr) {
                drawBGRAsset(top_fb, tileData.first, tileData.second, screenY, screenX, topWidth, topHeight, false);
            }
            Trainer *t = conn->trainers[y][x];
            if (t) {
                drawBGRAsset(top_fb, t->bi, RED_0_bgr_size, screenY, screenX, topWidth, topHeight, true);
            } else if (conn->wob[y][x]) {
                drawBGRAsset(top_fb, conn->wob[y][x]->bi, RED_0_bgr_size, screenY, screenX, topWidth, topHeight, true);
            } else if (conn->npcs[y][x]) {
                drawBGRAsset(top_fb, conn->npcs[y][x]->bi, RED_0_bgr_size, screenY, screenX, topWidth, topHeight, true);
            }
            
        }
    }
}

int Gui::getCurrentFrame() const {
    bool moving = stepPhase == StepPhase::MOVING || (stepPhase == StepPhase::LANDING && phaseFrame < currentStepFrames / 2);
    switch (facing) {
        case Direction::SOUTH:
            return moving ? 0 + timesMoved : 1;
        case Direction::NORTH:
            return moving ? 3 + timesMoved : 4;
        case Direction::WEST:
            return moving ? 7 : 6;
        case Direction::EAST:
            return moving ? 9 : 8;
    }
    return 1;
}

bool Gui::canMove(Direction d) {
    if (switchingMaps) {
        return true;
    }
    if (!currentMap.empty() && !currentMap[0].empty() && playerX == static_cast<int>(currentMap[0].size())) {
        print("You beat the game! Now fly out of here and save... and maybe talk to the professor, too!");
        playerX = 1;
        startFlying();
        return true;
    }
    int nx = playerX;
    int ny = playerY;
    switch (d) {
        case Direction::SOUTH:
            ny++;
            break;
        case Direction::NORTH:
            ny--;
            break;
        case Direction::WEST:
            nx--;
            break;
        case Direction::EAST:
            nx++;
            break;
    }
    if (nx < 0 || ny < 0 || ny >= static_cast<int>(currentMap.size()) || nx >= static_cast<int>(currentMap[0].size())) {
        return false;
    }
    /*Npc *n=pm->npcs[ny][nx];
    if (n) {
        n->interact(player);
        return n->dead;
    }*/
    switch (tileTypes[playerY][playerX]) {
        case 5:
            return d == Direction::SOUTH;
        case 9:
            return d != Direction::NORTH && tileTypes[ny][nx] != 0;
        case 10:
            return d == Direction::WEST;
        case 11:
            return d == Direction::EAST;
    }
    switch (tileTypes[ny][nx]) {
        case 0:
            return false;
        case 3:
            print(printable(pm->name) + "... I think? Man, I wish I could read!");
            return false;
        case 4:
            return player->leadersBeaten[4] && player->hasMove(SURF);
        case 5:
            return d == Direction::SOUTH;
        case 6:
            return player->leadersBeaten[1] && player->hasMove(CUT);
        case 9:
            return d != Direction::SOUTH;
        case 10:
            return d == Direction::WEST;
        case 11:
            return d == Direction::EAST;
        case 19:
            return player->leadersBeaten[3] && player->hasMove(STRENGTH);
        case 20:
            print("Ew, that stinks!");
            if (utils::rand01() < 0.1) {
                if (utils::rand01() < 0.01) {
                    print("Whoay! Who would throw this away?!");
                    player->give(Item::ITEM_MAP["Rare Candy"]);
                } else {
                    print("Oh no, I'll have to wash my hands...");
                    player->give(Item::ITEM_MAP["...Secret Sauce"]);
                }
            }
            return false;
    }
    return true;
}

std::string Gui::printable(const std::string &s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (size_t i = 0; i < s.size(); i++) {
        char c = s[i];
        if (i > 0) {
            char prev = s[i - 1];
            if (std::isdigit(static_cast<unsigned char>(c)) && std::isalpha(static_cast<unsigned char>(prev))) {
                out.push_back(' ');
            } else if (std::isupper(static_cast<unsigned char>(c)) &&
                       std::islower(static_cast<unsigned char>(prev))) {
                out.push_back(' ');
            }
        }
        out.push_back(c);
    }
    return out;
}

int Gui::elevate(const std::string &s, bool B, int m) {
    std::string prompt = (pm && pm->name == "ViridianBunglerHouse") ? "..." : "Enter the floor you would like to go to:";
    stopMovement();

    if (B && player && LIFT_KEY && !player->hasItem(LIFT_KEY)) {
        print("Weird, nothing is happening.");
        return 0;
    }

    std::string input = promptText(prompt);
    if (input.empty()) {
        return 0;
    }
    if (pm && pm->name == "ViridianBunglerHouse") {
        if (input == "DEATH" && player && player->pokedex.size() > 150 && player->pokedex[150]) {
            loadMap(PokeMap::POKEMAPS["RocketHideoutB1F"]);
            return 1;
        }
        return 0;
    }
    std::string u = input;
    std::transform(u.begin(), u.end(), u.begin(), ::toupper);
    if (B && !u.empty() && u[0] == 'B') {
        u = u.substr(1);
    }
    if (!u.empty() && u.back() == 'F') {
        u.pop_back();
    }
    int q = 0;
    try {
        q = std::stoi(u);
    } catch (...) {
        print(input + ", huh? Yeah, okay buddy.");
        return 0;
    }
    if (q <= 0 || q > m) {
        print("Uhh, you know there are only " + std::to_string(m) + " floors, right?");
        return 0;
    }
    if (B && q == 3) {
        print("...no.");
        return 0;
    }
    loadMap(PokeMap::POKEMAPS[s + (B ? "B" + std::to_string(q) : std::to_string(q)) + "F"]);
    print("Wee!");
    return q;
}

char Gui::randomSpooky() {
    return static_cast<char>(32 + utils::randInt(0, 95));
}

void Gui::printSpooky() {
    std::string s;
    for (int i = 0; i < 10; i++) {
        s.push_back(randomSpooky());
    }
    s += "DEATH";
    for (int i = 0; i < 10; i++) {
        s.push_back(randomSpooky());
    }
    print(s);
}

void Gui::advanceStep() {
    
    float delta = static_cast<float>(GameConfig3DS::TILE_SIZE) / static_cast<float>(currentStepFrames);
    int dx = 0;
    int dy = 0;
    PokeMap *newMap = nullptr;
    switch (facing) {
        case Direction::SOUTH:
            dy = 1;
            if (playerY == static_cast<int>(currentMap.size()) - 1 && connections[1] != nullptr) {
                newMap = connections[1];
                playerX -= connOffsets[1];
                playerY = -1;
            }
            break;
        case Direction::NORTH:
            dy = -1;
            if (playerY == 0 && connections[0] != nullptr) {
                newMap = connections[0];
                playerX -= connOffsets[0];
                playerY = static_cast<int>(newMap->grid.size());
            }
            break;
        case Direction::WEST:
            dx = -1;
            if (playerX == 0 && connections[2] != nullptr) {
                newMap = connections[2];
                playerX = static_cast<int>(newMap->grid[0].size());
                playerY -= connOffsets[2];
            }
            break;
        case Direction::EAST:
            dx = 1;
            if (playerX == static_cast<int>(currentMap[0].size()) - 1 && connections[3] != nullptr) {
                newMap = connections[3];
                playerX = -1;
                playerY -= connOffsets[3];
            }
            break;
    }
    if (newMap != nullptr) {
        mapName = newMap->name;
        loadMap(newMap);
        switchingMaps = true;
        currentStepFrames /= 2;
    }
    bool isBump = currentStepFrames == BUMP_STEP_FRAMES;
    int nextX = playerX + dx;
    int nextY = playerY + dy;
    if (isBump) {
        bool b = nextX >= 0 && nextY >= 0 && nextY < static_cast<int>(currentMap.size()) && nextX < static_cast<int>(currentMap[0].size());

        if (b && (pm->grid[nextY][nextX] == 582 || pm->grid[nextY][nextX] == 583)) {
            stopMovement();
            phaseFrame = currentStepFrames;
            if (player && player->team[0]) {
                int r = utils::randInt(1, 100);
                print("Honestly, this one doesn't really look like " + player->team[0]->nickname + ", although it says it's supposed to be... " + Monster::MONSTERS[r].name + "?");
            } else {
                print("Honestly, this one doesn't really look like that pokemon...");
            }
            return;
        }

        if (tileTypes[playerY][playerX] == 2 || tileTypes[playerY][playerX] == 18) {
            Warp *w = pm->getWarp(playerY, playerX);
            if (w == nullptr) {
                w = pm->getNearbyWarp(playerY, playerX);
                if (w == nullptr) {
                    inMenu = true;
                    int n = elevate("RocketHideout", true, 4);
                    if (n > 0) {
                        playerY = HIDEOUT_Y[n];
                    } else {
                        playerY--;
                    }
                    inMenu = false;
                    return;
                }
            }
            stepPhase = StepPhase::NONE;
            loadMap(w->pm);
            playerX = w->col;
            playerY = w->row;
            offsetX = 0;
            offsetY = 0;
            currentStepFrames /= 2;
            return;
        }

        if (pm->healX == playerX && pm->healY == playerY) {
            stopMovement();
            player->healTeam();
            phaseFrame = currentStepFrames;
            lastHeal = pm;
            FlyLocation::visit(pm->name);
            print("Your team was fully healed!");
            return;
        }

        if (b) {
            WorldObject *wo = pm->wob[nextY][nextX];
            if (wo) {
                stopMovement();
                std::optional<bool> res = wo->stepOn(this);
                if (res.has_value() && res.value()) {
                    pm->stepOn(player, nextX, nextY);
                } else if (res.has_value() && !res.value()) {
                    playerX -= dx;
                    playerY -= dy;
                    offsetX = 0;
                    offsetY = 0;
                }
                phaseFrame = currentStepFrames;
                return;
            }
            if (pm->npcs[nextY][nextX] != nullptr) {
                stopMovement();
                phaseFrame = currentStepFrames;
                bool oldCanMap = canMap;
                canMap = false;
                pm->npcs[nextY][nextX]->interact(player);

                if (pm && pm->name == "Daycare" && player && !player->team.empty() && player->team[0]) {
                    int p = 5000;
                    int d = player->team[0]->dexNum;
                    if (d == 0) {
                        p = 50000;
                    } else if (d > 132) {
                        p = 25000;
                    }
                    if (p > player->money) {
                        print("Wait, you're sooo poor! You need at least $" + std::to_string(p) +
                              " for this child, I guess I'm keeping this one...");
                    } else {
                        player->money -= p;
                        print("You spent $" + std::to_string(p) + "...");
                        player->give(new Battler(player->team[0]));
                    }
                }

                canMap = oldCanMap;
                return;
            }
            if (pm->grid[playerY][playerX] == 122 && pm->name == "GameCorner") {
                stopMovement();
                phaseFrame = currentStepFrames;
                if (player->hasItem(FAKE_ID)) {
                    if (player->money < bet) {
                        print("No! I want to bet $" + std::to_string(bet) + " if I play, but I'm broke!");
                    } else {
                        static std::mt19937 s_blackjackRng(std::random_device{}());

                        blackjackActive = true;
                        blackjackBet = bet;
                        blackjackFinished = false;
                        blackjackTieFrames = 0;
                        blackjackHideFirst = true;

                        blackjackDeck.clear();
                        blackjackDeck.reserve(52);
                        for (int i = 0; i < 52; i++) {
                            blackjackDeck.push_back(i);
                        }
                        std::shuffle(blackjackDeck.begin(), blackjackDeck.end(), s_blackjackRng);
                        auto drawCard = [&]() {
                            int c = blackjackDeck.back();
                            blackjackDeck.pop_back();
                            return c;
                        };

                        blackjackPlayer.clear();
                        blackjackDealer.clear();
                        blackjackPlayer.push_back(drawCard());
                        blackjackPlayer.push_back(drawCard());
                        blackjackDealer.push_back(drawCard());
                        blackjackDealer.push_back(drawCard());

                        auto cardValue = [](int c) {
                            int r = c % 13;
                            if (r == 0) {
                                return 11;
                            }
                            if (r >= 9) {
                                return 10;
                            }
                            return r + 1;
                        };
                        auto handValue = [&](const std::vector<int> &hand) {
                            int sum = 0;
                            int aces = 0;
                            for (int c : hand) {
                                sum += cardValue(c);
                                if (c % 13 == 0) {
                                    aces++;
                                }
                            }
                            while (sum > 21 && aces-- > 0) {
                                sum -= 10;
                            }
                            return sum;
                        };

                        if (handValue(blackjackPlayer) == 21) {
                            blackjackHideFirst = false;
                            blackjackFinished = true;
                            blackjackPlayerWon = true;
                            player->money += blackjackBet;
                            blackjackActive = false;
                            blackjackBet = 0;
                            bet *= 2;
                            print("You won $" + std::to_string(bet / 2) + "!");
                        }
                    }
                } else {
                    print("Crap, it wants me to scan an ID!!");
                }
                return;
            }
            switch (pm->grid[nextY][nextX]) {
                case 143:
                    stopMovement();
                    phaseFrame = currentStepFrames;
                    print("Hey baby, come sit on my lap!");
                    print("(Eek, I'd better get out of here!)");
                    heldDirection = Direction::EAST;
                    facing = Direction::EAST;
                    hasHeldDirection = true;
                    return;
                case 35:
                case 220:
                case 388:
                case 420:
                    stopMovement();
                    inMenu = true;
                    depWith = true;
                    choosingFromLongList = true;
                    longArr.clear();
                    longArr.push_back("Deposit");
                    longArr.push_back("Withdraw");
                    phaseFrame = currentStepFrames;
                    return;
                case 51:
                case 52:
                case 65:
                case 66:
                case 139:
                case 141:
                case 326:
                case 329:
                case 534:
                case 535:
                    stopMovement();
                    phaseFrame = currentStepFrames;
                    print("No " + player->name + ", you told yourself you'd stop eating plants!");
                    return;
                case 3:
                case 4:
                case 56:
                case 57:
                case 317:
                case 328:
                case 435:
                case 438:
                case 496:
                case 668:
                    stopMovement();
                    phaseFrame = currentStepFrames;
                    if (player && !player->team.empty() && player->team[0]) {
                        print("Hot dog, that looks just like " + player->team[0]->nickname + "!");
                    } else {
                        print("Hot dog, that looks familiar!");
                    }
                    return;
                case 189:
                case 190:
                case 192:
                case 193:
                case 279:
                case 280:
                case 281:
                case 282:
                case 284:
                case 285:
                    stopMovement();
                    phaseFrame = currentStepFrames;
                    print("Gah, I can never find what I need! May as well speak to a worker...");
                    return;
                case 2:
                    stopMovement();
                    phaseFrame = currentStepFrames;
                    print("Here lies... huh? I wish I could read!");
                    return;
                case 42:
                case 378:
                    stopMovement();
                    phaseFrame = currentStepFrames;
                    print("Golly gee, I could stand here all day looking at this view!");
                    return;
                case 44:
                case 45:
                case 61:
                case 161:
                case 350:
                    stopMovement();
                    phaseFrame = currentStepFrames;
                    print("Books, huh? Boooring!");
                    return;
                case 9:
                case 13:
                case 14:
                    stopMovement();
                    phaseFrame = currentStepFrames;
                    print("Whoa, this one's a beauty! But I'll never afford it.");
                    return;
                case 20:
                    stopMovement();
                    phaseFrame = currentStepFrames;
                    print("Aw great, now my hands are covered in oil! I hate getting lubed up.");
                    return;
                case 36:
                    stopMovement();
                    phaseFrame = currentStepFrames;
                    print("How the heck do I work this crazy thing?!");
                    return;
                case 31:
                    stopMovement();
                    phaseFrame = currentStepFrames;
                    print("Nope, DEFINITELY not going in there!");
                    return;
                case 41:
                    stopMovement();
                    phaseFrame = currentStepFrames;
                    print("Gasp! Who would hang this in their house?!");
                    return;
                case 62:
                    stopMovement();
                    phaseFrame = currentStepFrames;
                    print("A trophy for comedy! How pathetic.");
                    return;
                case 177:
                case 178:
                    stopMovement();
                    phaseFrame = currentStepFrames;
                    heldDirection = Direction::SOUTH;
                    hasHeldDirection = true;
                    print("NO, I HATE SCHOOL!");
                    return;
                case 179:
                case 180:
                    stopMovement();
                    phaseFrame = currentStepFrames;
                    hasHeldDirection = false;
                    print("Nope, " + player->name + " is NOT a nerd!");
                    return;
                case 185:
                case 188:
                case 333:
                    stopMovement();
                    phaseFrame = currentStepFrames;
                    print("Hehehe look at all those squiggles... I wonder what they mean!");
                    return;
                case 194:
                    stopMovement();
                    phaseFrame = currentStepFrames;
                    print("Limited time sale on EVERYTHING?! I'd better act quick!");
                    return;
                case 226:
                case 614:
                case 633:
                    stopMovement();
                    phaseFrame = currentStepFrames;
                    print("It'd probably be rude to drink this... slurp!");
                    return;
                case 302:
                case 303:
                    stopMovement();
                    phaseFrame = currentStepFrames;
                    print("Well, I suppose a nibble couldn't hurt... CRUNCH!");
                    return;
                case 314:
                    stopMovement();
                    phaseFrame = currentStepFrames;
                    print("Wait, is there somebody INSIDE there? I'd better keep my distance...");
                    return;
                case 347:
                    stopMovement();
                    phaseFrame = currentStepFrames;
                    print("Blah blah words blah, LAME!");
                    return;
                case 391:
                    stopMovement();
                    phaseFrame = currentStepFrames;
                    print("A bachelor's degree in... computer science? Well that's worthless!");
                    return;
                case 206:
                    stopMovement();
                    phaseFrame = currentStepFrames;
                    print("Are those... pokeballs? Maybe I should just snatch one... eh, seems risky!");
                    return;
                case 455:
                    stopMovement();
                    phaseFrame = currentStepFrames;
                    print("This must be what the region looked like thousands of years ago, neat!");
                    return;
                case 464:
                    stopMovement();
                    phaseFrame = currentStepFrames;
                    print("What? It just says *ROCKS*, no duh!!");
                    return;
                case 575:
                    stopMovement();
                    phaseFrame = currentStepFrames;
                    print("Hehe, ohhh yeah, that's the good stuff!");
                    return;
                case 627:
                case 629:
                    stopMovement();
                    phaseFrame = currentStepFrames;
                    print("It says here the ship is... sinking?! I'd better get out of here!");
                    return;
                case 625:
                    stopMovement();
                    phaseFrame = currentStepFrames;
                    print("Yikes, this thing could tip over at any second!");
                    return;
                case 631:
                case 632:
                    stopMovement();
                    phaseFrame = currentStepFrames;
                    print("Is that... slowpoke tail? My my, how SCRUMPTIOUS indeed!");
                    return;
                case 164:
                case 165:
                    stopMovement();
                    phaseFrame = currentStepFrames;
                    print("No time to sleep, I've got pokemon to catch!");
                    return;
                case 195:
                case 379:
                    stopMovement();
                    phaseFrame = currentStepFrames;
                    print("Whoa, I don't think I'm old enough to look at this...");
                    return;
                case 196:
                case 390:
                    stopMovement();
                    phaseFrame = currentStepFrames;
                    print("Crud, he hit me with Rat Gambit again! I suck at this game.");
                    return;
                default:
                    break;
            }

            int martSize = 0;
            MartItem *mi = pm->getMartItems(playerX, playerY, martSize);
            if (mi == nullptr) {
                if (pm->grid[nextY][nextX] == 187) {
                    stopMovement();
                    phaseFrame = currentStepFrames;
                    print("Hm, better not touch that...");
                    return;
                }
            } else if (!buying && !selling) {
                stopMovement();
                martItems.clear();
                for (int i = 0; i < martSize; i++) {
                    martItems.push_back(mi[i]);
                }
                phaseFrame = currentStepFrames;
                canMap = false;
                print("Welcome to the mart!");
                buySell = true;
                inMenu = true;
                choosingFromLongList = true;
                psi = 0;
                longArr.clear();
                longArr.push_back("Buy");
                longArr.push_back("Sell");
                return;
            }
        }
    } else if (isBump) {
        float t = static_cast<float>(phaseFrame) / static_cast<float>(std::max(1, currentStepFrames - 1));
        float push = (t < 0.5f) ? (t * 2.0f) : ((1.0f - t) * 2.0f);
        float bumpDelta = delta * 0.35f;
        offsetX += dx * bumpDelta * push;
        offsetY += dy * bumpDelta * push;
    } else if (canMove(facing)) {
        offsetX += dx * delta;
        offsetY += dy * delta;
    }
    phaseFrame++;
    if (phaseFrame >= currentStepFrames) {
        
        switchingMaps = false;
        phaseFrame = 0;
        stepPhase = StepPhase::NONE;
        offsetX = 0;
        offsetY = 0;
        timesMoved = timesMoved == 0 ? 2 : 0;
        if (!isBump) {
            int prevType = -1;
            if (playerX >= 0 && playerY >= 0 && playerY < static_cast<int>(tileTypes.size()) &&
                playerX < static_cast<int>(tileTypes[0].size())) {
                prevType = tileTypes[playerY][playerX];
            }
            playerX = nextX;
            playerY = nextY;
            if (playerX < 0 || playerY < 0 || playerY >= static_cast<int>(currentMap.size()) ||
                playerX >= static_cast<int>(currentMap[0].size())) {
                return;
            }

            auto startForcedStep = [&](Direction d) {
                if (!canMove(d)) {
                    return;
                }
                facing = d;
                currentStepFrames = NUM_STEP_FRAMES;
                stepPhase = StepPhase::MOVING;
                phaseFrame = 0;
            };

            int curType = tileTypes[playerY][playerX];
            if (pm && player && curType == 13 && prevType != 13 && pm->name.rfind("PokemonTower", 0) == 0) {
                player->healTeam();
                print("You feel power flow through you. Your pokemon have been healed!");
            }
            if (curType == 4) {
                surfing = true;
            } else if (curType == 1 || curType == 8 || curType == 12) {
                surfing = false;
            }

            if (curType == 5) {
                startForcedStep(Direction::SOUTH);
                return;
            }
            if (curType == 10) {
                startForcedStep(Direction::WEST);
                return;
            }
            if (curType == 11) {
                startForcedStep(Direction::EAST);
                return;
            }

            if (curType == 14) {
                spinning = true;
                startForcedStep(Direction::WEST);
                return;
            }
            if (curType == 15) {
                spinning = true;
                startForcedStep(Direction::EAST);
                return;
            }
            if (curType == 16) {
                spinning = true;
                startForcedStep(Direction::NORTH);
                return;
            }
            if (curType == 17) {
                spinning = true;
                startForcedStep(Direction::SOUTH);
                return;
            }

            if (spinning && pm && pm->grid[playerY][playerX] == 545) {
                spinning = false;
            }

            if (spinning) {
                startForcedStep(facing);
                return;
            }

            if (curType == 7) {
                if (spinning) {
                    spinning = false;
                }
                if (pm->grid[playerY][playerX] == 182) {
                    inMenu = true;
                    elevate("CeladonMart", false, 5);
                    inMenu = false;
                    return;
                }
                if (pm->grid[playerY][playerX] == 697) {
                    inMenu = true;
                    int f = elevate("SilphCo", false, 11);
                    if (f > 0) {
                        playerX = SILPH_X[f];
                    }
                    inMenu = false;
                    return;
                }
                Warp *w = pm->getWarp(playerY, playerX);
                if (w) {
                    PokeMap *from = pm;
                    loadMap(w->pm);
                    playerX = w->col;
                    playerY = w->row;
                    offsetX = 0;
                    offsetY = 0;
                    if (playerX == 0 && playerY == 6 && from && from->name == "RedsHouse2F") {
                        player->ballin = true;
                        Trainer::addEliteFour();
                        print("What? How did I get back here??");
                    }
                    return;
                }
            }
            if (pm->healX == playerX && pm->healY == playerY) {
                if (spinning) {
                    spinning = false;
                }
                player->healTeam();
                lastHeal = pm;
                FlyLocation::visit(pm->name);
                print("Your team was fully healed!");
            }
            WorldObject *wo = pm->wob[playerY][playerX];
            if (wo) {
                if (spinning) {
                    spinning = false;
                }
                std::optional<bool> res = wo->stepOn(this);
                if (!res.has_value()) {
                    wildMon = new Battler(wo->level, wo->mon);
                    bool oldCanMap = canMap;
                    canMap = false;
                    int result = BattleState::wildBattle(player->team, wildMon);
                    if (result < 0) {
                        blackout();
                    } else {
                        player->money += result * (player->hasItem(AMULET_COIN) ? 2 : 1);
                        pm->stepOn(player, playerX, playerY);
                    }
                    battling = false;
                    wildMon = nullptr;
                    canMap = oldCanMap;
                    offsetX = 0;
                    offsetY = 0;
                    return;
                }
                if (res.value()) {
                    pm->stepOn(player, playerX, playerY);
                } else {
                    playerX -= dx;
                    playerY -= dy;
                    offsetX = 0;
                    offsetY = 0;
                }
            }
            Npc *n = pm->npcs[playerY][playerX];
            if (n) {
                if (spinning) {
                    spinning = false;
                }
                stopMovement();
                bool oldCanMap = canMap;
                canMap = false;
                n->interact(player);

                if (pm && pm->name == "Daycare" && player && !player->team.empty() && player->team[0]) {
                    int p = 5000;
                    int d = player->team[0]->dexNum;
                    if (d == 0) {
                        p = 50000;
                    } else if (d > 132) {
                        p = 25000;
                    }
                    if (p > player->money) {
                        print("Wait, you're sooo poor! You need at least $" + std::to_string(p) +
                              " for this child, I guess I'm keeping this one...");
                    } else {
                        player->money -= p;
                        print("You spent $" + std::to_string(p) + "...");
                        player->give(new Battler(player->team[0]));
                    }
                }

                canMap = oldCanMap;

                if (n->dead) {
                    pm->npcs[playerY][playerX] = nullptr;
                } else {
                    playerX -= dx;
                    playerY -= dy;
                }
                offsetX = 0;
                offsetY = 0;
                return;
            }
            if (pm->sight[playerY][playerX] != 0) {
                if (spinning) {
                    spinning = false;
                }
                Trainer *t = pm->getTrainer(playerX, playerY, pm->sight[playerY][playerX]);
                if (t) {
                    //fixes map display bug
                    bool oldCanMap = canMap;
                    canMap = false;
                    print(t->phrases[0]);
                    battling = true;
                    wildMon = nullptr;
                    int result = BattleState::trainerBattle(player->team, t->type, t->team);
                    battling = false;
                    if (result < 0) {
                        blackout();
                        for (Battler *b : t->team) {
                            b->fullyHeal();
                        }
                    } else {
                        int r = (result + t->reward) * (player->hasItem(AMULET_COIN) ? 2 : 1);
                        player->money += r;
                        print("You got $" + std::to_string(r) + " for winning!");
                        print(t->phrases[1]);
                        t->beat(player);
                        pm->deleteTrainer(t, pm->sight[playerY][playerX]);
                        Trainer::E4Trainer *e4 = dynamic_cast<Trainer::E4Trainer*>(t);
                        if (e4 && e4->id == 8) {
                            Trainer::addEliteFour();
                        }
                    }
                    offsetX = 0;
                    offsetY = 0;
                    canMap = oldCanMap;
                    wildMon = nullptr;
                    return;
                }
            }
            if (--repelSteps == 0) {
                print("Your repel ran out!");
            } else {
                repelSteps = std::max(repelSteps, 0);
            }
            int type = tileTypes[playerY][playerX];
            if (type == 12 || type == 8 || type == 4) {
                if (utils::rand01() < 0.1) {
                    std::string encounterType = type == 12 ? "Tall Grass" : (type == 8 ? "Cave" : "Surfing");
                    bool oldCanMap = canMap;
                    canMap = false;
                    wildMon = pm->getRandomEncounter(encounterType);
                    if (wildMon && (repelSteps == 0 || (player->team[0] && wildMon->level >= player->team[0]->level))) {
                        int result = BattleState::wildBattle(player->team, wildMon);
                        if (result < 0) {
                            blackout();
                        } else {
                            player->money += result * (player->hasItem(AMULET_COIN) ? 2 : 1);
                        }
                        battling = false;
                        wildMon = nullptr;
                        offsetX = 0;
                        offsetY = 0;
                        canMap = oldCanMap;
                        return;
                    }
                    canMap = oldCanMap;
                }
            }
        }
        offsetX = 0;
        offsetY = 0;
    }
}

void Gui::save() {
    if (!pm) {
        return;
    }
    if (inMenu || battling || choosingFromLongList || buySell) {
        return;
    }
    if (pm->name == "RocketHideoutB1F" && playerX == 3) {
        printSpooky();
        printSpooky();
        printSpooky();
        printSpooky();
        print("You will not be saved.");
        return;
    }
    if (pm->name == "ChampionsRoom" || pm->name == "LancesRoom" || pm->name == "AgathasRoom" || pm->name == "BrunosRoom" ||
        pm->name == "LoreleisRoom") {
        printSpooky();
        print("You will not be saved.");
        return;
    }
    std::string out;
    out += pm->name + "," + std::to_string(playerX) + "," + std::to_string(playerY) + "," +
           std::to_string(static_cast<int>(facing)) + "," + std::to_string(repelSteps) + "," + lastHeal->name + "," +
           (player->ballin ? "1" : "0") + "," + std::to_string(player->numCaught) + "," + std::to_string(player->money) +
           "," + player->name + "\n";
    if (!player->pc.empty()) {
        player->pc[0]->append(out);
        for (size_t i = 1; i < player->pc.size(); i++) {
            out.push_back(';');
            player->pc[i]->append(out);
        }
    }
    out.push_back('\n');
    if (player->team[0]) {
        player->team[0]->append(out);
        for (size_t i = 1; i < player->team.size(); i++) {
            if (!player->team[i]) {
                break;
            }
            out.push_back(';');
            player->team[i]->append(out);
        }
    }
    auto appendBoolLine = [&](const std::vector<bool> &v) {
        out.push_back('\n');
        for (bool b : v) {
            out.push_back(b ? '1' : '0');
        }
    };
    appendBoolLine(player->trainersBeaten);
    appendBoolLine(player->leadersBeaten);
    appendBoolLine(player->gioRivalsBeaten);
    appendBoolLine(player->objectsCollected);
    appendBoolLine(player->pokedex);
    out.push_back('\n');
    if (!player->items.empty()) {
        Item *i = player->items[0];
        out += i->name + "," + std::to_string(i->quantity);
        for (size_t j = 1; j < player->items.size(); j++) {
            i = player->items[j];
            out += ";" + i->name + "," + std::to_string(i->quantity);
        }
    }
    out.push_back('\n');
    for (Move *m : player->tmHms) {
        out += "," + m->name;
    }
    out.push_back('\n');
    for (size_t i = 1; i < 14 && i < FlyLocation::INDEX_MEANINGS.size(); i++) {
        auto it = FlyLocation::FLY_LOCATIONS.find(FlyLocation::INDEX_MEANINGS[i]);
        out.push_back(it != FlyLocation::FLY_LOCATIONS.end() && it->second->visited ? '1' : '0');
    }
    std::ofstream file("save.txt", std::ios::binary);
    file << out;
    file.close();
    print("Saved!");
}

void Gui::startFlying() {
    if (!player || !player->hasMove(FLY) || player->leadersBeaten.size() <= 2 || !player->leadersBeaten[2]) {
        print("You can't fly right now!");
        return;
    }
    flying = true;
    inMenu = true;
    currentLoc = printable(pm->name);
    inside = true;
    for (int i = 0; i < 11; i++) {
        for (int j = 0; j < 11; j++) {
            if (pm->name == FlyLocation::NAME_MAP[i][j]) {
                inside = false;
                return;
            }
        }
    }
}

void Gui::useRevive(Battler *b, int n) {
    if (b->hp > 0) {
        print("Quit messing around!");
        notifyUseItem(false);
    } else {
        b->hp += n;
        print(b->nickname + "'s health was restored by " + std::to_string(n) + " points!");
        notifyUseItem(true);
    }
}

void Gui::healHp(Battler *b, int n) {
    if (b->hp == 0) {
        print("No, you'll need something stronger to heal this one...");
        notifyUseItem(false);
    } else if (b->hp == b->mhp) {
        print("Quit messing around!");
        notifyUseItem(false);
    } else {
        b->hp = std::min(b->hp + n, b->mhp);
        print(b->nickname + "'s health was restored by " + std::to_string(n) + " points!");
        notifyUseItem(true);
    }
}

void Gui::useElixir(Battler *b, int n) {
    bool used = false;
    for (size_t i = 0; i < b->moves.size(); i++) {
        if (b->moves[i] == nullptr) {
            break;
        }
        if (b->pp[i] < b->mpp[i]) {
            b->pp[i] = std::min(b->pp[i] + n, b->mpp[i]);
            used = true;
        }
    }
    if (used) {
        print(b->nickname + "'s PP was restored!");
        notifyUseItem(true);
    } else {
        print("DO NOT WASTE THAT!");
        notifyUseItem(false);
    }
}

void Gui::healStatus(Battler *b, const std::string &s) {
    if (b->status == s) {
        if (playerState && playerState->monster == b) {
            playerState->poisonDamage = 0;
        }
        b->status.clear();
        print(b->nickname + "'s status was healed!");
        notifyUseItem(true);
    } else {
        print("That wouldn't do anything!");
        notifyUseItem(false);
    }
}

void Gui::useRepel(int steps) {
    repelSteps = std::max(repelSteps, 0) + steps;
    player->use(usedItem);
    usedItem = nullptr;
    longArr.clear();
    usableItems.clear();
    choosingFromLongList = false;
    usingItemList = false;
    usingBattleItem = false;
    busedItem = false;
    rightClicked = false;
    clickedChoice = -1;
    psi = 0;
    closeMenus();
    print("You gained " + std::to_string(steps - 1) + " repel steps!");
}

void Gui::catchMon(double bm) {
    if (!wildMon) {
        print("This can't be used now.");
        notifyUseItem(false);
        return;
    }
    int thp = 3 * wildMon->mhp;
    int sb = 0;
    if (wildMon->status == "ASLEEP" || wildMon->status == "FROZEN") {
        sb = 10;
    } else if (wildMon->status == "POISONED" || wildMon->status == "BURNED" || wildMon->status == "PARALYZED") {
        sb = 5;
    }
    print("You threw the " + usedItem->name + "!");
    int rate = CATCH_RATES[wildMon->dexNum];
    int chance = std::max(((thp - 2 * wildMon->hp) * rate * bm) / thp, 1.0) + sb;
    if (utils::randInt(0, 256) <= chance) {
        battling = false;
        print("Oh baby! You caught it!");
        wildMon->nickname = wildMon->name;
        player->give(wildMon);
        wildMon = nullptr;
    } else {
        print("But it broke out!");
    }
    notifyUseItem(true);
}

void Gui::notifyUseItem(bool b) {
    if (b) {
        player->use(usedItem);
    }
    usedItem = nullptr;
    chosenMon = nullptr;
    tm = nullptr;
    choosingFromLongList = false;
    usingItemList = false;
    inMenu = false;
    busedItem = b;
    usingBattleItem = false;
}

void Gui::blackout() {
    Trainer::addEliteFour();
    loadMap(lastHeal);
    playerX = lastHeal->healX;
    playerY = lastHeal->healY;
    int n = player->money / 2;
    player->money -= n;
    player->healTeam();
    wildMon = nullptr;
    print("You dropped $" + std::to_string(n) + "!");
}

void Gui::drawRainbowText(u8* framebuffer, std::map<char, std::pair<const unsigned char*, unsigned int>>& fontMapData, const std::string &text, int x, int y, int fb_width, int fb_height) {
    std::string t = text;
    size_t p = 0;
    while ((p = t.find("=", p)) != std::string::npos) {
        t.erase(p, std::string("=").size());
    }
    
    int penX = x;
    int idx = 0;
    for (unsigned char ch : t) {
        if (ch == 0) continue;
        
        // Get font glyph data
        auto it = fontMapData.find(ch);
        if (it != fontMapData.end()) {
            auto glyphData = it->second;
            
            // Calculate rainbow color for this character
            float h = static_cast<float>((idx * 25) % 360);
            float s = 1.0f;
            float v = 1.0f;
            
            // HSV to RGB conversion
            h = std::fmod(h, 360.0f);
            if (h < 0) h += 360.0f;
            float c = v * s;
            float x_val = c * (1.0f - std::fabs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
            float m = v - c;
            float r_val = 0.0f, g_val = 0.0f, b_val = 0.0f;
            
            if (h < 60.0f) { r_val = c; g_val = x_val; }
            else if (h < 120.0f) { r_val = x_val; g_val = c; }
            else if (h < 180.0f) { g_val = c; b_val = x_val; }
            else if (h < 240.0f) { g_val = x_val; b_val = c; }
            else if (h < 300.0f) { r_val = x_val; b_val = c; }
            else { r_val = c; b_val = x_val; }
            
            u8 red = static_cast<u8>(std::round((r_val + m) * 255.0f));
            u8 green = static_cast<u8>(std::round((g_val + m) * 255.0f));
            u8 blue = static_cast<u8>(std::round((b_val + m) * 255.0f));
            
            // Draw colored glyph
            drawBGRAssetWithColor(framebuffer, glyphData.first, glyphData.second, y, penX, fb_width, fb_height, red, green, blue);
            y += 12; // Approximate character width
        }
        idx++;
    }
}
