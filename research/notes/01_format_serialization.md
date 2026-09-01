# FH2M file format — byte-level serialization (fheroes2 @ b086d1aa, 2026-09-01)

Source of truth:
- `C:/Users/gjoer/source/repos/fheroes2/src/fheroes2/maps/map_format_info.h` / `.cpp`
- `C:/Users/gjoer/source/repos/fheroes2/src/engine/serialize.h` / `.cpp`
- `C:/Users/gjoer/source/repos/fheroes2/src/engine/zzlib.h` / `.cpp`

---

## 1. File-level layout

```
offset 0:  magic word, 6 bytes: 'h' '2' 'm' 'a' 'p' 0x00
offset 6:  BaseMapFormat, plain (uncompressed), all multi-byte ints BIG-ENDIAN
after:     one single zlib (RFC 1950) stream, to EOF, no framing/size prefix
```

- Magic: `const std::array<uint8_t, 6> magicWord{ 'h', '2', 'm', 'a', 'p', '\0' };` — map_format_info.cpp:101. Written byte-by-byte via `fileStream.put(value)` (saveMap, cpp:837-839); read/compared byte-by-byte (cpp:788-792, 815-819).
- Min file size on load: `const size_t minFileSize{ 512 };` (cpp:105). Both `loadBaseMap` (cpp:783-785) and `loadMap` (cpp:810-812) reject files `< 512` bytes total. **saveMap never pads** — a generator must ensure the final file is >= 512 bytes.
- Extension `.fh2m`, discovered in the `maps` dir (maps_fileinfo.cpp:743,765; editor_save_map_window.cpp:67).

## 2. Version constants (map_format_info.cpp:107-110)

```cpp
constexpr uint16_t minimumSupportedVersion{ 2 };
// Change the version when there is a need to expand map format functionality.
constexpr uint16_t currentSupportedVersion{ 13 };
```

- On **save**, the version written is ALWAYS `currentSupportedVersion` (13) — `stream << currentSupportedVersion << ...` (cpp:440). `map.version` is ignored on write.
- On **load**, reject if `map.version < 2 || map.version > 13` (cpp:450-453).

### Version history (git -S on currentSupportedVersion; upgrade shims in map_format_info.cpp)

| ver | commit | date | change |
|-----|--------|------|--------|
| 3 | 5f2bf16d0 | 2024-06-02 | Graveyards moved ADVENTURE_DWELLINGS → ADVENTURE_MISCELLANEOUS (convertFromV2ToV3, cpp:112-160) |
| 4 | 880c2c79e | 2024-06-21 | New Cave object; dwelling indices >=18 shifted +1 (cpp:162-178) |
| 5 | 98c9e9e30 | 2024-07-26 | Small Cliff dirt; LANDSCAPE_MISCELLANEOUS >=128 +1 (cpp:180-196) |
| 6 | a6ee4c1c7 | 2024-11-22 | Lean-To; ADVENTURE_MISCELLANEOUS >=17 +1 (cpp:198-214) |
| 7 | 2817b51c5 | 2024-12-18 | Stone Liths; ADVENTURE_MISCELLANEOUS >=38 +1 (cpp:216-232) |
| 8 | 617f657d1 | 2024-12-28 | 3 Observation Tower variants; ADVENTURE_MISCELLANEOUS >=43 +3 (cpp:234-250) |
| 9 | 1048750bc | 2025-05-18 | + capturableObjectsMetadata (load gate `version > 8`, cpp:554-557); creatorNotes also v9-gated (f66ce2cd9, load gate cpp:466-471) |
| 10 | 8c7110a11 | 2025-09-05 | StandardObjectMetadata split into monster/artifact/resource metadata (gates cpp:547-551, 566-571; convertFromV9ToV10 cpp:252-302) |
| 11 | cf3fcd987 | 2026-01-31 | Multi-language: BaseMapFormat.translations (cpp:473-479) + MapFormat.translationInfo (cpp:573-578) |
| 12 | 4d4a42ed8 | 2026-02-01 | Roads rework, index recomputation (convertFromV11ToV12, cpp:304-408) |
| 13 | b5f7c0978 | 2026-02-22 | New Abandoned Mine variants; ADVENTURE_MINES index remap (convertFromV12ToV13, cpp:410-436). First shipped in tag **1.1.14**. |

## 3. StreamBase primitive encodings (serialize.h / serialize.cpp)

