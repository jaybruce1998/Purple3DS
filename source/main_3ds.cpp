// 3DS Conversion of Pokemon Purple - Main Entry Point
// Based on original Main.cpp with SDL replaced by 3DS graphics system

#include "BattleState.h"
#include "Blocker.h"
#include "Encounter.h"
#include "Evolution.h"
#include "FlyLocation.h"
#include "Giver.h"
#include "Item.h"
#include "LevelUpMove.h"
#include "all_battlers.h"
#include "MartItem.h"
#include "Monster.h"
#include "Move.h"
#include "Trainer.h"
#include "Types.h"
#include "Warp.h"
#include "WorldObject.h"
#include "debug.h"

// Add 3DS types
#include <3ds.h>

#include "Gui.h"
#include "Player.h"
#include "TmLearnsets.h"
#include "Trader.h"

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

#define SDL_BUTTON_LEFT 1

// External global gui instance
extern Gui* g_gui;

// Replace SDL with 3DS headers
#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <filesystem>
#include <functional>
#include <fstream>
#include <iostream>
#include <vector>

// Include our 3DS graphics system
#include "all_glyphs.h"
#include "all_tiles.h"
#include "all_sprites.hpp"
#include "all_battlers.h"
#include "map_data.h"
#include "map_definitions.h"

// External declarations for battler and sprite arrays
extern const unsigned char* battlers_bgr[];
extern const unsigned int battlers_bgr_sizes[];
extern const unsigned char RED_0_bgr[];
extern const unsigned int RED_0_bgr_size;

// External declarations for drawing functions (now in Gui.cpp)
extern void setPixel(u8* framebuffer, int x, int y, u8 r, u8 g, u8 b, int width);
extern void drawRect(u8* framebuffer, int x, int y, int rect_width, int rect_height, u8 r, u8 g, u8 b, int fb_width);
extern void drawBGRAsset(u8* framebuffer, const unsigned char* bgr_data, unsigned int bgr_size, int dst_x, int dst_y, int fb_width, int fb_height, bool flip_horizontal = false);
extern void displayText(u8* framebuffer, const std::map<char, std::pair<const unsigned char*, unsigned int>>& fontMapData, const char* text, int start_x, int start_y, int screen_width, int screen_height);

// Forward declarations
static void buildGameData();

// Global font map data
std::map<char, std::pair<const unsigned char*, unsigned int>> fontMapData;

// Now define buildGameData after globals are declared
void debugCheckpoint(const char *stepName, bool bottom) {
    // Show and wait for A button with proper input handling
    bool waiting = true;
    while (waiting && aptMainLoop()) {
        // Get fresh framebuffer every frame
        u16 width, height;
        u8* current_fb = gfxGetFramebuffer(bottom ? GFX_BOTTOM : GFX_TOP, GFX_LEFT, &width, &height);
        
        // Clear and draw every frame
        memset(current_fb, 255, width * height * 3);
        displayText(current_fb, fontMapData, stepName, 10, 10, width, height);
        
        hidScanInput();
        u32 kDown = hidKeysDown();
        if(kDown & KEY_A) {
            waiting = false;
        }
        
        // Flush and swap every frame
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }
}

static void buildGameData() {
    
    Types::buildTypes();
    //debugCheckpoint("Step 1/16: Types.buildTypes() DONE", inputManager);
    
    Monster::buildMonsters();
    //debugCheckpoint("Step 2/16: Monster.buildMonsters() DONE", inputManager);
    
    Move::buildMoves();
    //debugCheckpoint("Step 3/16: Move.buildMoves() DONE", inputManager);
    
    LevelUpMove::buildLevelUpMoves();
    //debugCheckpoint("Step 4/16: LevelUpMove.buildLevelUpMoves() DONE", inputManager);
    
    Evolution::buildEvolutions();
    //debugCheckpoint("Step 5/16: Evolution.buildEvolutions() DONE", inputManager);
    
    PokeMap::buildPokeMaps();
    //debugCheckpoint("Step 6/16: PokeMap.buildPokeMaps() DONE", inputManager);
    
    FlyLocation::buildWorldMap();
    //debugCheckpoint("Step 7/16: FlyLocation.buildWorldMap() DONE", inputManager);
    
    Warp::buildWarps();
    //debugCheckpoint("Step 8/16: Warp.buildWarps() DONE", inputManager);
    
    TmLearnsets::buildTmLearnsets();
    //debugCheckpoint("Step 9/16: TmLearnsets.buildTmLearnsets() DONE", inputManager);
    
    Encounter::buildEncounterRates();
    //debugCheckpoint("Step 10/16: Encounter.buildEncounterRates() DONE", inputManager);
    
    Item::buildItems();
    //debugCheckpoint("Step 11/16: Item.buildItems() DONE", inputManager);
    
    MartItem::buildMartItems();
    //debugCheckpoint("Step 12/16: MartItem.buildMartItems() DONE");
    
    Giver::buildGivers();
    //debugCheckpoint("Step 13/16: Giver.buildGivers() DONE");
    
    Blocker::buildBlockers();
    //debugCheckpoint("Step 14/16: Blocker.buildBlockers() DONE");
    
    Trader::buildTraders();
    //debugCheckpoint("Step 15/16: Trader.buildTraders() DONE");
    
    Npc::buildNpcs();
    //debugCheckpoint("Step 16/16: Npc.buildNpcs() DONE");
    
    //debugCheckpoint("buildGameData() COMPLETE! All 16 steps finished.");
}

