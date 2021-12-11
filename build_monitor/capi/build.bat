@echo off

setlocal

pushd %~dp0

cargo build --lib --release

if not exist include (
	mkdir include
)

echo #pragma once > include\build_monitor.h
echo. >> include\build_monitor.h
cbindgen >> include\build_monitor.h

popd