Endianness is a per-stream flag. For FH2M, **big-endian is explicitly enabled everywhere**:
- File stream: `fileStream.setBigendian( true )` in loadBaseMap/loadMap/saveMap (cpp:777, 804, 831).
- Compressed buffer: `compressed.setBigendian( true )` / `decompressed.setBigendian( true )` (cpp:493, 515).
`put16/put32` dispatch on the flag (serialize.cpp:149-157); `get16/get32` likewise (serialize.cpp:69-77). BE byte order = most significant byte first (RWStreamBuf::putBE32, serialize.cpp:251-257; StreamFile uses htobe16/32, serialize.cpp:571-589).

| C++ type | wire encoding | where |
|----------|---------------|-------|
| `bool` | 1 byte via put8; read as `get8() != 0` | serialize.cpp:79-84, 159-164 |
| `char`, `int8_t`, `uint8_t` | 1 byte | serialize.cpp:86-105, 166-185 |
| `int16_t`, `uint16_t` | 2 bytes BE (in FH2M) | serialize.cpp:107-119, 187-199 |
| `int32_t`, `uint32_t` | 4 bytes BE | serialize.cpp:121-133, 201-213 |
| enum (any) | its `std::underlying_type_t` (so 1 byte for all enums used here) | serialize.h:194-203, 344-348 |
| `std::string` / `string_view` | u32 length + raw bytes, **no** NUL terminator | serialize.cpp:135-142, 215-223 |
| `std::pair<A,B>` | A then B | serialize.h:205-209, 350-354 |
| `std::optional<T>` | bool hasValue + (T if true) — not used in FH2M | serialize.h:211-228, 356-367 |
| `std::vector<T>` / `std::list<T>` | u32 count + elements in order | serialize.h:230-248, 369-387 |
| `std::set<T>` | u32 count + elements | serialize.h:250-264, 389-397 |
| `std::map<K,V>` | u32 count + (K,V) pairs in ascending key order | serialize.h:266-282, 399-407 |
| `std::array<T,N>` | **u32 count (=N) is written too**; reader fails if count != N | serialize.h:284-299, 409-417 |
| `fheroes2::Point` | i32 x, i32 y — not used in FH2M | serialize.cpp:144-147, 225-228 |

Counts/lengths are `uint32_t` via put32/get32, so they are big-endian in this file.

Enum underlying types used by the format:
- `fheroes2::SupportedLanguage : uint8_t`, `English = 0` (game_language.h:27-54)
- `PlayerColor : uint8_t` (NONE 0x00, BLUE 0x01, GREEN 0x02, RED 0x04, YELLOW 0x08, ORANGE 0x10, PURPLE 0x20, UNUSED 0x80) and `PlayerColorsSet = std::underlying_type_t<PlayerColor>` = uint8_t (kingdom/color.h:46-58)
- `Maps::ObjectGroup : uint8_t`: NONE=0, ROADS=1, STREAMS=2, LANDSCAPE_MOUNTAINS=3, LANDSCAPE_ROCKS=4, LANDSCAPE_TREES=5, LANDSCAPE_WATER=6, LANDSCAPE_MISCELLANEOUS=7, LANDSCAPE_TOWN_BASEMENTS=8, LANDSCAPE_FLAGS=9, ADVENTURE_ARTIFACTS=10, ADVENTURE_DWELLINGS=11, ADVENTURE_MINES=12, ADVENTURE_POWER_UPS=13, ADVENTURE_TREASURES=14, ADVENTURE_WATER=15, ADVENTURE_MISCELLANEOUS=16, KINGDOM_HEROES=17, KINGDOM_TOWNS=18, MONSTERS=19, MAP_EXTRAS=20, GROUP_COUNT=21 (map_object_info.h:127-168)

`Funds` (7 x i32, resource.cpp:649-657), order: **wood, mercury, ore, sulfur, crystal, gems, gold** = 28 bytes.

## 4. Uncompressed part — BaseMapFormat (write: saveToStream, cpp:438-446)

```cpp
stream << currentSupportedVersion << map.isCampaign << map.difficulty << map.availablePlayerColors << map.humanPlayerColors << map.computerPlayerColors
       << map.alliances << map.playerRace << map.victoryConditionType << map.isVictoryConditionApplicableForAI << map.allowNormalVictory
       << map.victoryConditionMetadata << map.lossConditionType << map.lossConditionMetadata << map.width << map.mainLanguage << map.name << map.description
       << map.creatorNotes << map.translations;
```

Field-by-field (starting immediately after the 6-byte magic; all multi-byte BE):

