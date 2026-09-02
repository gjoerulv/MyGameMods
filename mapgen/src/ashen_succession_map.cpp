// "The Ashen Succession" - 72x72 four-player conquest map.
//
// Four random-race Margraves (Blue NW, Green NE, Red SW, Yellow SE) fight for the crown of a
// dying autumn realm. The map is a mirrored 3x3 grid: four corner homes, four shared border
// zones on the axes (each with a neutral toll town and contested seam mines) and a wasteland
// center holding the dead capital Kingsfall and four Cyclops-guarded gold caches.
//
// One quadrant (NW) is authored; everything is reflected across both axes (x' = 71 - x,
// y' = 71 - y). Seam objects (column 35 / row 35) are placed once per border zone. Anything
// whose entrance/guard must stay "below" the object (castles, mines, guards) is positioned in
// absolute space after the transform, never by mirroring the tile below. The southern castles
// (and their sawmills) sit five rows further north than the mirror image so that every hero's
// walk to its two passes and to free wood costs the same (castles always face south). Roads are
// authored tile lists, mirrored per quadrant, so every march has the same roads.
//
// Object table indices come from research/notes/02_tiles_objectgroups_tables.md (fheroes2 @
// b086d1aa; tables identical in the installed 1.1.17 release).

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include "mapgen.h"

#include "artifact.h"
#include "castle.h"
#include "ground.h"
#include "monster.h"
#include "resource.h"

namespace
{
    // ---- guard tuning (single place to balance the map; see mapgen/guard_model.py) ----
    constexpr int32_t G_HOME_GOLD_ROGUES = 28; // days 4-7; the home stepladder: gold above gems
    constexpr int32_t G_HOME_GEMS_WOLVES = 6; // days 3-5
    constexpr int32_t G_HOME_CHEST_BOARS = 10;
    constexpr int32_t G_HOME_ARTIFACT_BOARS = 20;
    constexpr int32_t G_PASS_A_WOLVES = 14; // home -> N/S zone, week 2
    constexpr int32_t G_PASS_B_ZOMBIES = 30; // home -> W/E zone, week 2
    constexpr int32_t G_N_SULFUR_ORCS = 25; // week 2
    constexpr int32_t G_N_CACHE_NOMADS = 15;
    constexpr int32_t G_N_TREASURE_NOMADS = 25; // week 3
    constexpr int32_t G_N_GOLD_NOMADS = 30; // seam gold mine, week 3
    constexpr int32_t G_N_LAB_ORCS = 50; // seam alchemist lab, week 3
    constexpr int32_t G_N_ARTIFACT_OGRES = 25; // seam major artifact, week 3-4
    constexpr int32_t G_W_CRYSTAL_ZOMBIES = 30; // week 2
    constexpr int32_t G_W_GOLD_MUMMIES = 30; // seam gold mine, week 3
    constexpr int32_t G_W_ARTIFACT_MUMMIES = 30; // seam major artifact, week 3-4
    constexpr int32_t G_GATE_NS_OGRE_LORDS = 25; // month 2
    constexpr int32_t G_GATE_WE_MINOTAUR_KINGS = 22; // month 2
    constexpr int32_t G_CENTER_MINE_CYCLOPS = 8; // month 2

    // Neutral garrisons (5 slots each). Neutral towns DO grow: Castle::ActionNewWeek joins one random
    // stack of the town race's base creature of a rolled dwelling level every week from week 2 (plus a
    // second one with 40% for towns, always for castles), and the join only lands when that creature
    // already has a stack or a slot is free. The counts below are calibrated with that growth included
    // (mapgen/guard_model.py); Kingsfall holds only upgraded creatures, so nothing can ever join it.
    constexpr int32_t RAVENSGATE_TYPES[5] = { Monster::GOBLIN, Monster::ORC_CHIEF, Monster::WOLF, Monster::OGRE_LORD, Monster::TROLL };
    constexpr int32_t RAVENSGATE_COUNTS[5] = { 55, 32, 18, 11, 4 };
    constexpr int32_t GREYFEN_TYPES[5] = { Monster::SKELETON, Monster::MUTANT_ZOMBIE, Monster::ROYAL_MUMMY, Monster::VAMPIRE, Monster::LICH };
    constexpr int32_t GREYFEN_COUNTS[5] = { 70, 36, 22, 9, 3 };
    constexpr int32_t KINGSFALL_TYPES[5] = { Monster::RANGER, Monster::VETERAN_PIKEMAN, Monster::MASTER_SWORDSMAN, Monster::CHAMPION, Monster::CRUSADER };
    constexpr int32_t KINGSFALL_COUNTS[5] = { 40, 30, 18, 12, 5 };

    // ---- geometry ----
    constexpr int32_t MAXC = 71;
    constexpr int32_t WALL = 23; // wall lines at 23 and 71-23 = 48
    // Engine passability (maps_tiles.cpp Tile::updatePassability): a ground-object tile can be entered
    // and left sideways/downwards (CENTER_ROW | BOTTOM_ROW) unless the tile BELOW it holds the same
    // object or a non-short object of the same ICN family, in which case it is fully impassable.
    // Consequences used here:
    //  * A vertical wall must be a contiguous stack of same-family mountains: every bottom-row tile
    //    then has the next mountain below it. The only sideways-walkable wall tiles are the bottom row
    //    of the mountain directly above a corridor, so the corridor is 2 rows tall and its guard stands
    //    on the TOP corridor row, covering that row as well.
    //  * A horizontal wall only needs every tile of its centre row occupied: no object tile can be
    //    entered from above or left upwards, whatever the object.
    constexpr int32_t GAP_A0 = 6; // vertical walls: home pass A, rows 6..7 (mirror 64..65)
    constexpr int32_t GAP_A1 = 7;
    constexpr int32_t GAP_B0 = 8; // horizontal walls: home pass B, cols 8..10 (mirror 61..63)
    constexpr int32_t GAP_B1 = 10;
    constexpr int32_t GAP_C0 = 35; // center gates: rows/cols 35..36 (guards on the top/left corridor tile)
    constexpr int32_t GAP_C1 = 36;

    // Castle rows. Every castle is entered from below; a northern hero starts south of its castle and
    // walks straight to pass B (y = 23), a southern hero starts south of its castle and must walk round
    // it to reach pass B (y = 48). Placing the southern castles five rows further north than the
    // mirror image (64) makes the hero -> pass A and hero -> pass B road costs match the north within
    // ~110 movement points (N: 1237 / 1050, S: 1237 / 1162).
    constexpr int32_t CASTLE_X = 9;
    constexpr int32_t CASTLE_Y_NORTH = 7;
    constexpr int32_t CASTLE_Y_SOUTH = 59;

    struct T
    {
        bool fx;
        bool fy;
    };
    struct P
    {
        int32_t x;
        int32_t y;
    };

    constexpr T QUADS[4] = { { false, false }, { true, false }, { false, true }, { true, true } }; // NW, NE, SW, SE
    constexpr PlayerColor COLORS[4] = { PlayerColor::BLUE, PlayerColor::GREEN, PlayerColor::RED, PlayerColor::YELLOW };
    const char * const CASTLE_NAMES[4] = { "Harrowfield", "Rookhaven", "Duskmere", "Cinderholt" };
    const char * const QUAD_NAMES[4] = { "NW", "NE", "SW", "SE" };

    P tf( const T & t, const int32_t x, const int32_t y )
    {
        return { t.fx ? MAXC - x : x, t.fy ? MAXC - y : y };
    }

    P castleTile( const T & t )
    {
        return { t.fx ? MAXC - CASTLE_X : CASTLE_X, t.fy ? CASTLE_Y_SOUTH : CASTLE_Y_NORTH };
    }

    using G = Maps::ObjectGroup;

