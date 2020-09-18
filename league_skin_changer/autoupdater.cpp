/* This file is part of LeagueSkinChanger by b3akers, licensed under the MIT license:
*
* MIT License
*
* Copyright (c) b3akers 2020
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/
#include "autoupdater.hpp"
#include "offsets.hpp"

#include <Windows.h>
#include <vector>
#include <cinttypes>
#include <string>

uint8_t* find_signature( const wchar_t* szModule, const char* szSignature ) {
	auto module = GetModuleHandle( szModule );
	static auto pattern_to_byte = [ ] ( const char* pattern ) {
		auto bytes = std::vector<int> {};
		auto start = const_cast<char*>( pattern );
		auto end = const_cast<char*>( pattern ) + strlen( pattern );

		for ( auto current = start; current < end; ++current ) {
			if ( *current == '?' ) {
				++current;
				if ( *current == '?' )
					++current;
				bytes.push_back( -1 );
			} else {
				bytes.push_back( strtoul( current, &current, 16 ) );
			}
		}
		return bytes;
	};

	auto dosHeader = (PIMAGE_DOS_HEADER)module;
	auto ntHeaders = (PIMAGE_NT_HEADERS)( (uint8_t*)module + dosHeader->e_lfanew );

	auto sizeOfImage = ntHeaders->OptionalHeader.SizeOfImage;
	auto patternBytes = pattern_to_byte( szSignature );
	auto scanBytes = reinterpret_cast<uint8_t*>( module );

	if ( !szModule && GetModuleHandle( L"stub.dll" ) ) {
		static const auto get_shadow_page = [ ] ( ) {
			const auto img_base = (DWORD)GetModuleHandle( nullptr );
			const auto img_dos_header = reinterpret_cast<IMAGE_DOS_HEADER*>( img_base );
			const auto img_nt_headers = reinterpret_cast<IMAGE_NT_HEADERS32*>( img_base + img_dos_header->e_lfanew );
			const auto lol_img_size = img_nt_headers->OptionalHeader.SizeOfImage;

			auto cur = std::uintptr_t( 0 );
			while ( true ) {
				MEMORY_BASIC_INFORMATION mbi;
				if ( !VirtualQuery( reinterpret_cast<void*>( cur ), &mbi, sizeof( mbi ) ) )
					break;

				const auto base_address = reinterpret_cast<std::uintptr_t>( mbi.BaseAddress );
				if ( mbi.State == MEM_COMMIT && mbi.Type == MEM_MAPPED && mbi.RegionSize == lol_img_size && mbi.Protect == PAGE_READWRITE )
					return base_address;
				else
					cur = base_address + mbi.RegionSize;
			}
			return std::uintptr_t( 0 );
		};

		scanBytes = reinterpret_cast<uint8_t*>( get_shadow_page() );
	}

	auto s = patternBytes.size( );
	auto d = patternBytes.data( );

	auto mem_info = MEMORY_BASIC_INFORMATION { 0 };

	for ( auto i = 0ul; i < sizeOfImage - s; ++i ) {
		bool found = true;
		for ( auto j = 0ul; j < s; ++j ) {
			if ( scanBytes[ i + j ] != d[ j ] && d[ j ] != -1 ) {
				found = false;
				break;
			}
		}
		if ( found ) {
			return &scanBytes[ i ];
		}
	}
	return nullptr;
}

class offset_signature {
public:
	std::vector<std::string> sigs;
	bool sub_base;
	bool read;
	int32_t additional;
	uint32_t* offset;
};

std::vector<offset_signature> sigs = {
	{
		{
			"A1 ? ? ? ? 83 C4 04 C6 40 36 15",
			"8B 35 ? ? ? ? 68 ? ? ? ? E8 ? ? ? ? 83 C4 04 C7 44 24 ? ? ? ? ? 89 44 24 18"
		},
		true,
		true,
		0,
		&offsets::global::GameClient
	},
	{
		{
			"8B 0D ? ? ? ? 8B F8 81 C1 ? ? ? ? 57",
			"A1 ? ? ? ? 85 C0 74 18 84 C9",
			"8B 0D ? ? ? ? 8D 54 24 14 8B 3D"
		},
		true,
		true,
		0,
		&offsets::global::Player
	},
	{
		{
			"8B 0D ? ? ? ? 50 8D 44 24 18",
			"8B 35 ? ? ? ? C7 44 24 ? ? ? ? ? C7 44 24 ? ? ? ? ? E8",
			"8B 0D ? ? ? ? 53 FF 37"
		},
		true,
		true,
		0,
		&offsets::global::ManagerTemplate_AIHero_
	},
	{
		{
			"89 1D ? ? ? ? 56 8D 4B 04",
			"8B 0D ? ? ? ? 83 C1 0C 89 14 24",
			"8B 0D ? ? ? ? 57 FF 74 24 08 E8 ? ? ? ? 8B F8"
		},
		true,
		true,
		0,
		&offsets::global::ChampionManager
	},
	{
		{
			"8B 1D ? ? ? ? 8B 49 08",
			"A1 ? ? ? ? F3 0F 10 40 ? F3 0F 11 44 24"
		},
		true,
		true,
		0,
		&offsets::global::ManagerTemplate_AIMinionClient_
	},
	{
		{
			"3B 05 ? ? ? ? 75 72"
		},
		true,
		true,
		0,
		&offsets::global::Riot__g_window
	},
	{
		{
			"A1 ? ? ? ? 55 57 53"
		},
		true,
		true,
		0,
		&offsets::global::GfxWinMsgProc
	},
	{
		{
			"8D 8E ? ? ? ? FF 74 24 58"
		},
		false,
		true,
		0,
		&offsets::ai_base::CharacterDataStack
	},
	{
		{
			"80 BE ? ? ? ? ? 75 50 0F 31 33 C9 66 C7 86 ? ? ? ? ? ? 89 44 24 18"
		},
		false,
		true,
		0,
		&offsets::ai_base::SkinId
	},
	{
		{
			"8B 86 ? ? ? ? 89 4C 24 08",
			"8B B7 ? ? ? ? FF 70 14",
			"8B AE ? ? ? ? 85 FF 74 46"
		},
		false,
		true,
		0,
		&offsets::material_registry::D3DDevice
	},
	{
		{
			"8A 86 ? ? ? ? 88 4C 24 17",
			"8A 86 ? ? ? ? 88 4C 24 4C 33 C9 0F B6 D0 84 C0 74 1E",
			"8A 86 ? ? ? ? 88 4C 24 1B"
		},
		false,
		true,
		-1,
		&offsets::ai_minion::IsLaneMinion
	},
	{
		{
			"83 EC 0C 53 55 56 57 8B F9 8B 47 04",
			"E8 ? ? ? ? 8B 74 24 14 85 F6 74 1E 8B C7 F0 0F C1 46 ? 75 15 8B 06 8B CE FF 10 F0 0F C1 7E ? 4F 75 07 8B 06 8B CE FF 50 04 5F 5E 5B"
		},
		true,
		false,
		0,
		&offsets::functions::CharacterDataStack__Push
	},
	{
		{
			"83 EC 18 53 56 57 8D 44 24 20",
			"E8 ? ? ? ? 8D 4C 24 14 E8 ? ? ? ? 8B 07"
		},
		true,
		false,
		0,
		&offsets::functions::CharacterDataStack__Update
	},
	{
		{
			"A1 ? ? ? ? 85 C0 75 0B 8B 0D ? ? ? ? 8B 01 FF 60 14",
			"E8 ? ? ? ? 8B 80 ? ? ? ? 89 46 68"
		},
		true,
		false,
		0,
		&offsets::functions::Riot__Renderer__MaterialRegistry__GetSingletonPtr
	},
	{
		{
			"83 EC 0C 56 8B 74 24 14 56 E8 ? ? ? ? 83 C4 04 89 74 24 04 89 44 24 08 A8 01",
			"E8 ? ? ? ? 8B 0D ? ? ? ? 83 C4 04 8B F0 6A 0B"
		},
		true,
		false,
		0,
		&offsets::functions::translateString_UNSAFE_DONOTUSE
	},
	{
		{
			"E8 ? ? ? ? 85 C0 74 3A 8B CE",
			"E8 ? ? ? ? 8B F8 3B F7 0F 84"
		},
		true,
		false,
		0,
		&offsets::functions::GetOwnerObject
	},
	{
		{
			"81 EC ? ? ? ? A1 ? ? ? ? 33 C4 89 84 24 ? ? ? ? 56 8B B4 24 ? ? ? ? 8B C6",
			"E8 ? ? ? ? 8B 54 24 30 83 C4 08 89 44 24 10"
		},
		true,
		false,
		0,
		&offsets::functions::CharacterData__GetCharacterPackage
	}
};

void autoupdater::start( ) {
	auto base = std::uintptr_t( GetModuleHandle( nullptr ) );
	for ( auto& sig : sigs ) {
		
		*sig.offset = 0;
		for ( auto& pattern : sig.sigs ) {
			auto address = find_signature( nullptr, pattern.c_str( ) );

			if ( !address )
				continue;

			if ( sig.read )
				address = *reinterpret_cast<uint8_t**>( address + ( pattern.find_first_of( "?" ) / 3 ) );
			else if ( address[ 0 ] == 0xE8 )
				address = address + *reinterpret_cast<uint32_t*>( address + 1 ) + 5;

			if ( sig.sub_base )
				address -= base;

			address += sig.additional;

			*sig.offset = reinterpret_cast<uint32_t>( address );
			break;
		}

		if ( !*sig.offset ) {
			MessageBoxA( nullptr, "Signature failed, please wait for update!", "AutoUpdater: Error", MB_OK | MB_ICONERROR );
			break;
		}
	}
}