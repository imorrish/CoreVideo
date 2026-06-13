// Unit tests for the line-oriented IPC helpers in engine-ipc.h.
//
// These cover the framing and write-failure semantics that the plugin and
// engine rely on: round-trip delivery, newline framing across multiple writes,
// the oversized-line guard, and — most importantly — that ipc_write_line()
// reports failure (rather than silently dropping bytes) when the peer is gone.
//
// The shared-memory and Windows pipe paths are not exercised here; this test is
// POSIX-only and is registered only on non-Windows platforms.

#include "engine-ipc.h"

#include <csignal>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

static int g_failures = 0;

static void check(bool ok, const char *what)
{
    if (!ok) {
        std::cerr << "FAIL: " << what << "\n";
        ++g_failures;
    }
}

// Round-trip a single line through a socketpair.
static void test_round_trip()
{
    int fds[2];
    check(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0, "socketpair created");

    check(ipc_write_line(fds[1], "{\"cmd\":\"ping\"}"), "write returns true");

    std::string line;
    check(ipc_read_line(fds[0], line), "read returns true");
    check(line == "{\"cmd\":\"ping\"}", "round-trip payload matches (newline stripped)");

    close(fds[0]);
    close(fds[1]);
}

// Two lines written back-to-back must be framed into two reads.
static void test_framing()
{
    int fds[2];
    check(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0, "socketpair created");

    check(ipc_write_line(fds[1], "first"), "write line 1");
    check(ipc_write_line(fds[1], "second"), "write line 2");

    std::string line;
    check(ipc_read_line(fds[0], line) && line == "first", "first line framed");
    check(ipc_read_line(fds[0], line) && line == "second", "second line framed");

    close(fds[0]);
    close(fds[1]);
}

// A line longer than max_len must be rejected rather than read unbounded.
static void test_oversized_line()
{
    int fds[2];
    check(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0, "socketpair created");

    // 64 bytes, no newline, with a max_len of 16 — read must give up.
    const std::string big(64, 'x');
    check(ipc_write_line(fds[1], big), "oversized payload written");

    std::string line;
    check(!ipc_read_line(fds[0], line, 16), "oversized line rejected");

    close(fds[0]);
    close(fds[1]);
}

// Writing to a peer whose read end is closed must report failure, not silently
// drop the message. This is the core guarantee the hardened path depends on.
static void test_broken_pipe()
{
    int fds[2];
    check(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0, "socketpair created");

    close(fds[0]); // drop the reader

    // First write may succeed into the socket buffer or fail immediately
    // depending on the platform; keep writing until the failure surfaces.
    bool saw_failure = false;
    for (int i = 0; i < 10000 && !saw_failure; ++i) {
        if (!ipc_write_line(fds[1], "{\"cmd\":\"frame\"}"))
            saw_failure = true;
    }
    check(saw_failure, "write to closed peer eventually returns false");

    close(fds[1]);
}

// An invalid fd must be rejected up front.
static void test_invalid_fd()
{
    check(!ipc_write_line(kIpcInvalidFd, "anything"), "write to invalid fd returns false");
}

int main()
{
    // A broken-pipe write would otherwise raise SIGPIPE and abort the test.
    std::signal(SIGPIPE, SIG_IGN);

    test_round_trip();
    test_framing();
    test_oversized_line();
    test_broken_pipe();
    test_invalid_fd();

    if (g_failures == 0)
        std::cout << "All IPC line-I/O tests passed\n";
    return g_failures == 0 ? 0 : 1;
}