    // ---- object table indices ----
    // LANDSCAPE_MOUNTAINS: pairs (TL->BR, TR->BL) for big variants.
    constexpr uint32_t MTN_WASTE_BIG = 44;
    constexpr uint32_t MTN_WASTE_MED1 = 48;
    // LANDSCAPE_TREES: pairs (TL->BR, TR->BL) for big/medium; singles otherwise.
    constexpr uint32_t TREE_EVIL_BIG = 6;
    constexpr uint32_t TREE_EVIL_MED = 8;
    constexpr uint32_t TREE_EVIL_SMALL1 = 10;
    constexpr uint32_t TREE_EVIL_SMALL2 = 11;
    constexpr uint32_t TREE_AUTUMN_BIG = 12;
    constexpr uint32_t TREE_AUTUMN_MED = 14;
    constexpr uint32_t TREE_AUTUMN_SMALL1 = 16;
    constexpr uint32_t TREE_AUTUMN_SMALL2 = 17;
    constexpr uint32_t TREE_DIRT_TALL = 61;
    constexpr uint32_t TREE_DIRT_TWO_TALL = 62;
    constexpr uint32_t TREE_GENERIC_DEAD = 64;
    constexpr uint32_t TREE_GENERIC_LOG = 65;
    constexpr uint32_t TREE_STUMPS_THREE = 66;
    constexpr uint32_t TREE_STUMPS_TWO = 67;
    constexpr uint32_t TREE_STUMP_ONE = 68;
    // LANDSCAPE_ROCKS
    constexpr uint32_t ROCK_DIRT_WIDE = 21;
    constexpr uint32_t ROCK_DIRT_BIG = 22;
    constexpr uint32_t ROCK_DIRT_MEDSMALL = 23;
    constexpr uint32_t ROCK_DIRT_MEDIUM = 24;
    constexpr uint32_t ROCK_DIRT_SMALL = 25;
    constexpr uint32_t ROCK_WASTE_SMALL = 27;
    constexpr uint32_t ROCK_WASTE_BIG = 28;
    constexpr uint32_t ROCK_WASTE_WIDE = 29;
    constexpr uint32_t ROCK_WASTE_TALL1 = 36;
    constexpr uint32_t ROCK_WASTE_TALL2 = 37;
    constexpr uint32_t ROCK_WASTE_TALL3 = 38;
    constexpr uint32_t ROCK_WASTE_TALL4 = 39;
    // LANDSCAPE_MISCELLANEOUS
    constexpr uint32_t LM_GRASS_LAKE_SMALL = 6;
    constexpr uint32_t LM_GRASS_FLOWERS_SMALL = 21;
    constexpr uint32_t LM_DIRT_LAKE_SMALL = 105;
    constexpr uint32_t LM_DIRT_SHRUB_WIDE1 = 106;
    constexpr uint32_t LM_DIRT_SHRUB = 109;
    constexpr uint32_t LM_DIRT_SHRUB2 = 110;
    constexpr uint32_t LM_DIRT_FLOWERS_WIDE = 111;
    constexpr uint32_t LM_DIRT_FLOWERS_SMALL = 115;
    constexpr uint32_t LM_DIRT_GRASS = 124;
    constexpr uint32_t LM_WASTE_CRACK = 129; // terrain layer, passable
    constexpr uint32_t LM_WASTE_SKULL = 132;
    constexpr uint32_t LM_WASTE_SHRUB = 133;
    constexpr uint32_t LM_WASTE_SHRUB2 = 136;
    constexpr uint32_t LM_WASTE_SHRUB3 = 137;
    constexpr uint32_t LM_WASTE_TAR_PIT = 138;
    constexpr uint32_t LM_WASTE_CRACK_V = 140;
    constexpr uint32_t LM_WASTE_CRACK_H = 141;
    // ADVENTURE_DWELLINGS
    constexpr uint32_t DW_PEASANT_HUT = 0;
    constexpr uint32_t DW_RUINS = 1;
    constexpr uint32_t DW_HALFLING_HOLE_DIRT = 5;
    constexpr uint32_t DW_WAGON_CAMP = 17;
    // ADVENTURE_POWER_UPS
    constexpr uint32_t PU_FOUNTAIN = 6;
    constexpr uint32_t PU_FORT = 10;
    constexpr uint32_t PU_GAZEBO = 11;
    constexpr uint32_t PU_WITCH_DOCTORS_HUT = 12;
    constexpr uint32_t PU_MERCENARY_CAMP = 13;
    constexpr uint32_t PU_SHRINE_SECOND = 15;
    constexpr uint32_t PU_STANDING_STONES = 18;
    constexpr uint32_t PU_TREE_OF_KNOWLEDGE = 20;
    // ADVENTURE_TREASURES
    constexpr uint32_t TR_RANDOM_RESOURCE = 8;
    constexpr uint32_t TR_CAMPFIRE = 10;
    // ADVENTURE_ARTIFACTS
    constexpr uint32_t ART_RANDOM_TREASURE = 92;
    constexpr uint32_t ART_RANDOM_MINOR = 93;
    constexpr uint32_t ART_RANDOM_MAJOR = 94;
    // ADVENTURE_MISCELLANEOUS
    constexpr uint32_t MISC_GRAVEYARD = 0;
    constexpr uint32_t MISC_HILL_FORT_DIRT = 3;
    constexpr uint32_t MISC_WINDMILL_DIRT = 6;
    constexpr uint32_t MISC_SIGN_GENERIC = 22;
    constexpr uint32_t MISC_WATER_WHEEL = 24;
    constexpr uint32_t MISC_TRADING_POST_WASTE = 32;
    constexpr uint32_t MISC_MAGIC_GARDEN = 41;
    constexpr uint32_t MISC_OBS_TOWER_GENERIC = 43;

    // ---- small helpers ----
    uint32_t hash2( const int32_t x, const int32_t y )
    {
        uint32_t h = static_cast<uint32_t>( x ) * 73856093U ^ static_cast<uint32_t>( y ) * 19349663U;
        h ^= h >> 13;
        h *= 0x5bd1e995U;
        h ^= h >> 15;
        return h;
    }

    // Big/medium mountains and trees come in (TL->BR, TR->BL) pairs; use the flipped sprite in x-mirrored quadrants.
    uint32_t pair( const uint32_t base, const T & t )
    {
        return t.fx ? base + 1 : base;
    }

    void pl( MapBuilder & b, const T & t, const int32_t x, const int32_t y, const G g, const uint32_t index, const std::string & label )
    {
        const P p = tf( t, x, y );
        b.place( p.x, p.y, g, index, label );
    }

    void deco( MapBuilder & b, const T & t, const int32_t x, const int32_t y, const G g, const uint32_t index )
    {
        const P p = tf( t, x, y );
        b.tryPlace( p.x, p.y, g, index, "deco" );
    }

    void guardAt( MapBuilder & b, const T & t, const int32_t x, const int32_t y, const int monsterId, const int32_t count, const std::string & label )
    {
        const P p = tf( t, x, y );
        b.placeMonster( p.x, p.y, monsterId, count, label );
    }

    // Mine/sawmill/lab at authored (x,y); the guard stands on the absolute tile below the transformed entrance.
    void mineAt( MapBuilder & b, const T & t, const int32_t x, const int32_t y, const int resource, const std::string & label, const int monsterId = 0,
                 const int32_t count = 0 )
    {
        const P p = tf( t, x, y );
        b.placeMine( p.x, p.y, resource, label );
        if ( monsterId != 0 ) {
            b.placeMonster( p.x, p.y + 1, monsterId, count, label + " guard" );
        }
    }

    void pileAt( MapBuilder & b, const T & t, const int32_t x, const int32_t y, const int resource )
    {
        const P p = tf( t, x, y );
        b.placeResourcePile( p.x, p.y, resource, 0 );
    }

    void chestAt( MapBuilder & b, const T & t, const int32_t x, const int32_t y )
    {
        const P p = tf( t, x, y );
        b.placeChest( p.x, p.y );
    }

    void signAt( MapBuilder & b, const T & t, const int32_t x, const int32_t y, const std::string & text )
    {
        const P p = tf( t, x, y );
        b.placeSign( p.x, p.y, MISC_SIGN_GENERIC, text );
    }

    // Random artifact with an explicit choice list limited to the original game's artifacts (ids 9..81).
    // Their adventure-map sprites resolve without Price of Loyalty assets; an empty list would let the
    // engine roll a PoL artifact whose sprite is unknown to a Succession Wars-only installation.
    void setRandomArtifactChoices( MapBuilder & b, const uint32_t uid, const uint32_t index )
    {
        int level = Artifact::ART_LEVEL_ALL_NORMAL;
        switch ( index ) {
        case ART_RANDOM_TREASURE:
            level = Artifact::ART_LEVEL_TREASURE;
            break;
        case ART_RANDOM_MINOR:
            level = Artifact::ART_LEVEL_MINOR;
            break;
        case ART_RANDOM_MAJOR:
            level = Artifact::ART_LEVEL_MAJOR;
            break;
        default:
            break;
        }
        auto & selected = b.map.artifactMetadata[uid].selected;
        selected.clear();
        for ( int id = Artifact::ARCANE_NECKLACE; id <= Artifact::BLACK_PEARL; ++id ) {
            const Artifact art( id );
            if ( art.isValid() && ( art.Level() & level ) != 0 ) {
                selected.push_back( id );
            }
        }
    }

    uint32_t randomArtifactAbs( MapBuilder & b, const int32_t x, const int32_t y, const uint32_t index, const std::string & label )
    {
        const uint32_t uid = b.place( x, y, G::ADVENTURE_ARTIFACTS, index, label );
        setRandomArtifactChoices( b, uid, index );
        return uid;
    }

    void randomArtifactAt( MapBuilder & b, const T & t, const int32_t x, const int32_t y, const uint32_t index, const std::string & label )
    {
        const P p = tf( t, x, y );
        randomArtifactAbs( b, p.x, p.y, index, label );
    }

    void setGarrison( MapBuilder & b, const uint32_t uid, const int32_t * types, const int32_t * counts )
    {
        auto & meta = b.map.castleMetadata[uid];
        for ( size_t i = 0; i < 5; ++i ) {
            meta.defenderMonsterType[i] = types[i];
            meta.defenderMonsterCount[i] = counts[i];
        }
    }

    // Lay an authored road (list of absolute tiles). Roads ignore the cosmetic mask on purpose: the
    // apron below a castle sits inside the town rectangle.
    void road( MapBuilder & b, const std::vector<P> & tiles )
    {
        for ( const P & p : tiles ) {
            b.addRoad( p.x, p.y );
        }
    }

    std::vector<P> mirrored( const std::vector<P> & tiles, const T & t )
    {
        std::vector<P> out;
        out.reserve( tiles.size() );
        for ( const P & p : tiles ) {
            out.push_back( tf( t, p.x, p.y ) );
        }
        return out;
    }

