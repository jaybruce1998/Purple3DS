#pragma once

namespace GameConfig3DS {
    // Original tile size was 16x16, scaled by 3.5x = 56x56
    // New scaling is 1.5x, so 16x16 -> 24x24
    static constexpr int TILE_SIZE = 24;
    
    // Screen dimensions for 3DS
    static constexpr int TOP_SCREEN_WIDTH = 400;
    static constexpr int TOP_SCREEN_HEIGHT = 240;
    static constexpr int BOTTOM_SCREEN_WIDTH = 320;
    static constexpr int BOTTOM_SCREEN_HEIGHT = 240;
    
    // For compatibility with original GameConfig
    static constexpr int WINDOW_HEIGHT = TOP_SCREEN_HEIGHT + BOTTOM_SCREEN_HEIGHT;
    static constexpr int WORLD_HEIGHT = TOP_SCREEN_HEIGHT;
    static constexpr int UI_HEIGHT = BOTTOM_SCREEN_HEIGHT;
    
    // Original game dimensions split in half
    static constexpr int ORIGINAL_WINDOW_WIDTH = 896;
    static constexpr int ORIGINAL_WINDOW_HEIGHT = 1344;
    static constexpr int ORIGINAL_WORLD_HEIGHT = 672;
    static constexpr int ORIGINAL_UI_HEIGHT = 672;
    
    // Scale factors to fit original content on 3DS screens
    static constexpr float TOP_SCALE_X = (float)TOP_SCREEN_WIDTH / ORIGINAL_WINDOW_WIDTH;
    static constexpr float TOP_SCALE_Y = (float)TOP_SCREEN_HEIGHT / ORIGINAL_WORLD_HEIGHT;
    static constexpr float BOTTOM_SCALE_X = (float)BOTTOM_SCREEN_WIDTH / ORIGINAL_WINDOW_WIDTH;
    static constexpr float BOTTOM_SCALE_Y = (float)BOTTOM_SCREEN_HEIGHT / ORIGINAL_UI_HEIGHT;
    
    // Menu dimensions (scaled for 3DS)
    static constexpr int MENU_BOX_HEIGHT = (int)(160 * BOTTOM_SCALE_Y);
    static constexpr int MENU_BOX_MARGIN = (int)(40 * BOTTOM_SCALE_X);
    static constexpr int MENU_OPTION_HEIGHT = (int)(25 * BOTTOM_SCALE_Y);
    static constexpr int MENU_OPTION_START_OFFSET = (int)(25 * BOTTOM_SCALE_Y);
    static constexpr int MENU_OPTION_COUNT = 4;
    static constexpr int LONG_LIST_START_Y = (int)(80 * BOTTOM_SCALE_Y);
    
    // Asset scaling: 1.5x instead of 3.5x
    static constexpr float ASSET_SCALE = 1.5f;
    static constexpr int ORIGINAL_SPRITE_SIZE = 16;
    static constexpr int SCALED_SPRITE_SIZE = (int)(ORIGINAL_SPRITE_SIZE * ASSET_SCALE);
    
    // Control mappings
    enum class MenuType {
        INVENTORY = 0,
        TMS = 1,
        POKEMON = 2,
        POKEDEX = 3,
        COUNT = 4
    };
}
