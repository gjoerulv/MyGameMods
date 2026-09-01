// "The King's Ransom" - 36x36 economy scenario definition.
//
// Blue (human, Knight, SW) must POSSESS 100,000 gold before the end of month 2 (day 56).
// Green (AI, Knight, NE) is a rival lord who contests the middle of the map.
//
// Layout: a mountain ridge runs NW->SE splitting the map into Blue's south-west and
// Green's north-east. Three lanes connect them:
//   - the West Road (snow): long, safe-ish, gold mine + crystal mine, dwarf country;
//   - the King's Vale (dirt basin in the ridge gap): neutral town, the King's Mine
//     (gold), gems mine, trading post - the contested economic heart;
//   - the Southern Sands (desert): optional treasure route - Sphinx, campfires,
//     a guarded hoard and the Endless Purse of Gold.
//
// All object table indices below come from the research notes
// (research/notes/02_tiles_objectgroups_tables.md) verified at fheroes2 @ b086d1aa.

#include <string>

#include "mapgen.h"

#include "artifact.h"
#include "castle.h"
#include "ground.h"
#include "monster.h"
#include "race.h"
#include "rand.h"
#include "resource.h"

namespace
{
    // ---- guard tuning (single place to balance the map) ----
    constexpr int32_t GUARD_PASS_B_ROGUES = 8; // west road entry
    constexpr int32_t GUARD_HOME_CHEST_ROGUES = 8; // Blue home guarded chest
    constexpr int32_t GUARD_CRYSTAL_DWARVES = 14;
    constexpr int32_t GUARD_GM1_DWARVES = 35; // western gold mine (~day 11-13 for good play)
    constexpr int32_t GUARD_NORTH_ZOMBIES = 28; // northern corridor
    constexpr int32_t GUARD_PASS_A_WOLVES = 16; // main pass into the vale
    constexpr int32_t GUARD_NE_GATE_ROGUES = 24; // Green-side gate into the vale
    constexpr int32_t GUARD_KINGS_MINE_NOMADS = 50; // the central gold mine (~day 15-18)
    constexpr int32_t GUARD_GEMS_NOMADS = 25;
    constexpr int32_t GUARD_TRADING_POST_ROGUES = 15;
    constexpr int32_t GUARD_GREEN_GOLD_ROGUES = 12; // Green's home gold mine (AI breaks it week 2)
    constexpr int32_t GUARD_SULFUR_ROGUES = 10; // Green-side sulfur mine
    constexpr int32_t GUARD_PASS_C_MEDUSAS = 30; // east pass (desert <-> Green)
    constexpr int32_t GUARD_DESERT_GATE_NOMADS = 25; // south-west desert entry
    constexpr int32_t GUARD_PURSE_NOMADS = 50; // Endless Purse of Gold
    constexpr int32_t GUARD_TROVE_ROGUES = 35; // SE corner hoard