    // Paint an organic ellipse as horizontal runs, never a lone tile: a single tile of a terrain that has
    // no four-sided transition sprite (everything except dirt) is silently reverted by the engine's
    // transition fix-up. Every paint call is a rectangle two rows tall (width = the narrower of its two
    // rows), so each painted tile has a same-terrain neighbour both horizontally and vertically; pairs go
    // middle-out. Run ends use ceil/floor around cx so mirrored copies match exactly, and the per-row
    // jitter keys on |y - cy| so the shape is symmetric under y' = 71 - y as well.
    void paintRuns( MapBuilder & b, const std::vector<T> & transforms, const double cx, const double cy, const double rx, const double ry, const int ground,
                    const uint32_t salt )
    {
        const auto halfWidth = [&]( const int32_t y ) -> double {
            const double ny = ( y - cy ) / ry;
            if ( ny * ny >= 1.0 ) {
                return 0.0;
            }
            const int32_t key = static_cast<int32_t>( std::lround( std::abs( y - cy ) * 2.0 ) );
            return rx * std::sqrt( 1.0 - ny * ny ) + ( static_cast<int32_t>( hash2( key, static_cast<int32_t>( salt ) ) % 3 ) - 1 );
        };

        const auto paintPair = [&]( const int32_t ya ) {
            const int32_t yb = ya + 1;
            if ( ya < 0 || yb > MAXC ) {
                return;
            }
            const double w = std::min( halfWidth( ya ), halfWidth( yb ) );
            if ( w < 1.0 ) {
                return;
            }
            const int32_t x0 = std::max<int32_t>( 0, static_cast<int32_t>( std::ceil( cx - w ) ) );
            const int32_t x1 = std::min<int32_t>( MAXC, static_cast<int32_t>( std::floor( cx + w ) ) );
            if ( x1 - x0 < 1 ) {
                return;
            }
            for ( const T & t : transforms ) {
                const P a = tf( t, x0, ya );
                const P c = tf( t, x1, yb );
                b.paintRect( std::min( a.x, c.x ), std::min( a.y, c.y ), std::max( a.x, c.x ), std::max( a.y, c.y ), ground );
            }
        };

        const int32_t yc = static_cast<int32_t>( std::floor( cy ) );
        const int32_t span = static_cast<int32_t>( ry ) + 1;
        paintPair( yc ); // rows yc, yc+1
        for ( int32_t d = 1; d <= span; ++d ) {
            paintPair( yc - d ); // rows yc-d, yc-d+1 (overlaps the previous pair by one row)
            paintPair( yc + d ); // rows yc+d, yc+d+1
        }
    }

    // ------------------------------------------------------------------ terrain
    void terrain( MapBuilder & b )
    {
        using namespace Maps;

        b.paintRect( 0, 0, MAXC, MAXC, Ground::DIRT );

        const std::vector<T> all4 = { QUADS[0], QUADS[1], QUADS[2], QUADS[3] };
        const std::vector<T> xPair = { QUADS[0], QUADS[1] };
        const std::vector<T> one = { QUADS[0] };

        // The four rocky wasteland ridges the last king raised: 7-wide bands under the mountain walls.
        b.paintRect( WALL - 3, 0, WALL + 3, MAXC, Ground::WASTELAND );
        b.paintRect( MAXC - WALL - 3, 0, MAXC - WALL + 3, MAXC, Ground::WASTELAND );
        b.paintRect( 0, WALL - 3, MAXC, WALL + 3, Ground::WASTELAND );
        b.paintRect( 0, MAXC - WALL - 3, MAXC, MAXC - WALL + 3, Ground::WASTELAND );

        // The Ashen Scar: the wasteland center, symmetric about (35.5, 35.5) by construction.
        paintRuns( b, one, 35.5, 35.5, 10.5, 10.5, Ground::WASTELAND, 11 );

        // The Greyfen: a small dead fen in each half of the W/E zones (rows 28..31 and 40..43), leaving
        // a dirt row between the fen and the ridge so the minimap separates them.
        paintRuns( b, all4, 10.0, 29.5, 7.5, 1.9, Ground::WASTELAND, 22 );

        // The last green fields: a grass meadow around each home castle (northern and southern rows).
        for ( const T & t : all4 ) {
            const P c = castleTile( t );
            paintRuns( b, { { false, false } }, c.x, c.y, 8.0, 5.5, Ground::GRASS, 33 );
        }

        // Dirt islands under the two Hill Forts in the center (the fort sprite is a dirt object).
        paintRuns( b, xPair, 29.5, 35.5, 3.6, 2.6, Ground::DIRT, 44 );
    }

    // ------------------------------------------------------------------ walls
    bool intersectsGap( const int32_t lo, const int32_t hi, const std::vector<P> & gaps )
    {
        for ( const P & g : gaps ) {
            if ( hi >= g.x && lo <= g.y ) {
                return true;
            }
        }
        return false;
    }

    // Vertical wall: a contiguous stack of big wasteland mountains, alternating the TL->BR and TR->BL
    // sprites (same ICN family, so every bottom-row tile is sealed by the mountain below it). Rows are
    // 3 apart except across the three corridors (rows 6..7, 35..36, 64..65), where they are 5 apart.
    // The list is symmetric under y' = 71 - y. The mirrored wall (x = 48) starts with the other sprite.
    void wallVertical( MapBuilder & b, const int32_t x, const bool mirrored )
    {
        static const int32_t rows[] = { 1, 4, 9, 12, 15, 18, 21, 24, 27, 30, 33, 38, 41, 44, 47, 50, 53, 56, 59, 62, 67, 70 };
        int k = 0;
        for ( const int32_t y : rows ) {
            const bool flip = ( ( k % 2 ) == 1 ) != mirrored;
            if ( b.tryPlace( x, y, G::LANDSCAPE_MOUNTAINS, MTN_WASTE_BIG + ( flip ? 1U : 0U ), "wall" ) == 0 ) {
                std::cerr << "WARNING: vertical wall mountain skipped at (" << x << "," << y << ")" << std::endl;
            }
            ++k;
        }
    }

    // Horizontal wall: big mountains every 5 columns (row 0 of the footprint is 5 wide, so the centre
    // row is contiguous), skipping the gaps and the crossing with the vertical walls, then every still
    // empty centre-row tile outside the gaps is sealed with a 1-tile wasteland blocker.
    void wallHorizontal( MapBuilder & b, const int32_t y, const std::vector<P> & gaps )
    {
        static const int32_t cols[] = { 3, 13, 18, 28, 33 }; // mirrored to 68, 58, 53, 43, 38; 8 = gap, 23 = crossing
        int k = 0;
        for ( const int32_t xa : cols ) {
            for ( int half = 0; half < 2; ++half ) {
                const int32_t x = ( half == 0 ) ? xa : MAXC - xa;
                if ( intersectsGap( x - 2, x + 2, gaps ) ) {
                    continue;
                }
                const bool flip = ( ( k % 2 ) == 1 ) != ( half == 1 );
                b.tryPlace( x, y, G::LANDSCAPE_MOUNTAINS, MTN_WASTE_BIG + ( flip ? 1U : 0U ), "wall" );
            }
            ++k;
        }

        constexpr G groups[] = { G::LANDSCAPE_ROCKS, G::LANDSCAPE_TREES, G::LANDSCAPE_ROCKS, G::LANDSCAPE_TREES, G::LANDSCAPE_ROCKS, G::LANDSCAPE_TREES, G::LANDSCAPE_ROCKS };
        constexpr uint32_t indices[] = { ROCK_WASTE_TALL1, TREE_GENERIC_DEAD, ROCK_WASTE_TALL3, TREE_STUMPS_THREE, ROCK_WASTE_SMALL, TREE_EVIL_SMALL1, ROCK_WASTE_TALL2 };
        constexpr size_t n = sizeof( indices ) / sizeof( indices[0] );
        for ( int32_t x = 0; x <= MAXC; ++x ) {
            if ( intersectsGap( x, x, gaps ) || b.isOccupied( x, y ) ) {
                continue;
            }
            const int32_t fx = x <= 35 ? x : MAXC - x; // same filler on mirrored tiles
            const size_t i = hash2( fx, y <= 35 ? y : MAXC - y ) % n;
            // Structural, so it ignores the cosmetic mask (a filler under a neighbouring mountain's top
            // sprite is hidden by that sprite and still seals the row).
            b.place( x, y, groups[i], indices[i], "wall filler" );
        }
    }

    void walls( MapBuilder & b )
    {
        const std::vector<P> horizontalGaps = { { GAP_B0, GAP_B1 }, { GAP_C0, GAP_C1 }, { MAXC - GAP_B1, MAXC - GAP_B0 } };

        wallVertical( b, WALL, false );
        wallVertical( b, MAXC - WALL, true );
        wallHorizontal( b, WALL, horizontalGaps );
        wallHorizontal( b, MAXC - WALL, horizontalGaps );
    }

