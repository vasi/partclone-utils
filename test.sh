#!/bin/bash
#
# partclone-utils' imagemount test script
#
# Usage: ./test.sh [debug]

set -eu

if [ "$#" -eq 1 ] && [ "$1" == "debug" ]; then
    set -x
fi

LOOP=/dev/loop6
LOOP_IMAGE=/tmp/loop-image
PARTCLONE_IMAGE=/tmp/partclone-image
NBD=/dev/nbd1

reset() {
    sudo losetup -d $LOOP 2> /dev/null || true
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
    ./configure --enable-ntfs --enable-fat --enable-f2fs > /dev/null 2> /dev/null
    make -s -C src partclone.ntfs partclone.fat partclone.f2fs 2> /dev/null
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
    [ x`sudo md5sum $NBD 2> /dev/null | cut -d\  -f1` == x`sudo md5sum $LOOP | cut -d\  -f1` ]
}

check_fat() {
    check_generic
}

check_f2fs() {
    check_generic
}

check_ntfs() {
    local SIZE=`sudo wc -c $LOOP | cut -d\  -f1`
    SIZE=$[ SIZE / 4096 - 1 ]

    local HASH1=`sudo dd bs=4k count=$SIZE if=$NBD 2> /dev/null | md5sum | cut -d\  -f1`
    local HASH2=`sudo dd bs=4k count=$SIZE if=$LOOP 2> /dev/null | md5sum | cut -d\  -f1`
    [ x$HASH1 == x$HASH2 ]
}

go() {
    local FS=$1
    local SIZE=$2

    __go() {
        reset

        dd if=/dev/zero bs=512 count=$SIZE of=$LOOP_IMAGE 2> /dev/null || return 1
        sudo losetup $LOOP $LOOP_IMAGE || return 1
        sudo mkfs.$FS $LOOP > /dev/null 2> /dev/null || return 1

        sudo rm -f $PARTCLONE_IMAGE
        sudo ~/partclone/$VER/src/partclone.$FS -c -s $LOOP -o $PARTCLONE_IMAGE 2> /dev/null || return 1

        sudo src/imagemount -d $NBD -f $PARTCLONE_IMAGE || return 1

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
        echo " ver=$VER fs=$FS size=$SIZE"
    }

    _go v1
    _go v2
}

ERR=1

for VER in v1 v2; do
    for FS in fat f2fs ntfs; do
        [ -f ~/partclone/$VER/src/partclone.$FS ] || install_partclone
    done
done

sudo modprobe nbd

ERR=0

go fat 1000
go fat 999
go fat 1001
go f2fs 210048
go f2fs 210056
go ntfs 4031
go ntfs 4032
go ntfs 4040
