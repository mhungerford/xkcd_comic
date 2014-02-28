#!/bin/bash
convert $1 -type Grayscale -colorspace Gray -depth 1 -define png:compression-level=9 -define png:compression-strategy=0 -define png:exclude-chunk=all $1
#strategy 4 is fixed huffman tables
#convert $1 -interpolate Catrom -interpolative-resize "144!x168!" -black-threshold 70% -type Grayscale -colorspace Gray -depth 1 -define png:compression-level=0 converted/$1
#convert $1 -interpolate Catrom -interpolative-resize "144x168^" -gravity center -extent 144x168 -black-threshold 70% -type Grayscale -colorspace Gray -depth 1 -define png:compression-level=0 converted/$1
#convert $1 -adaptive-resize "144!x168!" -type Grayscale -colorspace Gray -depth 1 -define png:compression-level=0 converted/$1
#convert shadowfacts.png -morphology Thicken '1:0' -adaptive-resize 144\!x168\! -type Grayscale -colorspace Gray -depth 1 test.png
