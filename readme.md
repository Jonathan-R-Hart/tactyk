# TACTYK (should) Affix Captions To Your Kitten Scripting Engine

TACTYK-* is a private project aimed at developing an application scripting engine which uses a minimalist software design, maintains a strong security sandbox, and supports low-level programming.  A minimal design is to be understood both as a small codebase and as having no specified base platform or library support beyond language primitives (A runtime or host application is responsible for providing any library support).  Low level programming is to be understood as directly exposing underlying hardware features as language primitives, and minimizing the need for computationally expensive context switching (or any semblance thereof).

Due to the increased difficulty of low-level programming, TACTYK is also conceived and implemented as a general-purpose scripting engine, and not as a specific scripting language.  In the simple case, a language may be specified through a "Virtual Instruction Set Architecture" scheme (which defines how program code is transformed into assembly-language code). In the more interesting case, a standard Virtual ISA is used, but the platform provides access to the TACTYK emit interface, allowing compilers for high-level languages to be implemented entirely within the TACTYK security sandbox (the vast codebase that constitutes a conventional modern compiler infrastructure is one of the potential hazards this project aims to bypass).

At present, TACTYK is a personal project.  Though a fair amount of attention went into security, there are no security guarantees behind this project.  In its present state, it is highly inadvisable to use this for any purpose beyond research and development.  The author is not officially an expert in any aspect computer security and has no relevant credentials.  There has been a reasonable amount of study of material in public discourse on what tends to make programs vulnerable, and a few ideas about mitigations have been worked into the project, but none of this should be taken as a substitute for formal review (see "experimental security features" section for details).

Should the project gain attention and a reasonable level of backing and accountability, there will be an intent will be grow into a more serious role.

## Platform
Linux/Unix on amd64 (x86-64) architecture
Cross-platform and multi-architecture support are interesting ideas and lofty goals to have, but for a solo project, TACTYK is complex enough already.

## Features
- Small codebase
  According to cloc, the scripting engine uses about 5000 lines of C, the basic standalone runtime uses about 600 lines of C, and the supplemental auxiliary
  library currently uses about 700 lines.
  The specification for the building scripting language is about 11000 lines of code, and is augmented with a 700 line assembly header (mostly definitions and aliases).
    (this is a fairly expressive language - through aggressive subsetting, it can be reduced to just a few hundred lines).
- Total specification of low-level behavior through a "Virtual Instruction Set Architecture"
- A builtin assembly-like scripting language (see "TACTYK-PL")
- The defined script environment for the builtin language implements a security sandbox with a few unusual features (some aspects are possibly novel and may be worth an academic study).

## TACTYK-PL

TACTYK-PL is the default programming language which (with the scripting engine is designed for).

Main features:
- An assembly-like scripting language
- Exposes CPU registers and high-speed memory as language primitives
- low-level flow control operators which are decoupled from context switching
- Supports dynamic programming
- Partial interoperability with native/external libraries (can perform simple function calls, but the preference is that the external code should query and manipulate the TACTYK context data structure)
- General instruction categories are:  memory, bitwise, "Super-SIMD" (extra-wide SIMD operators spanning multiple registers), vectorized math, high-precision math, lightweight flow-control, and external calls
- x86 weirdness is generally abstracted out (lots of instruction variants to handle the very non-orthogonal underlying instruction set)
- Plays nicely with an aggressive security sandbox (detailed below)
- Fully specified with a Virtual Instruction Set Architecture
- Though an assembly-like language, mnemonics are mostly eschewed in favor of spelled out instruction names and parameters
- An optional dataflow syntax for representing relationships between registers visually

## Experimental Security Features
- Aggressive binary executable randomization
  By default, All script-influenced aspects of the generated executable are transformed with scrambling and permutation, to reduce the ability of adversaries to
  reliably encode instructions which are not explicitly scripted or permitted through the security model (code which would be accessed through exploitatively
  jumping into the middle of an instruction).  More specifically, this scrambles the immediate operands that go into machine instructions, and permutes
  the order in which instructions are laid out in assembly.

