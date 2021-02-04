#!/bin/bash
#
# partclone-utils' imagemount test script
#
# Usage: ./test.sh [debug]

set -eu

if [ "$#" -eq 1 ] && [ "$1" == "debug" ]; then
    set -x
fi

RAW_FS_IMAGE=/tmp/raw-fs-image
PARTCLONE_IMAGE=/tmp/partclone-image
NBD=/dev/nbd1
PARTCLONE_MOUNT_POINT=/tmp/mount-partclone-image

reset() {
    mkdir $PARTCLONE_MOUNT_POINT 2>/dev/null || true
    sudo umount $PARTCLONE_MOUNT_POINT 2>/dev/null || true
    sudo nbd-client -d $NBD || true
    sudo pkill imagemount || true
}

on_exit() {
    reset
    exit $ERR
}

trap on_exit EXIT

_install_partclone() {
    local DIR=$1
    local COMMIT=$2

    cd $DIR
    git checkout $COMMIT 2> /dev/null
    autoreconf 2> /dev/null || true
    automake --add-missing 2> /dev/null
    ./configure --enable-extfs --enable-ntfs --enable-fat --enable-f2fs > /dev/null 2> /dev/null
    make -s -C src partclone.extfs partclone.ntfs partclone.fat partclone.f2fs 2> /dev/null
    cd -
}

install_partclone() {
    mkdir -p ~/partclone
    [ -d ~/partclone/v1 ] || git clone https://github.com/Thomas-Tsai/partclone ~/partclone/v1 2> /dev/null
    [ -d ~/partclone/v2 ] || git clone ~/partclone/v1 ~/partclone/v2 2> /dev/null

    _install_partclone ~/partclone/v1 0.2.73
    _install_partclone ~/partclone/v2 master
}

check_generic() {
    [ x`sudo md5sum $NBD 2> /dev/null | cut -d\  -f1` == x`sudo md5sum $RAW_FS_IMAGE | cut -d\  -f1` ]
}

check_fat() {
    check_generic
}

check_f2fs() {
    check_generic
}

check_ntfs() {
    local SIZE=`sudo wc -c $RAW_FS_IMAGE | cut -d\  -f1`
    SIZE=$[ SIZE / 4096 - 1 ]

    local HASH1=`sudo dd bs=4k count=$SIZE if=$NBD 2> /dev/null | md5sum | cut -d\  -f1`
    local HASH2=`sudo dd bs=4k count=$SIZE if=$RAW_FS_IMAGE 2> /dev/null | md5sum | cut -d\  -f1`
    [ x$HASH1 == x$HASH2 ]
}

check_ext4() {
    check_generic
}

mkfs() {
    FS=$1
    ARGS=""
    if [ "z$FS" == "zntfs" ]; then
        ARGS="--force"
    fi
    sudo mkfs.$FS $ARGS $RAW_FS_IMAGE > /dev/null 2> /dev/null
    if [ $? -ne 0 ]; then
        echo "mkfs.$FS reported error" >&2
        return 1
    fi
}

go() {
    local FS=$1
    local PC_FS=$2
    local NUM_BLOCKS=$3

    __go() {
        reset

        dd if=/dev/zero bs=512 count=$NUM_BLOCKS of=$RAW_FS_IMAGE 2> /dev/null || return 1

        mkfs $FS
        if [ $? -ne 0 ]; then
            return 1
        fi

        sudo rm -f $PARTCLONE_IMAGE
        sudo ~/partclone/$VER/src/partclone.$PC_FS --clone --source $RAW_FS_IMAGE --output $PARTCLONE_IMAGE 2> /dev/null 
        if [ $? -ne 0 ]; then
            echo "partclone.$PC_FS reported error" >&2
            return 1
        fi

        sudo src/imagemount -d $NBD -f $PARTCLONE_IMAGE -r
        if [ $? -ne 0 ]; then
            echo "imagemount reported error" >&2
            return 1
        fi

        sudo mount -o ro $NBD $PARTCLONE_MOUNT_POINT
        if [ $? -ne 0 ]; then
            echo "mount reported error" >&2
            return 1
        fi

        sleep 1
        check_$FS || return 1

    }

    _go() {
        VER=$1
        GREEN='\033[0;32m'
        RED='\033[0;31m'
        NC='\033[0m' # No Color
        if __go; then
            echo -e "${GREEN}[OK  ]${NC}"
        else
            echo -e "${RED}[FAIL]${NC}"
            ERR=1
        fi
        echo " ver=$VER fs=$FS num_blocks=$NUM_BLOCKS"
    }

    _go v1
    _go v2
}

ERR=1

for VER in v1 v2; do
    for PC_FS in extfs fat f2fs ntfs; do
        [ -f ~/partclone/$VER/src/partclone.$PC_FS ] || install_partclone
    done
done

sudo modprobe nbd

ERR=0

# Note: Based on number of blocks, mkfs.fat is selecting FAT12 (not FAT16 or FAT32).
go fat  fat   1000
go fat  fat   999
go fat  fat   1001
go f2fs f2fs  210048
go f2fs f2fs  210056
go ntfs ntfs  4031
go ntfs ntfs  4032
go ntfs ntfs  4040
go ext4 extfs 20000
go ext4 extfs 1000