    // Dress the ridge bands (the four wasteland rows/columns either side of each wall line) with
    // scattered wasteland rocks, dead trees, skulls and cracks so the ridges read as ranges, not as a
    // picket line down an empty avenue. Mirrored by hashing the tile folded into the NW quadrant;
    // corridors and their approaches (+-2 tiles) and the tiles next to any guard are left clear, and
    // nothing goes onto a road (this runs after the roads are laid).
    void ridgeDecor( MapBuilder & b )
    {
        constexpr G groups[] = { G::LANDSCAPE_ROCKS, G::LANDSCAPE_TREES, G::LANDSCAPE_MISCELLANEOUS, G::LANDSCAPE_ROCKS, G::LANDSCAPE_TREES, G::LANDSCAPE_MISCELLANEOUS,
                                 G::LANDSCAPE_ROCKS, G::LANDSCAPE_MISCELLANEOUS, G::LANDSCAPE_TREES, G::LANDSCAPE_MISCELLANEOUS, G::LANDSCAPE_ROCKS, G::LANDSCAPE_TREES };
        constexpr uint32_t indices[] = { ROCK_WASTE_SMALL, TREE_GENERIC_DEAD, LM_WASTE_CRACK, ROCK_WASTE_TALL2, TREE_STUMP_ONE, LM_WASTE_SKULL,
                                         ROCK_WASTE_TALL4, LM_WASTE_SHRUB, TREE_STUMPS_TWO, LM_WASTE_CRACK_H, ROCK_WASTE_TALL1, TREE_EVIL_SMALL2 };
        constexpr size_t n = sizeof( indices ) / sizeof( indices[0] );

        const auto nearCorridor = [&]( const bool verticalBand, const int32_t x, const int32_t y ) {
            const int32_t along = verticalBand ? y : x;
            const int32_t g0 = verticalBand ? GAP_A0 : GAP_B0;
            const int32_t g1 = verticalBand ? GAP_A1 : GAP_B1;
            const bool a = along >= g0 - 2 && along <= g1 + 2;
            const bool c = along >= GAP_C0 - 2 && along <= GAP_C1 + 2;
            const bool m = along >= MAXC - g1 - 2 && along <= MAXC - g0 + 2;
            return a || c || m;
        };

        const auto tryDecor = [&]( const bool verticalBand, const int32_t x, const int32_t y ) {
            if ( nearCorridor( verticalBand, x, y ) || b.isOccupied( x, y ) || b.roadMask[b.idx( x, y )] != 0 ) {
                return;
            }
            const int32_t fx = x <= 35 ? x : MAXC - x;
            const int32_t fy = y <= 35 ? y : MAXC - y;
            const uint32_t h = hash2( fx + 17, fy + 29 );
            if ( h % 100 >= 16 ) {
                return;
            }
            const size_t i = ( h / 100 ) % n;
            b.tryPlace( x, y, groups[i], indices[i], "ridge decor" );
        };

        for ( int32_t y = 0; y <= MAXC; ++y ) {
            for ( const int32_t x : { WALL - 2, WALL - 1, WALL + 1, WALL + 2, MAXC - WALL - 2, MAXC - WALL - 1, MAXC - WALL + 1, MAXC - WALL + 2 } ) {
                tryDecor( true, x, y );
            }
        }
        for ( int32_t x = 0; x <= MAXC; ++x ) {
            for ( const int32_t y : { WALL - 2, WALL - 1, WALL + 1, WALL + 2, MAXC - WALL - 2, MAXC - WALL - 1, MAXC - WALL + 1, MAXC - WALL + 2 } ) {
                tryDecor( false, x, y );
            }
        }
    }

    // After everything else is placed: dress every walled-off, unreachable land tile with a 1-tile
    // blocker so no bare pockets remain inside the walls.
    void fillPockets( MapBuilder & b )
    {
        constexpr G groups[] = { G::LANDSCAPE_ROCKS, G::LANDSCAPE_TREES, G::LANDSCAPE_TREES, G::LANDSCAPE_ROCKS, G::LANDSCAPE_TREES, G::LANDSCAPE_ROCKS };
        constexpr uint32_t indices[] = { ROCK_WASTE_SMALL, TREE_EVIL_SMALL2, TREE_STUMP_ONE, ROCK_WASTE_TALL2, TREE_GENERIC_DEAD, ROCK_WASTE_TALL3 };
        constexpr size_t n = sizeof( indices ) / sizeof( indices[0] );

        b.refreshPassability();
        std::vector<bool> reachable;
        const P start = castleTile( QUADS[0] );
        b.computeReachability( start.x, start.y + 1, reachable );

        for ( int32_t y = 0; y <= MAXC; ++y ) {
            for ( int32_t x = 0; x <= MAXC; ++x ) {
                if ( reachable[static_cast<size_t>( y ) * ( MAXC + 1 ) + x] || b.isOccupied( x, y ) ) {
                    continue;
                }
                const int32_t fx = x <= 35 ? x : MAXC - x;
                const int32_t fy = y <= 35 ? y : MAXC - y;
                const size_t i = hash2( fx + 3, fy + 5 ) % n;
                b.tryPlace( x, y, groups[i], indices[i], "pocket filler" );
            }
        }
    }

    // ------------------------------------------------------------------ homes
    void home( MapBuilder & b, const int q )
    {
        const T & t = QUADS[q];
        const std::string qn = QUAD_NAMES[q];

        // Castle + hero. The hero must be on the absolute tile below the entrance.
        const P e = castleTile( t );
        const uint32_t castleUid = b.placeCastle( e.x, e.y, COLORS[q], 6 /*random race*/, true /*castle*/, CASTLE_NAMES[q] );
        {
            auto & meta = b.map.castleMetadata[castleUid];
            meta.customBuildings = true;
            meta.builtBuildings = { BUILD_CASTLE, DWELLING_MONSTER1, DWELLING_MONSTER2 };
        }
        b.placeHero( e.x, e.y + 1, COLORS[q], 6 /*random hero*/, std::string() );

        // Free wood (day 1, on the meadow: 600-650 move points from the castle) and ore (day 2).
        // The southern sawmill sits north-east of its castle so the walk matches the northern one.
        mineAt( b, t, t.fy ? 14 : 13, t.fy ? 13 : 4, Resource::WOOD, qn + " sawmill" );
        mineAt( b, t, 4, 15, Resource::ORE, qn + " ore mine" );

        // Guarded income: the gold mine is the bigger prize and carries the bigger guard. (Southern copies
        // sit two tiles further east so the unflipped sawmill and mine footprints never overlap; the walk
        // from the castle costs the same.)
        mineAt( b, t, t.fy ? 18 : 16, 12, Resource::GOLD, qn + " home gold mine", Monster::ROGUE, G_HOME_GOLD_ROGUES );
        mineAt( b, t, 2, 3, Resource::GEMS, qn + " gems mine", Monster::WOLF, G_HOME_GEMS_WOLVES );

        // Dwellings and XP/stat objects.
        pl( b, t, 15, 17, G::ADVENTURE_DWELLINGS, DW_PEASANT_HUT, qn + " peasant hut" );
        pl( b, t, 15, 19, G::ADVENTURE_DWELLINGS, DW_HALFLING_HOLE_DIRT, qn + " halfling hole" );
        pl( b, t, 18, 8, G::ADVENTURE_POWER_UPS, PU_GAZEBO, qn + " gazebo" );
        pl( b, t, 2, 9, G::ADVENTURE_POWER_UPS, PU_FOUNTAIN, qn + " fountain" );
        pl( b, t, 18, 16, G::ADVENTURE_MISCELLANEOUS, MISC_WINDMILL_DIRT, qn + " windmill" );
        pl( b, t, 16, 15, G::ADVENTURE_MISCELLANEOUS, MISC_WATER_WHEEL, qn + " water wheel" );
        pl( b, t, 2, 12, G::ADVENTURE_MISCELLANEOUS, MISC_MAGIC_GARDEN, qn + " magic garden" );

        // Pickups.
        chestAt( b, t, 18, 2 );
        chestAt( b, t, 6, 19 );
        guardAt( b, t, 6, 20, Monster::BOAR, G_HOME_CHEST_BOARS, qn + " chest guard" ); // directly below: diagonal guards do not protect in the engine
        randomArtifactAt( b, t, 1, 20, ART_RANDOM_MINOR, qn + " minor artifact" );
        guardAt( b, t, 2, 20, Monster::BOAR, G_HOME_ARTIFACT_BOARS, qn + " artifact guard" );
        pileAt( b, t, 16, 2, Resource::WOOD );
        pileAt( b, t, 7, 16, Resource::ORE );
        pileAt( b, t, 14, 9, Resource::GOLD );
        pl( b, t, 1, 0, G::ADVENTURE_TREASURES, TR_RANDOM_RESOURCE, qn + " random resource" ); // row 0: clear of the mirrored gems guard's 3x3

        // Sign naming the two roads out of the march (the fen town differs per side).
        const std::string ns = t.fy ? "north" : "south";
        const std::string ew = t.fx ? "west" : "east";
        const std::string ravens = t.fy ? "Ashford" : "Ravensgate";
        const std::string fen = t.fx ? "Mirefall" : "Greyfen";
        signAt( b, t, 14, 11,
                "The Margrave's roads: " + ew + " to the Ravens' Road and the toll town of " + ravens + ", " + ns + " to the fens and the fen-wardens of " + fen
                    + ". Both passes are watched. The capital lies beyond either." );

        // Stream feeding the water wheel.
        for ( int32_t y = 16; y <= 20; ++y ) {
            const P p = tf( t, 17, y );
            b.addStreamTile( p.x, p.y );
        }

        // Decor (tryPlace; symmetric under the transform; each sprite matched to its ground).
        deco( b, t, 16, 8, G::LANDSCAPE_TREES, pair( TREE_AUTUMN_BIG, t ) );
        deco( b, t, 3, 7, G::LANDSCAPE_TREES, pair( TREE_AUTUMN_MED, t ) );
        deco( b, t, 19, 10, G::LANDSCAPE_TREES, TREE_EVIL_SMALL1 );
        deco( b, t, 1, 16, G::LANDSCAPE_TREES, TREE_DIRT_TALL );
        deco( b, t, 10, 16, G::LANDSCAPE_TREES, TREE_DIRT_TWO_TALL );
        deco( b, t, 12, 17, G::LANDSCAPE_TREES, TREE_GENERIC_LOG );
        deco( b, t, 11, 18, G::LANDSCAPE_ROCKS, ROCK_DIRT_MEDIUM );
        deco( b, t, 1, 12, G::LANDSCAPE_ROCKS, ROCK_DIRT_SMALL );
        deco( b, t, 4, 12, G::LANDSCAPE_MISCELLANEOUS, LM_DIRT_LAKE_SMALL ); // two tiles wide (x-1..x): clear of every copy of the roads
        deco( b, t, 11, 2, G::LANDSCAPE_MISCELLANEOUS, LM_DIRT_SHRUB_WIDE1 );
        deco( b, t, 10, 13, G::LANDSCAPE_MISCELLANEOUS, LM_DIRT_FLOWERS_SMALL );
        deco( b, t, 5, 3, G::LANDSCAPE_MISCELLANEOUS, LM_GRASS_FLOWERS_SMALL );
        deco( b, t, 13, 20, G::LANDSCAPE_MISCELLANEOUS, LM_DIRT_GRASS );
        deco( b, t, 6, 2, G::LANDSCAPE_TREES, TREE_STUMPS_THREE );
        deco( b, t, 5, 17, G::LANDSCAPE_TREES, TREE_AUTUMN_SMALL2 );
        deco( b, t, 19, 4, G::LANDSCAPE_TREES, TREE_GENERIC_DEAD );
        deco( b, t, 8, 1, G::LANDSCAPE_TREES, pair( TREE_AUTUMN_MED, t ) );
        deco( b, t, 18, 13, G::LANDSCAPE_ROCKS, ROCK_DIRT_MEDIUM );
        deco( b, t, 12, 20, G::LANDSCAPE_TREES, TREE_STUMPS_TWO );
        deco( b, t, 5, 9, G::LANDSCAPE_MISCELLANEOUS, LM_GRASS_LAKE_SMALL );
        deco( b, t, 17, 1, G::LANDSCAPE_TREES, TREE_AUTUMN_SMALL1 );
        deco( b, t, 6, 1, G::LANDSCAPE_ROCKS, ROCK_DIRT_WIDE );
        deco( b, t, 15, 21, G::LANDSCAPE_TREES, TREE_GENERIC_DEAD );
    }

