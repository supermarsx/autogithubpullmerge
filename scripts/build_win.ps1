mkdir -Force build
cd build
cmake .. -G "NMake Makefiles"
cmake --build . -- /M
ctest
