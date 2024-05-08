TFminiPlus Library
============

## Brief:

Library for TFmini Plus lidar.

## Description:

Parsing an output data form TFmini Plus


## How to build tests.
For tests [googletest framework](https://github.com/google/googletest) uses. 	

Needed environment:
- cmake;
- gcc;
- g++;
- libgtest-dev;

For build tests type:

    cmake -S gtest/ -B gtest/build 
    cmake --build gtest/build
    make -C gtest/build/

For Run test type:

    cd gtest/build/; ctest; cd ../../
or 

    ./gtest/build/runtests

## How to use: 

1) Add this repository to your project as a submodule.
2) Add \*.c files to your compiler.
3) Add "include" directories (\*.h files).
4) ...

## References

1) [SJ-PM-TFmini Plus A05 Product Manual](https://github.com/TFmini/TFmini-Plus)

