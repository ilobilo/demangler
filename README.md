# Demangler
Name demangler from llvm with floating point support removed. Can be used in a kernel.
This code is from https://github.com/llvm/llvm-project

## Usage
* Meson: ``dependency('demangler_dep')``
* Everything else: add ``include/`` to your include directories and compile ``source/*.cpp``
* In a freestanding environment, use can use https://github.com/osdev0/freestnd-cxx-hdrs
* You can use either ``__cxa_demangle(string, bufferptr, lenptr, errptr)`` from <cxxabi.h> or ``llvm::demangle(string)`` from <demangler/Demangle.h>
* Use ``only_itanium=true`` or compile just ``source/ItaniumDemangle.cpp`` and ``source/cxa_demangle.cpp`` to enable only ``__cxa_demangle`` and ``ItaniumDemangle.h``