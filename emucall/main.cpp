#include <source/imports.hpp>

auto main ( ) -> std::int32_t
{
	if ( !memory->attach ( L"RustClient.exe" ) )
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

	/* without this the "ida:" column in the trace is just rip + 0x180000000 rather than a real rebase */
	cpu.set_module_base ( memory->game_assembly );

	caller->set_emulator ( &cpu );

	caller->register_functions ( );

	caller->register_modules ( { memory->game_assembly, memory->unity_player } );

	/* example: */
	// caller->call<type>( base + rva );

	caller->set_emulator ( nullptr );

	memory->detach ( );

	return 1;
}
