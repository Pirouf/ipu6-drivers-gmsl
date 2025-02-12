#!/bin/bash

source /usr/share/camera/gmsl-serdes-debug.sh

echo "------------------------------------------------"
echo "#### GMSL-A Serializer max9295 (0x42 i2c-1)     "
ser_debug A
echo ""
echo "#### GMSL-G Serializer max9295 (0x44 i2c-1)     "
ser_debug G
echo ""
echo "------------------------------------------------"
echo "#### GMSL-E Serializer max9295 (0x62 i2c-2)     "
ser_debug E
echo ""
echo "#### GMSL-K Serializer max9295 (0x64 i2c-2)"
ser_debug K
echo "------------------------------------------------"
echo "#### GMSL-F Serializer max9295 (0x44 i2c-2)     "
ser_debug F
echo ""
echo "#### GMSL-L Serializer max9295 (0x44 i2c-2)"
ser_debug L
echo "------------------------------------------------"

