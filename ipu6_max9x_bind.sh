#!/bin/bash
#
# Dependency: v4l-utils
v4l2_util=$(which v4l2-ctl)
media_util=$(which media-ctl)
if [ -z ${v4l2_util} ]; then
	echo "v4l2-ctl not found, install with: sudo apt install v4l-utils"
	exit 1
fi
quiet=0
while [[ $# -gt 0 ]]; do
	case $1 in
		-q|--quiet)
			quiet=1
			shift
		;;
		-m|--mux)
			shift
			mux_param=$1
			shift
		;;
		-s|--sensor)
			sensor=$2
			shift
		;;
		-h|--help)
			echo "-q -m -h" 
		;;
		-f)
			fmt="$2"
			shift
			;;
		*)
			quiet=0
			shift
		;;
		esac
done

sensor="${sensor:-isx031}"
if [ ${sensor} = "isx031" ]; then
	fmt="${fmt:-[fmt:UYVY8_1X16/1920x1536]}"
	# fmt="${fmt:-[fmt:UYVY8_1X16/1600x1280]}"
	# fmt="${fmt:-[fmt:UYVY8_1X16/1024x768]}"
elif [ ${sensor} = "imx390" ]; then
	fmt="${fmt:-[fmt:SGRBG12_1X12/1920x1200]}"
	# fmt="${fmt:-[fmt:SRGGB12_1X12/1936x1096]}"
	# fmt="${fmt:-[fmt:SRGGB12_1X12/1280x720]}"
	# fmt="${fmt:-[fmt:SRGGB12_1X12/1024x768]}"
elif [ ${sensor} = "ar0234" ]; then
	fmt="${fmt:-[fmt:SGRBG10_1X10/1280x960]}"
fi

declare entities=()
while IFS= read -r line; do 
	entities+=("$line")
done < <(media-ctl -p | grep entity | sed -e 's/.*: //;s/ (.*$//')

declare -A des_mux_to_index=([a]=0 [b]=0 [c]=0 [d]=0 [e]=0 [f]=0 [g]=1 [h]=1 [i]=1 [k]=1 [j]=1 [l]=1 [A]=0 [E]=0 [G]=1 [H]=1 [I]=1 [J]=1 [K]=1 [L]=1 [M]=1)

find_entity() {
	local name=$1
	for e in "${entities[@]}"; do
		if [[ "${e}" = *"${name}"* ]]; then
			echo -n "${e}"
			return
		fi
	done
	#echo "$1 not found" >&2
	exit 1
}

des_node() {
	local mux=$1
	case ${mux} in
		a|g) echo -n "max9296 a";;
		A|G|H|I) echo -n "max96712 e";;
		b|h) echo -n "max9296 b";;
		c|i) echo -n "max9296 c";;
		d|j) echo -n "max9296 d";;
		e|k) echo -n "max9296 e";;
		E|K|L|M) echo -n "max96712 e";;
		f|l) echo -n "max9296 f";;
	esac
}
des_src_pad() {
	local mux=$1
	echo -n "\"$(des_node ${mux})\":0"
}
des_sink_pad() {
	local mux=$1
	echo -n "\"$(des_node ${mux})\":$(( 4 + ${des_mux_to_index[${mux}]} ))"
}

ser_node() {
	local mux=$1
	find_entity "max9295 ${mux}"
}
ser_src_pad() {
	local mux=$1
	echo -n "\"$(ser_node ${mux})\":2"
}
ser_sink_pad() {
	local mux=$1
	echo -n "\"$(ser_node ${mux})\":0"
}

sen_node() {
	local mux=$1
	find_entity "${sensor} ${mux}"
}
sen_src_pad() {
	local mux=$1
	echo -n "\"$(sen_node ${mux})\":0"
}

# mapping for ISX031/IMX390 mux entity to IPU6 BE SOC entity matching.
declare -A media_mux_capture_link=( [a]='' [b]='1 ' [c]='2 ' [d]='3 ' [e]='4 ' [f]='5 ' [g]='' [h]='1 ' [i]='2 ' [j]='3 ' [k]='4 ' [l]='5 ' [A]='' [E]='4 ' [G]='' [H]='' [I]='' [J]='' [K]='4 ' [K]='4 ' [M]='4 ')
# mapping for ISX031/IMX390 mux entity to IPU6 CSI-2 entity matching.
declare -A media_mux_csi2_link=( [a]=0 [b]=1 [c]=2 [d]=3 [e]=4 [f]=5 [g]=0 [h]=1 [i]=2 [j]=3 [k]=4 [l]=5 [A]=0 [E]=4 [G]=0 [H]=0 [I]=0 [J]=0 [K]=4 [L]=4 [M]=4 )

declare -A media_mux_capture_pad=(
	[a]=0
	[b]=0
	[c]=0
	[d]=0
	[e]=0
	[f]=0
	[g]=1
	[h]=1
	[i]=1
	[j]=1
	[k]=1
	[l]=1
	[A]=0
	[E]=0
	[G]=1
	[H]=2
	[I]=3
	[J]=4
	[K]=2
	[L]=3
	[M]=4
)


