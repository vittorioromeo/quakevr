"""Exercise shared metadata across prop, portal, hand, ragdoll, reload and vanilla fixtures."""
import argparse
import json
from pathlib import Path
import re

import prop_query_test


def run(args):
    original = prop_query_test.fixtures

    def fixtures():
        for name, commands, vrgame in original():
            expanded = []
            for command in commands:
                if command == "vr_prop_query_test":
                    expanded.append("vr_modelmetadata_test")
                expanded.append(command)
            yield name, expanded, vrgame

    prop_query_test.fixtures = fixtures
    args.reference_exe = None
    prop_query_test.run(args)
    results = []
    for name, _, _ in original():
        log = (Path(args.output) / name / "qconsole.log").read_text(errors="replace")
        matches = re.findall(r"modelmetadata: PASS checks=(\d+) loaded=(\d+)", log)
        assert matches and all(int(checks) > 30000 and int(loaded) > 0 for checks, loaded in matches), name
        results.append(dict(fixture=name, sweeps=[dict(checks=int(c), loaded=int(m)) for c, m in matches]))
        print(f"PASS metadata {name}: {len(matches)} sweeps", flush=True)
    (Path(args.output) / "metadata-summary.json").write_text(json.dumps(results, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", required=True, help="Private test base under build-cmake")
    parser.add_argument("--exe", required=True)
    parser.add_argument("--output", required=True)
    run(parser.parse_args())