| # | field | wire type | bytes |
|---|-------|-----------|-------|
| 1 | version (always 13) | u16 | 2 |
| 2 | isCampaign | u8 (bool) | 1 |
| 3 | difficulty | u8 | 1 |
| 4 | availablePlayerColors | u8 (PlayerColorsSet) | 1 |
| 5 | humanPlayerColors | u8 | 1 |
| 6 | computerPlayerColors | u8 | 1 |
| 7 | alliances | u32 count + count x u8 | 4+n |
| 8 | playerRace (array<u8,6>) | u32 count(=6) + 6 x u8 | 10 |
| 9 | victoryConditionType | u8 | 1 |
| 10 | isVictoryConditionApplicableForAI | u8 (bool) | 1 |
| 11 | allowNormalVictory | u8 (bool) | 1 |
| 12 | victoryConditionMetadata | u32 count + count x u32 | 4+4n |
| 13 | lossConditionType | u8 | 1 |
| 14 | lossConditionMetadata | u32 count + count x u32 | 4+4n |
| 15 | width | i32 | 4 |
| 16 | mainLanguage | u8 (SupportedLanguage) | 1 |
| 17 | name | u32 len + bytes | 4+n |
| 18 | description | u32 len + bytes | 4+n |
| 19 | creatorNotes | u32 len + bytes | 4+n |
| 20 | translations (map<SupportedLanguage, TranslationBaseMapMetadata>) | u32 count + per entry: u8 key + 3 strings (name, description, creatorNotes; TranslationBaseMapMetadata op<< cpp:748-756) | 4+... |

Minimum size of this section with everything empty: 2+1+1+1+1+1+4+10+1+1+1+4+1+4+4+1+4+4+4+4 = **54 bytes**.

Load side (loadFromStream Base, cpp:448-482): reads version, validates [2,13]; reads fields 2-15; rejects `width <= 0` (cpp:459-462); reads 16-18; creatorNotes only if version >= 9; translations only if version >= 11; returns `!stream.fail()`.

## 5. Compressed remainder — MapFormat (write: saveToStream, cpp:484-504)

```cpp
RWStreamBuf compressed;
compressed.setBigendian( true );
compressed << map.additionalInfo << map.tiles << map.dailyEvents << map.rumors << map.castleMetadata << map.heroMetadata << map.sphinxMetadata << map.signMetadata
           << map.adventureMapEventMetadata << map.selectionObjectMetadata << map.capturableObjectsMetadata << map.monsterMetadata << map.artifactMetadata
           << map.resourceMetadata << map.translationInfo;
const std::vector<uint8_t> temp = Compression::zipData( compressed.data(), compressed.size(), false );
stream.putRaw( temp.data(), temp.size() );
```

Plaintext (pre-compression) field order for v13, all BE:

1. **additionalInfo** — `vector<u32>`: u32 count + u32s (campaign only, empty otherwise).
2. **tiles** — `vector<TileInfo>`; must contain exactly `width*width` entries (load check cpp:540-543).
   - `TileInfo` (op<< cpp:599-602): u16 `terrainIndex`, u8 `terrainFlags`, `vector<TileObjectInfo> objects` (u32 count + entries). Empty tile = 7 bytes.
   - `TileObjectInfo` (op<< cpp:589-592): u32 `id`, u8 `group` (ObjectGroup), u32 `index` = 9 bytes.
