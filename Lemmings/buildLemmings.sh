#!/bin/bash
if [[ "$1" == "build" ]]; then
	ndk-build
	ant debug
	adb install -r bin/ImageTargets-debug.apk
elif [[ "$1" == "clean" ]]; then
	ndk-build clean
	ant clean
fi