    // ---- object table indices (verified against map_object_info.cpp @ b086d1aa) ----
    // ADVENTURE_MISCELLANEOUS
    constexpr uint32_t MISC_GRAVEYARD_SNOW = 1;
    constexpr uint32_t MISC_WINDMILL_SNOW = 5;
    constexpr uint32_t MISC_SIGN_DESERT = 21;
    constexpr uint32_t MISC_SIGN_GENERIC = 22;
    constexpr uint32_t MISC_WATER_WHEEL_GENERIC = 24;
    constexpr uint32_t MISC_SKELETON_DESERT = 30;
    constexpr uint32_t MISC_SPHINX = 31;
    constexpr uint32_t MISC_TRADING_POST_GENERIC = 33;
    constexpr uint32_t MISC_MAGIC_GARDEN = 41;
    constexpr uint32_t MISC_OBS_TOWER_SNOW = 45;
    // ADVENTURE_POWER_UPS
    constexpr uint32_t PU_FAERIE_RING_GRASS = 3;
    constexpr uint32_t PU_FOUNTAIN = 6;
    constexpr uint32_t PU_GAZEBO = 11;
    constexpr uint32_t PU_WITCH_DOCTORS_HUT = 12;
    constexpr uint32_t PU_TREE_OF_KNOWLEDGE = 20;
    // ADVENTURE_DWELLINGS
    constexpr uint32_t DW_PEASANT_HUT = 0;
    constexpr uint32_t DW_DESERT_TENT = 9;
    constexpr uint32_t DW_ARCHERS_HOUSE = 13;
    constexpr uint32_t DW_WAGON_CAMP = 17;
    // ADVENTURE_TREASURES
    constexpr uint32_t TR_CAMPFIRE = 10;
    constexpr uint32_t TR_CAMPFIRE_DESERT = 12;
    // LANDSCAPE_MOUNTAINS (big = 5x3 footprint)
    constexpr uint32_t MTN_GENERIC_BIG1 = 0;
    constexpr uint32_t MTN_GRASS_BIG1 = 6;
    constexpr uint32_t MTN_GRASS_BIG2 = 7;
    constexpr uint32_t MTN_GRASS_MED1 = 8;
    constexpr uint32_t MTN_SNOW_BIG1 = 12;
    constexpr uint32_t MTN_SNOW_BIG2 = 13;
    constexpr uint32_t MTN_DIRT_BIG1 = 36;
    constexpr uint32_t MTN_DIRT_BIG2 = 37;
    constexpr uint32_t MTN_DIRT_MED1 = 40;
    constexpr uint32_t MTN_DESERT_DUNE1 = 58;
    constexpr uint32_t MTN_DESERT_DUNE3 = 60;
    constexpr uint32_t MTN_DIRT_MOUND1 = 61;
    // LANDSCAPE_TREES
    constexpr uint32_t TREE_DECID_BIG1 = 0;
    constexpr uint32_t TREE_DECID_MED1 = 2;
    constexpr uint32_t TREE_DECID_SMALL1 = 4;
    constexpr uint32_t TREE_DECID_SMALL2 = 5;
    constexpr uint32_t TREE_AUTUMN_BIG1 = 12;
    constexpr uint32_t TREE_AUTUMN_MED1 = 14;
    constexpr uint32_t TREE_AUTUMN_SMALL1 = 16;
    constexpr uint32_t TREE_SNOWFIR_BIG1 = 30;
    constexpr uint32_t TREE_SNOWFIR_MED1 = 32;
    constexpr uint32_t TREE_SNOWFIR_SMALL1 = 34;
    constexpr uint32_t TREE_GRASS_THREE = 36;
    constexpr uint32_t TREE_GRASS_SINGLE = 38;
    constexpr uint32_t TREE_SNOW_DEAD1 = 45;
    constexpr uint32_t TREE_DESERT_PALMS_TALL = 54;
    constexpr uint32_t TREE_DESERT_PALM_LONELY = 57;
    constexpr uint32_t TREE_DESERT_PALM_SMALL = 58;
    constexpr uint32_t TREE_GENERIC_DEAD = 64;
    constexpr uint32_t TREE_GENERIC_STUMPS = 66;
    // LANDSCAPE_ROCKS
    constexpr uint32_t ROCK_GRASS_MEDIUM = 2;
    constexpr uint32_t ROCK_GRASS_SMALL1 = 4;
    constexpr uint32_t ROCK_GRASS_SMALL2 = 6;
    constexpr uint32_t ROCK_SNOW_SMALL = 11;
    constexpr uint32_t ROCK_SNOW_MEDIUM = 14;
    constexpr uint32_t ROCK_DIRT_MEDIUM = 24;
    constexpr uint32_t ROCK_DIRT_SMALL = 25;
    // LANDSCAPE_MISCELLANEOUS (decor)
    constexpr uint32_t LM_GRASS_LAKE_MEDIUM = 5;
    constexpr uint32_t LM_GRASS_SHRUB_WIDE = 7;
    constexpr uint32_t LM_GRASS_FLOWERS1 = 13;
    constexpr uint32_t LM_GRASS_FLOWERS2 = 20;
    constexpr uint32_t LM_GRASS_FLOWERS_SMALL = 21;
    constexpr uint32_t LM_SNOW_FROZEN_LAKE_MED = 38;
    constexpr uint32_t LM_DIRT_SHRUB_WIDE = 106;
    constexpr uint32_t LM_DIRT_FLOWERS = 111;
    constexpr uint32_t LM_DIRT_MEADOW = 121;
    constexpr uint32_t LM_DESERT_CACTUS_MED1 = 87;
    constexpr uint32_t LM_DESERT_CACTUS_MED2 = 92;
    constexpr uint32_t LM_DESERT_CACTUS_SMALL = 90;

    void terrain( MapBuilder & b )
    {
        using namespace Maps;

        // Base: everything grass.
        b.paintRect( 0, 0, 35, 35, Ground::GRASS );

        // West / north-west snow country. The eastern edge is a bounded random walk so the
        // boundary reads as a natural ragged treeline, not a painted rectangle.
        {
            int32_t edge = 8;
            for ( int32_t y = 0; y <= 23; ++y ) {
                int32_t lo = 7;
                int32_t hi = 11;
                if ( y >= 21 ) {
                    lo = 4;
                    hi = 6;
                }
                else if ( y >= 15 ) {
                    lo = 6;
                    hi = 9;
                }
                edge += static_cast<int32_t>( Rand::Get( 0, 2 ) ) - 1;
                if ( edge < lo ) {
                    edge = lo;
                }
                if ( edge > hi ) {
                    edge = hi;
                }
                b.paintRect( 0, y, edge, y, Ground::SNOW );
            }
        }

        // Central dirt basin - the King's Vale.
        b.paintBlob( 18, 15, 7, 6, Ground::DIRT );
        b.paintBlob( 15, 18, 5, 4, Ground::DIRT );
        b.paintBlob( 21, 12, 5, 4, Ground::DIRT );

        // South-east desert - the Southern Sands. Bulk rectangles first (a 1-tile-wide strip
        // cannot get transition images and the engine's recovery would repaint it), then a
        // random-walk fringe on top of the bulk. Clamps keep the desert-variant objects
        // (tent, palms, sphinx, dunes) on sand.
        b.paintRect( 27, 28, 35, 35, Ground::DESERT );
        b.paintRect( 25, 31, 26, 35, Ground::DESERT );
        {
            int32_t top = 30;
            for ( int32_t x = 25; x <= 35; ++x ) {
                int32_t lo = 29;
                int32_t hi = 31;
                if ( x >= 33 ) {
                    lo = 25;
                    hi = 27;
                }
                else if ( x >= 30 ) {
                    lo = 26;
                    hi = 28;
                }
                else if ( x >= 27 ) {
                    lo = 27;
                    hi = 29;
                }
                top += static_cast<int32_t>( Rand::Get( 0, 2 ) ) - 1;
                if ( top < lo ) {
                    top = lo;
                }
                if ( top > hi ) {
                    top = hi;
                }
                // Extend the fringe from the bulk upward: paint a 2-tile-tall band ending at
                // the walk position so every painted tile touches existing desert.
                b.paintRect( x, top, x, ( x >= 27 ? 29 : 32 ), Ground::DESERT );
            }
        }

        // Wasteland scar around the east pass.
        b.paintBlob( 32, 24, 3, 2, Ground::WASTELAND );
    }

