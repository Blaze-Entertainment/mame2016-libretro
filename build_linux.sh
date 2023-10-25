#!/bin/bash

SCRIPT_DIR="$(dirname "$(which "$0")")"
SCRIPT_DIR="$(realpath "$SCRIPT_DIR")"

FILTER_DIR="${SCRIPT_DIR}/src/mame"
FILTER_PATH=""
FILTER_FILE=""
FILTER_FILE_DEFAULT="arcade.flt"

NUM_THREADS=""
DEBUG_OPT=""
SYMBOLS_OPT=""
CLEAN_TARGET=""
FORCE_UPDATE_DRIVLIST=0

# Parse arguments
while [[ "$#" -gt 0 ]]
do
	case "$1" in
		-j|--num-threads)
			NUM_THREADS="$2"
			shift
			;;
		-f|--filter-file)
			FILTER_FILE="$(basename "$2")"
			shift
			;;
		-d|--debug-tools)
			DEBUG_OPT="EVERCADE_DEBUG=1"
			;;
		-s|--symbols)
			SYMBOLS_OPT="SYMBOLS=1"
			;;
		-c|--clean)
			CLEAN_TARGET="clean"
			;;
		-u|--update-drivlist)
			FORCE_UPDATE_DRIVLIST=1
			;;
		*)
			echo "Unknown argument passed: $1"
			exit 1
			;;
	esac
	shift
done

# Validate filter file
if [[ ! -z "$FILTER_FILE" ]]
then
	FILTER_PATH="${FILTER_DIR}/${FILTER_FILE}"
else
	FILTER_PATH="${FILTER_DIR}/${FILTER_FILE_DEFAULT}"
fi

if [[ ! -f "$FILTER_PATH" ]]
then
	echo "ERROR: Invalid filter file: $FILTER_PATH"
	exit 2
fi

if [[ $FORCE_UPDATE_DRIVLIST -eq 1 ]]
then
	touch "$FILTER_PATH"
fi

# If NUM_THREADS is unset, use all available
case "$NUM_THREADS" in
	(*[!0-9]*|'')
		NUM_THREADS="$(getconf _NPROCESSORS_ONLN)"
		echo "Using all available threads ($NUM_THREADS)"
		;;
	(*)
		echo "Using $NUM_THREADS threads"
		;;
esac

# Perform build
make -f Makefile.libretro TARGET=mame SUBTARGET=arcade \
	${FILTER_FILE:+ SOURCEFILTER="$FILTER_FILE"} \
	${DEBUG_OPT:+ "$DEBUG_OPT"} \
	${SYMBOLS_OPT:+ "$SYMBOLS_OPT"} \
	NOASM=1 OPENMP=0 \
	-j$NUM_THREADS \
	${CLEAN_TARGET:+ "$CLEAN_TARGET"}
