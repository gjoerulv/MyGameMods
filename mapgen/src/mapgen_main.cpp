// King's Ransom .fh2m generator - main / builder implementation.
//
// Usage:
//   mapgen generate <out.fh2m> [seed]
//   mapgen inspect  <file.fh2m>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <queue>
#include <set>
#include <sstream>

#include "mapgen.h"

#include "artifact.h"
#include "castle.h"
#include "direction.h"
#include "ground.h"
#include "map_format_helper.h"
#include "maps_tiles.h"
#include "maps_tiles_helper.h"
#include "game_over.h"
#include "maps_fileinfo.h"
#include "monster.h"
#include "mp2.h"
#include "players.h"
#include "race.h"
#include "rand.h"
#include "resource.h"
#include "settings.h"
#include "ui_map_object.h"
#include "world.h"
#include "world_object_uid.h"

namespace
{
    [[noreturn]] void die( const std::string & message )
    {
        std::cerr << "FATAL: " << message << std::endl;
        std::exit( 1 );
    }

    const int8_t neighborDx[8] = { -1, 0, 1, 1, 1, 0, -1, -1 };
    const int8_t neighborDy[8] = { -1, -1, -1, 0, 1, 1, 1, 0 };
    const int neighborDir[8] = { Direction::TOP_LEFT, Direction::TOP,    Direction::TOP_RIGHT,   Direction::RIGHT,
                                 Direction::BOTTOM_RIGHT, Direction::BOTTOM, Direction::BOTTOM_LEFT, Direction::LEFT };
}

void MapBuilder::init( const int32_t width, const uint32_t seed )
{
    // Deterministic output: reseed the engine RNG (it only affects cosmetic image variants).
    Rand::CurrentThreadRandomDevice() = Rand::PCG32( seed );

    W = width;
    map = {};
    map.width = width;
    map.tiles.clear();
    map.tiles.resize( static_cast<size_t>( width ) * width );

    world.generateUninitializedMap( width );
    for ( int32_t i = 0; i < width * width; ++i ) {
        world.getTile( i ).setIndex( i );
    }

    Maps::resetObjectUID();

    // Same as the Editor's new-map flow: start from an all-water map.
    for ( int32_t i = 0; i < width * width; ++i ) {
        Maps::setTerrainOnTile( map, i, Maps::Ground::WATER );
    }
}

void MapBuilder::paintRect( const int32_t x0, const int32_t y0, const int32_t x1, const int32_t y1, const int groundId )
{
    assert( x0 <= x1 && y0 <= y1 && x0 >= 0 && y0 >= 0 && x1 < W && y1 < W );
    Maps::setTerrainWithTransition( map, idx( x0, y0 ), idx( x1, y1 ), groundId );
}

void MapBuilder::paintTile( const int32_t x, const int32_t y, const int groundId )
{
    paintRect( x, y, x, y, groundId );
}

void MapBuilder::paintBlob( const int32_t cx, const int32_t cy, const int32_t rx, const int32_t ry, const int groundId )
{
    for ( int32_t y = cy - ry; y <= cy + ry; ++y ) {
        for ( int32_t x = cx - rx; x <= cx + rx; ++x ) {
            if ( x < 0 || y < 0 || x >= W || y >= W ) {
                continue;
            }
            const double nx = static_cast<double>( x - cx ) / rx;
            const double ny = static_cast<double>( y - cy ) / ry;
            const double d = nx * nx + ny * ny;
            if ( d <= 0.75 || ( d <= 1.05 && Rand::Get( 0, 99 ) < 60 ) ) {
                paintTile( x, y, groundId );
            }
        }
    }
}

