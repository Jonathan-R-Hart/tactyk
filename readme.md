# TACTYK (should) Affix Captions To Your Kitten Scripting Engine

This is an experimental security-focused minimalist application scripting engine.

At present, TACTYK is a personal project.  Though this is a security project, there are no security guarantees behind this, and in its present state, it is highly inadvisable to use this for any purpose beyond research and development.  The author is *not* an expert in any aspect computer security (and also has no relevant credentials).  

Should the project gain attention and a reasonable level of backing (and presumably accountability), the intent will be grow into a serious security role.

## Platform:
Linux/Unix on amd64 (x86-64) architecture

## Features:
- Small codebase 
  The main program has ~3000 lines of C which are specific to TACTYK (counting only components that would be included in a library)
  Default setup uses ~1250 lines of configuration (specification for each scriptable function) and a 600 line assembly header (mostly definitions)
- Total specification of low-level behavior through a "Virtual Instruction Set Architecture"

## Experimental Security Features
- Aggressive binary executable randomization
  All script-influenced aspects of the generated executable are transformed with scrambling and permutation, to reduce the ability of adversaries to
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
  I do not know if it is an appropriate way to allocate memory (it does cause rapid allocatation and de-allocation of many memory pages udner the current design), 
  so this is disabled by default (add #define USE_TACTYK_ALLOCATOR to enable it).
  Addendum:  I've thought this through a bit more.  The largest problem with the TACTYK approach seems to be simply that it results in a very large number of system 
  calls.  This case would probably better be handled by a Kernel function which performs batched memory allocation and randomization (and presumbly an alternative
  allocator which uses a kernel-provided address space layout for servicing application memory allocations instead of a conventional heap).
  A quick Internet search for "batched memory allocation" did not yield a meaningful result, so this one might have to start with a proposal.

- Exopointers
  Registers which reference common statically allocated [from script perspective] data structures are offset significantly from the actual address of the data structure.
  The offsets are random 30-bit integers generated during tactyk initialization.  These values are inserted as displacement constants in all operations which access
  the relevant data structures.  The main intent of this is to force extra random bytes into executable code which are not under the control of adversarial content.
  Secondarilly, this also somewhat improves ASLR, as randomly selected displacement constants might be harder to obtain through exploits than raw pointers.

## Auditability Focus
TACTYK is intended to be auditable under an alternative standard:  A competent individual or a small team should be able to conduct the audit (or verify the results of a professional audit).  The amount of code in the project is to be kept low.  Dependencies are to be kept to a minimum.  When there is a choice between a complex and 
broadly capable function and a simple and narrowly defined function, the simple function should win out (unless there is an actual need for the additional features from
the complex one).


## Dependencies:
C Standard library
POSIX interface (mainly mmap.h for obtaining executable memory)
Netwide-Assembler (not linked - run as a separate system process and interacted with through file IO)

Netwide Assembler is technically a large dependency, but the assembly-language templates in use are probably more relevant to considerations of complexity.  It could also theoretically be replaced by switching everything to binary templates - at the cost of having to understand x86 machine code instead of x86 assembly.

## TACTYK-examples dependencies:

SDL2

## Getting Started

Install Cmake

Install Netwide-Assembler (NASM)

Install SDL2

download/unpack OR git-clone

navigate to the project directory

mkdir build

cmake -B build .

cd build

cmake --build .

./tdemo examples/julia.tkp

./tdemo examples/emit.tkp

./tdemo examples/fib.tkp

./tdemo examples/lazy_quine.tkp

./tdemo examples/hello_sdl.tkp

./tdemo examples/recursive.tkp


## History
I considered embedding a scripting engine, attempted to examine its source code, noticed that it was beyond 20000 lines of C, and that the C code contains non-trivial 
macros (on top of comments from the Internet suggesting that it was not a secure), then *balked balked and started up a project to try to build a scripting engine a bit 
more to my liking.  As was the case with every other attempt, the result was yet another under-performing script engine.  Then I decided to try defining a few Assembly Language functions and have them transfer control to each other through a jump table, then only build the jump table with C.  This resulted in scriptable activity taking place at about half my system clock rate (with some clear assistance from speculative execution), thereby initiating project TACTYK.  

(*) WebAssembly with a compiler in the sandbox would have satisfied the requirement for a minimalist and auditable scripting engine, but I didn't think to do that until development of TACTYK was well underway.  And by then, TACTYK was already getting seemingly novel experimental security features.




