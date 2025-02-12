#!/bin/bash

source /usr/share/camera/gmsl-serdes-debug.sh

#echo "------------------------------------------------"
echo "#### GMSL-A Deserializer max9296 (0x48 i2c-1) "
des_debug A
echo ""
echo "#### GMSL-E Deserializer max9296 (0x48 i2c-2) "
des_debug E
echo ""
echo "#### GMSL-F Deserializer max9296 (0x4a i2c-2)   "
des_debug F
#echo "------------------------------------------------"