uint32_t MapBuilder::place( const int32_t x, const int32_t y, const Maps::ObjectGroup group, const uint32_t index, const std::string & label )
{
    if ( x < 0 || y < 0 || x >= W || y >= W ) {
        die( "place out of bounds: " + label );
    }

    const auto & objects = Maps::getObjectsByGroup( group );
    if ( index >= objects.size() ) {
        die( "object index out of range: " + label );
    }

    const Maps::ObjectInfo & info = objects[index];

    if ( !Maps::setObjectOnTile( world.getTile( idx( x, y ) ), info, false ) ) {
        die( "setObjectOnTile failed: " + label + " at (" + std::to_string( x ) + "," + std::to_string( y ) + ")" );
    }

    Maps::addObjectToMap( map, idx( x, y ), group, index );

    const uint32_t uid = Maps::getLastObjectUID();

    PlacedObject po;
    po.x = x;
    po.y = y;
    po.group = group;
    po.index = index;
    po.uid = uid;
    po.isAction = MP2::isOffGameActionObject( info.objectType );
    po.label = label;
    placed.push_back( po );

    return uid;
}

uint32_t MapBuilder::placeCastle( const int32_t x, const int32_t y, const PlayerColor color, const int raceSlot, const bool isCastle, const std::string & name )
{
    const int ground = world.getTile( idx( x, y ) ).GetGround();
    const uint32_t basementId = static_cast<uint32_t>( fheroes2::getTownBasementId( ground ) );

    place( x, y, Maps::ObjectGroup::LANDSCAPE_TOWN_BASEMENTS, basementId, name + " (basement)" );

    const uint32_t uid = Maps::getLastObjectUID();
    Maps::setLastObjectUID( uid - 1 );

    const uint32_t townIndex = static_cast<uint32_t>( raceSlot * 2 + ( isCastle ? 0 : 1 ) );
    place( x, y, Maps::ObjectGroup::KINGDOM_TOWNS, townIndex, name );

    // Refresh the road tile in front of the entrance (turns into the castle apron piece).
    Maps::updateRoadOnTile( map, idx( x, y + 1 ) );

    if ( color == PlayerColor::NONE ) {
        Maps::setDefaultCastleDefenderArmy( map.castleMetadata[uid] );
    }

    const int colorIndex = Color::GetIndex( color );

    Maps::setLastObjectUID( uid - 1 );
    place( x - 1, y, Maps::ObjectGroup::LANDSCAPE_FLAGS, static_cast<uint32_t>( colorIndex * 2 ), name + " (left flag)" );
    Maps::setLastObjectUID( uid - 1 );
    place( x + 1, y, Maps::ObjectGroup::LANDSCAPE_FLAGS, static_cast<uint32_t>( colorIndex * 2 + 1 ), name + " (right flag)" );

    map.castleMetadata[uid].customName = name;

    return uid;
}

uint32_t MapBuilder::placeHero( const int32_t x, const int32_t y, const PlayerColor color, const int raceSlot, const std::string & name )
{
    const int colorIndex = Color::GetIndex( color );
    const uint32_t uid = place( x, y, Maps::ObjectGroup::KINGDOM_HEROES, static_cast<uint32_t>( colorIndex * 7 + raceSlot ), "hero " + name );
    if ( !name.empty() ) {
        map.heroMetadata[uid].customName = name;
    }
    return uid;
}

uint32_t MapBuilder::placeMine( const int32_t x, const int32_t y, const int resourceType, const std::string & label )
{
    const int ground = world.getTile( idx( x, y ) ).GetGround();
    const uint32_t index = static_cast<uint32_t>( fheroes2::getMineObjectInfoId( resourceType, ground ) );
    return place( x, y, Maps::ObjectGroup::ADVENTURE_MINES, index, label );
}

uint32_t MapBuilder::placeMonster( const int32_t x, const int32_t y, const int monsterId, const int32_t count, const std::string & label )
{
    const uint32_t uid = place( x, y, Maps::ObjectGroup::MONSTERS, static_cast<uint32_t>( monsterId - 1 ), label );
    map.monsterMetadata[uid].count = count;
    return uid;
}

