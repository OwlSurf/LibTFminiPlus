#bin/bash
rm -r gtest/build
cmake -DCMAKE_BUILD_TYPE=Debug -S gtest/ -B gtest/build;
cmake --build gtest/build;
make -C gtest/build/;