3. **dailyEvents** — `vector<DailyEvent>` (op<< cpp:609-613): string `message`, u8 `humanPlayerColors`, u8 `computerPlayerColors`, u32 `firstOccurrenceDay`, u32 `repeatPeriodInDays`, `Funds resources` (7 x i32: wood, mercury, ore, sulfur, crystal, gems, gold).
4. **rumors** — `vector<string>`.
5. **castleMetadata** — `map<u32, CastleMetadata>` (op<< cpp:626-630); per value: string `customName`, array<i32,5> `defenderMonsterType` (u32 5 + 20B), array<i32,5> `defenderMonsterCount`, bool `customBuildings`, vector<u32> `builtBuildings`, vector<u32> `bannedBuildings`, map<u8,i32> `mustHaveSpells`, vector<i32> `bannedSpells`, array<i32,6> `availableToHireMonsterCount`.
6. **heroMetadata** — `map<u32, HeroMetadata>` (op<< cpp:638-644); per value: string `customName`, i32 `customPortrait`, array<i32,5> `armyMonsterType`, array<i32,5> `armyMonsterCount`, array<i32,14> `artifact`, array<i32,14> `artifactMetadata`, vector<i32> `availableSpells`, bool `isOnPatrol`, u8 `patrolRadius`, array<i8,8> `secondarySkill`, array<u8,8> `secondarySkillLevel`, i16 `customLevel`, i32 `customExperience`, i16 `customAttack`, i16 `customDefense`, i16 `customKnowledge`, i16 `customSpellPower`, i16 `magicPoints`, u8 `race`.
7. **sphinxMetadata** — `map<u32, SphinxMetadata>` (op<< cpp:654-657): string `riddle`, vector<string> `answers`, i32 `artifact`, i32 `artifactMetadata`, Funds `resources`.
8. **signMetadata** — `map<u32, SignMetadata>` (op<< cpp:664-667): string `message`.
9. **adventureMapEventMetadata** — `map<u32, AdventureMapEventMetadata>` (op<< cpp:674-679): string `message`, u8 `humanPlayerColors`, u8 `computerPlayerColors`, bool `isRecurringEvent`, i32 `artifact`, i32 `artifactMetadata`, Funds `resources`, i16 `attack`, i16 `defense`, i16 `knowledge`, i16 `spellPower`, i32 `experience`, u8 `secondarySkill`, u8 `secondarySkillLevel`, i32 `monsterType`, i32 `monsterCount`.
10. **selectionObjectMetadata** — `map<u32, SelectionObjectMetadata>` (op<< cpp:688-691): vector<i32> `selectedItems`.
11. **capturableObjectsMetadata** — `map<u32, CapturableObjectMetadata>` (op<< cpp:698-701): u8 `ownerColor` (PlayerColor enum).
12. **monsterMetadata** — `map<u32, MonsterMetadata>` (op<< cpp:708-711): i32 `count`, i32 `joinCondition`, bool `isWeeklyGrowthDisabled`, vector<i32> `selected`.
13. **artifactMetadata** — `map<u32, ArtifactMetadata>` (op<< cpp:718-721): i32 `radius`, i32 `captureCondition`, vector<i32> `selected`.
14. **resourceMetadata** — `map<u32, ResourceMetadata>` (op<< cpp:728-731): i32 `count`.
15. **translationInfo** — `map<SupportedLanguage(u8), TranslationFormat>`; `TranslationFormat` (op<< cpp:758-762): vector<string> `dailyEvents`, vector<string> `rumors`, map<u32,string> `castleMetadata`, map<u32,string> `heroMetadata`, map<u32,TranslationSphinxMetadata> `sphinxMetadata` (string `riddle` + vector<string> `answers`, op<< cpp:738-741), map<u32,string> `signMetadata`, map<u32,string> `adventureMapEventMetadata`.

All metadata maps are keyed by **object UID** (`TileObjectInfo.id`), except translationInfo keyed by language.

Minimum plaintext with all 15 top-level containers empty except tiles: 14 x 4 (empty container counts) + 4 (tiles count) + 7 x width^2.

Load side (loadFromStream Map, cpp:506-584): base part, then `getRaw(0)` (all remaining bytes; empty → fail), `unzipData` (empty result → fail), then reads in the same order with version gates: `standardMetadata` (deprecated map<u32, array<i32,3>>, cpp:41-44, 547-551) is read **between rumors and castleMetadata** only when version < 10; `capturableObjectsMetadata` only when version > 8; `monsterMetadata/artifactMetadata/resourceMetadata` only when version > 9; `translationInfo` only when version >= 11. Then upgrade shims run.

## 6. Compression details (zzlib.cpp)

- `Compression::zipData(src, size, isMaximumCompression)` (zzlib.cpp:130-160) = zlib `compress2(...)`, i.e. **standard zlib-wrapped deflate (RFC 1950)**: 2-byte zlib header + deflate stream + 4-byte Adler-32 (BE). windowBits = default 15. Map saving passes `isMaximumCompression = false` → `Z_DEFAULT_COMPRESSION` (level 6); header bytes then 0x78 0x9C. Any valid zlib stream (any level/strategy, windowBits <= 15) is acceptable to the loader.
- **One single zlib block**, written with `stream.putRaw` with **no size prefix, no framing** (cpp:499-501); it simply runs to EOF. The loader reads all remaining bytes via `stream.getRaw(0)` (cpp:518) and calls `Compression::unzipData(data, size)` (zzlib.cpp:40-101) which uses `uncompress()` with a guessed output size (srcSize x 7, doubling on Z_BUF_ERROR).
- NOT the framed `zipStreamBuf`/`unzipStream` format (that one — u32 rawSize + u32 zipSize + u16 version + u16 unused + blob, zzlib.cpp:162-202) — that framing is used by save-game files, **not** by .fh2m.
- Note: zlib's `uncompress()` stops at Z_STREAM_END, so trailing bytes after the zlib stream are ignored by zlib itself (useful if padding to reach 512 bytes; behavior is zlib's, not fheroes2's — verify empirically before relying on it).

## 7. Sanity checks and public API

