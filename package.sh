#!/bin/bash
mkdir -p gui_gal
cp -r res gui_gal/res
cp -r build gui_gal/build
tar -czvf gui_gal.tar.gz gui_gal