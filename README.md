### Introduction
This repository fixes the error message "cannot verify: Invalid argument" that
imagemount emits when trying to mount an image from the latest version of
partclone.

Specifically, the imagemount of this repository supports the newer image format 0002.

### Testing
```
sudo apt install libtool automake build-essential pkg-config uuid-dev libssl-dev ntfs-3g-dev

git clone https://github.com/Thomas-Tsai/partclone.git
cd partclone
./configure --enable-ntfs
make -sC src -j`nproc` partclone.ntfs
mv src/ntfs.partclone.image{,_v2}
git checkout fdf3c8e0c63cc8f25e8adbc915307fd0bf1cf590 # last commit of partclone that uses v0001
make -sC src -j`nproc` partclone.ntfs
mv src/ntfs.partclone.image{,_v1}

sudo src/partclone.ntfs_v2 -L ~/partclone.log -c -s /dev/sda5 -o ~/ntfs.partclone.image_v2
dd if=/dev/zero of=ntfs.image bs=1k count=1024000
sudo losetup /dev/loop0 ~/ntfs.image
sudo src/partclone.ntfs_v2 -L ~/partclone.log -r -s ~/ntfs.partclone.image_v2 -o /dev/loop0
sudo src/partclone.ntfs_v1 -L ~/partclone.log -c -s /dev/loop0 -o ~/ntfs.partclone.image_v1
sudo losetup -d /dev/loop0

md5sum ~/ntfs.image

sudo modprobe nbd
git clone https://github.com/prekageo/partclone-utils.git
cd partclone-utils
./configure
make -sC src -j`nproc` imagemount

sudo ./src/imagemount -D -d /dev/nbd0 -f ~/ntfs.partclone.image_v1 &
sleep 1
sudo md5sum /dev/nbd0
sudo pkill imagemount

sudo ./src/imagemount -D -d /dev/nbd0 -f ~/ntfs.partclone.image_v2 &
sleep 1
sudo md5sum /dev/nbd0
sudo pkill imagemount
```
