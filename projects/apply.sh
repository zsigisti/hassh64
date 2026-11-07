#!/bin/sh
# Apply a hassh64 port to a checkout of its project:
#
#   projects/apply.sh <project> <path-to-checkout>
#
# Runs `git am` with projects/<project>/<project>.patch (made against the
# upstream commit named in that project's README), then writes the current
# hassh.h from this repository, with the MIT notice on top, to the place the
# project vendors it and folds it into the last commit. Patches leave the
# header out, so every port picks up the header of the hassh64 you have.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
root=$(dirname "$here")
name=${1:?usage: apply.sh <project> <path-to-checkout>}
dest=${2:?usage: apply.sh <project> <path-to-checkout>}
patch=$here/$name/$name.patch
[ -f "$patch" ] || { echo "no port named $name" >&2; exit 1; }

case $name in
    wren)    header=src/vm/hassh.h ;;
    xcc)     header=src/util/hassh.h ;;
    melonDS) header=src/hassh/hassh.h ;;
    *)       header= ;;   # e.g. miku-os uses the Rust crate from crates.io
esac

cd "$dest"
git am --3way --keep-cr "$patch"
if [ -n "$header" ]; then
    mkdir -p "$(dirname "$header")"
    version=$(sed -n 's/^#define HASSH_VERSION "\(.*\)"/\1/p' "$root/hassh.h")
    {
        echo "/*"
        echo " * Vendored from https://github.com/zsigisti/hassh64 (v$version, hassh.h unmodified"
        echo " * below this notice)."
        echo " *"
        sed 's/^/ * /; s/ *$//' "$root/LICENSE"
        echo " */"
        cat "$root/hassh.h"
    } > "$header"
    git add "$header"
    git commit -q --amend --no-edit
fi
echo "applied $name (hassh64 header: ${header:-none, uses the crate})"
