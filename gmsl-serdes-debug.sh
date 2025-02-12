#!/bin/bash

i2ctransfer_16be_8() {
    # Originally by Josh Watts - converted to bash function by Chris White
    if [[ "$#" -lt 2 ]]; then
	echo "Usage: $0 <bus> <i2c addr> <reg> [<val> [<mask>]]" >&2
	return 1
    fi

    local -r bus="$1"
    local -r addr="$2"
    local -r reg_h="$(( ($3 >> 8) & 0xFF ))"
    local -r reg_l="$(( ($3 >> 0) & 0xFF ))"

    if [[ "$#" -ge 5 ]]; then
	local -r val="$4"
	local -r mask="$5"
	local t=$(i2ctransfer_16be_8 ${1} ${2} ${3})
	printf '0x%x: 0x%x -> ' ${3} $t
	t=$( printf '0x%x' $(( ( t & ~($mask) ) | ($val & $mask) )) )
	echo "$t"
	i2ctransfer_16be_8 ${1} ${2} ${3} ${t}
    elif [[ "$#" -ge 4 ]]; then
	local -r val="$4"
	i2ctransfer -f -y "$bus" "w3@$addr" "$reg_h" "$reg_l" "$val"
    else
	i2ctransfer -f -y "$bus" "w2@$addr" "$reg_h" "$reg_l" r1
    fi
}

i2ctransfer_16be_16le() {
    if [ $# -lt 2 ]; then
	echo "Usage: $0 <bus> <addr> <reg> [<val>]" >&2
	exit 1
    fi

    local -r BUS=${1}
    local -r ADDR=${2}
    local -r REG_H=$(( (${3} >> 8) & 0xFF ))
    local -r REG_L=$(( (${3} >> 0) & 0xFF ))

    if [[ "$#" -ge 5 ]]; then
	local -r val="${4}"
	local -r mask="${5}"
	local -a t=( $(i2ctransfer_16be_16le ${1} ${2} ${3}) )
	t=$(printf '0x%x' $(( ( t & ~($mask) ) | ($val & $mask) )) )
	i2ctransfer_16be_16le ${1} ${2} ${3} ${t}
    elif [ $# -ge 4 ]; then
	local -r VAL_H=$(( (${4:-} >> 8) & 0xFF ))
	local -r VAL_L=$(( (${4:-} >> 0) & 0xFF ))
	i2ctransfer -f -y ${BUS} w4@${ADDR} ${REG_H} ${REG_L} ${VAL_L} ${VAL_H}
    else
	# i2ctransfer -f -y ${BUS} w2@${ADDR} ${REG_H} ${REG_L} r2
	local -a t=( $(i2ctransfer -f -y ${BUS} w2@${ADDR} ${REG_H} ${REG_L} r2) )
	printf '0x%x\n' $(( ${t[1]} << 8 | ${t[0]} ))
    fi
}

des_reg() {
    local d=$1
    local r=$2
    local v=${3:-}
    local m=${4:-}
    local b
    local a
    case $d in
	# Update Advantech R360 GMSL2 (b)uss and (a)ddress for your situation
	A|G) b=1; a=0x48;;
	E|K) b=2; a=0x48;;
	F|L) b=2; a=0x4a;;
	# Update Axiomtek GMSL2 (b)uss and (a)ddress for your situation
	a|g) b=4; a=0x48;;
	b|h) b=4; a=0x4a;;
	c|i) b=4; a=0x68;;
	d|j) b=4; a=0x6c;;
	*) return 1
    esac

    if [[ "$#" -ge 4 ]]; then
	i2ctransfer_16be_8 $b $a $r $v $m
    elif [[ "$#" -ge 3 ]]; then
	i2ctransfer_16be_8 $b $a $r $v
    else
	i2ctransfer_16be_8 $b $a $r
    fi
}

ser_reg() {
    local d=$1
    local r=$2
    local v=${3:-}
    local m=${4:-}
    local a
    local b
    case $d in
	# Update Advantech R360 GMSL2 (b)uss and (a)ddress for your situation (0 and 1 is default address)
	0) b=1; a=0x40;;
	A) b=1; a=0x42;;
	G) b=1; a=0x44;;
	1) b=2; a=0x40;;
	E) b=2; a=0x62;;
	K) b=2; a=0x64;;
	F) b=2; a=0x42;;
	L) b=2; a=0x44;;
	# Update Axiomtek ROBOX500 GMSL2 (b)uss and (a)ddress for your situation (2 is default address)
	2) b=4; a=0x40;;
	a) b=4; a=0x42;;
	g) b=4; a=0x43;;
	b) b=4; a=0x44;;
	h) b=4; a=0x45;;
	c) b=4; a=0x62;;
	i) b=4; a=0x63;;
	d) b=4; a=0x64;;
	j) b=4; a=0x65;;
	*) return 1
    esac

    if [[ "$#" -ge 4 ]]; then
	i2ctransfer_16be_8 $b $a $r $v $m
    elif [[ "$#" -ge 3 ]]; then
	i2ctransfer_16be_8 $b $a $r $v
    else
	i2ctransfer_16be_8 $b $a $r
    fi
}

