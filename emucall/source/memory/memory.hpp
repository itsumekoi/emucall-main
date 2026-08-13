#pragma once
#include <source/imports.hpp>

class mem_lib
{
public:
	auto read_memory ( std::uintptr_t address, void* buffer, std::size_t size ) -> bool
	{
		if ( !address || !buffer || !size )
		{
			return false;
		}

		return ReadProcessMemory ( this->handle, ( LPVOID )address, buffer, size, nullptr );
	}

	auto get_module_base ( std::uint32_t pid, const wchar_t* name ) -> std::uint64_t
	{
		if ( !pid || !name )
		{
			return 0;
		}

		const auto handle = OpenProcess ( PROCESS_QUERY_INFORMATION, FALSE, pid );
		if ( !handle || handle == INVALID_HANDLE_VALUE )
		{
			return 0;
		}

		const static auto ntdll = GetModuleHandleA ( "ntdll" );
		const static auto nt_query_virtual_memory = reinterpret_cast< NTSTATUS ( __stdcall* ) ( HANDLE, void*, std::int32_t, void*, std::size_t, std::size_t* ) > ( GetProcAddress ( ntdll, "NtQueryVirtualMemory" ) );

		if ( !nt_query_virtual_memory )
		{
			CloseHandle ( handle );
			return 0;
		}

		auto current = 0ull;
		MEMORY_BASIC_INFORMATION mbi{ };

		while ( VirtualQueryEx ( handle, reinterpret_cast< void* > ( current ), &mbi, sizeof ( mbi ) ) )
		{
			current = reinterpret_cast< std::uintptr_t > ( mbi.BaseAddress ) + mbi.RegionSize;

			if ( mbi.Type != MEM_MAPPED && mbi.Type != MEM_IMAGE )
			{
				continue;
			}

			auto buffer = std::vector< std::uint8_t > ( 1024 );
			auto bytes = std::size_t{ };

			if ( nt_query_virtual_memory ( handle, mbi.BaseAddress, 2, buffer.data ( ), buffer.size ( ), &bytes ) != 0 )
			{
				continue;
			}

			const auto unicode_str = reinterpret_cast< UNICODE_STRING* > ( buffer.data ( ) );

			if ( !unicode_str->Buffer || !std::wcsstr ( unicode_str->Buffer, name )
				 || std::wcsstr ( unicode_str->Buffer, L".mui" ) )
			{
				continue;
			}

			const auto result = reinterpret_cast< std::uint64_t > ( mbi.BaseAddress );
			CloseHandle ( handle );
			return result;
		}

		CloseHandle ( handle );
		return 0;
	}

	auto get_process_id ( const wchar_t* name ) -> std::uint32_t
	{
		if ( !name )
		{
			return 0;
		}

		const auto snapshot = CreateToolhelp32Snapshot ( TH32CS_SNAPPROCESS, 0 );
		if ( !snapshot || snapshot == INVALID_HANDLE_VALUE )
		{
			return 0;
		}

		PROCESSENTRY32W entry{ };
		entry.dwSize = sizeof ( PROCESSENTRY32W );

		if ( !Process32FirstW ( snapshot, &entry ) )
		{
			CloseHandle ( snapshot );
			return 0;
		}

		do
		{
			if ( !std::wcsstr ( entry.szExeFile, name ) )
			{
				continue;
			}

			const auto pid = entry.th32ProcessID;
			CloseHandle ( snapshot );

			return pid;
		} while ( Process32NextW ( snapshot, &entry ) );

		CloseHandle ( snapshot );
		return 0;
	}

	auto attach ( std::uint32_t pid ) -> bool
	{
		this->detach ( );

		if ( !pid )
		{
			return false;
		}

		const auto opened = OpenProcess ( PROCESS_ALL_ACCESS, FALSE, pid );

		if ( !opened || opened == INVALID_HANDLE_VALUE )
		{
			return false;
		}

		this->handle = opened;
		this->process_id = pid;

		this->unity_player = this->get_module_base ( pid, L"UnityPlayer.dll" );
		this->game_assembly = this->get_module_base ( pid, L"GameAssembly.dll" );

		return true;
	}

	auto attach ( const wchar_t* name ) -> bool
	{
		const auto pid = this->get_process_id ( name );

		if ( !this->attach ( pid ) )
		{
			return false;
		}

		this->image_base = this->get_module_base ( pid, name );

		return true;
	}

	auto detach ( ) -> void
	{
		if ( this->handle && this->handle != INVALID_HANDLE_VALUE )
		{
			CloseHandle ( this->handle );
		}

		this->handle = nullptr;
		this->process_id = 0;
		this->image_base = 0;
		this->unity_player = 0;
		this->game_assembly = 0;
	}
public:
	HANDLE handle = nullptr;
	std::uint32_t process_id = 0;
	std::uint64_t image_base = 0;
	std::uint64_t unity_player = 0;
	std::uint64_t game_assembly = 0;
};

const auto memory = new mem_lib ( );
