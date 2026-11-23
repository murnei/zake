#!/bin/bash

set -e

rm -f zake

INC="../inc"
SRC="../src"

g++ -I$INC $SRC/*.cpp -o zrn/zake

cd zrn/
./zake
cd ..

