# Copyright (c) 2007-2008 Roy Marples <roy@marples.name>
# Released under the 2-clause BSD license.

ifconfig_depend()
{
	program /sbin/ifconfig /bin/ifconfig
	provide interface
}

_up()
{
	ifconfig "${IFACE}" up
}

_down()
{
	ifconfig "${IFACE}" down
}

_exists()
{
	[ -e /sys/class/net/"$IFACE" ]
}

_ifindex()
{
	local index=-1
	local f v
	if [ -e /sys/class/net/"${IFACE}"/ifindex ]; then
		index=$(cat /sys/class/net/"${IFACE}"/ifindex)
	else
		for f in /sys/class/net/*/ifindex ; do
			v=$(cat $f)
			[ $v -gt $index ] && index=$v
		done
		: $(( index += 1 ))
	fi
	echo "${index}"
	return 0
}

_is_wireless()
{
	# Support new sysfs layout
	[ -d /sys/class/net/"${IFACE}"/wireless -o \
		-d /sys/class/net/"${IFACE}"/phy80211 ] && return 0

	[ ! -e /proc/net/wireless ] && return 1
	grep -Eq "^[[:space:]]*${IFACE}:" /proc/net/wireless
}

_set_flag()
{
	ifconfig "${IFACE}" "$1"
}

_get_mac_address()
{
	local mac=$(LC_ALL=C ifconfig "${IFACE}" | \
	sed -n -e 's/.* \(HWaddr\|ether\) \(..:..:..:..:..:..\).*/\2/p')

	case "${mac}" in
		00:00:00:00:00:00);;
		44:44:44:44:44:44);;
		FF:FF:FF:FF:FF:FF);;
		"");;
		*) echo "${mac}"; return 0;;
	esac

	return 1
}

_set_mac_address()
{
	ifconfig "${IFACE}" hw ether "$1"
}

_get_inet_address()
{
	set -- $(LC_ALL=C ifconfig "${IFACE}" |
	sed -n -e 's/.*\(inet addr:\|inet \)\([^ ]*\).*\(Mask:\|netmask \)\([^ ]*\).*/\2 \4/p')
	[ -z "$1" ] && return 1

	echo -n "$1"
	shift
	echo "/$(_netmask2cidr "$1")"
}

_get_inet_addresses()
{
	local iface=${IFACE} i=0
	local addrs="$(_get_inet_address)"

	while true; do
		local IFACE="${iface}:${i}"
		_exists || break
		local addr="$(_get_inet_address)"
		[ -n "${addr}" ] && addrs="${addrs}${addrs:+ }${addr}"
		: $(( i += 1 ))
	done
	echo "${addrs}"
}

_cidr2netmask()
{
	local cidr="$1" netmask="" done=0 i=0 sum=0 cur=128
	local octets= frac=

	local octets=$(( cidr / 8 ))
	local frac=$(( cidr % 8 ))
	while [ ${octets} -gt 0 ]; do
		netmask="${netmask}.255"
		: $(( octets -= 1 ))
		: $(( done += 1 ))
	done

	if [ ${done} -lt 4 ]; then
		while [ ${i} -lt ${frac} ]; do
			: $(( sum += cur ))
			: $(( cur /= 2 ))
			: $(( i += 1 ))
		done
		netmask="${netmask}.${sum}"
		: $(( done += 1 ))

		while [ ${done} -lt 4 ]; do
			netmask="${netmask}.0"
			: $(( done += 1 ))
		done
	fi

	echo "${netmask#.*}"
}

_add_address()
{
	if [ "$1" = "127.0.0.1/8" -a "${IFACE}" = "lo" ]; then
		ifconfig "${IFACE}" "$@" 2>/dev/null
		return 0
	fi

	case "$1" in
		*:*) ifconfig "${IFACE}" inet6 add "$@"; return $?;;
	esac

	# IPv4 is tricky - ifconfig requires an aliased device
	# for multiple addresses
	local iface="${IFACE}"
	if LC_ALL=C ifconfig "${iface}" | grep -Eq '\<inet (addr:)?.*'; then
		# Get the last alias made for the interface and add 1 to it
		i=$(ifconfig | sed '1!G;h;$!d' | grep -m 1 -o "^${iface}:[0-9]*" \
			| sed -n -e 's/'"${iface}"'://p')
		: $(( i = ${i:-0} + 1 ))
		iface="${iface}:${i}"
	fi

	# ifconfig doesn't like CIDR addresses
	local ip="${1%%/*}" cidr="${1##*/}" netmask=
	if [ -n "${cidr}" -a "${cidr}" != "${ip}" ]; then
		netmask="$(_cidr2netmask "${cidr}")"
		shift
		set -- "${ip}" netmask "${netmask}" "$@"
	fi

	local arg= cmd=
	while [ -n "$1" ]; do
		case "$1" in
			brd)
				if [ "$2" = "+" ]; then
					shift
				else
					cmd="${cmd} broadcast"
				fi
				;;
			peer) cmd="${cmd} pointopoint";;
			*) cmd="${cmd} $1";;
		esac
		shift
	done

	ifconfig "${iface}" ${cmd}
}