    // ------------------------------------------------------------------ N/S zones
    // t.fy = false builds the N zone (Ravensgate), t.fy = true the S zone (Ashford). t.fx is ignored:
    // seam objects are placed once, half objects for both x-halves.
    void northZone( MapBuilder & b, const T & tz )
    {
        const T t = { false, tz.fy };
        const std::string zn = t.fy ? "S" : "N";
        const char * townName = t.fy ? "Ashford" : "Ravensgate";

        // --- seam column ---
        const P te = tf( t, 35, 4 );
        const uint32_t townUid = b.placeCastle( te.x, te.y, PlayerColor::NONE, 1 /*Barbarian*/, false /*town*/, townName );
        {
            auto & meta = b.map.castleMetadata[townUid];
            meta.customBuildings = true;
            meta.builtBuildings = { BUILD_TENT, DWELLING_MONSTER1, DWELLING_MONSTER2 };
            setGarrison( b, townUid, RAVENSGATE_TYPES, RAVENSGATE_COUNTS );
        }

        // Seam prizes sit on the seam column; the old road splits into two mirrored branches (x = 33 and
        // x = 38) around them so neither half's players walk through a seam guard's 3x3.
        // Mines and the camp are entered from below, so the northern copies sit two rows further north
        // than the mirror image: that makes the walk from each zone's pass row to every seam prize the same.
        mineAt( b, t, 35, t.fy ? 11 : 8, Resource::GOLD, zn + " seam gold mine", Monster::NOMAD, G_N_GOLD_NOMADS );
        pl( b, t, 35, t.fy ? 14 : 12, G::ADVENTURE_POWER_UPS, PU_MERCENARY_CAMP, zn + " mercenary camp" );
        mineAt( b, t, 35, t.fy ? 17 : 15, Resource::MERCURY, zn + " alchemist lab", Monster::ORC, G_N_LAB_ORCS );
        // The toll town's vault: the major artifact on the far side of the town from the road.
        randomArtifactAt( b, t, 35, 0, ART_RANDOM_MAJOR, zn + " seam major artifact" );
        guardAt( b, t, 36, 0, Monster::OGRE, G_N_ARTIFACT_OGRES, zn + " seam artifact guard" );

        // --- the two halves ---
        for ( int half = 0; half < 2; ++half ) {
            const T th = { half == 1, t.fy };
            const std::string hn = zn + ( half == 0 ? "-west" : "-east" );

            mineAt( b, th, 29, 3, Resource::SULFUR, hn + " sulfur mine", Monster::ORC, G_N_SULFUR_ORCS );
            pl( b, th, 31, 7, G::ADVENTURE_DWELLINGS, DW_WAGON_CAMP, hn + " wagon camp" );
            pl( b, th, 27, 13, G::ADVENTURE_POWER_UPS, PU_WITCH_DOCTORS_HUT, hn + " witch doctor's hut" );
            pl( b, th, 29, 17, G::ADVENTURE_POWER_UPS, PU_FORT, hn + " fort" );
            pl( b, th, 27, 5, G::ADVENTURE_MISCELLANEOUS, MISC_OBS_TOWER_GENERIC, hn + " toll tower" );

            chestAt( b, th, 30, 13 );
            pileAt( b, th, 31, 13, Resource::GOLD );
            guardAt( b, th, 30, 14, Monster::NOMAD, G_N_CACHE_NOMADS, hn + " cache guard" );

            randomArtifactAt( b, th, 30, 19, ART_RANDOM_TREASURE, hn + " treasure artifact" );
            guardAt( b, th, 30, 20, Monster::NOMAD, G_N_TREASURE_NOMADS, hn + " treasure guard" );

            pl( b, th, 27, 15, G::ADVENTURE_TREASURES, TR_CAMPFIRE, hn + " campfire" );
            pileAt( b, th, 31, 16, Resource::ORE );
            pileAt( b, th, 31, 10, Resource::WOOD );

            // One warning sign per half, beside each pass exit's natural line to the town.
            signAt( b, th, 29, 10, "The Ravens' Road. The hill-clans of " + std::string( townName ) + " take their toll in gold or in blood, and keep both. "
                                   "The old road to the capital starts at the town gate." );

            // Decor: hill country - autumn trees, dead groves, dirt rocks (all on dirt).
            deco( b, th, 28, 11, G::LANDSCAPE_TREES, pair( TREE_AUTUMN_BIG, th ) );
            deco( b, th, 30, 12, G::LANDSCAPE_TREES, pair( TREE_EVIL_MED, th ) );
            deco( b, th, 28, 1, G::LANDSCAPE_ROCKS, ROCK_DIRT_BIG );
            deco( b, th, 32, 2, G::LANDSCAPE_TREES, TREE_DIRT_TWO_TALL );
            deco( b, th, 27, 10, G::LANDSCAPE_MISCELLANEOUS, LM_DIRT_SHRUB );
            deco( b, th, 28, 21, G::LANDSCAPE_TREES, TREE_GENERIC_DEAD );
            deco( b, th, 33, 4, G::LANDSCAPE_ROCKS, ROCK_DIRT_SMALL );
            deco( b, th, 27, 8, G::LANDSCAPE_TREES, TREE_STUMPS_TWO );
            deco( b, th, 30, 5, G::LANDSCAPE_MISCELLANEOUS, LM_DIRT_FLOWERS_WIDE );
            deco( b, th, 26, 17, G::LANDSCAPE_TREES, TREE_GENERIC_DEAD );
            deco( b, th, 32, 15, G::LANDSCAPE_MISCELLANEOUS, LM_DIRT_GRASS );
            deco( b, th, 27, 3, G::LANDSCAPE_TREES, TREE_AUTUMN_SMALL2 );
        }
    }

