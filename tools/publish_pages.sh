#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Build the GitHub Pages site for release VERSION (tools/make_pages.py: landing page, emulator, web
# installer, firmware download) and push it to the gh-pages branch of the public repository, replacing what
# was there.
#   tools/publish_pages.sh 0.0.60          (after ./build.sh --release 0.0.60)
# The remote is $PAGES_REMOTE (default: github), the repository named by REPO in tools/make_pages.py.
set -e
cd "$(dirname "$0")/.."
V="${1:?usage: tools/publish_pages.sh VERSION}"
PKG="build/zen-1-$V.fwsc"
[ -f "$PKG" ] || { echo "no $PKG: run ./build.sh --release $V first"; exit 1; }
if grep -q '^REPO = .*OWNER' tools/make_pages.py; then
    echo "set REPO in tools/make_pages.py to the public repository first"; exit 1
fi
FM_VERSION="$V" sh web/emu/build.sh
SITE_SHA="$(git rev-parse --short public 2>/dev/null || true)" python3 tools/make_pages.py "$PKG" "$V" build/pages
REMOTE="$(git remote get-url "${PAGES_REMOTE:-github}")"
SHA="$(git rev-parse --short public 2>/dev/null || git rev-parse --short HEAD)"
cd build/pages
rm -rf .git
git init -q -b gh-pages
git add -A
N="24679932+aceyprime@users.noreply.github.com"     # (AceyPrime and the GitHub no-reply address, as publish_public.sh)
GIT_AUTHOR_NAME=AceyPrime GIT_COMMITTER_NAME=AceyPrime GIT_AUTHOR_EMAIL="$N" GIT_COMMITTER_EMAIL="$N" git commit -q -m "Site for ZEN-1 $V (from $SHA)"
git push -q -f "$REMOTE" gh-pages
rm -rf .git
# ask GitHub Pages to build it now: a push alone has left a first deploy stuck on "building"
REPO="$(printf %s "$REMOTE" | sed -E 's#(git@github.com:|https://github.com/)##; s#\.git$##')"
gh api -X POST "repos/$REPO/pages/builds" >/dev/null 2>&1 || echo "(could not ask GitHub Pages to build: check the repository's Pages settings)"
echo "pushed the site for ZEN-1 $V to gh-pages"
