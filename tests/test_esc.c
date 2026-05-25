#include <stdio.h>
#include <windows.h>


int main() {
    int total = 50;
    
    for (int i = 0; i <= total; ++i) {
        // 1. Calculate percentage
        int percent = (i * 100) / total;
        
        // 2. Return to start of line (\r) and clear it (\033[K)
        printf("\r\033[K");
        
        // 3. Draw the progress bar
        printf("Progress: [");
        for (int j = 0; j < total; ++j) {
            if (j < i) printf("#");
            else printf("-");
        }
        
        // 4. Print percentage (ensure stdout is flushed so it draws immediately)
        printf("] %d%%", percent);
       fflush(stdout);
        
        // Simulate work
        Sleep(50);
    }
    
    // Drop to a new line when finished
    printf("\nDone!\n");
    return 0;
}