#include <source/imports.hpp>

auto main ( ) -> std::int32_t
{
	if ( !memory->attach ( L"demodx11.exe" ) )
	{
		return 1;
	}

	std::vector< std::uint8_t > fake_stack ( 0x400000 );

	auto cpu = emulator
	(
		[ &fake_stack ]( std::uint64_t address, void* buffer, std::size_t size ) -> bool
		{
			if ( address >= extcall::stack_base && address + size <= extcall::stack_base + fake_stack.size ( ) )
			{
				std::memcpy ( buffer, &fake_stack[ address - extcall::stack_base ], size );

				return true;
			}

			if ( caller->heap_read ( address, buffer, size ) )
			{
				return true;
			}

			return memory->read_memory ( address, buffer, size );
		},
		[ &fake_stack ]( std::uint64_t address, const void* buffer, std::size_t size ) -> bool
		{
			if ( address >= extcall::stack_base && address + size <= extcall::stack_base + fake_stack.size ( ) )
			{
				std::memcpy ( &fake_stack[ address - extcall::stack_base ], buffer, size );

				return true;
			}

			caller->heap_write ( address, buffer, size );

			return true;
		}
	);

	caller->set_emulator ( &cpu );

	caller->register_functions ( );

	caller->register_modules ( { memory->image_base } );

	/* example: */
	// caller->call<type>( base + rva );

	cpu.set_module_base(memory->image_base);

	// cpu.set_debug(true);

	// static uobject* StaticFindObject(uobject * Class, uobject * InOuter, const wchar_t* Name, bool ExactClass);
	auto function = caller->call<std::uintptr_t>(
		memory->image_base + 0x229BF30,
		std::uint64_t{ 0 }, std::uint64_t{ 0 },
		wide_string(L"Engine.KismetSystemLibrary.GetGameName"), 0
	);

	const auto kismet = caller->call<std::uintptr_t>(
		memory->image_base + 0x229BF30,
		std::uint64_t{ 0 }, std::uint64_t{ 0 },
		wide_string(L"Engine.KismetSystemLibrary"), 0
	);

	// FString params buffer (16 bytes: data ptr + count + max) in fake stack
	const auto params_addr = extcall::stack_base + 0x200000;
	{
		std::uint8_t zero[ 16 ] = { };
		cpu.get_memory ( ).write ( params_addr, zero, sizeof ( zero ) );
	}

	// void __fastcall UObject::ProcessEvent(this, UFunction*, void* params)
	caller->call<void>(
		memory->image_base + 0x22187C0,
		kismet, function, params_addr
	);

	{
		const auto data_ptr = cpu.get_memory ( ).read< std::uint64_t > ( params_addr );
		const auto num = cpu.get_memory ( ).read< std::int32_t > ( params_addr + 8 );

		std::printf ( "GetGameName result: data=0x%llx num=%d\n",
					   static_cast< unsigned long long > ( data_ptr ),
					   num );

		if ( data_ptr && num > 0 && num < 512 )
		{
			std::vector< wchar_t > text ( num + 1, 0 );
			cpu.get_memory ( ).read ( data_ptr, text.data ( ), static_cast< std::size_t > ( num ) * sizeof ( wchar_t ) );
			std::wprintf ( L"GetGameName: \"%s\"\n", text.data ( ) );
		}
	}

	cpu.set_debug(false);

	caller->set_emulator ( nullptr );

	memory->detach ( );

	return 1;
}