# all available GSML2 ports, each one represent physically connected camera.
# muxes a, b, c, d, e, f  usually single link cameras
# muxes g, h, i, i, k, l is aggregated link cameras, 'a' coupled to 'g'
mux_list=${mux_param:-'a b c d e f g h i j k l A E G H I J K L M'}

# Find media device.
# For case with usb camera plugged in during the boot,
# usb media controller will occupy index 0
mdev=$(${v4l2_util} --list-devices | grep -A1 ipu6 | grep media)
[[ -z "${mdev}" ]] && exit 0

out() {
	echo -n "${@}    " >&2
	"${@}"
	echo "        RET=$?" >&2
}

media_ctl_cmd="${media_util} -d ${mdev}"
# media-ctl -r # <- this can be used to clean-up all bindings from media controller
# cache media-ctl output
dot=$($media_ctl_cmd --print-dot)

# IMX390/ISX031/AR0234 MUX. Can be {a, b, c, d, e, f} + Aggregated {g, h, i, j, l, k}.
for mux in $mux_list; do

        # skip for non-existing serializer
	e=$(sen_node ${mux})
	if [ -z "${e}" ]; then
		continue;
	fi

	[[ $quiet -eq 0 ]] && echo "Bind max9x mux ${mux} .. " >&2

	csi2="CSI-2 ${media_mux_csi2_link[${mux}]}"
	csi2_be_soc="CSI2 BE SOC ${media_mux_csi2_link[${mux}]}"
	be_soc_cap="BE SOC ${media_mux_capture_link[${mux}]}capture"
	cap_pad="${media_mux_capture_pad[${mux}]}"
	vc_id="${des_mux_to_index[${mux}]}"

	# out $media_ctl_cmd -l "${des_node_src[${mux}]} -> \"Intel IPU6 ${csi2}\":0[1]"
	out $media_ctl_cmd -l "$(des_src_pad ${mux}) -> \"Intel IPU6 ${csi2}\":0[1]"
	out $media_ctl_cmd -l "\"Intel IPU6 ${csi2}\":1 -> \"Intel IPU6 ${csi2_be_soc}\":0[1]"
	out $media_ctl_cmd -l "\"Intel IPU6 ${csi2_be_soc}\":$((${cap_pad}+1)) -> \"Intel IPU6 ${be_soc_cap} $((${cap_pad}+0))\":0[5]"

	# Set vc_id by using route : subdev entity '['pad-number '/' stream-number '->' pad-number '/' stream-number '[' route-flags ']' ']' ;
	# example:  /usr/bin/media-ctl -d /dev/media0 -R "Intel IPU6 CSI2 BE SOC 4"[3/0->0]
	#out $media_ctl_cmd -R "\"$(ser_node ${mux})\"[0/$((${vc_id}+0))->2)[1]]"
	#out $media_ctl_cmd -R "\"$(des_node ${mux})\"[4/$((${vc_id}+0))->0)[1]]"
	#out $media_ctl_cmd -R "\"Intel IPU6 ${csi2}\"[0/$((${vc_id}+0))->0)[1]]"
	#out $media_ctl_cmd -R "\"Intel IPU6 ${csi2_be_soc}\"[0/$((${vc_id}+0))->$((${cap_pad}+1))[1]]"

	out $media_ctl_cmd -V "$(sen_src_pad ${mux}) ${fmt}"
	out $media_ctl_cmd -V "$(ser_sink_pad ${mux}) ${fmt}"
	out $media_ctl_cmd -V "$(ser_src_pad ${mux}) ${fmt}"
	out $media_ctl_cmd -V "$(des_sink_pad ${mux}) ${fmt}"
	out $media_ctl_cmd -V "$(des_src_pad ${mux}) ${fmt}"

	# out $media_ctl_cmd -V "${ser_node_sink[${mux}]} ${fmt}"
	# out $media_ctl_cmd -V "${ser_node_src[${mux}]} ${fmt}"
	# out $media_ctl_cmd -V "${des_node_sink[${mux}]} ${fmt}"
	# out $media_ctl_cmd -V "${des_node_src[${mux}]} ${fmt}"

	out $media_ctl_cmd -V "\"Intel IPU6 ${csi2}\":0 ${fmt}"
	out $media_ctl_cmd -V "\"Intel IPU6 ${csi2}\":1 ${fmt}"
	out $media_ctl_cmd -V "\"Intel IPU6 ${csi2_be_soc}\":0 ${fmt}"
	out $media_ctl_cmd -V "\"Intel IPU6 ${be_soc_cap} $((${cap_pad}+0))\":0 ${fmt}"

	cap_dev=$($media_ctl_cmd -e "Intel IPU6 ${be_soc_cap} $((${cap_pad}+0))")
	out ln -snf ${cap_dev} /dev/video-${sensor}-${mux}

done
