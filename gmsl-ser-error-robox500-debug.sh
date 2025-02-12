#!/bin/bash

source /usr/share/camera/gmsl-serdes-debug.sh

echo "------------------------------------------------"
echo "#### GMSL-A Serializer max9295 (0x42 i2c-4)     "
ser_debug a
#echo ""
#echo "#### GMSL-G Serializer max9295 (0x43 i2c-4)     "
#ser_debug g
echo "------------------------------------------------"
echo "#### GMSL-B Serializer max9295 (0x44 i2c-4     "
ser_debug b
#echo ""
#echo "#### GMSL-H Serializer max9295 (0x45 i2c-4)"
#ser_debug h
echo "------------------------------------------------"
echo "#### GMSL-C Serializer max9295 (0x62 i2c-4)     "
ser_debug c
#echo ""
#echo "#### GMSL-I Serializer max9295 (0x63 i2c-4)"
#ser_debug i
echo "------------------------------------------------"
echo "#### GMSL-D Serializer max9295 (0x64 i2c-4)     "
ser_debug d
#echo ""
#echo "#### GMSL-J Serializer max9295 (0x65 i2c-4)"
#ser_debug j
echo "------------------------------------------------"
