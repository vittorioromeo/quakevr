"""make_fixtures.py <cache dir> -- the Slipseer parser's test pages (vr_slipseer.cpp, maps_slipseer_test), trimmed from
pages the game's own polite fetch saved in <base>/cache/slipseer/ (no fetching here).

Each fixture keeps only the markup the parser reads: a listing page's first maps (their structItem blocks) and its
"next" link; a map page's og:image, breadcrumbs, title, Download button, description and JSON-LD block. The text is
the site's and its authors' (Slipseer, slipseer.com: "Drain" by Snarg, "Violacea" and "Caliber" by their authors),
kept as test data only.

Usage: python Misc/quakevr/slipseer_fixtures/make_fixtures.py <base>/cache/slipseer
"""

import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))


def page_for(cache, url):
    for name in os.listdir(cache):
        if not name.endswith(".meta"):
            continue
        meta = open(os.path.join(cache, name), encoding="utf-8").read()
        if "url\t" + url + "\n" in meta:
            return open(os.path.join(cache, name[:-5]), encoding="utf-8", errors="replace").read()
    raise SystemExit("not in the cache: " + url)


def element(html, start):
    """The element starting at `start` (its tag name's open/close pairs counted)."""
    name = re.match(r"<([a-zA-Z0-9]+)", html[start:]).group(1).lower()
    depth = 0
    for m in re.finditer(r"<(/?)" + name + r"\b[^>]*>", html[start:], re.I):
        depth += -1 if m.group(1) else 1
        if depth == 0:
            return html[start:start + m.end()]
    raise SystemExit("unclosed <%s>" % name)


def first(html, pattern):
    m = re.search(pattern, html, re.S)
    if not m:
        raise SystemExit("missing: " + pattern)
    return m


def listing(cache, keep):
    html = page_for(cache, "https://www.slipseer.com/index.php?resources/categories/maps.1/")
    out = ["<!-- trimmed from https://www.slipseer.com/index.php?resources/categories/maps.1/ (Slipseer) -->", "<html><body>"]
    for m in re.finditer(r'<div class="structItem structItem--resource ', html):
        block = element(html, m.start())
        title = re.search(r'data-tp-primary="on">([^<]*)<', block).group(1)
        if title in keep:
            out.append(block)
    out.append(first(html, r'<a href="[^"]*" class="pageNav-jump pageNav-jump--next">').group(0) + "Next</a>")
    out.append("</body></html>")
    return "\n".join(out)


def detail(cache, url):
    html = page_for(cache, url)
    out = ["<!-- trimmed from %s (Slipseer) -->" % url, "<html><head>"]
    out.append(first(html, r'<meta property="og:image"[^>]*>').group(0))
    out.append("</head><body>")
    out.append(element(html, first(html, r'<ul class="p-breadcrumbs ').start()))
    out.append(element(html, first(html, r'<h1 class="p-title-value').start()))
    out.append(element(html, first(html, r'<a href="[^"]*" class="button button--icon button--icon--download').start()))
    out.append(element(html, first(html, r'<article class="resourceBody-main').start()))
    out.append(first(html, r'<script type="application/ld\+json">.*?</script>').group(0))
    out.append("</body></html>")
    return "\n".join(out)


def main():
    cache = sys.argv[1]
    files = {
        "listing.html": listing(cache, {"Drain", "Violacea", "Caliber"}),
        "drain.html": detail(cache, "https://www.slipseer.com/index.php?resources/drain.672/"),
        "violacea.html": detail(cache, "https://www.slipseer.com/index.php?resources/violacea.655/"),
    }
    for name, text in files.items():
        with open(os.path.join(HERE, name), "w", encoding="utf-8", newline="\n") as f:
            f.write(text + "\n")
        print(name, len(text))


if __name__ == "__main__":
    main()
