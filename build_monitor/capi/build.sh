#!/bin/sh

pushd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")"

cargo build --lib --release

if [ ! -d include ]; then
	mkdir include
fi

echo "#pragma once" > include/build_monitor.h
echo "" >> include/build_monitor.h
cbindgen >> include/build_monitor.h

popd