Load-time checks, in order:
1. non-empty path (cpp:799-801)
2. fopen "rb" success (cpp:806-808)
3. `fileSize >= 512` (cpp:810-812; minFileSize cpp:105)
4. 6 magic bytes match exactly (cpp:815-819)
5. `2 <= version <= 13` (cpp:451)
6. `width > 0` (cpp:459)
7. compressed blob non-empty; decompression non-empty (cpp:518-530)
8. `tiles.size() == width*width` (cpp:540-543)
9. `!stream.fail()` (stream sets FAILURE on short reads; array reads fail on count mismatch, serialize.h:287-294)

Public functions (map_format_info.h:424-430):
```cpp
bool loadBaseMap( const std::string & path, BaseMapFormat & map );
bool loadMap( const std::string & path, MapFormat & map );
bool saveMap( const std::string & path, const MapFormat & map );
bool saveMap( OStreamBase & stream, const MapFormat & map );   // no magic word written!
bool loadMap( IStreamBase & stream, MapFormat & map );          // no magic word read!
```
The stream-based overloads (cpp:844-852) do NOT handle the magic word or the min-size check; only the path-based ones do.

## 8. Compatibility: HEAD vs release 1.1.17

- `git diff 1.1.17 HEAD -- src/fheroes2/maps/map_format_info.h src/fheroes2/maps/map_format_info.cpp` → **0 lines**. The files are byte-identical.
- Tag 1.1.17 = 2685c2188b541660f1ce261b554c3e92f79b1775; HEAD = b086d1aa8b921163712aec2fb8188f4d0d375b09.
- Both have `minimumSupportedVersion = 2`, `currentSupportedVersion = 13` (1.1.17 cpp:107,110 — verified via `git show 1.1.17:...`).
- Format v13 first shipped in release **1.1.14**.
- **A map written by HEAD code (format v13) loads in release 1.1.17. Serializers are identical.**
- Only post-1.1.17 serialization-adjacent change in log: 6ebf0204d added the `isMaximumCompression` argument to `zipData` (map saving uses `false` = same Z_DEFAULT_COMPRESSION as before) — no on-disk format impact; and zlib level does not affect readability anyway.

## 9. operator<< / operator>> locations (all in map_format_info.cpp unless noted)

| struct | op<< | op>> |
|--------|------|------|
| TileObjectInfo | 589-592 | 594-597 |
| TileInfo | 599-602 | 604-607 |
| DailyEvent | 609-613 | 615-619 |
| StandardObjectMetadata (deprecated, read-only) | — | 621-624 |
| CastleMetadata | 626-630 | 632-636 |
| HeroMetadata | 638-644 | 646-652 |
| SphinxMetadata | 654-657 | 659-662 |
| SignMetadata | 664-667 | 669-672 |
| AdventureMapEventMetadata | 674-679 | 681-686 |
| SelectionObjectMetadata | 688-691 | 693-696 |
| CapturableObjectMetadata | 698-701 | 703-706 |
| MonsterMetadata | 708-711 | 713-716 |
| ArtifactMetadata | 718-721 | 723-726 |
| ResourceMetadata | 728-731 | 733-736 |
| TranslationSphinxMetadata | 738-741 | 743-746 |
| TranslationBaseMapMetadata | 748-751 | 753-756 |
| TranslationFormat | 758-762 | 764-768 |
| Funds | resource.cpp:649-652 | resource.cpp:654-657 |

These operators are declared inside `namespace Maps::Map_Format` (cpp:48-96) for ADL; not in the header.

## 10. Generator checklist / caveats

1. Write `h2map\0` (6 bytes), then BaseMapFormat fields (section 4), then one zlib-compressed blob of section 5. All ints big-endian, including container counts.
2. Always write version = 13 (u16).
3. `std::array` fields DO carry a redundant u32 element-count prefix; readers hard-fail on mismatch.
4. `tiles` must have exactly `width*width` elements; `width > 0` (square maps only). Standard sizes: 36/72/108/144.
5. Total file must be >= 512 bytes or the loader rejects it — small uniform maps can compress below this; add real content (e.g. description/creatorNotes are in the uncompressed part, so they count and are easy to size).
6. Maps are serialized in ascending key order (std::map) — emit sorted keys for byte-identical round-trips (loader itself doesn't verify order).
7. Strings: u32 length + raw bytes, no terminator. Length caps used by editor UI (not enforced by loader): messageCharLimit 999, nameCharLimit 30 (map_format_info.h:59-60).
8. The compressed blob must decompress successfully and non-empty even if all containers are empty (counts of zero still produce plaintext, so this is automatic).
9. `PlayerColorsSet` is a bitmask of PlayerColor bits (BLUE 0x01 ... PURPLE 0x20), one byte.
