#!/usr/bin/env python3
"""Grade all iteration-2 runs."""
import json, os, re
from pathlib import Path

BASE = Path("/home/mistake/workspace/chipwhisper/packages/stm32-firmware-dev/workspace/iteration-2")
EVALS_JSON = Path("/home/mistake/workspace/chipwhisper/packages/stm32-firmware-dev/evals/evals.json")
evals_data = json.loads(EVALS_JSON.read_text())

COMPOUND_SPLIT = re.compile(r'\|(?=file_exists:|file_contains_regex:|file_not_contains_regex:|file_contains:|agent_behavior)')

# Manual grading for agent_behavior assertions
# Based on agent summaries from the task notifications
AGENT_BEHAVIOR_GRADES = {
    ("eval-1", "with_skill"): {
        "phase_minus_one_compat_check": (True, "called embedded_docs_list_chips + list_supported_chips"),
        "mcp_i2c_pin_query": (True, "queried embedded-docs for I2C1 pin mapping"),
        "sim_verification_attempted": (True, "started QEMU sim, verified vector table"),
    },
    ("eval-1", "without_skill"): {
        "phase_minus_one_compat_check": (False, "no MCP compatibility check performed"),
        "mcp_i2c_pin_query": (False, "no embedded-docs MCP queries"),
        "sim_verification_attempted": (False, "compiled but did not run QEMU simulation"),
    },
    ("eval-2", "with_skill"): {
        "phase_minus_one_compat_check": (True, "dual MCP check with F103/F100 comparison table"),
        "mcp_multi_peripheral_query": (True, "queried embedded-docs for I2C1+SPI1 pin mapping/registers"),
        "sim_gdb_verification": (True, "QEMU sim verified vector table, caught 8KB RAM issue, GDB register checks"),
    },
    ("eval-2", "without_skill"): {
        "phase_minus_one_compat_check": (False, "no MCP dual check"),
        "mcp_multi_peripheral_query": (False, "no embedded-docs queries"),
        "sim_gdb_verification": (True, "QEMU runtime test with qemu_test.sh, static analysis, no GDB register checks"),
    },
    ("eval-3", "with_skill"): {
        "report_docs_unsupported": (True, "clearly reported F407 not in embedded-docs (F103 only)"),
        "report_sim_supported": (True, "reported netduinoplus2 QEMU machine available"),
        "sim_verification_attempted": (True, "QEMU started, found HSIRDY bug, GDB+monitor verification"),
    },
    ("eval-3", "without_skill"): {
        "report_docs_unsupported": (False, "no docs availability discussion"),
        "report_sim_supported": (False, "no sim availability discussion"),
        "sim_verification_attempted": (False, "no simulation performed"),
    },
    ("eval-4", "with_skill"): {
        "dual_mcp_check_both_called": (True, "called BOTH embedded_docs_list_chips AND list_supported_chips"),
        "report_manual_only_quadrant": (True, "reported MANUAL ONLY quadrant with compatibility matrix"),
        "clear_warning_to_user": (True, "warned: no MCP docs, no QEMU, code untested until hardware"),
        "suggests_adding_chip_support": (True, "suggested adding SVD + QEMU machine for future support"),
    },
    ("eval-4", "without_skill"): {
        "dual_mcp_check_both_called": (False, "no MCP calls at all"),
        "report_manual_only_quadrant": (False, "no compatibility matrix reported"),
        "clear_warning_to_user": (False, "no warning about docs/sim limitations"),
        "suggests_adding_chip_support": (False, "no suggestion to add chip support"),
    },
}

def split_compound(check_str):
    return [p.strip() for p in COMPOUND_SPLIT.split(check_str)]

def glob_files(outdir):
    files = []
    for root, dirs, fnames in os.walk(outdir):
        for f in fnames:
            if not f.endswith(('.o','.elf','.hex','.bin','.map')):
                files.append(os.path.join(root, f))
    return files

def find_file(outdir, path_pattern):
    """Find a file by name pattern (e.g., 'src/i2c_bus.c' or '*.ld')."""
    if '*' in path_pattern or '?' in path_pattern:
        import fnmatch
        for root, dirs, fnames in os.walk(outdir):
            for f in fnames:
                if fnmatch.fnmatch(f, path_pattern):
                    return os.path.join(root, f)
        return None
    else:
        fname = os.path.basename(path_pattern)
        for root, dirs, fnames in os.walk(outdir):
            if fname in fnames:
                return os.path.join(root, fname)
        return None

def file_exists_pattern(outdir, pattern):
    return find_file(outdir, pattern) is not None

def file_contains_in_file(outdir, file_pattern, content_pattern):
    """Check if a specific file contains content pattern."""
    fpath = find_file(outdir, file_pattern)
    if not fpath:
        return False
    try:
        return content_pattern in Path(fpath).read_text(errors='ignore')
    except:
        return False

def file_contains_any(outdir, pattern):
    for f in glob_files(outdir):
        try:
            if pattern in Path(f).read_text(errors='ignore'):
                return True
        except:
            pass
    return False