// Override utils::readLines for 3DS
namespace utils {
    std::vector<std::string> readLines_3ds(const std::string &path) {
        // For other files, try to read normally or return empty
        std::ifstream file(path);
        if (!file.is_open()) {
            return {}; // Return empty instead of throwing
        }
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(file, line)) {
            lines.push_back(line);
        }
        return lines;
    }
}

// Macro to override readLines calls
#define readLines readLines_3ds
void playGame() {
    while (aptMainLoop()) {
        // Handle input - convert 3DS input to SDL events for Gui
        hidScanInput();
        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();
        
        // Track previous D-pad and circle pad state for release detection
        static u32 lastDPad = 0;
        static u32 lastCPad = 0;
        u32 currentDPad = kHeld & (KEY_DLEFT | KEY_DRIGHT | KEY_DUP | KEY_DDOWN);
        u32 currentCPad = kHeld & (KEY_CPAD_LEFT | KEY_CPAD_RIGHT | KEY_CPAD_UP | KEY_CPAD_DOWN);
        
        // Check for D-pad releases (Arrow keys)
        u32 releasedDPad = lastDPad & ~currentDPad;
        if (releasedDPad & KEY_DLEFT) {
            SDL_Event releaseEvent;
            memset(&releaseEvent, 0, sizeof(SDL_Event));
            releaseEvent.type = SDL_KEYUP;
            releaseEvent.key.sym = SDLK_LEFT;
            g_gui->handleEvent(releaseEvent);
        }
        if (releasedDPad & KEY_DRIGHT) {
            SDL_Event releaseEvent;
            memset(&releaseEvent, 0, sizeof(SDL_Event));
            releaseEvent.type = SDL_KEYUP;
            releaseEvent.key.sym = SDLK_RIGHT;
            g_gui->handleEvent(releaseEvent);
        }
        if (releasedDPad & KEY_DUP) {
            SDL_Event releaseEvent;
            memset(&releaseEvent, 0, sizeof(SDL_Event));
            releaseEvent.type = SDL_KEYUP;
            releaseEvent.key.sym = SDLK_UP;
            g_gui->handleEvent(releaseEvent);
        }
        if (releasedDPad & KEY_DDOWN) {
            SDL_Event releaseEvent;
            memset(&releaseEvent, 0, sizeof(SDL_Event));
            releaseEvent.type = SDL_KEYUP;
            releaseEvent.key.sym = SDLK_DOWN;
            g_gui->handleEvent(releaseEvent);
        }
        
        // Check for circle pad releases (WASD keys)
        u32 releasedCPad = lastCPad & ~currentCPad;
        if (releasedCPad & KEY_CPAD_LEFT) {
            SDL_Event releaseEvent;
            memset(&releaseEvent, 0, sizeof(SDL_Event));
            releaseEvent.type = SDL_KEYUP;
            releaseEvent.key.sym = SDLK_a;
            g_gui->handleEvent(releaseEvent);
        }
        if (releasedCPad & KEY_CPAD_RIGHT) {
            SDL_Event releaseEvent;
            memset(&releaseEvent, 0, sizeof(SDL_Event));
            releaseEvent.type = SDL_KEYUP;
            releaseEvent.key.sym = SDLK_d;
            g_gui->handleEvent(releaseEvent);
        }
        if (releasedCPad & KEY_CPAD_UP) {
            SDL_Event releaseEvent;
            memset(&releaseEvent, 0, sizeof(SDL_Event));
            releaseEvent.type = SDL_KEYUP;
            releaseEvent.key.sym = SDLK_w;
            g_gui->handleEvent(releaseEvent);
        }
        if (releasedCPad & KEY_CPAD_DOWN) {
            SDL_Event releaseEvent;
            memset(&releaseEvent, 0, sizeof(SDL_Event));
            releaseEvent.type = SDL_KEYUP;
            releaseEvent.key.sym = SDLK_s;
            g_gui->handleEvent(releaseEvent);
        }
        
        lastDPad = currentDPad;
        lastCPad = currentCPad;
        
        // Convert 3DS input to SDL event for Gui
        SDL_Event sdlEvent;
        memset(&sdlEvent, 0, sizeof(SDL_Event));
        
        if (kDown & KEY_A) {
            // Toggle auto-battle instead of Enter/Return
            g_gui->autoBattle = !g_gui->autoBattle;
            
            // Display auto-battle status on bottom screen
            u16 bottomWidth, bottomHeight;
            u8* bottom_fb = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &bottomWidth, &bottomHeight);
            memset(bottom_fb, 255, bottomWidth * bottomHeight * 3);
            
            std::string statusText = g_gui->autoBattle ? "Auto-battle enabled" : "Auto-battle disabled";
            displayText(bottom_fb, fontMapData, statusText.c_str(), 0, 0, bottomWidth, bottomHeight);
            gfxFlushBuffers();
            gfxSwapBuffers();
            gspWaitForVBlank(); // Brief pause to show status
            
            // Clear bottom screen after showing status
            memset(bottom_fb, 255, bottomWidth * bottomHeight * 3);
        }
        else if (kDown & KEY_START) {
            // Save game (mapped to what used to be Enter key)
            sdlEvent.type = SDL_KEYDOWN;
            sdlEvent.key.sym = SDLK_RETURN;
            g_gui->handleEvent(sdlEvent);
        }
        else if (kDown & KEY_X) {
            // X button opens inventory (mapped to SDLK_i)
            sdlEvent.type = SDL_KEYDOWN;
            sdlEvent.key.sym = SDLK_i;
            g_gui->handleEvent(sdlEvent);
        }
        else if (kDown & KEY_L) {
            // L button switches to previous menu
            sdlEvent.type = SDL_KEYDOWN;
            sdlEvent.key.sym = SDLK_BACKSPACE; // Use backspace for previous menu
            g_gui->handleEvent(sdlEvent);
        }
        else if (kDown & KEY_R) {
            // R button switches to next menu  
            sdlEvent.type = SDL_KEYDOWN;
            sdlEvent.key.sym = SDLK_r; // Use r for next menu
            g_gui->handleEvent(sdlEvent);
        }
        else if (kDown & KEY_B) {
            sdlEvent.type = SDL_KEYDOWN;
            sdlEvent.key.sym = SDLK_ESCAPE;
            g_gui->handleEvent(sdlEvent);
        }
        else if (kDown & KEY_Y) {
            // Y button toggles flying mode
            sdlEvent.type = SDL_KEYDOWN;
            sdlEvent.key.sym = SDLK_m; // Use m key for flying (matches original)
            g_gui->handleEvent(sdlEvent);
        }
        else if (kDown & KEY_DLEFT) {
            sdlEvent.type = SDL_KEYDOWN;
            sdlEvent.key.sym = SDLK_LEFT;
            g_gui->handleEvent(sdlEvent);
        }
        else if (kDown & KEY_DRIGHT) {
            sdlEvent.type = SDL_KEYDOWN;
            sdlEvent.key.sym = SDLK_RIGHT;
            g_gui->handleEvent(sdlEvent);
        }
        else if (kDown & KEY_DUP) {
            sdlEvent.type = SDL_KEYDOWN;
            sdlEvent.key.sym = SDLK_UP;
            g_gui->handleEvent(sdlEvent);
        }
        else if (kDown & KEY_DDOWN) {
            sdlEvent.type = SDL_KEYDOWN;
            sdlEvent.key.sym = SDLK_DOWN;
            g_gui->handleEvent(sdlEvent);
        }
        else if (kDown & KEY_CPAD_LEFT) {
            sdlEvent.type = SDL_KEYDOWN;
            sdlEvent.key.sym = SDLK_a;
            g_gui->handleEvent(sdlEvent);
        }
        else if (kDown & KEY_CPAD_RIGHT) {
            sdlEvent.type = SDL_KEYDOWN;
            sdlEvent.key.sym = SDLK_d;
            g_gui->handleEvent(sdlEvent);
        }
        else if (kDown & KEY_CPAD_UP) {
            sdlEvent.type = SDL_KEYDOWN;
            sdlEvent.key.sym = SDLK_w;
            g_gui->handleEvent(sdlEvent);
        }
        else if (kDown & KEY_CPAD_DOWN) {
            sdlEvent.type = SDL_KEYDOWN;
            sdlEvent.key.sym = SDLK_s;
            g_gui->handleEvent(sdlEvent);
        }
        else if (kDown & KEY_DLEFT) {
            sdlEvent.type = SDL_KEYDOWN;
            sdlEvent.key.sym = SDLK_LEFT;
            g_gui->handleEvent(sdlEvent);
        }
        else if (kDown & KEY_DRIGHT) {
            sdlEvent.type = SDL_KEYDOWN;
            sdlEvent.key.sym = SDLK_RIGHT;
            g_gui->handleEvent(sdlEvent);
        }
        else if (kDown & KEY_DUP) {
            sdlEvent.type = SDL_KEYDOWN;
            sdlEvent.key.sym = SDLK_UP;
            g_gui->handleEvent(sdlEvent);
        }
        else if (kDown & KEY_DDOWN) {
            sdlEvent.type = SDL_KEYDOWN;
            sdlEvent.key.sym = SDLK_DOWN;
            g_gui->handleEvent(sdlEvent);
        }
        else if (kHeld & KEY_CPAD_LEFT) {
            sdlEvent.type = SDL_KEYDOWN;
            sdlEvent.key.sym = SDLK_a;
            g_gui->handleEvent(sdlEvent);
        }
        else if (kHeld & KEY_CPAD_RIGHT) {
            sdlEvent.type = SDL_KEYDOWN;
            sdlEvent.key.sym = SDLK_d;
            g_gui->handleEvent(sdlEvent);
        }
        else if (kHeld & KEY_CPAD_UP) {
            sdlEvent.type = SDL_KEYDOWN;
            sdlEvent.key.sym = SDLK_w;
            g_gui->handleEvent(sdlEvent);
        }
        else if (kHeld & KEY_CPAD_DOWN) {
            sdlEvent.type = SDL_KEYDOWN;
            sdlEvent.key.sym = SDLK_s;
            g_gui->handleEvent(sdlEvent);
        }
        else if (kDown & KEY_TOUCH) {
            // Convert touch to mouse click
            touchPosition touch;
            hidTouchRead(&touch);
            sdlEvent.type = SDL_MOUSEBUTTONDOWN;
            sdlEvent.button.button = SDL_BUTTON_LEFT;
            sdlEvent.button.x = touch.px;
            sdlEvent.button.y = touch.py;
            g_gui->handleEvent(sdlEvent);
        }
        else if (kHeld & KEY_TOUCH) {
            // Convert touch to mouse release
            touchPosition touch;
            hidTouchRead(&touch);
            sdlEvent.type = SDL_MOUSEBUTTONUP;
            sdlEvent.button.button = SDL_BUTTON_LEFT;
            sdlEvent.button.x = touch.px;
            sdlEvent.button.y = touch.py;
            g_gui->handleEvent(sdlEvent);
        }
        
        // Update and render game
        g_gui->update();
        g_gui->render();
        
        // Frame timing - 3DS equivalent of SDL_Delay(16)
        gspWaitForVBlank();
    }
}
// Helper function to start game from save file
void startFromSave(std::ifstream& saveFile, Player*& player) {
    std::string pInfo;
    std::string pcS;
    std::string partyS;
    std::string trainS;
    std::string leadS;
    std::string gioRS;
    std::string wobS;
    std::string dexS;
    std::string itemS;
    std::string tmS;
    std::string fLoc;
    std::getline(saveFile, pInfo);
    std::getline(saveFile, pcS);
    std::getline(saveFile, partyS);
    std::getline(saveFile, trainS);
    std::getline(saveFile, leadS);
    std::getline(saveFile, gioRS);
    std::getline(saveFile, wobS);
    std::getline(saveFile, dexS);
    std::getline(saveFile, itemS);
    std::getline(saveFile, tmS);
    std::getline(saveFile, fLoc);
    player = new Player(pcS, partyS, dexS, itemS, tmS);
    for (size_t i = 0; i < fLoc.size() && i + 1 < FlyLocation::INDEX_MEANINGS.size(); i++) {
        if (fLoc[i] == '1') {
            FlyLocation::FLY_LOCATIONS[FlyLocation::INDEX_MEANINGS[i + 1]]->visited = true;
        }
    }
    Trainer::buildTrainers(player, trainS, leadS, gioRS);
    WorldObject::buildWorldObjects(player, wobS);
    if (player->hasItem(Item::ITEM_MAP["Shiny Charm"])) {
        Player::SHINY_CHANCE = 256;
    }
    
    // Create global Gui instance with save info to restore map position
    g_gui = new Gui(player, pInfo);
}

