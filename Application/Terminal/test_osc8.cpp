// Small test program for OSC 8 hyperlink support
// Compile: cl test_osc8.cpp /Fe:test_osc8.exe
// Usage:   test_osc8.exe  (run inside the ecode terminal)

#include <cstdio>
#include <string>

// ESC ] 8 ; params ; url ST
// ST = ESC \  or  BEL (\a)
static void hyperlink(const char* url, const char* text) {
    // open
    printf("\x1b]8;;%s\x1b\\", url);
    // linked text
    printf("%s", text);
    // close
    printf("\x1b]8;;\x1b\\");
}

int main() {
    // --- basic hyperlinks ---
    printf("\x1b[1m=== OSC 8 Hyperlink Test ===\x1b[m\n\n");

    hyperlink("https://example.com", "https://example.com");
    printf("\n");

    hyperlink("https://github.com", "GitHub");
    printf("  <- named link\n\n");

    // --- link with SGR colors ---
    printf("Colored link: ");
    printf("\x1b[38;5;46m");  // green fg
    hyperlink("https://en.wikipedia.org/wiki/ANSI_escape_code", "Wikipedia: ANSI escape code");
    printf("\x1b[m");
    printf("\n\n");

    // --- link with explicit color (foreground is NOT default, so should keep green) ---
    printf("Explicit red + link: ");
    printf("\x1b[31m");
    hyperlink("https://example.com/red", "this should be red");
    printf("\x1b[m");
    printf("\n\n");

    // --- adjacent links ---
    printf("Adjacent links: ");
    hyperlink("https://a.com", "link-A");
    printf(" ");
    hyperlink("https://b.com", "link-B");
    printf(" ");
    hyperlink("https://c.com", "link-C");
    printf("\n\n");

    // --- link spanning multiple words ---
    printf("Multi-word link: ");
    hyperlink("https://example.com/long-path", "click here to visit a long URL");
    printf("\n\n");

    // --- multiple links on one line ---
    printf("Inline links: visit ");
    hyperlink("https://google.com", "Google");
    printf(" or ");
    hyperlink("https://duckduckgo.com", "DuckDuckGo");
    printf(" to search\n\n");

    // --- link with underline SGR ---
    printf("With SGR underline: ");
    printf("\x1b[4m");
    hyperlink("https://example.com/sgr", "SGR underlined link");
    printf("\x1b[m");
    printf("\n\n");

    // --- link with bold+italic ---
    printf("Bold italic link: ");
    printf("\x1b[1;3m");
    hyperlink("https://example.com/fancy", "fancy link");
    printf("\x1b[m");
    printf("\n\n");

    // --- link in the middle of text ---
    printf("You can find the source at ");
    hyperlink("https://github.com/user/repo", "our GitHub repo");
    printf(". Please file issues there.\n\n");

    // --- link containing spaces in URL ---
    printf("URL with spaces: ");
    hyperlink("https://example.com/path%20with%20spaces", "path with spaces (percent-encoded)");
    printf("\n\n");

    // --- link with no params between ;; ---
    printf("Minimal OSC 8: ");
    printf("\x1b]8;;https://example.com/minimal\x1b\\minimal\x1b]8;;\x1b\\");
    printf("\n\n");

    printf("\x1b[1m=== End of test ===\x1b[m\n");
    return 0;
}