    void ridge( MapBuilder & b )
    {
        using G = Maps::ObjectGroup;

        // South-western wall of the vale (NW -> SE). Big mountains have a 5x3 footprint.
        b.place( 5, 10, G::LANDSCAPE_MOUNTAINS, MTN_SNOW_BIG1, "ridge A (snow)" );
        b.place( 9, 12, G::LANDSCAPE_MOUNTAINS, MTN_GRASS_BIG1, "ridge B" );
        b.place( 12, 14, G::LANDSCAPE_MOUNTAINS, MTN_GRASS_BIG2, "ridge C" );
        b.place( 15, 17, G::LANDSCAPE_MOUNTAINS, MTN_DIRT_BIG1, "ridge D" );
        // --- Pass A gap (around y19, x13..18) ---
        b.place( 19, 21, G::LANDSCAPE_MOUNTAINS, MTN_DIRT_BIG2, "ridge E" );
        b.place( 23, 22, G::LANDSCAPE_MOUNTAINS, MTN_GRASS_BIG1, "ridge F" );
        b.place( 26, 23, G::LANDSCAPE_MOUNTAINS, MTN_GRASS_BIG2, "ridge G" );
        b.place( 29, 24, G::LANDSCAPE_MOUNTAINS, MTN_GRASS_BIG1, "ridge H" );
        // --- Pass C gap (x31..35, y23..26) ---
        b.place( 34, 28, G::LANDSCAPE_MOUNTAINS, MTN_GENERIC_BIG1, "ridge I (desert side)" );

        // Northern wall of the vale (separates the north corridor).
        b.place( 15, 8, G::LANDSCAPE_MOUNTAINS, MTN_GRASS_BIG2, "vale north wall K" );
        b.place( 19, 9, G::LANDSCAPE_MOUNTAINS, MTN_GRASS_BIG1, "vale north wall L" );

        // Seal the ridge's west flank so the Pass A guard cannot be bypassed from Blue's side.
        b.place( 6, 15, G::LANDSCAPE_MOUNTAINS, MTN_SNOW_BIG2, "ridge west flank (snow)" );
        b.place( 10, 17, G::LANDSCAPE_MOUNTAINS, MTN_DIRT_BIG1, "ridge west flank (dirt)" );
        b.place( 10, 9, G::LANDSCAPE_MOUNTAINS, MTN_DIRT_MED1, "ridge west flank north" );
        b.place( 8, 8, G::LANDSCAPE_ROCKS, ROCK_SNOW_SMALL, "flank pocket rock" );
        b.place( 8, 13, G::LANDSCAPE_ROCKS, ROCK_DIRT_MEDIUM, "flank pocket rock 2" );
        b.place( 9, 14, G::LANDSCAPE_ROCKS, ROCK_DIRT_SMALL, "flank pocket rock 3" );

        // Plug the accidental slit between ridge B and wall K (column x12).
        b.place( 12, 10, G::LANDSCAPE_TREES, TREE_DECID_MED1, "ridge filler tree" );
        b.place( 12, 11, G::LANDSCAPE_ROCKS, ROCK_GRASS_MEDIUM, "ridge filler rock" );
        b.place( 12, 12, G::LANDSCAPE_ROCKS, ROCK_GRASS_SMALL1, "ridge filler rock 2" );

        // Narrow the NE gate into the vale.
        b.place( 26, 11, G::LANDSCAPE_TREES, TREE_DECID_MED1, "NE gate tree" );
        b.place( 22, 9, G::LANDSCAPE_ROCKS, ROCK_GRASS_SMALL2, "NE gate rock" );

        // South wall of pass C, desert side.
        b.place( 31, 27, G::LANDSCAPE_MOUNTAINS, MTN_DESERT_DUNE3, "pass C dune" );
    }

