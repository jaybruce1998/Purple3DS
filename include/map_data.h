#ifndef MAP_DATA_H
#define MAP_DATA_H

#include <map>
#include <utility>
#include "all_glyphs.h"
#include "all_tiles.h"

#ifdef __cplusplus

// Initialize tile array data (801 tiles: 0-800)
std::pair<const unsigned char*, unsigned int>* initTileArrayData();

// Initialize font map data  
std::map<char, std::pair<const unsigned char*, unsigned int>> initFontMapData();

#endif

#endif // MAP_DATA_H
