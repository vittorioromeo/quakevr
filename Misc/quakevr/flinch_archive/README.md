# Flinch test suites (archived)

The physical flinch (vr_flinch 1/2) was removed on 2026-10-19; see docs/vr-port/FLINCH_RETROSPECTIVE.md.
These are the headless suites the flinch workers used (fl3: shotgun -> super shotgun -> blow sequence; fl4: 8-run
cases + side-view frame strips; fl6/fl7: 6-7 cases x 8 runs per class). They drive `vr_flinch_test*` commands that no
longer exist on next: to reuse them, check out a commit before the removal (e.g. the flinch7 merge) or port the
test commands. Kept as reference only; not part of the build or of eval.sh.
