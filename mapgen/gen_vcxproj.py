#!/usr/bin/env python3
"""Generate mapgen.vcxproj: a console tool linking the full fheroes2 engine.

Reads the engine source list from fheroes2's own VisualStudio/fheroes2/sources.props
(so it always matches the checked-out revision), excludes the game's main()
translation unit and the .rc resource, and adds the generator sources.

Deterministic: output depends only on sources.props content and this script.
"""

import glob
import os
import re
import sys

FHEROES2_ROOT = os.environ.get("FHEROES2_ROOT", r"C:\Users\gjoer\source\repos\fheroes2")
HERE = os.path.dirname(os.path.abspath(__file__))

SOURCES_PROPS = os.path.join(FHEROES2_ROOT, "VisualStudio", "fheroes2", "sources.props")
EXCLUDE = {r"src\fheroes2\game\fheroes2.cpp"}

INCLUDE_DIRS = [
    r"src\engine", r"src\fheroes2\agg", r"src\fheroes2\ai", r"src\fheroes2\army",
    r"src\fheroes2\audio", r"src\fheroes2\battle", r"src\fheroes2\campaign",
    r"src\fheroes2\castle", r"src\fheroes2\dialog", r"src\fheroes2\editor",
    r"src\fheroes2\game", r"src\fheroes2\gui", r"src\fheroes2\h2d",
    r"src\fheroes2\heroes", r"src\fheroes2\image", r"src\fheroes2\kingdom",
    r"src\fheroes2\maps", r"src\fheroes2\monster", r"src\fheroes2\resource",
    r"src\fheroes2\spell", r"src\fheroes2\system", r"src\fheroes2\world",
    r"src\thirdparty\libsmacker",
]


def main() -> None:
    with open(SOURCES_PROPS, encoding="utf-8") as f:
        props = f.read()

    sources = re.findall(r'<ClCompile Include="([^"]+)"', props)
    kept = [s for s in sources if s not in EXCLUDE]
    if len(kept) != len(sources) - len(EXCLUDE):
        sys.exit(f"expected to exclude {len(EXCLUDE)} sources, kept {len(kept)} of {len(sources)}")

    inc = ";".join(rf"$(Fheroes2Root)\{d}" for d in INCLUDE_DIRS)
    sdl_inc = r"$(Fheroes2Root)\VisualStudio\packages\sdl2\include;$(Fheroes2Root)\VisualStudio\packages\sdl2\include\SDL2"
    libdir = r"$(Fheroes2Root)\VisualStudio\packages\sdl2\lib\$(PlatformTarget)"

    engine_sources = "\n".join(
        rf'    <ClCompile Include="$(Fheroes2Root)\{s}" />' for s in kept
    )

    mapgen_sources = "\n".join(
        rf'    <ClCompile Include="$(MSBuildThisFileDirectory)src\{os.path.basename(p)}" />'
        for p in sorted(glob.glob(os.path.join(HERE, "src", "*.cpp")))
    )

    vcxproj = rf"""<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="14.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup Label="ProjectConfigurations">
    <ProjectConfiguration Include="Release|x64">
      <Configuration>Release</Configuration>
      <Platform>x64</Platform>
    </ProjectConfiguration>
  </ItemGroup>
  <PropertyGroup Label="Globals">
    <ProjectGuid>{{B7A9BF37-6019-4A4A-8110-2B9B7E62E2A2}}</ProjectGuid>
    <Keyword>Win32Proj</Keyword>
    <RootNamespace>mapgen</RootNamespace>
    <TargetName>mapgen</TargetName>
    <Fheroes2Root>{FHEROES2_ROOT}</Fheroes2Root>
  </PropertyGroup>
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.Default.props" />
  <PropertyGroup Label="Configuration">
    <ConfigurationType>Application</ConfigurationType>
    <PlatformToolset>v143</PlatformToolset>
    <CharacterSet>MultiByte</CharacterSet>
    <UseDebugLibraries>false</UseDebugLibraries>
  </PropertyGroup>
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.props" />
  <PropertyGroup>
    <OutDir>$(MSBuildThisFileDirectory)build\$(Platform)\$(Configuration)\</OutDir>
    <IntDir>$(MSBuildThisFileDirectory)build\$(Platform)\$(Configuration)\obj\</IntDir>
  </PropertyGroup>
  <ItemDefinitionGroup>
    <ClCompile>
      <ConformanceMode>true</ConformanceMode>
      <LanguageStandard>stdcpp17</LanguageStandard>
      <WarningLevel>Level3</WarningLevel>
      <TreatWarningAsError>false</TreatWarningAsError>
      <SDLCheck>true</SDLCheck>
      <Optimization>MaxSpeed</Optimization>
      <PreprocessorDefinitions>_CRT_SECURE_NO_WARNINGS;WITH_IMAGE;%(PreprocessorDefinitions)</PreprocessorDefinitions>
      <MultiProcessorCompilation>true</MultiProcessorCompilation>
      <AdditionalIncludeDirectories>{inc};{sdl_inc};%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
      <DebugInformationFormat>None</DebugInformationFormat>
    </ClCompile>
    <Link>
      <SubSystem>Console</SubSystem>
      <AdditionalLibraryDirectories>{libdir};%(AdditionalLibraryDirectories)</AdditionalLibraryDirectories>
      <AdditionalDependencies>SDL2.lib;SDL2_mixer.lib;SDL2_image.lib;zlib.lib;%(AdditionalDependencies)</AdditionalDependencies>
      <GenerateDebugInformation>false</GenerateDebugInformation>
    </Link>
    <PostBuildEvent>
      <Command>xcopy /D /Y /Q "$(Fheroes2Root)\VisualStudio\packages\sdl2\lib\$(PlatformTarget)\*.dll" "$(OutDir)"</Command>
    </PostBuildEvent>
  </ItemDefinitionGroup>
  <ItemGroup>
{engine_sources}
{mapgen_sources}
  </ItemGroup>
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets" />
</Project>
"""

    out = os.path.join(HERE, "mapgen.vcxproj")
    with open(out, "w", encoding="utf-8") as f:
        f.write(vcxproj)
    mapgen_count = len(glob.glob(os.path.join(HERE, "src", "*.cpp")))
    print(f"wrote {out} with {len(kept)} engine sources + {mapgen_count} mapgen sources")


if __name__ == "__main__":
    main()