    void blueHome( MapBuilder & b )
    {
        using G = Maps::ObjectGroup;

        b.placeCastle( 8, 28, PlayerColor::BLUE, 0 /*Knight*/, true /*castle*/, "Highmarch" );
        {
            auto & meta = b.map.castleMetadata[b.placed.back().uid];
            meta.customBuildings = true;
            meta.builtBuildings = { BUILD_CASTLE, DWELLING_MONSTER1, DWELLING_MONSTER2 };
        }
        b.placeHero( 8, 29, PlayerColor::BLUE, 0 /*Knight*/, "Sir Aldric" );

        b.place( 13, 31, G::ADVENTURE_MINES, 48, "Blue sawmill" ); // grass sawmill
        b.placeMine( 4, 24, Resource::ORE, "Blue ore mine" );

        b.place( 13, 25, G::ADVENTURE_MISCELLANEOUS, MISC_WATER_WHEEL_GENERIC, "water wheel" );

        b.placeChest( 11, 33 );
        b.placeChest( 15, 27 );
        b.placeMonster( 15, 28, Monster::ROGUE, GUARD_HOME_CHEST_ROGUES, "home chest guard" );

        b.placeResourcePile( 10, 30, Resource::WOOD, 0 );
        b.placeResourcePile( 5, 32, Resource::WOOD, 0 );
        b.placeResourcePile( 3, 29, Resource::ORE, 0 );
        b.placeResourcePile( 13, 22, Resource::GOLD, 0 );

        b.place( 4, 31, G::ADVENTURE_DWELLINGS, DW_PEASANT_HUT, "peasant hut" );
        b.place( 12, 34, G::ADVENTURE_DWELLINGS, DW_ARCHERS_HOUSE, "archer's house" );
        b.place( 16, 32, G::ADVENTURE_POWER_UPS, PU_GAZEBO, "gazebo" );
        b.place( 10, 24, G::ADVENTURE_POWER_UPS, PU_FOUNTAIN, "fountain" );
        b.place( 5, 34, G::ADVENTURE_POWER_UPS, PU_FAERIE_RING_GRASS, "faerie ring" );

        b.placeSign( 11, 30, MISC_SIGN_GENERIC,
                     "East through the vale lies Marketstead, and beyond it Lord Renfrew's Ironvale. The King's Mine pays a fortune - to whoever can take it from the Iron Company." );
    }

    void westRoadRegion( MapBuilder & b )
    {
        using G = Maps::ObjectGroup;

        b.placeMonster( 1, 10, Monster::ROGUE, GUARD_PASS_B_ROGUES, "pass B guard" );

        // Crystal mine in the far NW snow.
        b.placeMine( 3, 5, Resource::CRYSTAL, "crystal mine" );
        b.placeMonster( 3, 6, Monster::DWARF, GUARD_CRYSTAL_DWARVES, "crystal mine guard" );

        // Western gold mine (GM1).
        b.placeMine( 3, 16, Resource::GOLD, "western gold mine" );
        b.placeMonster( 3, 17, Monster::DWARF, GUARD_GM1_DWARVES, "western gold mine guard" );

        b.place( 5, 20, G::ADVENTURE_MISCELLANEOUS, MISC_WINDMILL_SNOW, "windmill" );
        b.place( 2, 22, G::ADVENTURE_MISCELLANEOUS, MISC_OBS_TOWER_SNOW, "observation tower" );

        b.placeChest( 1, 14 );
        b.placeResourcePile( 1, 6, Resource::CRYSTAL, 0 );
        b.placeResourcePile( 1, 18, Resource::GOLD, 0 );
        b.placeResourcePile( 5, 18, Resource::WOOD, 0 );

        b.placeSign( 1, 17, MISC_SIGN_GENERIC, "The dwarf-road: slow, cold, and honest. Their gold is real enough." );
    }

    void northCorridor( MapBuilder & b )
    {
        using G = Maps::ObjectGroup;

        b.place( 2, 7, G::ADVENTURE_MISCELLANEOUS, MISC_GRAVEYARD_SNOW, "graveyard" );
        b.placeMonster( 14, 3, Monster::ZOMBIE, GUARD_NORTH_ZOMBIES, "north corridor guard" );
        b.place( 10, 4, G::ADVENTURE_POWER_UPS, PU_WITCH_DOCTORS_HUT, "witch doctor's hut" );
        b.place( 12, 5, G::ADVENTURE_POWER_UPS, PU_TREE_OF_KNOWLEDGE, "tree of knowledge" );

        b.placeResourcePile( 9, 2, Resource::GOLD, 0 );
        b.placeResourcePile( 16, 2, Resource::ORE, 0 );
        b.placeChest( 18, 4 );
    }

