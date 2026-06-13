// Tests for the line-oriented IPC helpers in engine-ipc.h
// (ipc_read_line / ipc_write_line). POSIX-only: uses a real pipe pair.

#include "engine-ipc.h"

#include <iostream>
#include <string>
#include <unistd.h>

static int failures = 0;

static void fail(const char *message)
{
    std::cerr << "FAIL: " << message << "\n";
    ++failures;
}

// Write raw bytes into the write end of a fresh pipe, then return the read fd.
struct Pipe {
    int read_fd  = -1;
    int write_fd = -1;
    ~Pipe()
    {
        if (read_fd >= 0)  close(read_fd);
        if (write_fd >= 0) close(write_fd);
    }
};

static bool make_pipe(Pipe &p)
{
    int fds[2];
    if (pipe(fds) != 0) return false;
    p.read_fd  = fds[0];
    p.write_fd = fds[1];
    return true;
}

static void write_raw(int fd, const std::string &data)
{
    const char *ptr = data.c_str();
    size_t rem = data.size();
    while (rem > 0) {
        ssize_t n = write(fd, ptr, rem);
        if (n <= 0) break;
        ptr += static_cast<size_t>(n);
        rem -= static_cast<size_t>(n);
    }
}

int main()
{
    // --- Basic single line, newline terminated ---
    {
        Pipe p;
        if (!make_pipe(p)) { fail("pipe() failed"); return 1; }
        write_raw(p.write_fd, "hello world\n");
        std::string line;
        if (!ipc_read_line(p.read_fd, line)) fail("expected to read a line");
        if (line != "hello world") fail("line content mismatch");
    }

    // --- Multiple lines read sequentially ---
    {
        Pipe p;
        if (!make_pipe(p)) { fail("pipe() failed"); return 1; }
        write_raw(p.write_fd, "first\nsecond\nthird\n");
        std::string line;
        if (!ipc_read_line(p.read_fd, line) || line != "first")  fail("first line wrong");
        if (!ipc_read_line(p.read_fd, line) || line != "second") fail("second line wrong");
        if (!ipc_read_line(p.read_fd, line) || line != "third")  fail("third line wrong");
    }

    // --- Empty line is valid and returns true ---
    {
        Pipe p;
        if (!make_pipe(p)) { fail("pipe() failed"); return 1; }
        write_raw(p.write_fd, "\nnext\n");
        std::string line = "garbage";
        if (!ipc_read_line(p.read_fd, line)) fail("empty line should still return true");
        if (!line.empty()) fail("empty line should clear the output string");
        if (!ipc_read_line(p.read_fd, line) || line != "next") fail("line after empty wrong");
    }

    // --- EOF without trailing newline returns false (partial data discarded) ---
    {
        Pipe p;
        if (!make_pipe(p)) { fail("pipe() failed"); return 1; }
        write_raw(p.write_fd, "no-newline");
        close(p.write_fd);
        p.write_fd = -1; // signal EOF to reader
        std::string line;
        if (ipc_read_line(p.read_fd, line)) fail("EOF without newline should return false");
    }

    // --- Immediate EOF returns false ---
    {
        Pipe p;
        if (!make_pipe(p)) { fail("pipe() failed"); return 1; }
        close(p.write_fd);
        p.write_fd = -1;
        std::string line;
        if (ipc_read_line(p.read_fd, line)) fail("immediate EOF should return false");
    }

    // --- Over-long line is rejected (returns false once max_len exceeded) ---
    {
        Pipe p;
        if (!make_pipe(p)) { fail("pipe() failed"); return 1; }
        const size_t max_len = 8;
        // Write more than max_len bytes with no newline within the limit.
        write_raw(p.write_fd, std::string(64, 'x') + "\n");
        close(p.write_fd);
        p.write_fd = -1;
        std::string line;
        if (ipc_read_line(p.read_fd, line, max_len))
            fail("line exceeding max_len should be rejected");
    }

    // --- A line exactly at the limit followed by newline is accepted ---
    {
        Pipe p;
        if (!make_pipe(p)) { fail("pipe() failed"); return 1; }
        const size_t max_len = 5;
        // 5 chars then newline: the size check fires only when out.size() >= max_len
        // *before* appending, so a 5-char payload is the largest accepted line.
        write_raw(p.write_fd, "abcde\n");
        std::string line;
        if (!ipc_read_line(p.read_fd, line, max_len))
            fail("line of exactly max_len should be accepted");
        if (line != "abcde") fail("max-length line content mismatch");
    }

    // --- Round-trip: ipc_write_line output is read back by ipc_read_line ---
    {
        Pipe p;
        if (!make_pipe(p)) { fail("pipe() failed"); return 1; }
        ipc_write_line(p.write_fd, "join 12345");
        ipc_write_line(p.write_fd, "quit");
        std::string line;
        if (!ipc_read_line(p.read_fd, line) || line != "join 12345")
            fail("round-trip first message wrong");
        if (!ipc_read_line(p.read_fd, line) || line != "quit")
            fail("round-trip second message wrong");
    }

    if (failures == 0) {
        std::cout << "All IPC line I/O tests passed.\n";
        return 0;
    }
    std::cerr << failures << " test(s) failed.\n";
    return 1;
}