    // ------------------------------------------------------------------ W/E zones
    // t.fx = false builds the W zone (Greyfen), t.fx = true the E zone (Mirefall).
    void westZone( MapBuilder & b, const T & tz )
    {
        const T t = { tz.fx, false };
        const std::string zn = t.fx ? "E" : "W";
        const char * townName = t.fx ? "Mirefall" : "Greyfen";

        // --- seam row ---
        const P te = tf( t, 6, 33 );
        const uint32_t townUid = b.placeCastle( te.x, te.y, PlayerColor::NONE, 5 /*Necromancer*/, false /*town*/, townName );
        {
            auto & meta = b.map.castleMetadata[townUid];
            meta.customBuildings = true;
            meta.builtBuildings = { BUILD_TENT, DWELLING_MONSTER1, DWELLING_MONSTER2 };
            setGarrison( b, townUid, GREYFEN_TYPES, GREYFEN_COUNTS );
        }

        mineAt( b, t, 16, 35, Resource::GOLD, zn + " seam gold mine", Monster::MUMMY, G_W_GOLD_MUMMIES );
        pl( b, t, 11, 35, G::ADVENTURE_POWER_UPS, PU_STANDING_STONES, zn + " standing stones" );
        randomArtifactAt( b, t, 1, 35, ART_RANDOM_MAJOR, zn + " seam major artifact" );
        guardAt( b, t, 2, 36, Monster::MUMMY, G_W_ARTIFACT_MUMMIES, zn + " seam artifact guard" );

        // --- the two halves ---
        for ( int half = 0; half < 2; ++half ) {
            const T th = { t.fx, half == 1 };
            const std::string hn = zn + ( half == 0 ? "-north" : "-south" );

            mineAt( b, th, 5, 29, Resource::CRYSTAL, hn + " crystal mine", Monster::ZOMBIE, G_W_CRYSTAL_ZOMBIES );
            pl( b, th, 11, 28, G::ADVENTURE_MISCELLANEOUS, MISC_GRAVEYARD, hn + " graveyard" );
            pl( b, th, 16, 31, G::ADVENTURE_DWELLINGS, DW_RUINS, hn + " ruins" );
            pl( b, th, 18, 32, G::ADVENTURE_POWER_UPS, PU_SHRINE_SECOND, hn + " shrine" );
            pl( b, th, 14, 30, G::ADVENTURE_POWER_UPS, PU_TREE_OF_KNOWLEDGE, hn + " tree of knowledge" );

            chestAt( b, th, 13, 33 );
            pileAt( b, th, 1, 31, Resource::GOLD );
            pileAt( b, th, 8, 29, Resource::CRYSTAL );
            pileAt( b, th, 19, 30, Resource::WOOD );
            pl( b, th, 1, 27, G::ADVENTURE_TREASURES, TR_CAMPFIRE, hn + " campfire" );

            // One warning sign per half, beside the pass-B road where it enters the fen.
            signAt( b, th, 11, 31, "The " + std::string( townName ) + " fen. The fen-wardens of " + townName + " bury their toll-takers with the gold. "
                                   "The dead keep the road to the capital." );

            // Decor: dead country - evil trees, cracks, skulls (wasteland sprites on the fen, dirt sprites on dirt).
            deco( b, th, 3, 26, G::LANDSCAPE_TREES, pair( TREE_EVIL_BIG, th ) );
            deco( b, th, 14, 27, G::LANDSCAPE_TREES, pair( TREE_EVIL_MED, th ) );
            deco( b, th, 2, 32, G::LANDSCAPE_ROCKS, ROCK_DIRT_MEDIUM );
            deco( b, th, 12, 30, G::LANDSCAPE_MISCELLANEOUS, LM_WASTE_SKULL );
            deco( b, th, 7, 30, G::LANDSCAPE_MISCELLANEOUS, LM_WASTE_CRACK );
            deco( b, th, 12, 31, G::LANDSCAPE_TREES, TREE_GENERIC_DEAD );
            deco( b, th, 17, 29, G::LANDSCAPE_TREES, TREE_EVIL_SMALL1 );
            deco( b, th, 11, 33, G::LANDSCAPE_MISCELLANEOUS, LM_DIRT_SHRUB2 );
            deco( b, th, 2, 25, G::LANDSCAPE_TREES, TREE_STUMP_ONE );
            deco( b, th, 20, 33, G::LANDSCAPE_TREES, TREE_EVIL_SMALL2 );
            deco( b, th, 4, 34, G::LANDSCAPE_ROCKS, ROCK_DIRT_SMALL );
        }
    }

    // ------------------------------------------------------------------ center
    void center( MapBuilder & b )
    {
        // Kingsfall - the dead capital, a built Knight castle held by the Old Guard.
        const uint32_t uid = b.placeCastle( 35, 34, PlayerColor::NONE, 0 /*Knight*/, true /*castle*/, "Kingsfall" );
        {
            auto & meta = b.map.castleMetadata[uid];
            meta.customBuildings = true;
            meta.builtBuildings = { BUILD_CASTLE, BUILD_TAVERN, BUILD_WELL, BUILD_MARKETPLACE, BUILD_MAGEGUILD1, DWELLING_MONSTER1, DWELLING_MONSTER2, DWELLING_MONSTER3, DWELLING_MONSTER4 };
            setGarrison( b, uid, KINGSFALL_TYPES, KINGSFALL_COUNTS );
        }

        // The Company's caches: one per diagonal.
        for ( int q = 0; q < 4; ++q ) {
            const T & t = QUADS[q];
            const std::string qn = std::string( "center " ) + QUAD_NAMES[q];
            mineAt( b, t, 29, 29, Resource::GOLD, qn + " cache gold mine", Monster::CYCLOPS, G_CENTER_MINE_CYCLOPS );
            // Pickups around the guard (all inside its protected 3x3).
            const P m = tf( t, 29, 29 );
            b.placeResourcePile( m.x - 1, m.y + 1, Resource::GOLD, 0 );
            b.placeChest( m.x + 1, m.y + 1 );
            randomArtifactAbs( b, m.x, m.y + 2, ART_RANDOM_TREASURE, qn + " cache artifact" );

            deco( b, t, 29, 26, G::LANDSCAPE_ROCKS, ROCK_WASTE_TALL4 );
            deco( b, t, 26, 30, G::LANDSCAPE_TREES, TREE_GENERIC_DEAD );
            deco( b, t, 30, 32, G::LANDSCAPE_MISCELLANEOUS, LM_WASTE_CRACK );
            deco( b, t, 33, 29, G::LANDSCAPE_MISCELLANEOUS, LM_WASTE_SHRUB2 );
        }

        // Trading Posts beside the N/S seam roads (Company quartermasters), guards on the tile below.
        for ( const T & t : { QUADS[0], QUADS[2] } ) {
            const P p = tf( t, 32, 26 );
            // Unguarded: the gates already cost a fight, and a guard zone here sat between the S gate and
            // the S caches, which made the southern players' walk to their caches measurably longer.
            b.place( p.x, p.y, G::ADVENTURE_MISCELLANEOUS, MISC_TRADING_POST_WASTE, std::string( t.fy ? "S" : "N" ) + " trading post" );
        }

        // Hill Forts on the W/E seam (on dirt islands).
        for ( const T & t : { QUADS[0], QUADS[1] } ) {
            const P p = tf( t, 30, 36 );
            b.place( p.x, p.y, G::ADVENTURE_MISCELLANEOUS, MISC_HILL_FORT_DIRT, std::string( t.fx ? "E" : "W" ) + " hill fort" );
        }

        // Landmark decor of the Scar, x-mirrored in pairs so both gate roads see the same passability
        // (kept off the seam roads and the dirt islands; cracks are terrain-layer and never block).
        for ( const T & t : { QUADS[0], QUADS[1] } ) {
            deco( b, t, 30, 33, G::LANDSCAPE_ROCKS, ROCK_WASTE_WIDE );
            deco( b, t, 34, 38, G::LANDSCAPE_MISCELLANEOUS, LM_WASTE_SKULL );
            deco( b, t, 30, 41, G::LANDSCAPE_MISCELLANEOUS, LM_WASTE_CRACK_H );
            deco( b, t, 34, 39, G::LANDSCAPE_MISCELLANEOUS, LM_WASTE_CRACK_V );
            deco( b, t, 31, 30, G::LANDSCAPE_MISCELLANEOUS, LM_WASTE_SHRUB3 );
            deco( b, t, 28, 39, G::LANDSCAPE_TREES, TREE_EVIL_SMALL1 );
            deco( b, t, 33, 41, G::LANDSCAPE_ROCKS, ROCK_WASTE_SMALL );
        }
        static_cast<void>( LM_WASTE_TAR_PIT );
        static_cast<void>( MTN_WASTE_MED1 );
    }

    // ------------------------------------------------------------------ gates
    void gates( MapBuilder & b )
    {
        // Home passes (one A and one B per quadrant). Pass A: top corridor row in ABSOLUTE terms (see the
        // geometry note), so it also covers the sideways-walkable band row above the gap. Pass B: centre.
        for ( int q = 0; q < 4; ++q ) {
            const T & t = QUADS[q];
            const int32_t ax = t.fx ? MAXC - WALL : WALL;
            const int32_t ay = t.fy ? MAXC - GAP_A1 : GAP_A0;
            b.placeMonster( ax, ay, Monster::WOLF, G_PASS_A_WOLVES, std::string( QUAD_NAMES[q] ) + " pass A guard" );
            guardAt( b, t, 9, WALL, Monster::ZOMBIE, G_PASS_B_ZOMBIES, std::string( QUAD_NAMES[q] ) + " pass B guard" );
        }
        // Center gates: N/S on the horizontal walls, W/E on the vertical walls.
        b.placeMonster( 35, WALL, Monster::OGRE_LORD, G_GATE_NS_OGRE_LORDS, "N gate guard" );
        b.placeMonster( 35, MAXC - WALL, Monster::OGRE_LORD, G_GATE_NS_OGRE_LORDS, "S gate guard" );
        b.placeMonster( WALL, 35, Monster::MINOTAUR_KING, G_GATE_WE_MINOTAUR_KINGS, "W gate guard" );
        b.placeMonster( MAXC - WALL, 35, Monster::MINOTAUR_KING, G_GATE_WE_MINOTAUR_KINGS, "E gate guard" );
    }

