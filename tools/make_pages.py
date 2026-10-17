#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Make the ZEN-1 site for GitHub Pages (REPO below; the site is that repository's Pages):

  index.html                    what ZEN-1 is, what is in it, and the ways in (web/site/index.html)
  guide/index.html              every control, knob, mood and scene in detail (web/site/guide.html)
  img/                          screens (docs/img; the gallery from the host simulator: tools/site_gallery.sh)
  emu/                          ZEN-1 in the browser (build/emu, from web/emu/build.sh)
  install/index.html            the web installer (web/zen_installer.html, with fm1pkg.js, fm1ota.js and
                                the package's metadata inlined; Chrome or Edge, Web MIDI)
  firmware/zen-1-VERSION.fwsc   the package the installer writes; also the download

  tools/make_pages.py build/zen-1-1.0.fwsc 1.0 OUT_DIR

The package must be a release build (./build.sh --release X.Y): its identity, FM-1_6XXYYZZ, is what
the installer checks the download against and what the FM-1 reports after the install. The wasm,
worklet scripts get the version as a query string so a plain reload sees a new build."""
import html
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

SRC = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SRC / "web"))
from make_site import product_of, strip_module  # noqa: E402  (Felucca's: the package format)

REPO = "https://github.com/aceyprime/zen-1"   # the public repository

SITE = SRC / "web" / "site"                     # the landing page and the guide (their text lives there)


def bust(path, version):
    """the scripts and the wasm are fetched by name: give them the version as a query string"""
    v = re.sub(r"[^A-Za-z0-9.-]", "-", version)
    s = path.read_text(encoding="utf-8")
    for name in ("zen1.wasm", "worklet.js", "bench.js", "syx.js", "session.js"):
        s = re.sub(r'(["\'](?:\./)?' + re.escape(name) + r')(["\'])', r"\1?v=" + v + r"\2", s)
    path.write_text(s, encoding="utf-8")


def main(pkg, version, out):
    pkg, out = Path(pkg), Path(out)
    raw = pkg.read_bytes()
    product = product_of(raw)
    if not re.fullmatch(r"FM-1_6\d{6}", product):
        raise SystemExit(f"{pkg}: identity {product!r}: make a release build (./build.sh --release X.Y)")
    if b"FELUCCA-LOADER-1" not in raw:
        raise SystemExit(f"{pkg}: no update loader in it")
    name = f"zen-1-{re.sub(r'[^A-Za-z0-9.-]', '-', version)}.fwsc"
    sha = os.environ.get("SITE_SHA", "")       # (tools/publish_pages.sh: the public branch's commit)
    try:
        sha = sha or subprocess.run(["git", "rev-parse", "--short", "HEAD"], cwd=SRC, capture_output=True, text=True).stdout.strip() or "?"
    except OSError:
        sha = "?"
    def fill(t):
        return t.replace("__VERSION__", html.escape(version)).replace("__PKG__", name).replace("__REPO_SHORT__", re.sub(r"^https?://", "", REPO)) \
                .replace("__REPO__", REPO) \
                .replace("__SHA__", sha)
    if out.exists():
        shutil.rmtree(out)
    for d in ("install", "firmware"):
        (out / d).mkdir(parents=True)
    shutil.copy(pkg, out / "firmware" / name)
    page = fill((SRC / "web" / "zen_installer.html").read_text(encoding="utf-8"))
    lib = strip_module((SRC / "web" / "fm1pkg.js").read_text(encoding="utf-8")) + "\n" + \
        strip_module((SRC / "web" / "fm1ota.js").read_text(encoding="utf-8"))
    meta = json.dumps({"version": version, "product": product, "pkg": "../firmware/" + name})
    for mark in ("/*LIB*/", "/*META*/"):
        if page.count(mark) != 1:
            raise SystemExit(f"zen_installer.html must contain {mark} once")
    (out / "install" / "index.html").write_text(page.replace("/*LIB*/", lib).replace("/*META*/", meta), encoding="utf-8")
    tpkg = os.environ.get("TESTING_PKG", "")
    if tpkg:                                    # (local only: a testing build's own install page, install/testing/)
        traw = Path(tpkg).read_bytes()
        tproduct = product_of(traw)
        if tproduct != "FM-1_600" or b"FELUCCA-LOADER-1" not in traw:
            raise SystemExit(f"{tpkg}: identity {tproduct!r}: not a testing build (./build.sh, no --release)")
        tname = name.replace(".fwsc", "-testing.fwsc")
        shutil.copy(tpkg, out / "firmware" / tname)
        tmeta = json.dumps({"version": version + " testing", "product": tproduct, "pkg": "../../firmware/" + tname})
        tpage = page.replace('href="../"', 'href="../../"').replace(
            '<h1>Install ZEN-1 <span style="color:var(--dim);font-weight:400">禅</span></h1>',
            '<h1>Install ZEN-1 <span style="color:var(--dim);font-weight:400">testing build</span></h1>\n'
            '  <p class="small">The testing build: REC shows the speed readout (F, frames a second). Put the '
            '<a href="../" style="color:inherit">release</a> back when done.</p>')
        (out / "install" / "testing").mkdir()
        (out / "install" / "testing" / "index.html").write_text(tpage.replace("/*LIB*/", lib).replace("/*META*/", tmeta),
                                                                encoding="utf-8")
    shutil.copytree(SRC / "docs" / "img", out / "img")
    (out / "index.html").write_text(fill((SITE / "index.html").read_text(encoding="utf-8")), encoding="utf-8")
    (out / "guide").mkdir()
    (out / "guide" / "index.html").write_text(fill((SITE / "guide.html").read_text(encoding="utf-8")), encoding="utf-8")
    for page, base in ((out / "index.html", out), (out / "guide" / "index.html", out / "guide")):   # (every picture
        for src in re.findall(r'src="([^"]+\.png)"', page.read_text(encoding="utf-8")):            # there)
            if not (base / src).resolve().exists():
                raise SystemExit(f"{page}: no {src}: run tools/site_gallery.sh")
    emu = SRC / "build" / "emu"                 # (the audition bench is a developer's tool: web/bench/, not on the site)
    if not (emu / "zen1.wasm").exists():
        raise SystemExit("no build/emu/zen1.wasm: run web/emu/build.sh (needs Emscripten)")
    (out / "emu").mkdir()
    for f in ("index.html", "worklet.js", "zen1.wasm"):
        shutil.copy(emu / f, out / "emu" / f)
    (out / "emu" / "index.html").write_text(fill((out / "emu" / "index.html").read_text(encoding="utf-8")), encoding="utf-8")
    for f in ("emu/index.html",):
        bust(out / f, version)
    (out / ".nojekyll").write_text("")
    print(f"site: {out}: index.html, install/ ({product}), emu/, firmware/{name} ({len(raw)} B)" +
          (", install/testing/ (FM-1_600)" if tpkg else ""))


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:4])
