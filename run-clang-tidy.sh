#!/usr/bin/env bash
#
# clang-tidy-fix.sh — run clang-tidy over the project and (by default) apply its fixes.
#
# What it does:
#   Runs run-clang-tidy across every file in the compilation database, skipping
#   anything under a vendor/ directory. By default it APPLIES clang-tidy's fixes
#   in place, then runs clang-format on ONLY the lines clang-tidy changed (via
#   git clang-format, using the .clang-format found in the repo). It refuses to
#   apply anything unless your git working tree is clean, so the result lands as
#   a reviewable `git diff`.
#
#   The "clean tree" check ignores this script itself, so you can edit and run it
#   in the same commit without stashing.
#
# Usage:
#   clang-tidy-fix.sh [-c|--console-if-dirty] [build-dir]
#
#   build-dir                Directory holding compile_commands.json.
#                            Defaults to out/build/x64-debug.
#   -c, --console-if-dirty   If the tree is dirty, don't error — just print
#                            findings to the console and apply nothing.
#   -h, --help               Show usage.
#
# Behavior summary:
#   tree clean            -> apply fixes, then clang-format the changed lines
#   tree dirty + -c       -> console-only, no fixes, no formatting
#   tree dirty, no -c     -> error and exit
#
# Requirements:
#   Must be run inside a git repository. Needs run-clang-tidy and git-clang-format
#   (both ship with LLVM) on PATH, and a compile_commands.json in the build dir.
#
set -euo pipefail

console_if_dirty=0
build_dir=""

usage() {
    echo "usage: $(basename "$0") [-c|--console-if-dirty] [build-dir]" >&2
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -c|--console-if-dirty) console_if_dirty=1; shift ;;
        -h|--help)             usage; exit 0 ;;
        --)                    shift; break ;;
        -*)                    echo "error: unknown option '$1'" >&2; usage; exit 1 ;;
        *)
            if [[ -n "$build_dir" ]]; then
                echo "error: unexpected argument '$1'" >&2; usage; exit 1
            fi
            build_dir="$1"; shift ;;
    esac
done

build_dir="${build_dir:-out/build/x64-debug}"

if [[ ! -f "$build_dir/compile_commands.json" ]]; then
    echo "error: no compile_commands.json in '$build_dir'" >&2
    usage
    exit 1
fi

if ! git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    echo "error: not inside a git repository" >&2
    exit 1
fi

repo_root="$(git rev-parse --show-toplevel)"
self_rel="$(realpath --relative-to="$repo_root" "$0")"

# match the whole repo (from root, so it works from any subdir), minus this script
clean_pathspec=(':/' ":(top,exclude)$self_rel")

if [[ -z "$(git status --porcelain -- "${clean_pathspec[@]}")" ]]; then
    tree_clean=1
else
    tree_clean=0
fi

source_filter='^(?!.*\/vendor\/).*'

if [[ "$tree_clean" -eq 1 ]]; then
    fix_file="$(mktemp --suffix=.yaml)"
    trap 'rm -f "$fix_file"' EXIT
    run-clang-tidy -p "$build_dir" -quiet \
        -source-filter "$source_filter" \
        -fix -export-fixes "$fix_file"

    # format only the lines clang-tidy just changed, per the repo's .clang-format.
    # --force is needed because clang-tidy leaves those files unstaged; git
    # clang-format then restricts itself to the changed line ranges vs HEAD.
    # exit 1 just means "reformatted something" -- only >1 is a real error.
    rc=0
    git clang-format --force --style=file || rc=$?
    if ((rc > 1)); then
        echo "error: git clang-format failed (exit $rc)" >&2
        exit "$rc"
    fi
elif [[ "$console_if_dirty" -eq 1 ]]; then
    echo "warning: working tree dirty -> running console-only (no fixes)" >&2
    run-clang-tidy -p "$build_dir" -quiet \
        -source-filter "$source_filter"
else
    echo "error: working tree not clean; refusing to apply fixes" >&2
    echo "commit or stash your changes, or pass -c to run console-only" >&2
    exit 1
fi