def file_contains_regex(outdir, pattern):
    for f in glob_files(outdir):
        try:
            if re.search(pattern, Path(f).read_text(errors='ignore'), re.IGNORECASE):
                return True
        except:
            pass
    return False

def file_not_contains_regex(outdir, pattern):
    for f in glob_files(outdir):
        try:
            if re.search(pattern, Path(f).read_text(errors='ignore'), re.IGNORECASE):
                return False
        except:
            pass
    return True

def evaluate_one(outdir, check_str, eval_dir_name, run_type):
    if check_str.startswith("file_exists:"):
        pat = check_str[len("file_exists:"):].strip()
        return file_exists_pattern(outdir, pat), f"exists:{pat}"
    elif check_str.startswith("file_contains_regex:"):
        pat = check_str[len("file_contains_regex:"):].strip()
        return file_contains_regex(outdir, pat), f"regex:{pat[:80]}"
    elif check_str.startswith("file_not_contains_regex:"):
        pat = check_str[len("file_not_contains_regex:"):].strip()
        return file_not_contains_regex(outdir, pat), f"no_regex:{pat[:60]}"
    elif check_str.startswith("file_contains:"):
        rest = check_str[len("file_contains:"):].strip()
        # Two formats: "file_contains:pattern" (search all files) or "file_contains:filename:pattern" (specific file)
        # Check if it looks like "filename:pattern" with a known source file extension
        if ':' in rest:
            parts = rest.split(':', 1)
            file_part, content_part = parts[0], parts[1]
            # If file_part looks like a filename (has .c, .h, .s, etc), treat as file-specific search
            if '.' in file_part and any(ext in file_part for ext in ['.c', '.h', '.s', '.ld']):
                return file_contains_in_file(outdir, file_part, content_part), f"file_contains:{file_part}:{content_part[:40]}"
        # Fallback: search all files
        return file_contains_any(outdir, rest), f"contains_any:'{rest[:60]}'"
    elif check_str == "agent_behavior":
        return True, "agent_behavior"  # Placeholder, overridden below
    else:
        return False, f"unknown:{check_str[:60]}"

def evaluate_check(outdir, check_str, eval_dir_name, run_type):
    parts = split_compound(check_str)
    if len(parts) == 1:
        p, ev = evaluate_one(outdir, parts[0], eval_dir_name, run_type)
        return p, ev

    all_passed = True
    any_passed = False
    evidences = []
    for part in parts:
        p, ev = evaluate_one(outdir, part, eval_dir_name, run_type)
        evidences.append(f"{ev}={'Y' if p else 'N'}")
        if not p:
            all_passed = False
        else:
            any_passed = True

    first_type = parts[0].split(":")[0]
    passed = all_passed if first_type == "file_exists" else any_passed
    return passed, "; ".join(evidences)

def grade_run(eval_id, run_type):
    eval_dir = next((d for d in BASE.iterdir() if d.is_dir() and d.name.startswith(f"eval-{eval_id}-")), None)
    if not eval_dir:
        return []
    outdir = eval_dir / run_type / "outputs"
    if not outdir.exists():
        return []
    eval_def = next((e for e in evals_data["evals"] if e["id"] == eval_id), None)
    if not eval_def:
        return []

    eval_key = (f"eval-{eval_id}", run_type)
    behavior_grades = AGENT_BEHAVIOR_GRADES.get(eval_key, {})

    results = []
    for a in eval_def["assertions"]:
        passed, evidence = evaluate_check(outdir, a["check"], eval_dir.name, run_type)
        # Override agent_behavior with manual grades
        if a["check"] == "agent_behavior" and a["id"] in behavior_grades:
            passed, evidence = behavior_grades[a["id"]]
        results.append({"assertion_id": a["id"], "description": a["description"], "passed": passed, "evidence": evidence})
    return results

# Main
for eval_def in evals_data["evals"]:
    eid = eval_def["id"]
    print(f"\n{'='*60}\nEval {eid}: {eval_def['eval_name']}")
    for run_type in ["with_skill", "without_skill"]:
        print(f"\n  --- {run_type} ---")
        results = grade_run(eid, run_type)
        if not results:
            print("  SKIPPED")
            continue
        n_pass = sum(1 for r in results if r["passed"])
        for r in results:
            print(f"  [{'PASS' if r['passed'] else 'FAIL'}] {r['assertion_id']}: {r['evidence'][:140]}")
        eval_dir = next(d for d in BASE.iterdir() if d.is_dir() and d.name.startswith(f"eval-{eid}-"))
        grading = {
            "expectations": [{"text": r["description"], "passed": r["passed"], "evidence": r["evidence"]} for r in results],
            "summary": {"passed": n_pass, "failed": len(results) - n_pass, "total": len(results)}
        }
        (eval_dir / run_type / "grading.json").write_text(json.dumps(grading, indent=2, ensure_ascii=False))
        print(f"  -> {n_pass}/{len(results)} passed")
print("\nDone!")
