# emucall-main

`emucall` calls functions inside another process without injecting into it. It is a from-scratch x86-64 interpreter: it reads the target's own instruction bytes over `ReadProcessMemory` and executes them in your address space against a virtual CPU, a fake stack and a synthetic thread environment. No module is loaded into the target and no thread is hijacked. The example keeps writes in emulated memory; applying captured writes to the target is optional and requires an explicit caller-provided writer.

It works against any x86-64 Windows process — the development target happened to be an IL2CPP Unity game, but nothing in the emulator assumes one.

## How it works

A call sets up a virtual context and runs the target's code until it returns:

- **Stack** — a local buffer mapped at a fixed virtual base, with `rsp` seeded below the return address. Reads and writes that land in that range hit the buffer; everything else falls through to the real process.
- **Return detection** — the return address is a sentinel (`0xDEADBEEFDEADBEEF`); execution stops when `rip` reaches it, and the return value is read out of `rax` or `xmm0`.
- **Heap** — an emulated arena for anything the callee allocates, plus argument marshalling for managed, ANSI and wide strings so you can pass strings into functions that expect them.
- **Thread environment** — a synthetic TEB and 64 TLS slots, so code that touches thread-local state doesn't fault out.
- **External calls** — imports and runtime helpers that can't be sensibly interpreted are intercepted at their entry point and serviced by a handler instead of being stepped through.
- **Guard rails** — a per-call instruction limit (default 10,000) so a mispredicted branch spins out rather than hanging, and a ring buffer of the last 32 instructions that can be dumped when a call fails.

## Usage

Header-only. Add `emucall/` to your include path and include `source/imports.hpp`.

```cpp
if ( !memory->attach ( L"game.exe" ) )
{
    return 1;
}

std::vector< std::uint8_t > fake_stack ( 0x400000 );

auto cpu = emulator
(
    /* read  */ [ & ] ( std::uint64_t address, void* buffer, std::size_t size ) -> bool { ... },
    /* write */ [ & ] ( std::uint64_t address, const void* buffer, std::size_t size ) -> bool { ... }
);

caller->set_emulator ( &cpu );
caller->register_functions ( );

caller->register_modules ( { module_a, module_b } );

const auto result = caller->call< std::uint64_t > ( base + rva, argument );
```

`register_modules` takes as many module bases as you like and sizes each one from its own PE headers. Those ranges decide what gets interpreted: an address inside a registered module is executed instruction by instruction, and anything outside is treated as an external call and serviced by a hook. Register nothing and everything is interpreted, which is the right default when you only care about one self-contained function.

The two callbacks decide where each access goes — the fake stack, the emulated heap, or the real process. `main.cpp` is a working skeleton of that wiring.

Instruction fetches use a code-page cache and a decoded-instruction cache.
Guest writes invalidate affected cached code, and resetting the emulator flushes
both caches. `get_last_instruction_count()` and `get_last_faulted()` report the
last run's execution count and whether it stopped on a fault.

`pending_writes()` groups shadow-heap writes into contiguous regions. The
`commit_writes()` overloads accept either an address window or a region filter,
plus a writer callback. They apply only accepted whole regions and return the
number of bytes accepted by the writer; they do not automatically write to the
target. Captured writes remain available until the shadow heap is cleared.

Instance methods take the `this` pointer as the first argument, and `extcall::injected` marks calls whose arguments are already staged in the emulated heap:

```cpp
caller->call< void > ( address, extcall::injected, this_ptr, argument );
```

## Coverage

The decoder and instruction handlers cover the general-purpose integer set, control flow, the x87 and SSE floating-point paths and part of AVX. Coverage is driven by what real target functions actually execute, so it is broad rather than complete — an unimplemented opcode fails the call and reports the instruction rather than silently returning garbage.

| | lines |
|---|---|
| decoder | 4,846 |
| floating point | 4,156 |
| arithmetic | 2,853 |
| call layer | 1,285 |
| emulator core | 1,306 |
| movement / flow / AVX | 1,221 |

## Targets

Nothing here is tied to a particular game or engine. There are no hardcoded addresses or build-specific RVAs anywhere in the library, so nothing goes stale when a target updates, and modules are registered dynamically by base address — any executable, any number of them.

The decoder, the emulator core and the call layer only care about x86-64 machine code and the Windows calling convention. `register_functions` installs the Win32 and heap hooks, which are what most native code actually needs from its environment.

The one engine-flavoured piece is optional: `managed_string` arguments and `set_string_class` marshal IL2CPP strings, because that is what the development target used. Passing `ansi_string`, `wide_string` or plain integer and pointer arguments needs none of it. Supporting a different runtime's string or object layout means adding a marshaller next to those, not changing the emulator.

## Building

Windows x64, C++20, MSVC. Open `emucall.slnx` and build Release|x64, or:

```
msbuild emucall.slnx /p:Configuration=Release /p:Platform=x64
```

`mem_lib` opens the target with `PROCESS_ALL_ACCESS` and exposes both
`read_memory()` and `write_memory()`. The example uses only the read operation;
its emulated write callback updates local shadow memory.

## License

MIT. See [LICENSE](LICENSE).
