#ifndef MAP_DEFINITIONS_H
#define MAP_DEFINITIONS_H

#include <vector>
#include <string>
#include <map>

// Map data structure: map name -> 2D array of tile IDs
extern std::map<std::string, std::vector<std::vector<int>>> g_allMaps;

// Function to initialize all map data from tileIds.txt
void initializeAllMaps();

// Function to get a list of all map names in order
std::vector<std::string> getMapNames();

// Function to get map data by name
const std::vector<std::vector<int>>* getMapData(const std::string& mapName);

#endif // MAP_DEFINITIONS_H
