#pragma once

// King's Ransom map generator - builder framework on top of real fheroes2 engine helpers.
// Links the full fheroes2 engine (minus the game's main) so every placement, terrain
// transition, road connection and the final serialization run through the exact code
// the in-game Editor uses.

#include <cstdint>
#include <string>
#include <vector>

#include "color.h"
#include "map_format_info.h"
#include "map_object_info.h"

class MapBuilder
{
public:
    Maps::Map_Format::MapFormat map;
    int32_t W{ 0 };

    struct PlacedObject
    {
        int32_t x{ 0 };
        int32_t y{ 0 };
        Maps::ObjectGroup group{ Maps::ObjectGroup::NONE };
        uint32_t index{ 0 };
        uint32_t uid{ 0 };
        bool isAction{ false };
        std::string label;
    };

    std::vector<PlacedObject> placed;

    // When set, place() refuses to put a ground-level object part on an already occupied tile (die with
    // the label). Off by default because King's Ransom deliberately stacks overlapping ridge mountains
    // and its output must stay byte-identical; new maps should switch it on at the top of their build.
    bool strictPlacement{ false };

    // Ground-level occupancy (OBJECT/BACKGROUND parts of every placed object), maintained by place().
    std::vector<uint8_t> occupancy;
    // Cosmetic reservations: tiles covered by a placed object's top-level sprite parts, plus the editor's
    // {-2..2, -3..1} rectangle around every town/castle. place() ignores it (a hero must stand below a
    // castle), tryPlace() and routeRoad() honour it so decor and roads never sit under a tower or crown.
    std::vector<uint8_t> softMask;
    // Tiles that already carry a road (maintained by addRoad).
    std::vector<uint8_t> roadMask;

    void init( int32_t width, uint32_t seed );

    int32_t idx( int32_t x, int32_t y ) const
    {
        return y * W + x;
    }

    // Terrain painting (with automatic transitions, same as the Editor brush).
    void paintRect( int32_t x0, int32_t y0, int32_t x1, int32_t y1, int groundId );
    void paintTile( int32_t x, int32_t y, int groundId );
    // Rough organic blob: filled ellipse with jittered edge.
    void paintBlob( int32_t cx, int32_t cy, int32_t rx, int32_t ry, int groundId );

    // Core object placement: engine setObjectOnTile (world) + addObjectToMap (map format).
    // Dies when any ground-level part of the object would land on an already occupied tile or off the
    // map, unless allowOverlap is set (used for the parts of a compound castle).
    uint32_t place( int32_t x, int32_t y, Maps::ObjectGroup group, uint32_t index, const std::string & label, bool allowOverlap = false );
    // Same as place() but returns 0 and places nothing when the footprint is not free.
    uint32_t tryPlace( int32_t x, int32_t y, Maps::ObjectGroup group, uint32_t index, const std::string & label );
    // True when every ground-level tile of the object is on the map, on land and unoccupied.
    bool canPlace( int32_t x, int32_t y, Maps::ObjectGroup group, uint32_t index ) const;
    // True for water tiles, off-map tiles and tiles holding a ground-level object part.
    bool isOccupied( int32_t x, int32_t y ) const;

    // Compound castle/town, exactly like EditorInterface::_placeCastle.
    uint32_t placeCastle( int32_t x, int32_t y, PlayerColor color, int raceSlot, bool isCastle, const std::string & name );

    uint32_t placeHero( int32_t x, int32_t y, PlayerColor color, int raceSlot, const std::string & name );
    uint32_t placeMine( int32_t x, int32_t y, int resourceType, const std::string & label );
    uint32_t placeMonster( int32_t x, int32_t y, int monsterId, int32_t count, const std::string & label );
    uint32_t placeResourcePile( int32_t x, int32_t y, int resourceType, int32_t amount ); // amount 0 = engine random
    uint32_t placeChest( int32_t x, int32_t y );
    uint32_t placeArtifact( int32_t x, int32_t y, int artifactId, const std::string & label );
    uint32_t placeSign( int32_t x, int32_t y, uint32_t signIndex, const std::string & message );

    void addRoad( int32_t x, int32_t y );
    void addStreamTile( int32_t x, int32_t y );

    // Recompute engine passability for the whole world (placement itself does not).
    void refreshPassability();
    // Lay a road along the cheapest passable path between two tiles (monsters are walked through,
    // other action objects are avoided). Returns false when no path exists.
    bool routeRoad( int32_t x0, int32_t y0, int32_t x1, int32_t y1 );

    // Cheapest movement cost between two tiles under the engine's rules (a step costs the source tile's
    // ground penalty without Pathfinding, 75 when both tiles carry a road, diagonal steps x1.5), with
    // monsters and pickups walked through and guard protection ignored. -1 when unreachable. Used for
    // per-player fairness tables. Requires current passability.
    int32_t movementCost( int32_t x0, int32_t y0, int32_t x1, int32_t y1 ) const;

    // Prints every placed monster stack and castle garrison with engine army strength, plus a
    // per-monster unit table, for guard calibration (consumed by mapgen/guard_model.py).
    void printStrengthReport() const;

    // Finalization: updateMapPlayers + world passability refresh. Returns false when the
    // engine considers the map broken.
    bool finalize();

    bool save( const std::string & path ) const;

    // --- validation helpers ---
    // BFS over engine passability from a start tile; fills 'reachable' (one bool per tile).
    // With monstersBlock the BFS never steps onto a monster tile nor onto a tile the engine reports as
    // protected by a monster (Maps::isTileUnderProtection, the same rule the game applies when a hero
    // moves): the result is what a hero can reach without fighting a single guard (the wall-leak test).
    // Requires current passability (call refreshPassability() or finalize() first).
    void computeReachability( int32_t startX, int32_t startY, std::vector<bool> & reachable, bool monstersBlock = false ) const;
    // Returns list of placed action objects that cannot be interacted with from the reachable set.
    std::vector<std::string> checkActionReachability( const std::vector<bool> & reachable ) const;

    std::string asciiTerrain() const;
    std::string asciiPassability( const std::vector<bool> & reachable ) const;
};

// A generatable map. Each scenario lives in its own src/<name>_map.cpp defining a
// build function, and is listed in the registry (src/map_registry.cpp).
struct MapDefinition
{
    const char * name; // CLI name, e.g. "kings_ransom"
    const char * title; // human-readable title
    int32_t width; // 36 / 72 / 108 / 144
    uint32_t defaultSeed; // deterministic default
    void ( *build )( MapBuilder & );
};

const std::vector<MapDefinition> & getMapRegistry();
