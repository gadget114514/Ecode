#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <regex>
#include <map>

// A simple static analyzer to find duplicate assignments in the key handling logic
int main() {
    std::string path = "d:/ws/Ecode/src/WindowHandlers_Input.inl";
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to open " << path << std::endl;
        return 1;
    }

    std::string line;
    std::vector<std::string> lines;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }

    // We are looking for lines inside:
    // if ((g_getKeyState(VK_CONTROL) & 0x8000) && !(g_getKeyState(VK_MENU) & 0x8000) && !(g_getKeyState(VK_SHIFT) & 0x8000)) { ... }
    
    bool insideCtrlBlock = false;
    std::map<char, int> assignments;

    for (size_t i = 0; i < lines.size(); ++i) {
        const std::string& l = lines[i];

        // Detect start of Ctrl block (heuristically)
        if (l.find("g_getKeyState(VK_CONTROL)") != std::string::npos && 
            l.find("!(g_getKeyState(VK_MENU)") != std::string::npos &&
            l.find("!(g_getKeyState(VK_SHIFT)") != std::string::npos) {
            insideCtrlBlock = true;
            continue;
        }

        // Detect end of block (heuristically, by indentation or next if)
        if (insideCtrlBlock) {
            if (l.find("if ((g_getKeyState(VK_CONTROL)") != std::string::npos) {
                // Next block started
                insideCtrlBlock = false;
            }
        }

        if (insideCtrlBlock) {
            // Look for wParam == 'X'
            std::regex re(R"(wParam\s*==\s*'([A-Z])')");
            std::smatch match;
            if (std::regex_search(l, match, re)) {
                char key = match[1].str()[0];
                if (assignments.count(key)) {
                    std::cout << "WARNING: Duplicate binding for Ctrl+" << key << " found on line " << (i + 1) << std::endl;
                    std::cout << "Original usage maybe on line " << assignments[key] << std::endl;
                } else {
                    assignments[key] = (int)(i + 1);
                    // std::cout << "Found Ctrl+" << key << " on line " << (i + 1) << std::endl;
                }
            }
        }
    }

    std::cout << "Finished checking for duplicates in Ctrl block." << std::endl;
    return 0;
}
