// Registry of generatable maps.
//
// To add a map:
//   1. create src/<name>_map.cpp defining `void buildMyMap( MapBuilder & b )`
//   2. declare the build function below and add one MapDefinition entry
//   3. re-run gen_vcxproj.py (it globs src/*.cpp) and rebuild

#include "mapgen.h"

void buildKingsRansom( MapBuilder & b ); // kings_ransom_map.cpp
void buildAshenSuccession( MapBuilder & b ); // ashen_succession_map.cpp

const std::vector<MapDefinition> & getMapRegistry()
{
    static const std::vector<MapDefinition> registry = {
        { "kings_ransom", "The King's Ransom", 36, 20260901U, buildKingsRansom },
        { "ashen_succession", "The Ashen Succession", 72, 20260901U, buildAshenSuccession },
        // Add new maps here.
    };
    return registry;
}
