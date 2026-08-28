#!/usr/bin/env bash
#
# download_graphs.sh - download and prepare SNAP graph datasets.
#
# Accepts EITHER:
#   1) a single URL to a SNAP archive (.tar.gz/.tgz, .gz, .zip, or plain .txt)
#   2) a path to a text file containing one URL per line (blank lines and
#      lines starting with '#' are ignored)
#
# For each URL:
#   - downloads the archive into <output_dir>/raw/           (skipped if already present)
#   - extracts/decompresses it and places the single edge-list file directly
#     at <output_dir>/<graph_name>.txt, ready for load_csr() to read
#
# Usage:
#   ./download_graphs.sh [-o output_dir] [-f] <url>
#   ./download_graphs.sh [-o output_dir] [-f] <urls.txt>
#
# Options:
#   -o DIR   output directory (default: ../data)
#   -f       force re-download / re-extraction even if already present
#   -h       show this help
#
# Examples:
#   ./download_graphs.sh https://snap.stanford.edu/data/roadNet-CA.txt.gz
#   ./download_graphs.sh -o ../data urls.txt
#
# Example urls.txt:
#   https://snap.stanford.edu/data/roadNet-CA.txt.gz
#   https://snap.stanford.edu/data/email-Enron.txt.gz
#   # comments and blank lines are ignored
#   https://snap.stanford.edu/data/com-youtube.ungraph.txt.gz

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTPUT_DIR="$SCRIPT_DIR/../data"
FORCE=0

log()  { printf '[%s] %s\n' "$(date +'%Y-%m-%d %H:%M:%S')" "$*" >&2; }
die()  { log "ERROR: $*"; exit 1; }

usage() {
    sed -n '2,32p' "$0" | sed 's/^# \{0,1\}//'
    exit "${1:-0}"
}

while getopts ":o:fh" opt; do
    case "$opt" in
        o) OUTPUT_DIR="$OPTARG" ;;
        f) FORCE=1 ;;
        h) usage 0 ;;
        \?) die "unknown option: -$OPTARG" ;;
        :)  die "option -$OPTARG requires an argument" ;;
    esac
done
shift $((OPTIND - 1))

[ $# -eq 1 ] || usage 1
INPUT="$1"

command -v curl >/dev/null 2>&1 || die "curl is required but not found in PATH"

RAW_DIR="$OUTPUT_DIR/raw"
mkdir -p "$RAW_DIR"


download_one() {
    local url="$1"
    local fname graph_name archive_path target_txt tmp_dir found_txt n_found

    fname="$(basename "${url%%\?*}")"           # strip query string, keep basename
    [ -n "$fname" ] || { log "skip: could not derive filename from '$url'"; return 1; }

    # graph_name = filename with known archive/compression extensions stripped
    graph_name="$fname"
    graph_name="${graph_name%.tar.gz}"
    graph_name="${graph_name%.tgz}"
    graph_name="${graph_name%.gz}"
    graph_name="${graph_name%.zip}"
    graph_name="${graph_name%.txt}"
    [ -n "$graph_name" ] || graph_name="graph"

    archive_path="$RAW_DIR/$fname"
    target_txt="$OUTPUT_DIR/$graph_name.txt"

    if [ -f "$target_txt" ] && [ "$FORCE" -ne 1 ]; then
        log "skip (already prepared): $graph_name  ->  $target_txt"
        return 0
    fi

    if [ -f "$archive_path" ] && [ "$FORCE" -ne 1 ]; then
        log "skip download (already have): $archive_path"
    else
        log "downloading: $url"
        if ! curl -fSL --retry 3 --retry-delay 2 -o "$archive_path.part" "$url"; then
            rm -f "$archive_path.part"
            log "FAILED to download: $url"
            return 1
        fi
        mv "$archive_path.part" "$archive_path"
    fi

    log "preparing: $fname  ->  $target_txt"
    mkdir -p "$OUTPUT_DIR"

    case "$fname" in
        *.tar.gz|*.tgz|*.zip)
            tmp_dir="$(mktemp -d "$RAW_DIR/.extract_${graph_name}_XXXXXX")"
            if [ "$fname" != "${fname%.zip}" ]; then
                command -v unzip >/dev/null 2>&1 || die "unzip is required for .zip archives but not found"
                unzip -o -q "$archive_path" -d "$tmp_dir"
            else
                tar xzf "$archive_path" -C "$tmp_dir"
            fi

            n_found="$(find "$tmp_dir" -type f -name '*.txt' | wc -l | tr -d ' ')"
            if [ "$n_found" -eq 0 ]; then
                log "FAILED: no .txt file found inside $fname"
                rm -rf "$tmp_dir"
                return 1
            fi
            if [ "$n_found" -gt 1 ]; then
                log "WARNING: $n_found .txt files found inside $fname, using the first (sorted): $(find "$tmp_dir" -type f -name '*.txt' | sort)"
            fi
            found_txt="$(find "$tmp_dir" -type f -name '*.txt' | sort | head -n1)"
            mv "$found_txt" "$target_txt"
            rm -rf "$tmp_dir"
            ;;
        *.gz)
            # plain gzip (typical SNAP .txt.gz), not a tar archive
            gunzip -k -c "$archive_path" > "$target_txt"
            ;;
        *)
            # already plain text / uncompressed: just copy it in
            cp "$archive_path" "$target_txt"
            ;;
    esac

    log "done: $graph_name  ->  $target_txt"
}


fail_count=0
total_count=0

if [ -f "$INPUT" ]; then
    log "reading URL list from: $INPUT"
    while IFS= read -r line || [ -n "$line" ]; do
        line="$(echo "$line" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')"
        [ -z "$line" ] && continue
        case "$line" in \#*) continue ;; esac
        total_count=$((total_count + 1))
        download_one "$line" || fail_count=$((fail_count + 1))
    done < "$INPUT"
else
    case "$INPUT" in
        http://*|https://*|ftp://*|file://*)
            total_count=1
            download_one "$INPUT" || fail_count=1
            ;;
        *)
            die "'$INPUT' is neither an existing file nor a recognizable URL"
            ;;
    esac
fi

log "summary: $((total_count - fail_count))/$total_count graph(s) prepared successfully"
[ "$fail_count" -eq 0 ] || exit 1