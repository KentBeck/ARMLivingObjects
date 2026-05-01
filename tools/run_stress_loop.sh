#!/bin/sh
set -eu

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
REPO_ROOT="$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)"

INTERPRETER="${INTERPRETER:-c}"
STRESS_ITERATIONS="${STRESS_ITERATIONS:-25}"
SLEEP_SECS="${SLEEP_SECS:-5}"
LOG_DIR="${LOG_DIR:-$REPO_ROOT/stress_loop_logs}"
BUILD_ON_HEAD_CHANGE="${BUILD_ON_HEAD_CHANGE:-1}"
RUN_SMOKE_ON_HEAD_CHANGE="${RUN_SMOKE_ON_HEAD_CHANGE:-1}"
WATCH_REF="${WATCH_REF:-HEAD}"

mkdir -p "$LOG_DIR"

timestamp() {
  date "+%Y-%m-%dT%H:%M:%S%z"
}

current_rev() {
  git -C "$REPO_ROOT" rev-parse "$WATCH_REF"
}

log_file_for_rev() {
  printf "%s/stress-%s.log" "$LOG_DIR" "$1"
}

run_chunk() {
  rev="$1"
  log_file="$(log_file_for_rev "$rev")"

  {
    printf "[%s] begin rev=%s interpreter=%s iterations=%s\n" \
      "$(timestamp)" "$rev" "$INTERPRETER" "$STRESS_ITERATIONS"
    ./bin/stress_smoke "$STRESS_ITERATIONS"
    printf "[%s] end rev=%s status=0\n" "$(timestamp)" "$rev"
  } >>"$log_file" 2>&1
}

last_rev=""

while true
do
  rev="$(current_rev)"
  if [ "$rev" != "$last_rev" ]; then
    log_file="$(log_file_for_rev "$rev")"
    {
      printf "[%s] head-change rev=%s\n" "$(timestamp)" "$rev"
    } >>"$log_file"

    if [ "$BUILD_ON_HEAD_CHANGE" = "1" ]; then
      {
        printf "[%s] make INTERPRETER=%s stress-smoke\n" "$(timestamp)" "$INTERPRETER"
        make INTERPRETER="$INTERPRETER" stress-smoke
      } >>"$log_file" 2>&1
    fi

    if [ "$RUN_SMOKE_ON_HEAD_CHANGE" = "1" ] && [ "$BUILD_ON_HEAD_CHANGE" != "1" ]; then
      {
        printf "[%s] smoke rev=%s\n" "$(timestamp)" "$rev"
        ./bin/stress_smoke
      } >>"$log_file" 2>&1
    fi

    last_rev="$rev"
  fi

  if ! run_chunk "$rev"; then
    log_file="$(log_file_for_rev "$rev")"
    {
      printf "[%s] failure rev=%s\n" "$(timestamp)" "$rev"
    } >>"$log_file"
    printf "stress loop failed at rev %s; see %s\n" "$rev" "$log_file" >&2
    exit 1
  fi

  if [ "$(current_rev)" != "$rev" ]; then
    continue
  fi

  sleep "$SLEEP_SECS"
done
