#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The site (build/site, from tools/make_pages.py) on this Mac, for installing a new build from Chrome or Edge:
#   tools/serve_site.sh          start it in the background if it isn't running, and say where it is
#   tools/serve_site.sh stop     stop it
# It serves whatever build/site holds, so a new build needs only a reload of the page, not a restart.
PORT=8765
cd "$(dirname "$0")/.." || exit 1
SITE="$PWD/build/site"
LOG="$PWD/build/serve_site.log"
running() { curl -s -o /dev/null --max-time 2 "http://127.0.0.1:$PORT/" ; }
pids() { lsof -ti "tcp:$PORT" -sTCP:LISTEN 2>/dev/null; }

if [ "$1" = "stop" ]; then
    p=$(pids)
    if [ -n "$p" ]; then
        kill $p && echo "site server stopped"
    else
        echo "site server was not running"
    fi
    exit 0
fi

if [ ! -f "$SITE/index.html" ]; then
    echo "no build/site yet: build a release and run tools/make_pages.py first" >&2
    exit 1
fi
if running; then
    echo "site server already running"
else
    # (--directory is looked up by name on every request: a rebuild that replaces build/site needs no restart)
    nohup python3 -m http.server "$PORT" --bind 127.0.0.1 --directory "$SITE" >"$LOG" 2>&1 &
    i=0
    while ! running && [ $i -lt 20 ]; do
        sleep 0.25
        i=$((i + 1))
    done
    if running; then
        echo "site server started"
    else
        echo "site server did not start: see $LOG" >&2
        exit 1
    fi
fi
echo "  install: http://127.0.0.1:$PORT/install/   (Chrome or Edge, the FM-1 on USB)"
[ -f "$SITE/install/testing/index.html" ] && echo "  testing: http://127.0.0.1:$PORT/install/testing/   (the testing build: REC shows the speed readout)"
echo "  site:    http://127.0.0.1:$PORT/"