// Helper function to start game from scratch
void startFromScratch(Player*& player) {
    // New game - no save file
    Trainer::buildTrainers();
    WorldObject::buildWorldObjects();
    player = new Player("Purple");
    
    // Create global Gui instance for new game
    g_gui = new Gui(player);
    
    std::string name = g_gui->promptText("Welcome to Pokemon Purple!\nWhat is your name?");
    if (!name.empty()) {
        name.erase(std::remove(name.begin(), name.end(), ','), name.end());
        if (name.empty()) {
            player->name = "Purple";
        } else {
            player->name = name.size() < 11 ? name : name.substr(0, 10);
        }
    }
    
    // Handle starter selection
    bool starterChosen = false;
    while (!starterChosen && aptMainLoop()) {
        // Clear screens
        u16 topWidth, topHeight;
        u8* top_fb = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &topWidth, &topHeight);
        memset(top_fb, 255, topWidth * topHeight * 3);
        
        u16 bottomWidth, bottomHeight;
        u8* bottom_fb = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &bottomWidth, &bottomHeight);
        memset(bottom_fb, 0, bottomWidth * bottomHeight * 3);
        
        // Draw starter selection prompt on top screen
        displayText(top_fb, fontMapData, "Which starter will you choose?", 10, 10, topWidth, topHeight);
        
        // Draw battler sprites on bottom screen in triangle arrangement
        // Top sprite: Bulbasaur (index 1) at x=115, y=20
        drawBGRAsset(bottom_fb, battlers_bgr[1], battlers_bgr_sizes[1], 20, 115, bottomWidth, bottomHeight, true);
        
        // Bottom left: Charmander (index 4) at x=25, y=130  
        drawBGRAsset(bottom_fb, battlers_bgr[4], battlers_bgr_sizes[4], 130, 25, bottomWidth, bottomHeight, true);
        
        // Bottom right: Squirtle (index 7) at x=205, y=130
        drawBGRAsset(bottom_fb, battlers_bgr[7], battlers_bgr_sizes[7], 130, 205, bottomWidth, bottomHeight, true);
        
        // Swap buffers
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
        
        // Handle input
        hidScanInput();
        u32 kDown = hidKeysDown();
        
        if (kDown & KEY_TOUCH) {
            // Get touch coordinates directly
            touchPosition touch;
            hidTouchRead(&touch);
            
            // Bottom screen is 320x240, arrange 90x90 sprites in triangle
            // Top sprite: centered at x=115, y=20
            // Bottom left: x=25, y=130
            // Bottom right: x=205, y=130
            if (touch.px >= 115 && touch.px < 205 && 
                touch.py >= 20 && touch.py < 110) {
                // Top sprite - battler_1 (Bulbasaur)
                Battler* bulbasaur = new Battler(1, &Monster::MONSTERS[1]);
                player->give(bulbasaur);
                starterChosen = true;
            }
            else if (touch.px >= 25 && touch.px < 115 && 
                     touch.py >= 130 && touch.py < 220) {
                // Bottom left - battler_4 (Charmander)
                Battler* charmander = new Battler(1, &Monster::MONSTERS[4]);
                player->give(charmander);
                starterChosen = true;
            }
            else if (touch.px >= 205 && touch.px < 295 && 
                     touch.py >= 130 && touch.py < 220) {
                // Bottom right - battler_7 (Squirtle)
                Battler* squirtle = new Battler(1, &Monster::MONSTERS[7]);
                player->give(squirtle);
                starterChosen = true;
            }
        }
    }
}

int main(int argc, char **argv) {
    // Initialize graphics - DIRECT CITRO3D LIKE PALLET_TOWN_REAL
    gfxInitDefault();
    
    // Initialize global font map data
    fontMapData = initFontMapData();
    
    // Initialize sprite map data
    initializeAllSprites();
    buildGameData();

    Player *player = nullptr;
    std::ifstream saveFile("save.txt");
    if (saveFile.good()) {
        try {
            startFromSave(saveFile, player);
        } catch (const std::exception& e) {
            debugCheckpoint("EXCEPTION in startFromSave");
        } catch (...) {
            debugCheckpoint("UNKNOWN EXCEPTION in startFromSave");
        }
    } else {
        try {
            startFromScratch(player);
        } catch (const std::exception& e) {
            debugCheckpoint("EXCEPTION in startFromScratch");
        } catch (...) {
            debugCheckpoint("UNKNOWN EXCEPTION in startFromScratch");
        }
    }
    playGame();
    debugCheckpoint("About to call gfxExit...");
    gfxExit();
    return 0;
}
