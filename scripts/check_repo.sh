#!/usr/bin/env bash
# OpenMotherVR-Provenance: clean
# Repository policy checks for CI (AGENTS.md, "Content rules").
#
#   scripts/check_repo.sh files
#       No game files, other mods' files or archive/ are tracked.
#   scripts/check_repo.sh commits <base> <head>
#       Every non-merge commit in base..head has a Conventional Commits 1.0.0
#       subject with the project's types and scopes, at most 72 characters,
#       no trailing period.
set -euo pipefail

fail=0
err() { echo "::error::$*"; fail=1; }

check_files() {
    local bad
    bad=$(git ls-files | grep -iE '(^|/)(ai\.exe|[^/]*\.pak|[^/]*shadercache[^/]*)$|^archive/' || true)
    if [[ -n "$bad" ]]; then
        err "game or other mods' files are tracked:"
        echo "$bad"
    fi
    # res/openvr_api.dll is Valve's loader, embedded on purpose.
    bad=$(git ls-files '*.dll' '*.exe' | grep -vx 'res/openvr_api.dll' || true)
    if [[ -n "$bad" ]]; then
        err "unexpected binaries are tracked:"
        echo "$bad"
    fi
}

check_commits() {
    local base=$1 head=$2
    local types='feat|fix|perf|refactor|docs|build|ci|test|chore|revert'
    # The scopes AGENTS.md lists ("Content rules"); keep the two in step.
    local scopes='hook|stereo|runtime|input|proxy|config|render|integration|game|audio|ui|scripts|util|core|platform|loader|tests|diag|cleanroom'
    local re="^($types)(\(($scopes)\))?!?: [^ ].*[^. ]$"
    local sha subject commits
    # Listed first, not read from a process substitution: that one's exit
    # status is ignored even under set -e, so an unreachable base (a
    # force-push) would pass with no commit checked.
    if ! commits=$(git rev-list --no-merges "$base..$head"); then
        err "cannot list the commits in $base..$head"
        return
    fi
    while read -r sha; do
        [[ -n "$sha" ]] || continue
        subject=$(git log -1 --format=%s "$sha")
        if ! [[ "$subject" =~ $re ]]; then
            err "${sha:0:7}: not a Conventional Commits subject with a listed type and scope: $subject"
        fi
        if (( ${#subject} > 72 )); then
            err "${sha:0:7}: subject is ${#subject} characters (max 72): $subject"
        fi
    done <<< "$commits"
}

case "${1:-}" in
    files)   check_files ;;
    commits) check_commits "$2" "$3" ;;
    *)       echo "usage: $0 files | commits <base> <head>" >&2; exit 2 ;;
esac
exit $fail