uint32_t MapBuilder::placeResourcePile( const int32_t x, const int32_t y, const int resourceType, const int32_t amount )
{
    uint32_t index = 0;
    switch ( resourceType ) {
    case Resource::WOOD:
        index = 0;
        break;
    case Resource::MERCURY:
        index = 1;
        break;
    case Resource::ORE:
        index = 2;
        break;
    case Resource::SULFUR:
        index = 3;
        break;
    case Resource::CRYSTAL:
        index = 4;
        break;
    case Resource::GEMS:
        index = 5;
        break;
    case Resource::GOLD:
        index = 6;
        break;
    default:
        die( "bad resource pile type" );
    }

    const uint32_t uid = place( x, y, Maps::ObjectGroup::ADVENTURE_TREASURES, index, "resource pile" );
    map.resourceMetadata[uid].count = amount;
    return uid;
}

uint32_t MapBuilder::placeChest( const int32_t x, const int32_t y )
{
    return place( x, y, Maps::ObjectGroup::ADVENTURE_TREASURES, 9, "treasure chest" );
}

uint32_t MapBuilder::placeArtifact( const int32_t x, const int32_t y, const int artifactId, const std::string & label )
{
    assert( artifactId >= Artifact::ARCANE_NECKLACE && artifactId < Artifact::MAGIC_BOOK );
    return place( x, y, Maps::ObjectGroup::ADVENTURE_ARTIFACTS, static_cast<uint32_t>( artifactId - Artifact::ARCANE_NECKLACE ), label );
}

uint32_t MapBuilder::placeSign( const int32_t x, const int32_t y, const uint32_t signIndex, const std::string & message )
{
    const uint32_t uid = place( x, y, Maps::ObjectGroup::ADVENTURE_MISCELLANEOUS, signIndex, "sign" );
    map.signMetadata[uid].message = message;
    return uid;
}

void MapBuilder::addRoad( const int32_t x, const int32_t y )
{
    if ( !Maps::setRoadOnTile( map, idx( x, y ) ) ) {
        std::cerr << "WARNING: road not placed at (" << x << "," << y << ")" << std::endl;
    }
}

void MapBuilder::addStreamTile( const int32_t x, const int32_t y )
{
    if ( !Maps::addStream( map, idx( x, y ) ) ) {
        std::cerr << "WARNING: stream not placed at (" << x << "," << y << ")" << std::endl;
    }
}

bool MapBuilder::finalize()
{
    if ( !Maps::updateMapPlayers( map ) ) {
        std::cerr << "updateMapPlayers failed" << std::endl;
        return false;
    }

    world.updatePassabilities();
    return true;
}

bool MapBuilder::save( const std::string & path ) const
{
    return Maps::Map_Format::saveMap( path, map );
}

void MapBuilder::computeReachability( const int32_t startX, const int32_t startY, std::vector<bool> & reachable ) const
{
    reachable.assign( static_cast<size_t>( W ) * W, false );

    std::queue<int32_t> queue;
    const int32_t start = idx( startX, startY );
    reachable[start] = true;
    queue.push( start );

    while ( !queue.empty() ) {
        const int32_t cur = queue.front();
        queue.pop();

        const int32_t cx = cur % W;
        const int32_t cy = cur / W;

        const Maps::Tile & fromTile = world.getTile( cur );

        // Do not path *through* non-removable action objects (heroes stop on them for good).
        // Monsters, enemy heroes and pickups are removable: fight/collect, then walk on.
        const MP2::MapObjectType fromType = fromTile.getMainObjectType();
        const bool isRemovable = fromType == MP2::OBJ_MONSTER || fromType == MP2::OBJ_HERO || fromType == MP2::OBJ_TREASURE_CHEST || fromType == MP2::OBJ_RESOURCE
                                 || fromType == MP2::OBJ_ARTIFACT || fromType == MP2::OBJ_CAMPFIRE;
        if ( cur != start && !isRemovable && MP2::isOffGameActionObject( fromType ) ) {
            continue;
        }

        for ( int d = 0; d < 8; ++d ) {
            const int32_t nx = cx + neighborDx[d];
            const int32_t ny = cy + neighborDy[d];
            if ( nx < 0 || ny < 0 || nx >= W || ny >= W ) {
                continue;
            }
            const int32_t ni = idx( nx, ny );
            if ( reachable[ni] ) {
                continue;
            }
            const Maps::Tile & toTile = world.getTile( ni );
            if ( toTile.isWater() ) {
                continue;
            }
            if ( !fromTile.isPassableTo( neighborDir[d] ) ) {
                continue;
            }
            if ( !toTile.isPassableFrom( Direction::Reflect( neighborDir[d] ) ) ) {
                continue;
            }
            reachable[ni] = true;
            queue.push( ni );
        }
    }
}

