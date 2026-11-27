dependencies:
```
vcpkg
cmake
emsdk (optional)
```
windows:
```
cmake -DCMAKE_TOOLCHAIN_FILE=native.cmake -B build-win32
cmake --build build-win32 --config Debug
```
linux:
```
cmake -DCMAKE_TOOLCHAIN_FILE=native.cmake -B build-linux #-G "Ninja Multi-Config"
cmake --build build-linux #--config Debug
```
wasm:
```
#cmake -DEMSCRIPTEN=1 -DCMAKE_TOOLCHAIN_FILE=emscripten.cmake -B build-wasm
emcmake cmake -DCMAKE_TOOLCHAIN_FILE=emscripten.cmake -B build-wasm
cmake --build build-wasm
npx serve -c serve.json
```