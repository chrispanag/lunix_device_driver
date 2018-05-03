#!/bin/bash

TCP_ENDPOINT=cerberus.cslab.ece.ntua.gr:49152

if [ $# -ne 1 ]; then
	cat <<EOF
Usage: $0 pts_port

Connect to the TCP endpoint $TCP_ENDPOINT
and forward all incoming data to pts_port.
EOF
	exit 1
fi
if ! which socat >/dev/null; then
	cat <<EOF
Could not find the 'socat' utility in the PATH.
You may have to install it using 'apt-get install socat' on Debian,
or the equivalent for your system.
EOF
	exit 1
fi

pts_port=$1

echo Connecting to $TCP_ENDPOINT 1>&2
exec socat -u TCP:$TCP_ENDPOINT $1
