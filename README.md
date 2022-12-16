# Demangler
C++, Microsoft C++, Rust and DLang name demangler\
This code is from https://github.com/llvm/llvm-project

## Usage
* if you use meson, do ``dependency('demangler_dep')``
* else, add ``include/`` to your include directories and ``source/*`` to C++ sources
* in a freestanding environment, use can use https://github.com/ilobilo/libstdcxx-headers but you might also need to supply your own non-freestanding headers
* you can use either ``__cxa_demangle(string, bufferptr, lenptr, errptr)`` or ``llvm::demangle(string)``