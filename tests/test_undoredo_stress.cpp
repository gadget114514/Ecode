#include "../include/PieceTable.h"
#include <algorithm>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#define VERIFY(cond, msg)                                                      \
  if (!(cond)) {                                                               \
    std::cerr << "UNDO/REDO FAILURE at line " << __LINE__ << " (Iteration "    \
              << i << "): " << msg << std::endl;                               \
    exit(1);                                                                   \
  }

void RunUndoRedoStressTest(int iterations) {
  PieceTable pt;
  std::vector<std::string> states;
  states.push_back(""); // Initial state

  std::mt19937 rng(456);
  std::uniform_int_distribution<int> opDist(
      0, 10); // 0-7: Edit, 8-9: Undo, 10: Redo

  std::cout << "Starting Undo/Redo Stress Test (" << iterations
            << " iterations)..." << std::endl;

  for (int i = 0; i < iterations; ++i) {
    int op = opDist(rng);

    if (op <= 7) { // Edit
      // If we are in a redo state, any edit should clear it.
      // But PieceTable handles this internally.

      size_t currentLen = pt.GetTotalLength();
      bool isInsert = (currentLen == 0) || (rng() % 2 == 0);

      if (isInsert) {
        size_t pos = (currentLen == 0) ? 0 : rng() % (currentLen + 1);
        std::string text = "state_" + std::to_string(i) + " ";
        pt.Insert(pos, text);
      } else {
        size_t pos = rng() % currentLen;
        size_t len = (rng() % (currentLen - pos)) % 20 + 1;
        pt.Delete(pos, len);
      }

      // Record new state
      std::string currentText = pt.GetText(0, pt.GetTotalLength());
      states.push_back(currentText);

      // Verification
      VERIFY(pt.GetText(0, pt.GetTotalLength()) == states.back(),
             "Content mismatch after edit");

    } else if (op <= 9) { // Undo
      if (pt.CanUndo() && states.size() > 1) {
        pt.Undo();
        states.pop_back(); // We don't store snapshots for redoable states in
                           // this simple list
                           // Wait, if it's a simple list we can't redo.
                           // Let's modify the strategy:
        // We need TWO lists: History (snapshots) and RedoStack (snapshots).
      }
    }
  }
  // That approach is a bit messy. Let's do a structured "Full Loop" stress test
  // instead.
}

// Structured Loop Stress:
// 1. N random edits.
// 2. Snapshot every step.
// 3. Undo all, verifying every step.
// 4. Redo all, verifying every step.
void RunStructuredLoopStress(int iterations, int editsPerLoop) {
  PieceTable pt;

  std::mt19937 rng(789);

  std::cout << "Starting Structured Undo/Redo Loop Stress (" << iterations
            << " loops of " << editsPerLoop << " edits)..." << std::endl;

  for (int i = 0; i < iterations; ++i) {
    pt = PieceTable();
    std::vector<std::string> snapshots;
    snapshots.push_back(""); // Start

    // 1. Edits
    for (int j = 0; j < editsPerLoop; ++j) {
      size_t curLen = pt.GetTotalLength();
      if (curLen == 0 || rng() % 3 != 0) { // Bias towards insert
        size_t pos = (curLen == 0) ? 0 : rng() % (curLen + 1);
        std::string text =
            "L" + std::to_string(i) + "E" + std::to_string(j) + "_";
        pt.Insert(pos, text);
      } else {
        size_t pos = rng() % curLen;
        size_t len = (rng() % std::min(curLen - pos, (size_t)10)) + 1;
        pt.Delete(pos, len);
      }
      snapshots.push_back(pt.GetText(0, pt.GetTotalLength()));

      // Sanity check current
      VERIFY(pt.GetText(0, pt.GetTotalLength()) == snapshots.back(),
             "Immediate content mismatch");
    }

    // 2. Full Undo
    for (int j = (int)snapshots.size() - 2; j >= 0; --j) {
      VERIFY(pt.CanUndo(), "Expected CanUndo to be true");
      pt.Undo();
      VERIFY(pt.GetText(0, pt.GetTotalLength()) == snapshots[j],
             "Undo content mismatch");
    }
    VERIFY(!pt.CanUndo(), "Expected CanUndo to be false at start");

    // 3. Full Redo
    for (int j = 1; j < snapshots.size(); ++j) {
      VERIFY(pt.CanRedo(), "Expected CanRedo to be true");
      pt.Redo();
      VERIFY(pt.GetText(0, pt.GetTotalLength()) == snapshots[j],
             "Redo content mismatch");
    }
    VERIFY(!pt.CanRedo(), "Expected CanRedo to be false at end");

    // 4. Branching History Test (Edit after Undo)
    pt.Undo();
    pt.Undo();
    VERIFY(pt.CanRedo(), "Should have redo stack after 2 undos");
    pt.Insert(0, "Branch_");
    VERIFY(!pt.CanRedo(), "Redo stack should be pruned after edit");
  }

  std::cout << "Structured Undo/Redo Loop Stress Passed!" << std::endl;
}

int main() {
  RunStructuredLoopStress(50, 200); // 50 loops * 200 edits = 10,000 edits total
  return 0;
}