- Global register allocation
  Within a TACTYK executable, each CPU register has a pre-determined purpose and type (set up through the virtual ISA).  This register allocation is the same
  throughout every function the entire generated binary (except for some cases where a temp register needs to be used for intermediate products).
  This greatly simplifies TACTYK at a low level, and should expected to do much to help resist type confusion attacks.
  (It might be worthwhile to propose global register allocation as an optional compiler option for cases where security and assurance are considered more important
  than optimizations for speed or size)

- Sidelined stack
  When running a TACTYK program, the stack registers are either zeroed out or replaced with small values that reference only unmapped memory.  Internally, TACTYK
  operates exclusively on the heap.  When there is a need to interact with the native context (either calling into a C function or returning to one), the stack
  registers are reloaded from an opaque data structure (and unloaded immediately if/when control returns to TACTYK).
  This converts any unexpected interaction with the stack into a segmentation fault, and is expected to be a strong countermeasure against all attacks which
  use stack corruption.
  More broadly, there also is no generally accessible local context (implicit data structure which can be addressed through unchecked array access).  "local" data
  is stored either in statically allocated slots with rigid accessors or in managed memory blocks (the memory blocks also must be bound to rigid accessors).

- Address Space Layout Randomization
  TACTYK includes an optional memory allocator which calls mmap for most dynamic memory allocation and requests pages with randomly generated 48-bit base addresses.
  I do not know if it is an appropriate way to allocate memory (it does cause rapid allocation and de-allocation of many memory pages under the current design),
  so this is disabled by default (add #define USE_TACTYK_ALLOCATOR to enable it).
  Addendum:  I've thought this through a bit more.  The largest problem with the TACTYK approach seems to be simply that it results in a very large number of system
  calls.  This case would probably better be handled by a Kernel function which performs batched memory allocation and randomization (and presumably an alternative
  allocator which uses a kernel-provided address space layout for servicing application memory allocations instead of a conventional heap).
  A quick Internet search for "batched memory allocation" did not yield a meaningful result, so this one might have to start with a proposal.

- Exopointers
  By default, registers which reference common statically allocated [from script perspective] data structures are offset significantly from the actual address of the data
  structure.  The offsets are random 30-bit integers generated during TACTYK initialization.  These values are inserted as displacement constants in all operations which
  access the relevant data structures.  The main intent of this is to force extra random bytes into executable code which are not under the control of adversarial content.
  Secondarily, this also somewhat improves ASLR, as randomly selected displacement constants might be harder to obtain through exploits than raw pointers.

- Dynamic Memory Access Restrictions
  All dynamic memory access within TACTYK is restricted to defined memory blocks (called "memblocks").  There are no operations which cause memory to be dynamically accessed
  within any data structure which contains critical or protected data (not even indirectly).  There is no ad-hoc mechanism for setting up dynamically addressable objects within TACTYK.
  Lack of dynamic memory access simplifies the memory model and should reduce the possibilities of exploits involving poorly implemented memory operations (moreso if the mmap-based
  ASLR system is enabled).

## Auditability Focus
TACTYK is intended to be auditable under an alternative standard:  A competent individual or a small team should be able to conduct the audit (or verify the results of a professional audit).  The amount of code in the project is to be kept low (or at least the amount of code required for critical components).  Dependencies are to be kept to a minimum.  When there is a choice between a complex and broadly capable function and a simple and narrowly defined function, the simple function should win out (unless there is an actual need for the additional features from the complex one).

## Dependencies:
C Standard library
POSIX interface (mainly mmap.h for obtaining executable memory)
Netwide-Assembler (not linked - run as a separate system process and interacted with through file IO)

Netwide Assembler is technically a large dependency, but the assembly-language templates in use are probably more relevant to considerations of complexity.  It could also theoretically be replaced by switching everything to binary templates - at the cost of having to understand x86 machine code instead of x86 assembly.

## Supplemental dependencies:

SDL2

At some point in the future, OpenGL is likely to be added as a dependency.  The side-project to integrate 3D rendering is presently able to draw a single triangle using a relatively modern approach.  Some of the vectorized math instructions where added as part of this effort.

## News
### Version 0.9.0
  New standalone runtime (replaces tdemo)
  - TACTYK programs may now be formally defined with a manifest file (script modules, script parameters, data files, export targets).
  - The standalone runtime contains a set of functions for printing to the system console, for waiting, and for obtaining an SDL-backed RGBA framebuffer with mouse events.
  - There is also a plan to add an OpenGL-based 3D rendering interface.
  Most experimental security features are now optional, but enabled by default, and may be disabled by invoking TACTYK executables with the "--perf" option.
  - Thus far, no major performance improvement was [casually] observed when disabling security features, but there have not yet been any really large TACTYK programs to test it with.
  - The biggest performance disadvantage from the experimental security features is expected to be from executable layout randomization, which severely undermines instruction cache coherence
      (unless modern x86 CPUs also analyze unconditional jumps and physically untangle programs and strip out instructions which could be considered vestigial).
  Support for dynamic programming
  - Ability to acquire the integer value of a label, store it on a register, use the stored value with goto and lwcall instructions,
  - Computed goto and lwcall is also supported
  - This uses the global jump table to obtain valid jump targets
  - This also implicitly uses the strong safety guarantees from the global register allocation scheme
      (all script instructions are required to begin and end with the TACTYK VM in a consistent/valid state, and to assume flow control is completely arbitrary).
  Code blocks are now recognizable by Virtual ISA operators (and the if statement is able to conditionally execute a subordinate code block).
  New bitwise instructions (find-bit, reverse-bits, reverse-bytes, lookup-table transform)
  Stash entries are now directly usable within many instructions (the variants that support this are mostly implemented with x86 register-memory instructions)
  Bus Notation (An optional/alternative dataflow syntax which maybe can help to visualize how programs make use of registers and memory locations)
    This is probably at best only useful in a few niche cases and the example code which was retrofitted to demonstrate it (julia_bus.tkp) was rendered less readable overall.
  
  Work has also begun on a 2D rendering component which operates entirely within the TACTYK sandbox, as well as a User interface.
  - SDL is used to display the output, as well as to collect mouse events and make them available to TACTYK scripts.
  - It can render UTF-8 format text with bitmap fonts (extracted from BDF-format bitmap fonts with a font subsetting utility program)
  - It can receive and handle mouse events (mouse movement and clicks).
  - It can load and display PNG images.
  - It internally uses a custom software-based rendering pipeline which loosely resembles a conventional rendering pipeline and thus is expected to be fairly flexible.
      (somewhat unconventionally, this rendering pipeline also includes a programmable rasterization stage)
  Additional TACTYK-PL components/utilities:
  - associative array handling
  - DEFLATE decompressor
  - generic decoder (with a configuration function to help it recognize Canonical Huffman Codes)
  - PNG image loader (most basic PNG features are supported, interlacing and gamma correction are not)
  - BDF-format bitmap font extractor/subsetter
  - text file reader (specialized for 8 and 16 char strings)

### Version 0.8.0
  Super-SIMD
    A SIMD interface using extra-wide operators which operate on multiple xmm registers.
  compute instruction (x87-backed flexible mathematical function evaluator which works like a simple calculator)
    Operations are not re-ordered.
    Has the ability to temporarily place up to 4 values into otherwise unused x87 registers and retrieve them (for complex calculations).
    supported operators:  load, add, subtract, multiply, divide, modulo, sine, cosine, tangent, arctangent, absolute-value, square-root, round, logN, log2, and exponentiation
    It also has reversed-order operators for non-commutative two-argument functions.
  xcompute - behaves similarly to compute, uses xmm registers, is able to directly assess any xmm register, but supports only basic operators
  Assignment/load/store operations for register groups
  SIMD-based vector/matrix operations
  Detailed error reports

### Verison 0.7.0
  TACTYK now has a semi-automated testing system: tactyk-test.
  Tactyk-test tests the Virtual Instruction Set Architecture.
  Tactyk-test runs a series of test scripts.
  Each test script provides program code, a series of functions to call, and expected state transitions to check for.
  A test passes if it completes a script without unexpected errors, accounts for all state transitions, and finds no unexpected state transitions.
  Each test script is run under its own process.
  Tactyk-test is able to run tests in parallel.
  
  A suite of tests have been created covering all of the builtin/example virtual ISA (tactyk_core.visa)
  
  Many defects have been identified and corrected as a result of the new testing system (mostly in code which was not used by tactyk examples and not covered by manual testing).
  
  The production of the test suite also led to a variety of improvements to stacks, memory management, and state management.

## Getting Started
```
OBTAIN PROJECT DEPENDENCIES
Install Cmake
Install Netwide-Assembler (NASM)
Install SDL2

CLONE THE PROJECT
download/unpack OR git-clone
navigate to the project directory

BUILD THE PROJECT
mkdir build
cmake -B build .
cd build
cmake --build .

RUNNING EXAMPLES
./trun examples/hello.tkp
./trun examples/fib.tkp
./trun examples/emit.tkp
./trun examples/hello_sdl.tkp
./trun examples/julia.tkp
./trun examples/recursive.tkp
./trun examples/matrix.tkp
./trun examples/matrix-simd.tkp

DEMONSTRATION OF THE IN-DEVELOPMENT USER-INTERFACE
./trun extras/programs/drawtest

RUNNING TACTYK TESTS
./ttest --jobs=16 tests/*
```

The project documentation is located in the "doc" directory.  A fair amount of the documentation is probably OK-ish, but overall, a technical writer with strong interrogation skills is "desperately" needed.  Until then, you will have to make do with the sample code and experimentation (and/or learn how to read/use the Virtual ISA specification, which is located in the "rsc" directory).  I recommend starting with "examples/hello.tkp", as that is probably one of the least insane bits of the project.

TACTYK project contains a fair amount of sample code to learn from.  These are located in the "examples", "extras", and "tests" directories.

## History
I considered embedding a scripting engine, attempted to examine its source code, noticed that it was beyond 20000 lines of C, and that the C code contains non-trivial
macros (on top of comments from the Internet suggesting that it was not a secure), then *balked and started up a project to try to build a scripting engine a bit
more to my liking.  As was the case with every other attempt, the result was yet another under-performing script engine.  Then I decided to try defining a few Assembly Language functions and have them transfer control to each other through a jump table, then only build the jump table with C.  This resulted in scriptable activity taking place at about half my system clock rate (with some clear assistance from speculative execution), thereby initiating project TACTYK.  

During the development of the project, I had a few insights into how programs get attacked, as well as some ideas about how to mitigate it, which were worked into the project as the above-mentioned "security features".  The apparent uniqueness of these solutions further motivated development of the project.

And as the project progressed, I wanted a more formal mechanism for specifying the behavior of the virtual machine (and something a that would be relatively easy extend).  This yielded the "Virtual Instruction Set Architecture" scheme, which has thus far proved to be quite effective toward that end - It has become quite rare to have a new TACTYK feature which requires non-trivial modifications the project's C code (and most do not even require that).

(*) WebAssembly with a compiler in the sandbox would have satisfied the requirement for a minimalist and auditable scripting engine, but I didn't think to do that until development of TACTYK was well underway.  And by then, TACTYK was already attaining major objectives and getting seemingly novel security features.



