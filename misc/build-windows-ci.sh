#!/bin/bash -e

# Intended to be used to build a Windows-native version on some Linux CI
(
cd $(dirname $0)/..
# we're now in the root: parent of the script dir.

OUTDIR=edid-decode-built

echo "Rebuild the app"
echo
make clean "$@"
make "$@"

echo "Creating output directory $OUTDIR and copying files"
rm -rf $OUTDIR
mkdir -p $OUTDIR
export FULL_OUTDIR="$(cd ${OUTDIR} && pwd)"
echo "  - full path: ${FULL_OUTDIR}"
echo
cp edid-decode.c $OUTDIR
cp edid-decode $OUTDIR/edid-decode.exe

cat misc/edid-txt.cmd | todos > $OUTDIR/edid-decode.html

echo "Rebuild the manpage HTML"
echo
groff -mandoc edid-decode.1 -T html > $OUTDIR/edid-decode.html

echo "Build the readme HTML"
echo
if [ "x$MARKDOWN" != "x" ] && which markdown > /dev/null; then
    markdown README.md > $OUTDIR/README.html
else
    # "Format" readme by appending markdeep footer
    cat README.md misc/markdeep-footer > $OUTDIR/README.html
    cp misc/markdeep.min.js $OUTDIR
    cat misc/markdeep-license.txt | todos > $OUTDIR/markdeep-license.txt
fi

echo "Update the version text file"
echo
git describe --all | todos > $OUTDIR/version.txt

echo "Contents of output directory:"
ls -la $OUTDIR
)