    // ------------------------------------------------------------------ roads
    // All roads are authored tile lists. Home roads: NW is authored, NE is its x-mirror; the southern
    // homes get their own list (the castle sits five rows higher and faces the other way relative to
    // pass B), SE is the x-mirror of SW. Zone roads run from each pass exit to the toll town, seam
    // roads from the toll town's gate to the center gate and on into Kingsfall, all avoiding every
    // guard's protected 3x3 except the pass/gate guards themselves. The toll towns and Kingsfall stand
    // on column 35, half a tile off the mirror axis, so the eastern approach to each is one tile longer.
    void roads( MapBuilder & b )
    {
        // --- northern home (authored NW) ---
        const std::vector<P> northApron = { { 9, 8 }, { 9, 9 }, { 9, 10 } };
        std::vector<P> northToB;
        for ( int32_t y = 11; y <= 25; ++y ) {
            northToB.push_back( { 9, y } );
        }
        std::vector<P> northToA;
        for ( int32_t x = 10; x <= 18; ++x ) {
            northToA.push_back( { x, 10 } );
        }
        for ( const P & p : { P{ 19, 9 }, P{ 20, 8 }, P{ 21, 7 }, P{ 22, 7 }, P{ 23, 7 }, P{ 24, 7 }, P{ 25, 7 }, P{ 26, 7 } } ) {
            northToA.push_back( p );
        }

        // --- southern home (authored SW; castle (9,59), hero (9,60)) ---
        const std::vector<P> southApron = { { 9, 60 }, { 9, 61 }, { 9, 62 } };
        std::vector<P> southToA = { { 10, 63 }, { 11, 64 }, { 12, 65 } };
        for ( int32_t x = 13; x <= 26; ++x ) {
            southToA.push_back( { x, 65 } );
        }
        std::vector<P> southToB = { { 8, 60 }, { 7, 60 }, { 6, 59 }, { 6, 58 }, { 6, 57 }, { 7, 56 }, { 8, 55 } };
        for ( int32_t y = 54; y >= 50; --y ) {
            southToB.push_back( { 8, y } );
        }
        for ( const P & p : { P{ 9, 49 }, P{ 9, 48 }, P{ 9, 47 }, P{ 9, 46 } } ) {
            southToB.push_back( p );
        }

        for ( int q = 0; q < 4; ++q ) {
            const T & t = QUADS[q];
            const T fxOnly = { t.fx, false };
            if ( !t.fy ) {
                road( b, mirrored( northApron, fxOnly ) );
                road( b, mirrored( northToB, fxOnly ) );
                road( b, mirrored( northToA, fxOnly ) );
            }
            else {
                road( b, mirrored( southApron, fxOnly ) );
                road( b, mirrored( southToA, fxOnly ) );
                road( b, mirrored( southToB, fxOnly ) );
            }
        }

        // --- N zone: pass A exits -> along row 6 -> the Ravensgate gate; seam column -> N gate -> Kingsfall ---
        std::vector<P> northZoneWest;
        for ( int32_t x = 27; x <= 34; ++x ) {
            northZoneWest.push_back( { x, 6 } );
        }
        road( b, northZoneWest );
        road( b, mirrored( northZoneWest, { true, false } ) ); // (44..37, 6): both halves end next to (35,6)

        // Two-wide from the town gate, then a west branch (x = 33) and its mirror (x = 38) around the seam
        // prizes, two-wide through the gate, then the seam column with a spur to each cache and the
        // detour round the west side of Kingsfall (clear of its towers).
        std::vector<P> northSeam;
        for ( int32_t y = 5; y <= 6; ++y ) {
            northSeam.push_back( { 35, y } );
            northSeam.push_back( { 36, y } );
        }
        std::vector<P> westBranch;
        for ( int32_t y = 7; y <= 20; ++y ) {
            westBranch.push_back( { 33, y } );
        }
        westBranch.push_back( { 34, 21 } );
        westBranch.push_back( { 35, 22 } );
        road( b, westBranch );
        road( b, mirrored( westBranch, { true, false } ) ); // (37,9),(38,10..20),(37,21),(36,22)
        for ( int32_t y = 23; y <= 24; ++y ) {
            northSeam.push_back( { 35, y } );
            northSeam.push_back( { 36, y } );
        }
        for ( int32_t y = 25; y <= 30; ++y ) {
            northSeam.push_back( { 35, y } );
        }
        for ( int32_t x = 31; x <= 33; ++x ) {
            northSeam.push_back( { x, 30 } ); // spur to the NW cache (joins the detour at (34,30))
        }
        for ( int32_t x = 36; x <= 40; ++x ) {
            northSeam.push_back( { x, 30 } ); // spur to the NE cache
        }
        for ( const P & p : { P{ 34, 30 }, P{ 33, 31 }, P{ 32, 32 }, P{ 32, 33 }, P{ 32, 34 }, P{ 33, 35 }, P{ 34, 35 }, P{ 35, 35 } } ) {
            northSeam.push_back( p ); // round the west side of Kingsfall, clear of its towers
        }
        road( b, northSeam );

        // --- S zone: pass A exits -> along row 65 -> round Ashford's corners to its gate; seam column -> S gate -> Kingsfall ---
        // Ashford faces south, so each half gets its own corner connector: along row 65 to the town's
        // corner, down to the town gate, and diagonally up to the seam column.
        std::vector<P> southZoneWest;
        for ( int32_t x = 27; x <= 31; ++x ) {
            southZoneWest.push_back( { x, 65 } );
        }
        for ( const P & p : { P{ 32, 65 }, P{ 32, 66 }, P{ 32, 67 }, P{ 33, 68 }, P{ 34, 68 }, P{ 33, 64 } } ) {
            southZoneWest.push_back( p );
        }
        for ( int32_t y = 63; y >= 52; --y ) {
            southZoneWest.push_back( { 33, y } ); // west branch round the seam prizes
        }
        southZoneWest.push_back( { 34, 51 } );
        southZoneWest.push_back( { 35, 50 } );
        road( b, southZoneWest );
        std::vector<P> southZoneEast = mirrored( southZoneWest, { true, false } ); // (39,65..67),(38,68),(37,68),(38,64..52),(37,51),(36,50)
        southZoneEast.push_back( { 36, 68 } );
        road( b, southZoneEast );

        std::vector<P> southSeam = { { 35, 68 } };
        for ( int32_t y = 49; y >= 46; --y ) {
            southSeam.push_back( { 35, y } ); // two-wide through the S gate
            southSeam.push_back( { 36, y } );
        }
        for ( int32_t y = 45; y >= 35; --y ) {
            southSeam.push_back( { 35, y } ); // the seam column to Kingsfall's gate
        }
        for ( int32_t x = 31; x <= 34; ++x ) {
            southSeam.push_back( { x, 43 } ); // spur to the SW cache, on its guard's row (the N spur is on row 30 for the same reason)
        }
        for ( int32_t x = 36; x <= 40; ++x ) {
            southSeam.push_back( { x, 43 } ); // spur to the SE cache
        }
        road( b, southSeam );

        // --- W zone: pass B exits -> down/up column 9 -> the Greyfen gate; fen edge -> W gate -> Kingsfall (E = x-mirror) ---
        std::vector<P> westZoneNorth;
        for ( int32_t y = 26; y <= 34; ++y ) {
            westZoneNorth.push_back( { 9, y } );
        }
        std::vector<P> westZoneSouth;
        for ( int32_t y = 45; y >= 36; --y ) {
            westZoneSouth.push_back( { 9, y } );
        }
        // The seam road hugs the axis row (35) so both halves join it at (9,35); it steps to row 36
        // beside the Standing Stones and to row 34 to pass the seam gold mine outside its guard's 3x3.
        std::vector<P> westSeam = { { 6, 34 }, { 7, 35 }, { 8, 35 }, { 9, 35 }, { 10, 36 }, { 11, 36 }, { 12, 36 }, { 13, 35 }, { 14, 34 }, { 15, 34 }, { 16, 34 },
                                    { 17, 33 }, { 18, 33 }, { 19, 34 } };
        for ( int32_t x = 20; x <= 35; ++x ) {
            westSeam.push_back( { x, 35 } ); // straight along the axis row to Kingsfall's gate
        }
        // Southern branch round the seam gold mine (its guard covers rows 35-37) for the southern half.
        const std::vector<P> westSouthBranch = { { 13, 36 }, { 14, 37 }, { 15, 38 }, { 16, 38 }, { 17, 38 }, { 18, 37 }, { 19, 36 } };
        for ( const T & t : { QUADS[0], QUADS[1] } ) {
            road( b, mirrored( westZoneNorth, t ) );
            road( b, mirrored( westZoneSouth, t ) );
            road( b, mirrored( westSouthBranch, t ) );
            std::vector<P> seam = mirrored( westSeam, t );
            if ( t.fx ) {
                seam.push_back( { 35, 35 } ); // the mirror ends at (36,35), one tile east of the gate
            }
            road( b, seam );
        }
    }

