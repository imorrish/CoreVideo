// Tests for the Zoom join-input parsing helpers in obs-utils.cpp
// (zoom_join_utils::parse_join_input / is_valid_meeting_id). Pure, no Qt/OBS.

#include "obs-utils.h"

#include <iostream>
#include <string>

using zoom_join_utils::parse_join_input;
using zoom_join_utils::is_valid_meeting_id;

static int failures = 0;

static void check(bool ok, const char *message)
{
    if (!ok) {
        std::cerr << "FAIL: " << message << "\n";
        ++failures;
    }
}

int main()
{
    // --- is_valid_meeting_id ---
    check(is_valid_meeting_id("1234567890"), "plain digits should be valid");
    check(is_valid_meeting_id("123 456 7890"), "spaces should be stripped and accepted");
    check(is_valid_meeting_id("123-456-7890"), "dashes should be stripped and accepted");
    check(is_valid_meeting_id("  88 99-00  "), "leading/trailing whitespace ok");
    check(!is_valid_meeting_id(""), "empty string is invalid");
    check(!is_valid_meeting_id("   "), "whitespace-only string is invalid");
    check(!is_valid_meeting_id("12a45"), "letters are invalid");
    check(!is_valid_meeting_id("- -"), "only separators is invalid (no digits)");

    // --- Raw meeting ID forms ---
    {
        auto r = parse_join_input("1234567890");
        check(r.meeting_id == "1234567890", "raw digits meeting id");
        check(r.passcode.empty(), "raw id has no passcode");
    }
    {
        auto r = parse_join_input("  123 456 7890 ");
        check(r.meeting_id == "1234567890", "raw id with spaces normalized");
    }
    {
        auto r = parse_join_input("123-456-7890");
        check(r.meeting_id == "1234567890", "raw id with dashes normalized");
    }
    {
        auto r = parse_join_input("not-a-meeting");
        check(r.meeting_id.empty(), "non-numeric raw input yields empty id");
    }
    {
        auto r = parse_join_input("");
        check(r.meeting_id.empty(), "empty input yields empty id");
    }

    // --- /j/ URL form with pwd ---
    {
        auto r = parse_join_input("https://zoom.us/j/123456789?pwd=secretpass");
        check(r.meeting_id == "123456789", "/j/ url meeting id");
        check(r.passcode == "secretpass", "/j/ url passcode");
    }

    // --- /wc/ join URL ---
    {
        auto r = parse_join_input("https://zoom.us/wc/987654321/join?pwd=abc123");
        check(r.meeting_id == "987654321", "/wc/ url meeting id");
        check(r.passcode == "abc123", "/wc/ url passcode");
    }

    // --- /s/ URL form ---
    {
        auto r = parse_join_input("https://zoom.us/s/555000111");
        check(r.meeting_id == "555000111", "/s/ url meeting id");
        check(r.passcode.empty(), "/s/ url without pwd has no passcode");
    }

    // --- zoommtg deep link with confno fallback ---
    {
        auto r = parse_join_input("zoommtg://zoom.us/join?confno=222333444&pwd=zzz");
        check(r.meeting_id == "222333444", "confno fallback meeting id");
        check(r.passcode == "zzz", "deep-link passcode");
    }

    // --- URL-encoded passcode is decoded ---
    {
        auto r = parse_join_input("https://zoom.us/j/12345?pwd=a%20b%2Bc");
        check(r.meeting_id == "12345", "encoded url meeting id");
        check(r.passcode == "a b+c", "url-decoded passcode (%20 -> space, %2B -> +)");
    }

    // --- '+' decodes to space in query values ---
    {
        auto r = parse_join_input("https://zoom.us/j/12345?password=hello+world");
        check(r.passcode == "hello world", "plus decodes to space; 'password' key honored");
    }

    // --- Auth token query params are extracted ---
    {
        auto r = parse_join_input(
            "https://zoom.us/j/777?pwd=p&zak=ZAKTOKEN&obf=OBFTOKEN"
            "&app_privilege_token=APPTOK");
        check(r.meeting_id == "777", "token url meeting id");
        check(r.passcode == "p", "token url passcode");
        check(r.user_zak == "ZAKTOKEN", "zak token extracted");
        check(r.on_behalf_token == "OBFTOKEN", "on-behalf token extracted");
        check(r.app_privilege_token == "APPTOK", "app privilege token extracted");
    }

    // --- Path marker takes precedence over confno fallback ---
    {
        auto r = parse_join_input("https://zoom.us/j/111?confno=999");
        check(r.meeting_id == "111", "path /j/ id wins over confno query fallback");
    }

    // --- Key matching is case-insensitive ---
    {
        auto r = parse_join_input("https://zoom.us/j/42?PWD=Mixed");
        check(r.passcode == "Mixed", "uppercase PWD key is recognized");
    }

    if (failures == 0) {
        std::cout << "All join-input parsing tests passed.\n";
        return 0;
    }
    std::cerr << failures << " test(s) failed.\n";
    return 1;
}