_add_route()
{
	local inet6= family=

	if [ "$1" = "-A" -o "$1" = "-f" -o "$1" = "-family" ]; then
		family="-A $2"
		shift; shift
	elif [ "$1" = "-4" ]; then
	    family="-A inet"
		shift
	elif [ "$1" = "-6" ]; then
	    family="-A inet6"
		shift
	fi

	if [ -n "${metric}" ]; then
		set -- "$@" metric ${metric}
	fi

	if [ $# -eq 3 ]; then
		set -- "$1" "$2" gw "$3"
	elif [ "$3" = "via" ]; then
		local one=$1 two=$2
		shift; shift; shift
		set -- "${one}" "${two}" gw "$@"
	fi

	case "$@" in
		*:*|default*) [ "$1" = "-net" -o "$1" = "-host" ] && shift;;
	esac

	route ${family} add "$@" dev "${IFACE}"
}

_delete_addresses()
{
	# We don't remove addresses from aliases
	case "${IFACE}" in
		*:*) return 0;;
	esac

	einfo "Removing addresses"
	eindent
	# iproute2 can add many addresses to an iface unlike ifconfig ...
	# iproute2 added addresses cause problems for ifconfig
	# as we delete an address, a new one appears, so we have to
	# keep polling
	while true; do
		local addr=$(_get_inet_address)
		[ -z "${addr}" ] && break

		if [ "${addr}" = "127.0.0.1/8" ]; then
			# Don't delete the loopback address
			[ "${IFACE}" = "lo" -o "${IFACE}" = "lo0" ] && break
		fi
		einfo "${addr}"
		ifconfig "${IFACE}" 0.0.0.0 || break
	done

	# Remove IPv6 addresses
	local addr=
	for addr in $(LC_ALL=C ifconfig "${IFACE}" | \
		sed -n -e 's/^.*\(inet6 addr:\|inet6\) \([^ ]*\) .*\(Scope:[^L]\|scopeid [^<]*<[^l]\).*/\2/p'); do
		if [ "${IFACE}" = "lo" ]; then
			case "${addr}" in
				"::1/128"|"/128") continue;;
			esac
		fi
		einfo "${addr}"
		ifconfig "${IFACE}" inet6 del "${addr}"
	done

	return 0
}

_has_carrier()
{
	return 0
}

_tunnel()
{
	iptunnel "$@"
}

ifconfig_pre_start()
{
	local tunnel=
	eval tunnel=\$iptunnel_${IFVAR}
	if [ -n "${tunnel}" ]; then
		# Set our base metric to 1000
		metric=1000
		ebegin "Creating tunnel ${IFVAR}"
		iptunnel add ${tunnel}
		eend $? || return 1
		_up
	fi

	# MTU support
	local mtu=
	eval mtu=\$mtu_${IFVAR}
	[ -n "${mtu}" ] && ifconfig "${IFACE}" mtu "${mtu}"

	# TX Queue Length support
	local len=
	eval len=\$txqueuelen_${IFVAR}
	[ -n "${len}" ] && ifconfig "${IFACE}" txqueuelen "${len}"

	return 0
}

ifconfig_post_stop()
{
	# Don't delete sit0 as it's a special tunnel
	[ "${IFACE}" = "sit0" ] && return 0

	[ -z "$(iptunnel show "${IFACE}" 2>/dev/null)" ] && return 0

	ebegin "Destroying tunnel ${IFACE}"
	iptunnel del "${IFACE}"
	eend $?
}

# Is the interface administratively/operationally up?
# The 'UP' status in ifconfig/iproute2 is the administrative status
# Operational state is available in iproute2 output as 'state UP', or the
# operstate sysfs variable.
# 0: up
# 1: down
# 2: invalid arguments
is_admin_up()
{
	local iface="$1"
	[ -z "$iface" ] && iface="$IFACE"
	ifconfig "${iface}" | \
	sed -n '1,1{ /flags=.*[<,]UP[,>]/{ q 0 }}; q 1; '
}

is_oper_up()
{
	local iface="$1"
	[ -z "$iface" ] && iface="$IFACE"
	read state </sys/class/net/"${iface}"/operstate
	[ "x$state" = "up" ]
}