    // ------------------------------------------------------------------ scenario
    void configureScenario( MapBuilder & b )
    {
        auto & m = b.map;

        m.name = "The Ashen Succession";
        m.description = "The High King of Vaelmark died at harvest with no heir. Four Margraves hold the four marches, and each means to be crowned. "
                        "Between them lie the toll towns of the old roads - Ravensgate and Ashford to the north and south, Greyfen and Mirefall in the fens "
                        "to the west and east - and the dead capital, Kingsfall, where the Old Guard keeps an empty throne. "
                        "Defeat the other three claimants to take the crown. "
                        "The toll towns and Kingsfall are strongly garrisoned: do not march on them before your second month. Each toll town is a built town "
                        "and Kingsfall a full castle with four dwellings, a Marketplace and a Mage Guild; whoever takes them gains a second seat of power. "
                        "Every march has the same roads, mines and passes. What differs is who reaches the middle first.";
        m.creatorNotes = "Generated scenario built against fheroes2 revision b086d1aa. 72x72, four mirrored marches, all slots human-playable, random races. "
                         "Designed for Normal difficulty. Victory: defeat all enemies.";

        m.difficulty = 1; // Normal
        m.isCampaign = false;

        const uint8_t four = static_cast<uint8_t>( PlayerColor::BLUE ) | static_cast<uint8_t>( PlayerColor::GREEN ) | static_cast<uint8_t>( PlayerColor::RED )
                             | static_cast<uint8_t>( PlayerColor::YELLOW );
        m.humanPlayerColors = four;
        m.computerPlayerColors = four;

        m.victoryConditionType = 0; // VICTORY_DEFEAT_EVERYONE
        m.isVictoryConditionApplicableForAI = true;
        m.allowNormalVictory = true;
        m.victoryConditionMetadata.clear();

        m.lossConditionType = 0; // LOSS_EVERYTHING
        m.lossConditionMetadata.clear();

        auto addEvent = [&]( const std::string & message, const uint32_t day ) {
            Maps::Map_Format::DailyEvent ev;
            ev.message = message;
            ev.humanPlayerColors = four;
            ev.computerPlayerColors = four;
            ev.firstOccurrenceDay = day;
            ev.repeatPeriodInDays = 0;
            m.dailyEvents.push_back( ev );
        };

        addEvent( "The King is dead. By the old law the crown falls to whichever Margrave holds the realm entire, and the other three have already called "
                  "their banners. Your march is yours to hold. The two roads out of it are watched by wolves and worse.",
                  1 );
        addEvent( "The toll towns have shut their gates. The hill-clans of Ravensgate and Ashford and the fen-wardens of Greyfen and Mirefall will treat "
                  "with no claimant. They will keep their walls and their gold until someone takes both.",
                  8 );
        addEvent( "One month since the harvest. Kingsfall has stopped answering letters. The Old Guard still drills in the square, and the Company's camps "
                  "in the Ashen Scar grow by the week.",
                  29 );
        addEvent( "Two months. Whoever holds the crown mines under Kingsfall now can pay for the war that ends this.", 57 );

        m.rumors = {
            "The Old Guard swore an oath to the throne, not to any claimant. They will fight whoever comes.",
            "Four gold mines lie in the Ashen Scar around the capital. The Company guards them with one-eyed giants.",
            "The hill-clans at Ravensgate and Ashford sell their axes to the highest bidder, and keep the coin.",
            "The fen-wardens of Greyfen and Mirefall bury their toll-takers with the gold.",
            "Wolves have come down from the marches since the harvest failed. The dead walk the fen road.",
            "The last king walled the four roads to the capital to keep his Margraves at home. The walls outlived him.",
        };
    }

    // Guard-sealed check, per hero: without fighting, a hero must reach action objects of its own home
    // only. Uses the engine's own protection rule (Maps::isTileUnderProtection). Aborts the build on a leak.
    void verifySealed( MapBuilder & b )
    {
        b.refreshPassability();
        for ( int q = 0; q < 4; ++q ) {
            const T & t = QUADS[q];
            const P c = castleTile( t );
            std::vector<bool> sealed;
            b.computeReachability( c.x, c.y + 1, sealed, true );

            size_t count = 0;
            bool leak = false;
            for ( const auto & po : b.placed ) {
                if ( !po.isAction || po.group == G::MONSTERS || !sealed[b.idx( po.x, po.y )] ) {
                    continue;
                }
                ++count;
                const bool inHomeX = t.fx ? po.x >= MAXC - 20 : po.x <= 20;
                const bool inHomeY = t.fy ? po.y >= MAXC - 20 : po.y <= 20;
                if ( !inHomeX || !inHomeY ) {
                    std::cerr << "LEAK: " << QUAD_NAMES[q] << " hero reaches " << po.label << " at (" << po.x << "," << po.y << ") without a fight" << std::endl;
                    leak = true;
                }
                else {
                    std::cerr << "  free: " << po.label << " (" << po.x << "," << po.y << ")" << std::endl;
                }
            }
            std::cerr << "sealed check " << QUAD_NAMES[q] << ": " << count << " free objects" << std::endl;
            if ( leak || count != 17 ) {
                std::cerr << "FATAL: guard-sealed check failed for " << QUAD_NAMES[q] << " (expected exactly the 17 free home objects)" << std::endl;
                std::exit( 1 );
            }
        }
    }

    // Per-player movement-cost table from the castle tile (where the hero starts at load) to the
    // objectives that decide the race, under engine movement rules with fights ignored. Printed for the
    // validation report; a spread above the tolerance is reported as a warning, not a build failure.
    void fairnessTable( MapBuilder & b )
    {
        struct Target
        {
            const char * name;
            P p[4];
        };
        std::vector<Target> targets;
        const auto add = [&]( const char * name, const std::function<P( const T & )> & f ) {
            Target tg{ name, {} };
            for ( int q = 0; q < 4; ++q ) {
                tg.p[q] = f( QUADS[q] );
            }
            targets.push_back( tg );
        };
        add( "pass A guard", []( const T & t ) { return P{ t.fx ? MAXC - WALL : WALL, t.fy ? MAXC - GAP_A1 : GAP_A0 }; } );
        add( "pass B guard", []( const T & t ) { return tf( t, 9, WALL ); } );
        add( "own sawmill", []( const T & t ) { return t.fy ? tf( t, 14, 13 ) : tf( t, 13, 4 ); } );
        add( "own ore mine", []( const T & t ) { return tf( t, 4, 15 ); } );
        add( "home gold guard", []( const T & t ) { const P m = tf( t, t.fy ? 18 : 16, 12 ); return P{ m.x, m.y + 1 }; } );
        add( "N/S toll town gate", []( const T & t ) { return P{ 35, t.fy ? 68 : 5 }; } );
        add( "W/E toll town gate", []( const T & t ) { return P{ t.fx ? MAXC - 6 : 6, 34 }; } );
        add( "N/S seam gold guard", []( const T & t ) { return P{ 35, t.fy ? MAXC - 11 + 1 : 9 }; } );
        add( "N/S seam artifact", []( const T & t ) { return P{ 35, t.fy ? MAXC : 0 }; } );

        add( "W/E seam gold guard", []( const T & t ) { return P{ t.fx ? MAXC - 16 : 16, 36 }; } );
        add( "N/S gate guard", []( const T & t ) { return P{ 35, t.fy ? MAXC - WALL : WALL }; } );
        add( "W/E gate guard", []( const T & t ) { return P{ t.fx ? MAXC - WALL : WALL, 35 }; } );
        add( "Kingsfall gate", []( const T & ) { return P{ 35, 35 }; } );
        add( "own centre cache guard", []( const T & t ) { return P{ tf( t, 29, 29 ).x, tf( t, 29, 29 ).y + 1 }; } );

        b.refreshPassability();
        std::cerr << "=== fairness: movement cost from each castle (engine rules, fights ignored) ===" << std::endl;
        int32_t worstSpread = 0;
        const char * worstName = "";
        for ( const Target & tg : targets ) {
            int32_t lo = INT32_MAX;
            int32_t hi = INT32_MIN;
            std::string line = std::string( "  " ) + tg.name + ":";
            for ( int q = 0; q < 4; ++q ) {
                const P c = castleTile( QUADS[q] );
                const int32_t cost = b.movementCost( c.x, c.y, tg.p[q].x, tg.p[q].y );
                line += std::string( " " ) + QUAD_NAMES[q] + "=" + std::to_string( cost );
                lo = std::min( lo, cost );
                hi = std::max( hi, cost );
            }
            const int32_t spread = hi - lo;
            line += "  spread=" + std::to_string( spread );
            std::cerr << line << std::endl;
            if ( spread > worstSpread ) {
                worstSpread = spread;
                worstName = tg.name;
            }
        }
        std::cerr << "fairness: worst spread " << worstSpread << " move points (" << worstName << ")"
                  << ( worstSpread > 250 ? "  FAIRNESS WARNING: above the 250 tolerance" : "" ) << std::endl;
    }
}

void buildAshenSuccession( MapBuilder & b )
{
    b.strictPlacement = true; // every overlap is a bug in a mirrored layout

    terrain( b );
    walls( b );

    for ( int q = 0; q < 4; ++q ) {
        home( b, q );
    }

    northZone( b, QUADS[0] ); // N: Ravensgate
    northZone( b, QUADS[2] ); // S: Ashford
    westZone( b, QUADS[0] ); // W: Greyfen
    westZone( b, QUADS[1] ); // E: Mirefall
    center( b );

    roads( b );
    ridgeDecor( b );
    gates( b );
    fillPockets( b );
    verifySealed( b );
    fairnessTable( b );
    configureScenario( b );
}
