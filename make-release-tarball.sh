# Gets the version in form X.Y.Z, or X.Y.Z-NN-gGITSHA, where NN is the number of commits since
# last tagged git sha.
VERSION=`git describe --tags`

# Create a gzipped tarball of the current folder (except this script), and use sed filename transform to ensure a subfolder
# is present within the tarball, as was the case with past releases of partclone-utils and will also prevent tarbombs.
tar cvzf /tmp/partclone-utils-${VERSION}.tar.gz --exclude=make-release-tarball.sh --transform "s,^,partclone-utils-${VERSION}/," .
