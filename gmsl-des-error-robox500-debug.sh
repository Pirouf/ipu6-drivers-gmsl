#!/bin/bash

source /usr/share/camera/gmsl-serdes-debug.sh

#echo "------------------------------------------------"
echo "#### GMSL-A Deserializer max9296 (0x48 i2c-4) "
des_debug a
echo ""
echo "#### GMSL-B Deserializer max9296 (0x4a i2c-4) "
des_debug b
echo ""
echo "#### GMSL-C Deserializer max9296 (0x68 i2c-4)   "
des_debug c
echo ""
echo "#### GMSL-D Deserializer max9296 (0x6c i2c-4)   "
des_debug d
#echo "------------------------------------------------"