    void kingsVale( MapBuilder & b )
    {
        using G = Maps::ObjectGroup;

        // Neutral town.
        b.placeCastle( 19, 14, PlayerColor::NONE, 0 /*Knight*/, false /*town*/, "Marketstead" );
        {
            auto & meta = b.map.castleMetadata[b.placed.back().uid];
            meta.customBuildings = true;
            meta.builtBuildings = { BUILD_TENT, DWELLING_MONSTER1 };
            meta.defenderMonsterType = { Monster::PEASANT, Monster::ARCHER, Monster::PIKEMAN, Monster::SWORDSMAN, Monster::CAVALRY };
            meta.defenderMonsterCount = { 35, 12, 15, 8, 3 };
        }

        // The King's Mine.
        b.placeMine( 15, 12, Resource::GOLD, "the King's Mine" );
        b.placeMonster( 15, 13, Monster::NOMAD, GUARD_KINGS_MINE_NOMADS, "King's Mine guard" );

        // Gems mine on the east side of the vale.
        b.placeMine( 23, 17, Resource::GEMS, "gems mine" );
        b.placeMonster( 23, 18, Monster::NOMAD, GUARD_GEMS_NOMADS, "gems mine guard" );

        // Trading post: marketplace rates without owning three marketplaces.
        b.place( 21, 17, G::ADVENTURE_MISCELLANEOUS, MISC_TRADING_POST_GENERIC, "trading post" );
        b.placeMonster( 21, 18, Monster::ROGUE, GUARD_TRADING_POST_ROGUES, "trading post guard" );

        // Bandit camp: recruit Rogues (cheap chaff for the ransom war).
        b.place( 17, 10, G::ADVENTURE_DWELLINGS, DW_WAGON_CAMP, "rogue wagon camp" );

        // Dress the walled-off hollow behind the King's Mine (visible but unreachable).
        b.place( 14, 14, G::LANDSCAPE_MOUNTAINS, MTN_DIRT_MOUND1, "hollow filler" );
        b.place( 16, 14, G::LANDSCAPE_ROCKS, ROCK_DIRT_SMALL, "hollow filler" );
        b.place( 13, 15, G::LANDSCAPE_ROCKS, ROCK_DIRT_MEDIUM, "hollow filler" );
        b.place( 15, 15, G::LANDSCAPE_MISCELLANEOUS, LM_DIRT_SHRUB_WIDE, "hollow filler" );

        b.place( 22, 13, G::ADVENTURE_MISCELLANEOUS, MISC_MAGIC_GARDEN, "magic garden" );

        b.placeResourcePile( 18, 11, Resource::GOLD, 0 );
        b.placeResourcePile( 17, 12, Resource::SULFUR, 0 );
        b.placeResourcePile( 21, 11, Resource::CRYSTAL, 0 );
        b.placeChest( 20, 18 );

        // Gate guards.
        b.placeMonster( 16, 19, Monster::WOLF, GUARD_PASS_A_WOLVES, "pass A guard" );
        b.placeMonster( 24, 12, Monster::ROGUE, GUARD_NE_GATE_ROGUES, "NE gate guard" );
    }

    void greenHome( MapBuilder & b )
    {
        using G = Maps::ObjectGroup;

        b.placeCastle( 28, 6, PlayerColor::GREEN, 0 /*Knight*/, true /*castle*/, "Ironvale" );
        {
            auto & meta = b.map.castleMetadata[b.placed.back().uid];
            meta.customBuildings = true;
            meta.builtBuildings = { BUILD_CASTLE, DWELLING_MONSTER1, DWELLING_MONSTER2 };
        }
        b.placeHero( 28, 7, PlayerColor::GREEN, 0 /*Knight*/, std::string() );

        b.place( 33, 8, G::ADVENTURE_MINES, 48, "Green sawmill" ); // grass sawmill
        b.placeMine( 24, 3, Resource::ORE, "Green ore mine" );

        b.placeMine( 32, 3, Resource::GOLD, "Green gold mine" );
        b.placeMonster( 32, 4, Monster::ROGUE, GUARD_GREEN_GOLD_ROGUES, "Green gold mine guard" );

        b.place( 25, 8, G::ADVENTURE_MISCELLANEOUS, MISC_MAGIC_GARDEN, "Green magic garden" );

        b.placeChest( 26, 2 );
        b.placeResourcePile( 30, 2, Resource::WOOD, 0 );
        b.placeResourcePile( 34, 5, Resource::ORE, 0 );
        b.placeResourcePile( 25, 5, Resource::WOOD, 0 );

        // Green's east lane (between home and pass C).
        b.placeMine( 32, 10, Resource::SULFUR, "sulfur mine" );
        b.placeMonster( 32, 11, Monster::ROGUE, GUARD_SULFUR_ROGUES, "sulfur mine guard" );
        b.placeChest( 33, 13 );
        b.placeResourcePile( 31, 18, Resource::ORE, 0 );
        b.placeResourcePile( 33, 20, Resource::GOLD, 0 );
    }

