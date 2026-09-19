#!/usr/bin/env bash
#
# Phase 1 integration tests for the minimal blocking HTTP server.
#
# Usage: integration_phase1.sh <server_bin> <client_bin> [port]
#
# Starts the server, checks it over real TCP, then stops it.
# Exit 0 = all passed, exit 1 = something failed.
#
# Checks:
#   1. status line is exactly "HTTP/1.1 200 OK"
#   2. Content-Length header exists and equals the body byte count
#   3. body is exactly "Hello, world!"
#   4. a second sequential connection also works (accept loop lives)
#   5. our C client receives "200 OK"
#   6. garbage input doesn't kill the server (next request still 200)
#   7. connect-then-immediately-close doesn't kill the server
#   8. server log contains no sanitizer / runtime-error reports

set -u
set -o pipefail

SERVER_BIN="${1:?usage: integration_phase1.sh <server_bin> <client_bin> [port]}"
CLIENT_BIN="${2:?usage: integration_phase1.sh <server_bin> <client_bin> [port]}"
PORT="${3:-8000}"

PASS=0
FAIL=0
LOG="/tmp/phase1_server.log"

pass() { PASS=$((PASS + 1)); echo "PASS: $1"; }
fail() { FAIL=$((FAIL + 1)); echo "FAIL: $1"; }

# --- start server ---------------------------------------------------------
"$SERVER_BIN" > "$LOG" 2>&1 &
SRV=$!
cleanup() { kill "$SRV" 2>/dev/null; }
trap cleanup EXIT

# wait until it listens (refused connections fail fast; 10s max)
UP=0
for _ in $(seq 1 10); do
    if curl -sf -o /dev/null --max-time 2 "http://localhost:$PORT/" 2>/dev/null; then
        UP=1
        break
    fi
    sleep 1
done
if [ "$UP" -ne 1 ]; then
    fail "server never listened on port $PORT"
    echo "--- server log ---"
    cat "$LOG"
    echo "passed=$PASS failed=$FAIL"
    exit 1
fi

# one request whose headers + body we inspect file by file
curl -s --max-time 5 -D /tmp/phase1_headers -o /tmp/phase1_body "http://localhost:$PORT/" \
    || fail "curl request failed"

# --- 1. status line -------------------------------------------------------
STATUS_LINE="$(head -n 1 /tmp/phase1_headers | tr -d '\r')"
if [ "$STATUS_LINE" = "HTTP/1.1 200 OK" ]; then
    pass "status line is HTTP/1.1 200 OK"
else
    fail "status line is '$STATUS_LINE'"
fi

# --- 2. Content-Length matches actual body bytes --------------------------
CLEN="$(grep -i '^content-length:' /tmp/phase1_headers | tr -d '\r' | awk '{print $2}')"
BODYLEN="$(wc -c < /tmp/phase1_body | tr -d ' ')"
if [ -n "$CLEN" ] && [ "$CLEN" = "$BODYLEN" ]; then
    pass "Content-Length $CLEN matches body bytes"
else
    fail "Content-Length header='$CLEN' body bytes='$BODYLEN'"
fi

# --- 3. exact body --------------------------------------------------------
if [ "$(cat /tmp/phase1_body)" = "Hello, world!" ]; then
    pass "body is exactly 'Hello, world!'"
else
    fail "unexpected body: '$(cat /tmp/phase1_body)'"
fi

# --- 4. second sequential connection (accept loop still alive) ------------
CODE="$(curl -s -o /dev/null -w '%{http_code}' --max-time 5 "http://localhost:$PORT/")"
if [ "$CODE" = "200" ]; then
    pass "second connection -> 200"
else
    fail "second connection -> $CODE"
fi

# --- 5. our C client ------------------------------------------------------
if "$CLIENT_BIN" 2>/dev/null | grep -q "200 OK"; then
    pass "C client receives 200 OK"
else
    fail "C client did not receive 200 OK"
fi

# --- 6. garbage input must not kill the server ----------------------------
# Raw TCP via bash's /dev/tcp (no nc dependency). The server should read the
# junk, answer 200 anyway, and keep looping.
printf 'THIS IS NOT HTTP!!!' > /dev/tcp/127.0.0.1/"$PORT" 2>/dev/null
sleep 1
CODE="$(curl -s -o /dev/null -w '%{http_code}' --max-time 5 "http://localhost:$PORT/")"
if [ "$CODE" = "200" ]; then
    pass "server survives garbage input"
else
    fail "server dead after garbage input (got $CODE)"
fi

# --- 7. connect + immediate close must not kill the server ----------------
# Server's recv() returns 0 here; it must just continue the loop.
: > /dev/tcp/127.0.0.1/"$PORT" 2>/dev/null
sleep 1
CODE="$(curl -s -o /dev/null -w '%{http_code}' --max-time 5 "http://localhost:$PORT/")"
if [ "$CODE" = "200" ]; then
    pass "server survives empty connection"
else
    fail "server dead after empty connection (got $CODE)"
fi

# --- 8. no sanitizer reports in the server log ----------------------------
if grep -qiE "sanitizer|runtime error" "$LOG"; then
    fail "sanitizer/runtime report in server log"
    grep -iE "sanitizer|runtime error" "$LOG" | head -5
else
    pass "no sanitizer/runtime reports"
fi

echo "---"
echo "passed=$PASS failed=$FAIL"
[ "$FAIL" -eq 0 ]