std::vector<std::string> MapBuilder::checkActionReachability( const std::vector<bool> & reachable ) const
{
    std::vector<std::string> problems;

    for ( const PlacedObject & po : placed ) {
        if ( !po.isAction ) {
            continue;
        }
        if ( reachable[idx( po.x, po.y )] ) {
            continue;
        }
        problems.push_back( po.label + " at (" + std::to_string( po.x ) + "," + std::to_string( po.y ) + ") is NOT reachable" );
    }

    return problems;
}

std::string MapBuilder::asciiTerrain() const
{
    std::ostringstream os;

    std::vector<char> grid( static_cast<size_t>( W ) * W );
    for ( int32_t i = 0; i < W * W; ++i ) {
        char c = '?';
        switch ( Maps::Ground::getGroundByImageIndex( map.tiles[i].terrainIndex ) ) {
        case Maps::Ground::WATER:
            c = '~';
            break;
        case Maps::Ground::GRASS:
            c = '.';
            break;
        case Maps::Ground::SNOW:
            c = '*';
            break;
        case Maps::Ground::SWAMP:
            c = 'w';
            break;
        case Maps::Ground::LAVA:
            c = 'l';
            break;
        case Maps::Ground::DESERT:
            c = ':';
            break;
        case Maps::Ground::DIRT:
            c = ',';
            break;
        case Maps::Ground::WASTELAND:
            c = 'x';
            break;
        case Maps::Ground::BEACH:
            c = 'b';
            break;
        default:
            break;
        }
        grid[i] = c;
    }

    // Overlays: roads/streams first, then objects (markers by group).
    for ( int32_t i = 0; i < W * W; ++i ) {
        for ( const auto & obj : map.tiles[i].objects ) {
            char c = 0;
            switch ( obj.group ) {
            case Maps::ObjectGroup::ROADS:
                c = '+';
                break;
            case Maps::ObjectGroup::STREAMS:
                c = '=';
                break;
            default:
                break;
            }
            if ( c != 0 && ( grid[i] == '.' || grid[i] == ',' || grid[i] == '*' || grid[i] == ':' || grid[i] == 'x' ) ) {
                grid[i] = c;
            }
        }
    }

    for ( const PlacedObject & po : placed ) {
        char c = 0;
        switch ( po.group ) {
        case Maps::ObjectGroup::KINGDOM_TOWNS:
            c = 'C';
            break;
        case Maps::ObjectGroup::KINGDOM_HEROES:
            c = 'H';
            break;
        case Maps::ObjectGroup::ADVENTURE_MINES:
            c = 'M';
            break;
        case Maps::ObjectGroup::MONSTERS:
            c = 'g';
            break;
        case Maps::ObjectGroup::ADVENTURE_TREASURES:
            c = 't';
            break;
        case Maps::ObjectGroup::ADVENTURE_ARTIFACTS:
            c = 'a';
            break;
        case Maps::ObjectGroup::ADVENTURE_DWELLINGS:
            c = 'd';
            break;
        case Maps::ObjectGroup::ADVENTURE_POWER_UPS:
            c = 'p';
            break;
        case Maps::ObjectGroup::ADVENTURE_MISCELLANEOUS:
            c = 'o';
            break;
        case Maps::ObjectGroup::LANDSCAPE_MOUNTAINS:
            c = '#';
            break;
        case Maps::ObjectGroup::LANDSCAPE_TREES:
            c = 'T';
            break;
        case Maps::ObjectGroup::LANDSCAPE_ROCKS:
            c = 'r';
            break;
        default:
            break;
        }
        if ( c != 0 ) {
            grid[idx( po.x, po.y )] = c;
        }
    }

    os << "    ";
    for ( int32_t x = 0; x < W; ++x ) {
        os << ( x % 10 );
    }
    os << '\n';
    for ( int32_t y = 0; y < W; ++y ) {
        os << ( y < 10 ? " " : "" ) << y << ( y < 10 ? "  " : "  " );
        for ( int32_t x = 0; x < W; ++x ) {
            os << grid[idx( x, y )];
        }
        os << '\n';
    }
    return os.str();
}