    void southernSands( MapBuilder & b )
    {
        using G = Maps::ObjectGroup;

        // South strip leading to the desert.
        b.placeSign( 19, 33, MISC_SIGN_GENERIC, "Beyond the sands the Iron Company counts its stolen gold. Few who go to reclaim it return." );
        b.placeChest( 22, 34 );
        b.place( 21, 31, G::ADVENTURE_TREASURES, TR_CAMPFIRE, "campfire (south strip)" );

        b.placeMonster( 26, 32, Monster::NOMAD, GUARD_DESERT_GATE_NOMADS, "desert gate guard" );

        // Desert proper.
        b.place( 33, 29, G::ADVENTURE_MISCELLANEOUS, MISC_SPHINX, "sphinx" );
        {
            auto & meta = b.map.sphinxMetadata[b.placed.back().uid];
            meta.riddle = "Neither felt nor seen, I am paid for a life unlived. Kings dread me, brigands crave me. Name me.";
            meta.answers = { "ransom", "a ransom", "the ransom" };
            meta.resources.gold = 3000;
        }

        b.placeArtifact( 30, 30, Artifact::ENDLESS_PURSE_GOLD, "Endless Purse of Gold" );
        b.placeMonster( 30, 31, Monster::NOMAD, GUARD_PURSE_NOMADS, "Endless Purse guard" );

        b.place( 27, 31, G::ADVENTURE_MISCELLANEOUS, MISC_SKELETON_DESERT, "skeleton remains" );
        b.place( 28, 29, G::ADVENTURE_DWELLINGS, DW_DESERT_TENT, "desert tent (nomads)" );
        b.place( 35, 31, G::ADVENTURE_TREASURES, TR_CAMPFIRE_DESERT, "campfire (desert east)" );
        b.placeChest( 28, 34 );

        // The SE hoard: chest + gold piles clustered around one strong guard.
        b.placeMonster( 33, 33, Monster::ROGUE, GUARD_TROVE_ROGUES, "hoard guard" );
        b.placeResourcePile( 33, 32, Resource::GOLD, 0 );
        b.placeResourcePile( 34, 33, Resource::GOLD, 0 );
        b.placeChest( 32, 33 );
        b.placeChest( 33, 34 );

        // East pass between the desert and Green's lane.
        b.placeMonster( 33, 24, Monster::MEDUSA, GUARD_PASS_C_MEDUSAS, "pass C guard" );
    }

    void roadsAndStreams( MapBuilder & b )
    {
        // Stream emerging below the ridge, running down past the water wheel.
        for ( int32_t y = 19; y <= 24; ++y ) {
            b.addStreamTile( 12, y );
        }

        // Main road: Highmarch -> pass A -> Marketstead -> NE gate -> Ironvale.
        const int32_t mainRoad[][2] = {
            { 8, 29 },  { 9, 29 },  { 10, 28 }, { 11, 27 }, { 12, 26 }, { 13, 26 }, { 14, 25 }, { 14, 24 }, { 15, 23 }, { 15, 22 }, { 15, 21 },
            { 15, 20 }, { 15, 19 }, { 16, 19 }, { 17, 19 }, { 18, 19 }, { 19, 18 }, { 19, 17 }, { 19, 16 }, { 21, 16 }, { 22, 15 }, { 23, 14 },
            { 23, 13 }, { 24, 12 }, { 25, 11 }, { 26, 10 }, { 27, 9 },  { 27, 8 },  { 28, 7 },
        };
        for ( const auto & p : mainRoad ) {
            b.addRoad( p[0], p[1] );
        }

        // West road: Highmarch -> ore mine -> the dwarf road up to the western gold mine.
        const int32_t westRoad[][2] = {
            { 8, 30 }, { 7, 30 }, { 6, 30 }, { 5, 29 }, { 4, 28 }, { 4, 27 }, { 4, 26 }, { 4, 25 },
            { 3, 26 }, { 2, 25 }, { 2, 24 }, { 2, 23 }, { 1, 22 }, { 1, 21 }, { 2, 20 }, { 2, 19 }, { 2, 18 }, { 3, 17 },
        };
        for ( const auto & p : westRoad ) {
            b.addRoad( p[0], p[1] );
        }
    }

