time /usr/bin/gcc -std=c11 -g code/ld50.c -o build/ld50 -Weverything -Wno-missing-braces -Wno-old-style-cast -Wno-zero-as-null-pointer-constant -Wno-missing-field-initializers -Wno-disabled-macro-expansion -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-c99-compat -Wno-unused-function -Wno-switch -Wno-unused-macros -Wno-unused-const-variable -Werror=implicit-function-declaration -Werror=int-conversion -Werror=compare-distinct-pointer-types -Werror=return-type -Werror=incompatible-pointer-types -lsdl2 -lsdl2_ttf  -I /usr/local/Cellar/sdl2/2.0.14_1/include/SDL2/ -I /usr/local/Cellar/sdl2_ttf/2.0.15/include/SDL2/ -L /usr/local/Cellar/sdl2/2.0.14_1/lib/ -L /usr/local/Cellar/sdl2_ttf/2.0.15/lib

cp data/*  build/
