project('demangler', 'cpp', license : 'Apache-2.0')

include = include_directories('include')
sources = files(
    'source/Demangle.cpp',
    'source/DLangDemangle.cpp',
    'source/ItaniumDemangle.cpp',
    'source/MicrosoftDemangle.cpp',
    'source/MicrosoftDemangleNodes.cpp',
    'source/RustDemangle.cpp',
    'source/cxa_demangle.cpp'
)

demangler_dep = declare_dependency(include_directories : include, sources : sources)

if meson.version().version_compare('>=0.54.0')
    meson.override_dependency('demangler', demangler_dep)
endif