    void decorations( MapBuilder & b )
    {
        using G = Maps::ObjectGroup;

        // Blue home: hedgerows and fields.
        b.place( 17, 29, G::LANDSCAPE_TREES, TREE_AUTUMN_MED1, "deco" );
        b.place( 17, 25, G::LANDSCAPE_TREES, TREE_DECID_SMALL1, "deco" );
        b.place( 2, 27, G::LANDSCAPE_TREES, TREE_DECID_SMALL2, "deco" );
        b.place( 10, 35, G::LANDSCAPE_TREES, TREE_DECID_MED1, "deco" );
        b.place( 6, 25, G::LANDSCAPE_MISCELLANEOUS, LM_GRASS_FLOWERS1, "deco" );
        b.place( 9, 31, G::LANDSCAPE_MISCELLANEOUS, LM_GRASS_FLOWERS_SMALL, "deco" );
        b.place( 12, 28, G::LANDSCAPE_MISCELLANEOUS, LM_GRASS_SHRUB_WIDE, "deco" );
        b.place( 16, 34, G::LANDSCAPE_ROCKS, ROCK_GRASS_SMALL1, "deco" );
        b.place( 3, 33, G::LANDSCAPE_MISCELLANEOUS, LM_GRASS_LAKE_MEDIUM, "deco lake" );
        b.place( 14, 29, G::LANDSCAPE_MISCELLANEOUS, LM_GRASS_FLOWERS2, "deco" );
        b.place( 6, 22, G::LANDSCAPE_TREES, TREE_GRASS_THREE, "deco" );
        b.place( 18, 31, G::LANDSCAPE_TREES, TREE_DECID_BIG1, "deco forest" );
        b.place( 19, 29, G::LANDSCAPE_TREES, TREE_AUTUMN_SMALL1, "deco" );

        // Snow west.
        b.place( 1, 13, G::LANDSCAPE_TREES, TREE_SNOWFIR_BIG1, "deco" );
        b.place( 6, 19, G::LANDSCAPE_TREES, TREE_SNOWFIR_MED1, "deco" );
        b.place( 5, 8, G::LANDSCAPE_MISCELLANEOUS, LM_SNOW_FROZEN_LAKE_MED, "deco" );
        b.place( 6, 13, G::LANDSCAPE_ROCKS, ROCK_SNOW_SMALL, "deco" );
        b.place( 1, 8, G::LANDSCAPE_TREES, TREE_SNOWFIR_SMALL1, "deco" );
        b.place( 5, 23, G::LANDSCAPE_ROCKS, ROCK_SNOW_MEDIUM, "deco" );
        b.place( 0, 4, G::LANDSCAPE_TREES, TREE_SNOW_DEAD1, "deco" );

        // North corridor: cold dead country.
        b.place( 10, 6, G::LANDSCAPE_TREES, TREE_GENERIC_DEAD, "deco" );
        b.place( 13, 6, G::LANDSCAPE_TREES, TREE_GENERIC_STUMPS, "deco" );
        b.place( 17, 2, G::LANDSCAPE_TREES, TREE_DECID_MED1, "deco" );
        b.place( 20, 3, G::LANDSCAPE_TREES, TREE_DECID_SMALL1, "deco" );
        b.place( 11, 2, G::LANDSCAPE_ROCKS, ROCK_GRASS_SMALL2, "deco" );
        b.place( 20, 1, G::LANDSCAPE_TREES, TREE_AUTUMN_MED1, "deco" );
        b.place( 24, 1, G::LANDSCAPE_TREES, TREE_DECID_SMALL2, "deco" );

        // NE corner behind Ironvale.
        b.place( 34, 1, G::LANDSCAPE_TREES, TREE_AUTUMN_SMALL1, "deco" );
        b.place( 31, 0, G::LANDSCAPE_ROCKS, ROCK_GRASS_SMALL1, "deco" );

        // The vale: worked-out bandit country.
        b.place( 14, 10, G::LANDSCAPE_MOUNTAINS, MTN_DIRT_MOUND1, "deco" );
        b.place( 20, 11, G::LANDSCAPE_MISCELLANEOUS, LM_DIRT_SHRUB_WIDE, "deco" );
        b.place( 17, 20, G::LANDSCAPE_MISCELLANEOUS, LM_DIRT_FLOWERS, "deco" );
        b.place( 22, 19, G::LANDSCAPE_ROCKS, ROCK_DIRT_MEDIUM, "deco" );
        b.place( 13, 18, G::LANDSCAPE_ROCKS, ROCK_DIRT_SMALL, "deco" );
        b.place( 21, 20, G::LANDSCAPE_MISCELLANEOUS, LM_DIRT_MEADOW, "deco" );

        // Green home.
        b.place( 31, 6, G::LANDSCAPE_TREES, TREE_DECID_MED1, "deco" );
        b.place( 24, 6, G::LANDSCAPE_TREES, TREE_DECID_SMALL1, "deco" );
        b.place( 34, 12, G::LANDSCAPE_ROCKS, ROCK_GRASS_MEDIUM, "deco" );
        b.place( 29, 2, G::LANDSCAPE_TREES, TREE_AUTUMN_MED1, "deco" );
        b.place( 30, 14, G::LANDSCAPE_TREES, TREE_GRASS_SINGLE, "deco" );
        b.place( 34, 16, G::LANDSCAPE_TREES, TREE_AUTUMN_SMALL1, "deco" );
        b.place( 30, 20, G::LANDSCAPE_TREES, TREE_DECID_MED1, "deco" );

        // Desert.
        b.place( 29, 33, G::LANDSCAPE_MISCELLANEOUS, LM_DESERT_CACTUS_MED1, "deco" );
        b.place( 31, 29, G::LANDSCAPE_MISCELLANEOUS, LM_DESERT_CACTUS_SMALL, "deco" );
        b.place( 35, 34, G::LANDSCAPE_MISCELLANEOUS, LM_DESERT_CACTUS_MED2, "deco" );
        b.place( 27, 30, G::LANDSCAPE_TREES, TREE_DESERT_PALM_LONELY, "deco" );
        b.place( 30, 34, G::LANDSCAPE_TREES, TREE_DESERT_PALM_SMALL, "deco" );
        b.place( 34, 26, G::LANDSCAPE_TREES, TREE_DESERT_PALMS_TALL, "deco" );
        b.place( 27, 33, G::LANDSCAPE_MOUNTAINS, MTN_DESERT_DUNE1, "deco dune" );

        // South-central meadows (between ridge F/G and the south strip).
        b.place( 19, 25, G::LANDSCAPE_TREES, TREE_AUTUMN_MED1, "deco" );
        b.place( 22, 27, G::LANDSCAPE_TREES, TREE_DECID_SMALL1, "deco" );
        b.place( 18, 27, G::LANDSCAPE_MISCELLANEOUS, LM_GRASS_FLOWERS2, "deco" );
        b.place( 24, 28, G::LANDSCAPE_ROCKS, ROCK_GRASS_SMALL2, "deco" );
        b.place( 21, 25, G::LANDSCAPE_MISCELLANEOUS, LM_GRASS_FLOWERS_SMALL, "deco" );

        // West-of-stream meadow above Highmarch.
        b.place( 9, 21, G::LANDSCAPE_TREES, TREE_GRASS_THREE, "deco" );
        b.place( 8, 24, G::LANDSCAPE_MISCELLANEOUS, LM_GRASS_FLOWERS1, "deco" );
        b.place( 10, 19, G::LANDSCAPE_ROCKS, ROCK_GRASS_SMALL1, "deco" );

        // Fill enclosed dead pockets inside the ridge so no bare walled-off tiles remain.
        b.place( 22, 22, G::LANDSCAPE_ROCKS, ROCK_GRASS_SMALL2, "pocket filler" );
        b.place( 22, 23, G::LANDSCAPE_TREES, TREE_DECID_SMALL2, "pocket filler" );
        b.place( 8, 12, G::LANDSCAPE_TREES, TREE_SNOWFIR_SMALL1, "pocket filler" );

        // Extra ridge dressing so the wall reads as a range, not a line of clones.
        b.place( 7, 13, G::LANDSCAPE_TREES, TREE_SNOWFIR_MED1, "ridge deco" );
        b.place( 11, 16, G::LANDSCAPE_ROCKS, ROCK_GRASS_MEDIUM, "ridge deco" );
        b.place( 17, 16, G::LANDSCAPE_MOUNTAINS, MTN_DIRT_MED1, "ridge deco" );
        b.place( 21, 23, G::LANDSCAPE_TREES, TREE_DECID_BIG1, "ridge deco" );
        b.place( 27, 25, G::LANDSCAPE_TREES, TREE_GRASS_THREE, "ridge deco" );
        b.place( 31, 25, G::LANDSCAPE_ROCKS, ROCK_GRASS_SMALL1, "ridge deco" );
        b.place( 24, 24, G::LANDSCAPE_MOUNTAINS, MTN_GRASS_MED1, "ridge deco" );
    }

