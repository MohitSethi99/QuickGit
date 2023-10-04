@echo off

echo Cloning libgit2 and building binaries

if not exist ".cache" mkdir .cache
git clone -q --depth=1 --branch=main https://github.com/libgit2/libgit2.git .cache/libgit2
pushd ".cache/libgit2"
mkdir build
cmake -S . -B build
cmake --build build --config Debug -j %NUMBER_OF_PROCESSORS%
cmake --build build --config Release -j %NUMBER_OF_PROCESSORS%
popd
robocopy ".cache/libgit2/build/Debug" "../QuickGit/vendor/libgit2/build/Debug" /E
robocopy ".cache/libgit2/build/Release" "../QuickGit/vendor/libgit2/build/Release" /E
robocopy ".cache/libgit2/include" "../QuickGit/vendor/libgit2/include" /E

pause
