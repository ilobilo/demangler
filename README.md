# Demangler
C++, Microsoft C++, Rust and DLang name demangler\
This code is from https://github.com/llvm/llvm-project

## Usage
* if you use meson, do ``dependency('demangler_dep')``
* else, add ``include/`` to your include directories and ``source/*`` to C++ sources
* in a freestanding environment, use can use https://github.com/ilobilo/libstdcxx-headers but you might also need to supply your own non-freestanding headers
* you can use either ``__cxa_demangle(string, bufferptr, lenptr, errptr)`` from <cxxabi.h> or ``llvm::demangle(string)`` from <demangler/Demangle.h>
* use ``only_itanium=true`` or compile just ``source/ItaniumDemangle.cpp`` and ``source/cxa_demangle.cpp`` to enable only ``__cxa_demangle`` and ``ItaniumDemangle.h``