sen_reg8() {
    local d=$1
    local r=$2
    local v=${3:-}
    local m=${4:-}
    local a
    local b
    case $d in
	# Update Advantech R360 GMSL2 (b)uss and (a)ddress for your situation (0 and 1 is default address)
	0) b=1; a=0x40;;
	A) b=1; a=0x42;;
	G) b=1; a=0x44;;
	1) b=2; a=0x40;;
	E) b=2; a=0x62;;
	K) b=2; a=0x64;;
	F) b=2; a=0x42;;
	L) b=2; a=0x44;;
	# Update Axiomtek ROBOX500 GMSL2 (b)uss and (a)ddress for your situation (2 is default address)
	2) b=4; a=0x40;;
	a) b=4; a=0x42;;
	g) b=4; a=0x43;;
	b) b=4; a=0x44;;
	h) b=4; a=0x45;;
	c) b=4; a=0x62;;
	i) b=4; a=0x63;;
	d) b=4; a=0x64;;
	j) b=4; a=0x65;;
	*) return 1
    esac

    if [[ "$#" -ge 4 ]]; then
	i2ctransfer_16be_8 $b $a $r $v $m
    elif [[ "$#" -ge 3 ]]; then
	i2ctransfer_16be_8 $b $a $r $v
    else
	i2ctransfer_16be_8 $b $a $r
    fi
}

sen_reg16() {
    local d=$1
    local r=$2
    local v=${3:-}
    local b
    local a
    case $d in
	# Update Advantech R360 GMSL2 (b)uss and (a)ddress for your situation (0 and 1 is default address)
	0) b=1; a=0x40;;
	A) b=1; a=0x42;;
	G) b=1; a=0x44;;
	1) b=2; a=0x40;;
	E) b=2; a=0x62;;
	K) b=2; a=0x64;;
	F) b=2; a=0x42;;
	L) b=2; a=0x44;;
	# Update Axiomtek ROBOX500 GMSL2 (b)uss and (a)ddress for your situation (2 is default address)
	2) b=4; a=0x40;;
	a) b=4; a=0x42;;
	g) b=4; a=0x43;;
	b) b=4; a=0x44;;
	h) b=4; a=0x45;;
	c) b=4; a=0x62;;
	i) b=4; a=0x63;;
	d) b=4; a=0x64;;
	j) b=4; a=0x65;;
	*) return 1
    esac

    if [[ "$#" -ge 4 ]]; then
	local m=${4}
	i2ctransfer_16be_16le $b $a $r $v $m
    elif [[ "$#" -ge 3 ]]; then
	i2ctransfer_16be_16le $b $a $r $v
    else
	i2ctransfer_16be_16le $b $a $r
    fi
}

des_debug() {
    local d=$1

    echo "MIPI_PHY17: tun_data_crc[5], tun_ecc_uncorr_err[4], tun_ecc_corr_err[3], vid_overflow_flag[0]"
    echo -n "0x341 "
    des_reg $d 0x341
    echo

    if false; then
	# The following are only available on CSI2 tunneling parts
	echo "MIPI_PHY18: (csi2_tx2_pkt_cnt / csi2_tx1_pkt_cnt)"
	echo -n "0x342 "
	des_reg $d 0x342
	echo

	echo "MIPI_PHY20: (phy1_pkt_cnt / phy0_pkt_cnt)"
	echo -n "0x344 "
	des_reg $d 0x344
	echo

	echo "MIPI_PHY21: (phy3_pkt_cnt / phy2_pkt_cnt)"
	echo -n "0x345 "
	des_reg $d 0x345
	echo

	echo "MIPI_TX2: MIPI TX status"
	echo -n "0x442 "
	des_reg $d 0x442
	echo
	# End CSI2 tunneling registers
    fi

    echo "VPRBS: VIDEO_LOCK[0]"
    echo -n "0x1dc "
    des_reg $d 0x1dc
    echo -n "0x1fc "
    des_reg $d 0x1fc

    echo "VIDEO_RX8"
    echo -n "0x108 "
    des_reg $d 0x108
    echo -n "0x11a "
    des_reg $d 0x11a
    echo -n "0x12c "
    des_reg $d 0x12c
    echo -n "0x13e "
    des_reg $d 0x13e

    echo -n "0x22 "
    des_reg $d 0x22
    echo -n "0x23 "
    des_reg $d 0x23
    echo -n "0x24 "
    des_reg $d 0x24
    echo -n "0x25 "
    des_reg $d 0x25

}


ser_debug() {
    ser_dump_reg() {
	SER=${1}
	REG=${2}
	echo -n "${REG}: "; ser_reg ${SER} ${REG};
    }

    #echo "config_spread_bit_ratio"
    #ser_dump_reg ${1} 0x1b03
    echo "Error flags"
    ser_dump_reg ${1} 0x1f
    echo "PCLKDET:"
    ser_dump_reg ${1} 0x102
    ser_dump_reg ${1} 0x10A
    ser_dump_reg ${1} 0x112
    ser_dump_reg ${1} 0x11A
    echo "MIPI PHY1 LP & HS errors:"
    ser_dump_reg ${1} 0x33b
    ser_dump_reg ${1} 0x33c
    echo "MIPI PHY2 LP & HS errors:"
    ser_dump_reg ${1} 0x33d
    ser_dump_reg ${1} 0x33e
    echo "MIPI CSI0 err lo & hi"
    ser_dump_reg ${1} 0x341
    ser_dump_reg ${1} 0x342
    echo "MIPI CSI1 err lo & hi"
    ser_dump_reg ${1} 0x343
    ser_dump_reg ${1} 0x344
    echo "IMG_ERR0 / GPIO5:"
    ser_dump_reg ${1} 0x2cd
    echo "IMG_ERR1 / GPIO6:"
    ser_dump_reg ${1} 0x2d0
    echo "START_PORT / CLK_SEL"
    ser_dump_reg ${1} 0x308
    echo "LANE23"
    ser_dump_reg ${1} 0x332
    echo "LANE01"
    ser_dump_reg ${1} 0x333
}
