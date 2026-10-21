#!/usr/bin/env bash
# OpenMotherVR-Provenance: clean
# Repository policy checks for CI (AGENTS.md, "Content rules").
#
#   scripts/check_repo.sh files
#       No game files, other mods' files or archive/ are tracked.
#   scripts/check_repo.sh commits [--warn] <base> <head>
#       Every non-merge commit in base..head has a Conventional Commits 1.0.0
#       subject with the project's types and scopes, at most 72 characters,
#       no trailing period. With --warn a bad subject is a warning: CI uses
#       it for pushes, whose commits are already in master's history and
#       are never rewritten, so a failure there could never be fixed.
#   scripts/check_repo.sh subject <message file>
#       The same rule for one commit message's first line: the commit-msg
#       hook (scripts/hooks/commit-msg), which stops a bad subject before
#       the commit exists.
set -euo pipefail

fail=0
err() { echo "::error::$*"; fail=1; }
warn_only=0
report() { if ((warn_only)); then echo "::warning::$*"; else err "$*"; fi; }

types='feat|fix|perf|refactor|docs|build|ci|test|chore|revert'
# The scopes AGENTS.md lists ("Content rules"); keep the two in step.
scopes='hook|stereo|runtime|input|proxy|config|render|integration|game|audio|ui|scripts|util|core|platform|loader|tests|diag|cleanroom'
subject_re="^($types)(\(($scopes)\))?!?: [^ ].*[^. ]$"

# One subject against the rules; `what` names it in the report.
check_subject() {
    local what=$1 subject=$2
    if ! [[ "$subject" =~ $subject_re ]]; then
        report "$what: not a Conventional Commits subject with a listed type and scope: $subject"
    fi
    if (( ${#subject} > 72 )); then
        report "$what: subject is ${#subject} characters (max 72): $subject"
    fi
}

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
    local sha commits
    # Listed first, not read from a process substitution: that one's exit
    # status is ignored even under set -e, so an unreachable base (a
    # force-push) would pass with no commit checked.
    if ! commits=$(git rev-list --no-merges "$base..$head"); then
        err "cannot list the commits in $base..$head"
        return
    fi
    while read -r sha; do
        [[ -n "$sha" ]] || continue
        check_subject "${sha:0:7}" "$(git log -1 --format=%s "$sha")"
    done <<< "$commits"
}

case "${1:-}" in
    files)   check_files ;;
    commits)
        if [[ "${2:-}" == --warn ]]; then warn_only=1; shift; fi
        check_commits "$2" "$3" ;;
    subject)
        # The first line of the message, as git will store it (comment
        # lines and a leading blank line do not count).
        check_subject "commit message" "$(grep -v '^#' "$2" | sed '/./,$!d' | head -n 1)" ;;
    *)       echo "usage: $0 files | commits [--warn] <base> <head> | subject <message file>" >&2; exit 2 ;;
esac
exit $fail