std::string MapBuilder::asciiPassability( const std::vector<bool> & reachable ) const
{
    std::ostringstream os;
    os << "    ";
    for ( int32_t x = 0; x < W; ++x ) {
        os << ( x % 10 );
    }
    os << '\n';
    for ( int32_t y = 0; y < W; ++y ) {
        os << ( y < 10 ? " " : "" ) << y << "  ";
        for ( int32_t x = 0; x < W; ++x ) {
            const Maps::Tile & tile = world.getTile( idx( x, y ) );
            char c;
            if ( tile.GetPassable() == 0 ) {
                c = '#';
            }
            else if ( reachable[idx( x, y )] ) {
                c = MP2::isOffGameActionObject( tile.getMainObjectType() ) ? 'A' : '.';
            }
            else {
                c = '!';
            }
            os << c;
        }
        os << '\n';
    }
    return os.str();
}

namespace
{
    bool filesIdentical( const std::string & a, const std::string & b )
    {
        std::ifstream fa( a, std::ios::binary );
        std::ifstream fb( b, std::ios::binary );
        if ( !fa || !fb ) {
            return false;
        }
        std::vector<char> da( ( std::istreambuf_iterator<char>( fa ) ), std::istreambuf_iterator<char>() );
        std::vector<char> db( ( std::istreambuf_iterator<char>( fb ) ), std::istreambuf_iterator<char>() );
        return da == db;
    }

    int generate( const MapDefinition & def, const std::string & outPath, const uint32_t seed )
    {
        MapBuilder builder;
        builder.init( def.width, seed );

        def.build( builder );

        if ( !builder.finalize() ) {
            die( "finalize failed" );
        }

        // --- validation: reachability from the tile in front of the Blue castle ---
        int32_t heroX = -1;
        int32_t heroY = -1;
        for ( const auto & po : builder.placed ) {
            if ( po.group == Maps::ObjectGroup::KINGDOM_HEROES ) {
                heroX = po.x;
                heroY = po.y;
                break;
            }
        }
        if ( heroX < 0 ) {
            die( "no hero placed" );
        }

        std::vector<bool> reachable;
        builder.computeReachability( heroX, heroY, reachable );

        const std::vector<std::string> problems = builder.checkActionReachability( reachable );

        std::cout << "=== terrain/objects ===\n" << builder.asciiTerrain() << '\n';
        std::cout << "=== passability (# blocked, . reachable, A action, ! unreachable) ===\n" << builder.asciiPassability( reachable ) << '\n';

        std::cout << "=== placed objects ===\n";
        for ( const auto & po : builder.placed ) {
            std::cout << "  (" << po.x << ',' << po.y << ") uid " << po.uid << " group " << static_cast<int>( po.group ) << " index " << po.index
                      << ( po.isAction ? "  [action]  " : "            " ) << po.label << '\n';
        }
        std::cout << "placed objects: " << builder.placed.size() << '\n';
        if ( problems.empty() ) {
            std::cout << "reachability: all action objects reachable from Blue start\n";
        }
        else {
            std::cout << "REACHABILITY PROBLEMS (" << problems.size() << "):\n";
            for ( const auto & p : problems ) {
                std::cout << "  " << p << '\n';
            }
        }

        if ( !builder.save( outPath ) ) {
            die( "saveMap failed" );
        }
        std::cout << "saved: " << outPath << '\n';

        // --- round trip through the real loader ---
        Maps::Map_Format::MapFormat loadedMap;
        if ( !Maps::Map_Format::loadMap( outPath, loadedMap ) ) {
            die( "round-trip loadMap FAILED" );
        }
        std::cout << "round-trip load: OK (version " << loadedMap.version << ", width " << loadedMap.width << ", tiles " << loadedMap.tiles.size() << ")\n";

        const std::string resavedPath = outPath + ".resaved";
        if ( !Maps::Map_Format::saveMap( resavedPath, loadedMap ) ) {
            die( "re-save FAILED" );
        }
        if ( filesIdentical( outPath, resavedPath ) ) {
            std::cout << "round-trip byte comparison: IDENTICAL\n";
        }
        else {
            std::cout << "round-trip byte comparison: DIFFERENT (investigate!)\n";
        }
        std::remove( resavedPath.c_str() );

        return problems.empty() ? 0 : 2;
    }

