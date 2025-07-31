mkdir -Force build
cd build
cmake .. -G "NMake Makefiles" -DBUILD_SHARED_LIBS=OFF
cmake --build . -- /M
ctest