    void configureScenario( MapBuilder & b )
    {
        auto & m = b.map;

        m.name = "The King's Ransom";
        m.description
            = "The King is taken. The Iron Company demands 100,000 gold before the second month ends - after that, there will be nothing left to ransom. "
              "As Steward of Highmarch you must raise the full sum and HOLD it in your treasury by day 56. "
              "Lord Renfrew of Ironvale covets the empty throne: he will pay nothing, and he profits from your failure. "
              "Defeating him alone will not save the King - only the gold will.";
        m.creatorNotes = "Generated scenario built against fheroes2 revision b086d1aa. Designed for Normal difficulty. Victory: accumulate 100,000 gold by day 56.";

        m.difficulty = 1; // Normal
        m.isCampaign = false;

        m.humanPlayerColors = static_cast<uint8_t>( PlayerColor::BLUE );
        m.computerPlayerColors = static_cast<uint8_t>( PlayerColor::GREEN );

        m.victoryConditionType = 5; // VICTORY_COLLECT_ENOUGH_GOLD
        m.isVictoryConditionApplicableForAI = false;
        m.allowNormalVictory = false;
        m.victoryConditionMetadata = { 100000 };

        m.lossConditionType = 3; // LOSS_OUT_OF_TIME
        m.lossConditionMetadata = { 56 };

        // Flavor timeline.
        Maps::Map_Format::DailyEvent intro;
        intro.message = "Word from the capital: the Iron Company holds the King in chains. Their price is 100,000 gold, due by the last day of the second month. "
                        "The Regent has emptied the vaults to give you a start - the rest is yours to raise, Steward.";
        intro.humanPlayerColors = static_cast<uint8_t>( PlayerColor::BLUE );
        intro.firstOccurrenceDay = 1;
        intro.repeatPeriodInDays = 0;
        b.map.dailyEvents.push_back( intro );

        Maps::Map_Format::DailyEvent oneMonth;
        oneMonth.message = "One month remains. The Regent writes: 'However the ledgers stand, Steward, only the gold in your treasury on the last day will count.'";
        oneMonth.humanPlayerColors = static_cast<uint8_t>( PlayerColor::BLUE );
        oneMonth.firstOccurrenceDay = 29;
        oneMonth.repeatPeriodInDays = 0;
        b.map.dailyEvents.push_back( oneMonth );

        Maps::Map_Format::DailyEvent finalWeek;
        finalWeek.message = "The final week. The Iron Company sharpens its knives. Whatever you mean to sell, sell it now - the count is taken on day 56.";
        finalWeek.humanPlayerColors = static_cast<uint8_t>( PlayerColor::BLUE );
        finalWeek.firstOccurrenceDay = 50;
        finalWeek.repeatPeriodInDays = 0;
        b.map.dailyEvents.push_back( finalWeek );

        b.map.rumors = {
            "They say the mine in the heart of the vale once paid for a crown in a single season.",
            "Lord Renfrew would sooner see the King rot than part with a single coin.",
            "The dwarves of the western road trade fair - cold hands, honest scales.",
            "A purse that never empties lies buried somewhere in the southern sands.",
            "Marketstead's traders will turn anything into gold - for a cut.",
        };
    }
}

void buildKingsRansom( MapBuilder & b )
{
    terrain( b );
    ridge( b );
    blueHome( b );
    westRoadRegion( b );
    northCorridor( b );
    kingsVale( b );
    greenHome( b );
    southernSands( b );
    roadsAndStreams( b );
    decorations( b );
    configureScenario( b );
}