    // Replicates the sequence the game itself performs when starting a scenario:
    // FileInfo::readResurrectionMap -> Settings::setCurrentMapInfo -> Players::Init/SetStartGame
    // -> World::loadResurrectionMap (see game_scenarioinfo.cpp:LoadNewMap and
    // game_auto_playtest.cpp:prepareMap at fheroes2 @ b086d1aa).
    int gameLoad( const std::string & path )
    {
        Maps::FileInfo fi;
        if ( !fi.readResurrectionMap( path, false /* not for editor */, fheroes2::SupportedLanguage::English ) ) {
            die( "FileInfo::readResurrectionMap FAILED (map would not appear in the scenario list)" );
        }

        std::cout << "FileInfo: name '" << fi.name << "', size " << fi.width << 'x' << fi.height << ", difficulty " << static_cast<int>( fi.difficulty ) << '\n';
        std::cout << "  human-only colors " << static_cast<int>( fi.HumanOnlyColors() ) << ", comp+human " << static_cast<int>( fi.AllowCompHumanColors() )
                  << ", computer-only " << static_cast<int>( fi.ComputerOnlyColors() ) << '\n';
        std::cout << "  ConditionWins bits:  0x" << std::hex << fi.ConditionWins() << std::dec << " (expect 0x20 = WINS_GOLD only)\n";
        std::cout << "  ConditionLoss bits:  0x" << std::hex << fi.ConditionLoss() << std::dec << " (expect 0x800 = LOSS_TIME)\n";
        std::cout << "  gold target: " << fi.getWinningGoldAccumulationValue() << " (expect 100000)\n";
        std::cout << "  loss days:   " << fi.LossCountDays() << " (expect 56)\n";
        std::cout << "  comp also wins: " << fi.WinsCompAlsoWins() << " (expect 0)\n";

        Settings & conf = Settings::Get();
        conf.setCurrentMapInfo( fi );

        Players & players = conf.GetPlayers();
        players.Init( conf.getCurrentMapInfo() );
        players.SetStartGame();

        if ( !world.loadResurrectionMap( conf.getCurrentMapInfo().filename ) ) {
            die( "World::loadResurrectionMap FAILED - the game would reject this map" );
        }

        std::cout << "World::loadResurrectionMap: OK. World " << world.w() << 'x' << world.h() << ", castles " << world.getCastleCount() << '\n';

        for ( const PlayerColor color : { PlayerColor::BLUE, PlayerColor::GREEN } ) {
            const Kingdom & kingdom = world.GetKingdom( color );
            std::cout << "  kingdom " << ( color == PlayerColor::BLUE ? "BLUE " : "GREEN" ) << ": castles " << kingdom.GetCastles().size() << ", heroes "
                      << kingdom.GetHeroes().size() << '\n';
        }

        return 0;
    }

