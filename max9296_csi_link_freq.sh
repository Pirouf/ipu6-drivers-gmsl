#!/bin/sh

mbps=$(( ${1:-2000} / 100 ))
bus=${2:-1}
des=${3:-0x48}

i2ctransfer -f -y $bus w3@$des 0x03 0x26 $(( 1<<5 | ${mbps} ))

v="$(i2ctransfer -f -y $bus w2@$des 0x03 0x13 r1)"
# Disable
v=$(( $v & ~(1<<1) ))
i2ctransfer -f -y $bus w3@$des 0x03 0x13 $v

sleep 0.5

# Enable
v=$(( $v | (1<<1) ))
i2ctransfer -f -y $bus w3@$des 0x03 0x13 $v

if [ ${mbps} -ge 1500 ]; then # Fixed, ge not gt
    # Manual deskew
    i2ctransfer -f -y $bus w3@$des 0x04 0x43 $(( 1<<5|1<<4 ))
    i2ctransfer -f -y $bus w3@$des 0x04 0x43 0
fi
