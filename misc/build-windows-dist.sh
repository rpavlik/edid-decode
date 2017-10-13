#!/bin/bash -e

# Intended to be used to build a Windows-native version on Windows under MSYS/MinGW-w64.
(
cd $(dirname $0)/..
# we're now in the root: parent of the script dir.

OUTDIR=edid-decode-built

echo "Rebuild the app"
echo
make clean
make

echo "Creating output directory $OUTDIR and copying files"
rm -rf $OUTDIR
mkdir -p $OUTDIR
export FULL_OUTDIR="$(cd ${OUTDIR} && pwd)"
echo "  - full path: ${FULL_OUTDIR}"
echo
cp edid-decode.{c,exe} misc/edid-txt.cmd $OUTDIR

echo "Rebuild the manpage HTML"
echo
groff -mandoc edid-decode.1 -T html > $OUTDIR/edid-decode.html

echo "Build the readme HTML"
echo
if [ "x$MARKDOWN" != "x" ] && which markdown > /dev/null; then
    markdown README.md | unix2dos > $OUTDIR/README.html
else
    # "Format" readme by appending markdeep footer
    cat README.md misc/markdeep-footer > $OUTDIR/README.html
    cp misc/markdeep.min.js misc/markdeep-license.txt $OUTDIR

fi

echo "Update the data text files"
echo
# This is only useful really if you have the upstream as origin and the fork as some other remote name.
git remote show origin | unix2dos > $OUTDIR/upstream.txt
git log origin/master^1..HEAD | unix2dos > $OUTDIR/revision.txt

echo "Installing in MinGW"
install -m 755 edid-decode.exe -t /mingw64/bin
)