    int inspect( const std::string & path )
    {
        Maps::Map_Format::MapFormat m;
        if ( !Maps::Map_Format::loadMap( path, m ) ) {
            die( "loadMap failed for " + path );
        }

        std::cout << "name:        " << m.name << '\n';
        std::cout << "description: " << m.description << '\n';
        std::cout << "version:     " << m.version << '\n';
        std::cout << "width:       " << m.width << '\n';
        std::cout << "difficulty:  " << static_cast<int>( m.difficulty ) << '\n';
        std::cout << "available/human/computer colors: " << static_cast<int>( m.availablePlayerColors ) << " / " << static_cast<int>( m.humanPlayerColors )
                  << " / " << static_cast<int>( m.computerPlayerColors ) << '\n';
        std::cout << "victory: type " << static_cast<int>( m.victoryConditionType ) << " AIapplicable " << m.isVictoryConditionApplicableForAI << " allowNormal "
                  << m.allowNormalVictory << " metadata:";
        for ( const uint32_t v : m.victoryConditionMetadata ) {
            std::cout << ' ' << v;
        }
        std::cout << '\n';
        std::cout << "loss: type " << static_cast<int>( m.lossConditionType ) << " metadata:";
        for ( const uint32_t v : m.lossConditionMetadata ) {
            std::cout << ' ' << v;
        }
        std::cout << '\n';

        size_t objectCount = 0;
        for ( const auto & t : m.tiles ) {
            objectCount += t.objects.size();
        }
        std::cout << "tile object records: " << objectCount << '\n';
        std::cout << "castles: " << m.castleMetadata.size() << ", heroes: " << m.heroMetadata.size() << ", monsters: " << m.monsterMetadata.size()
                  << ", resources: " << m.resourceMetadata.size() << ", signs: " << m.signMetadata.size() << ", sphinx: " << m.sphinxMetadata.size()
                  << ", dailyEvents: " << m.dailyEvents.size() << ", rumors: " << m.rumors.size() << '\n';
        return 0;
    }
}

namespace
{
    void printUsage()
    {
        std::cerr << "usage:\n"
                  << "  mapgen generate <mapname> <out.fh2m> [seed]\n"
                  << "  mapgen inspect  <file.fh2m>\n"
                  << "  mapgen gameload <file.fh2m>\n"
                  << "maps:\n";
        for ( const MapDefinition & def : getMapRegistry() ) {
            std::cerr << "  " << def.name << "  (" << def.title << ", " << def.width << 'x' << def.width << ", default seed " << def.defaultSeed << ")\n";
        }
    }

    const MapDefinition * findMap( const std::string & name )
    {
        for ( const MapDefinition & def : getMapRegistry() ) {
            if ( name == def.name ) {
                return &def;
            }
        }
        return nullptr;
    }
}

int main( int argc, char ** argv )
{
    if ( argc < 3 ) {
        printUsage();
        return 1;
    }

    const std::string mode = argv[1];

    if ( mode == "generate" ) {
        // New form: generate <mapname> <out.fh2m> [seed].
        // Back-compat: generate <out.fh2m> [seed] builds kings_ransom.
        std::string mapName = argv[2];
        int pathArg = 3;
        if ( mapName.size() > 5 && mapName.compare( mapName.size() - 5, 5, ".fh2m" ) == 0 ) {
            mapName = "kings_ransom";
            pathArg = 2;
        }
        const MapDefinition * def = findMap( mapName );
        if ( def == nullptr ) {
            std::cerr << "unknown map: " << mapName << '\n';
            printUsage();
            return 1;
        }
        if ( argc <= pathArg ) {
            printUsage();
            return 1;
        }
        const std::string outPath = argv[pathArg];
        const uint32_t seed = ( argc > pathArg + 1 ) ? static_cast<uint32_t>( std::strtoul( argv[pathArg + 1], nullptr, 10 ) ) : def->defaultSeed;
        return generate( *def, outPath, seed );
    }
    if ( mode == "inspect" ) {
        return inspect( argv[2] );
    }
    if ( mode == "gameload" ) {
        return gameLoad( argv[2] );
    }

    std::cerr << "unknown mode: " << mode << std::endl;
    printUsage();
    return 1;
}
