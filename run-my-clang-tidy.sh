run-clang-tidy -p build \
  -extra-arg=-std=c++17 \
  -extra-arg=--gcc-toolchain=/usr \
  -header-filter=.* \
  source/*.cpp
  