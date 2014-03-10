#!/bin/bash

for PNG in *.png; do 
  pngcrush -fix -rem alla -rem gAMA -rem cHRM -rem tRNS -rem sRGB -rem iCCP -rem bKGD -rem pHYs -rem tEXt -rem time -reduce -brute $PNG $PNG.m
  mv -f $PNG.m $PNG
done

