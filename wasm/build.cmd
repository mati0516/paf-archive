@echo off
echo Building WebAssembly with Docker Emscripten...

if not exist dist mkdir dist

docker run --rm -v "%cd%\..:/src" emscripten/emsdk emcc ^
    libpaf/src/*.c wasm/paf_wasm.c ^
    -Ilibpaf/include -DLIBPAF_EXPORTS ^
    -O3 ^
    -s WASM=1 ^
    -s EXPORTED_RUNTIME_METHODS="['ccall', 'cwrap', 'UTF8ToString', 'stringToNewUTF8', 'FS']" ^
    -s EXPORTED_FUNCTIONS="['_wasm_paf_list', '_wasm_paf_extract_all', '_wasm_paf_create', '_malloc', '_free']" ^
    -s ALLOW_MEMORY_GROWTH=1 ^
    -s FORCE_FILESYSTEM=1 ^
    -o wasm/paf.js

echo Build Complete.
