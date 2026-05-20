#include "../include/Buffer.h"
#include <algorithm>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#define VERIFY(cond, msg)                                                        \
  if (!(cond)) {                                                                 \
    std::cerr << "BUFFER STRESS FAILURE at line " << __LINE__ << ": " << msg    \
              << std::endl;                                                      \
    exit(1);                                                                     \
  }

static const char *kWords[] = {"alpha", "beta",  "gamma", "delta", "hello",
                               "world", "foo",   "bar",   "baz",   "qux"};
static std::string RandomWord(std::mt19937 &rng) {
  return kWords[rng() % 10];
}

// ──────────────────────────────────────────────────────────────
// 1. Insert/Delete/Replace stress: content vs std::string reference
// ──────────────────────────────────────────────────────────────
void StressInsertDelete(int iterations) {
  std::cout << "StressInsertDelete (" << iterations << " iterations)..."
            << std::endl;
  Buffer buf;
  std::string ref;
  std::mt19937 rng(1001);

  for (int i = 0; i < iterations; ++i) {
    size_t len = ref.size();
    int op = len == 0 ? 0 : (int)(rng() % 3);

    if (op == 0) {
      size_t pos = len == 0 ? 0 : rng() % (len + 1);
      std::string text = RandomWord(rng);
      if (rng() % 3 == 0) text += '\n';
      buf.Insert(pos, text);
      ref.insert(pos, text);
    } else if (op == 1) {
      size_t pos = rng() % len;
      size_t dlen = std::min((size_t)(rng() % 20 + 1), len - pos);
      buf.Delete(pos, dlen);
      ref.erase(pos, dlen);
    } else {
      size_t pos = rng() % len;
      size_t dlen = std::min((size_t)(rng() % 15 + 1), len - pos);
      std::string text = RandomWord(rng);
      buf.Replace(pos, pos + dlen, text);
      ref.replace(pos, dlen, text);
    }

    if (i % 200 == 0) {
      VERIFY(buf.GetTotalLength() == ref.size(), "Length mismatch");
      VERIFY(buf.GetText(0, buf.GetTotalLength()) == ref, "Content mismatch");
    }
  }

  VERIFY(buf.GetText(0, buf.GetTotalLength()) == ref, "Final content mismatch");
  std::cout << "  PASSED (final size: " << ref.size() << " bytes)" << std::endl;
}

// ──────────────────────────────────────────────────────────────
// 2. Undo/Redo stress: snapshot each edit, undo all, redo all,
//    then verify branching clears the redo stack
// ──────────────────────────────────────────────────────────────
void StressUndoRedo(int loops, int editsPerLoop) {
  std::cout << "StressUndoRedo (" << loops << " loops x " << editsPerLoop
            << " edits)..." << std::endl;
  std::mt19937 rng(2002);

  for (int loop = 0; loop < loops; ++loop) {
    Buffer buf;
    std::vector<std::string> snapshots;
    snapshots.push_back("");

    for (int j = 0; j < editsPerLoop; ++j) {
      size_t len = buf.GetTotalLength();
      if (len == 0 || rng() % 3 != 0) {
        size_t pos = len == 0 ? 0 : rng() % (len + 1);
        buf.Insert(pos, "e" + std::to_string(j) + "_");
      } else {
        size_t pos = rng() % len;
        size_t dlen = std::min((size_t)(rng() % 10 + 1), len - pos);
        buf.Delete(pos, dlen);
      }
      snapshots.push_back(buf.GetText(0, buf.GetTotalLength()));
    }

    // Undo all
    for (int j = (int)snapshots.size() - 2; j >= 0; --j) {
      VERIFY(buf.CanUndo(), "Expected CanUndo=true");
      buf.Undo();
      VERIFY(buf.GetText(0, buf.GetTotalLength()) == snapshots[j],
             "Undo content mismatch");
    }
    VERIFY(!buf.CanUndo(), "Expected CanUndo=false at origin");

    // Redo all
    for (int j = 1; j < (int)snapshots.size(); ++j) {
      VERIFY(buf.CanRedo(), "Expected CanRedo=true");
      buf.Redo();
      VERIFY(buf.GetText(0, buf.GetTotalLength()) == snapshots[j],
             "Redo content mismatch");
    }
    VERIFY(!buf.CanRedo(), "Expected CanRedo=false at tip");

    // Branching: undo 2, make a new edit, redo stack must clear
    if (buf.CanUndo()) buf.Undo();
    if (buf.CanUndo()) buf.Undo();
    VERIFY(buf.CanRedo(), "Expected redo stack after 2 undos");
    buf.Insert(0, "branch_");
    VERIFY(!buf.CanRedo(), "Redo stack should clear after new edit");
  }
  std::cout << "  PASSED" << std::endl;
}

// ──────────────────────────────────────────────────────────────
// 3. Caret/Selection stress: caret and anchor always in [0, length]
//    and selection range is always ordered
// ──────────────────────────────────────────────────────────────
void StressCaretSelection(int iterations) {
  std::cout << "StressCaretSelection (" << iterations << " iterations)..."
            << std::endl;
  Buffer buf;
  const int NLINES = 50;
  for (int i = 0; i < NLINES; ++i)
    buf.Insert(buf.GetTotalLength(), "Line " + std::to_string(i) + "\n");

  size_t totalLen = buf.GetTotalLength();
  std::mt19937 rng(3003);

  for (int i = 0; i < iterations; ++i) {
    switch (rng() % 7) {
    case 0: buf.SetCaretPos(rng() % (totalLen + 1)); break;
    case 1: buf.MoveCaretHome(); break;
    case 2: buf.MoveCaretEnd(); break;
    case 3: buf.MoveCaretUp(); break;
    case 4: buf.MoveCaretDown(); break;
    case 5: buf.MoveCaret(+1); break;
    case 6: {
      size_t anchor = rng() % (totalLen + 1);
      buf.SetSelectionAnchor(anchor);
      buf.SetCaretPos(rng() % (totalLen + 1));
      break;
    }
    }

    size_t caret = buf.GetCaretPos();
    size_t anchor = buf.GetSelectionAnchor();
    VERIFY(caret <= totalLen, "Caret out of bounds");
    VERIFY(anchor <= totalLen, "Anchor out of bounds");

    if (buf.HasSelection()) {
      size_t selStart, selEnd;
      buf.GetSelectionRange(selStart, selEnd);
      VERIFY(selStart <= selEnd, "Selection range inverted");
      VERIFY(selEnd <= totalLen, "Selection end out of bounds");
    }
  }
  std::cout << "  PASSED" << std::endl;
}

// ──────────────────────────────────────────────────────────────
// 4. Folding stress: invariant visible + folded == total at every step
// ──────────────────────────────────────────────────────────────
void StressFolding(int iterations) {
  std::cout << "StressFolding (" << iterations << " iterations)..." << std::endl;
  Buffer buf;
  const int NLINES = 100;
  for (int i = 0; i < NLINES; ++i)
    buf.Insert(buf.GetTotalLength(), "Line " + std::to_string(i) + "\n");

  size_t totalLines = buf.GetTotalLines();
  std::mt19937 rng(4004);

  for (int i = 0; i < iterations; ++i) {
    size_t line = rng() % NLINES;
    if (rng() % 2 == 0)
      buf.FoldLine(line);
    else
      buf.UnfoldLine(line);

    size_t visible = buf.GetVisibleLineCount();
    size_t foldedCount = buf.GetFoldedLines().size();
    VERIFY(visible + foldedCount == totalLines,
           "Folding invariant: visible + folded != total");
  }

  // Unfold everything: visible must equal total
  for (size_t ln = 0; ln < totalLines; ++ln)
    buf.UnfoldLine(ln);
  VERIFY(buf.GetVisibleLineCount() == totalLines,
         "After full unfold, visible != total");
  VERIFY(buf.GetFoldedLines().empty(), "Folded set should be empty");

  std::cout << "  PASSED" << std::endl;
}

// ──────────────────────────────────────────────────────────────
// 5. Find stress: Buffer::Find vs std::string reference
// ──────────────────────────────────────────────────────────────
void StressFind(int iterations) {
  std::cout << "StressFind (" << iterations << " iterations)..." << std::endl;
  Buffer buf;
  std::string ref;
  const char *patterns[] = {"foo", "bar", "baz", "alpha", "hello"};
  std::mt19937 rng(5005);

  // Build up some content first
  for (int i = 0; i < 200; ++i) {
    std::string word = RandomWord(rng);
    if (rng() % 4 == 0) word += '\n';
    else word += ' ';
    buf.Insert(buf.GetTotalLength(), word);
    ref += word;
  }

  for (int i = 0; i < iterations; ++i) {
    std::string pat = patterns[rng() % 5];
    bool forward = rng() % 2 == 0;
    bool matchCase = rng() % 2 == 0;
    size_t startPos = rng() % (ref.size() + 1);

    size_t actual = buf.Find(pat, startPos, forward, false, matchCase);

    size_t expected;
    if (matchCase) {
      expected = forward ? ref.find(pat, startPos)
                         : ref.rfind(pat, startPos);
    } else {
      std::string rLower = ref, pLower = pat;
      std::transform(rLower.begin(), rLower.end(), rLower.begin(), ::tolower);
      std::transform(pLower.begin(), pLower.end(), pLower.begin(), ::tolower);
      expected = forward ? rLower.find(pLower, startPos)
                         : rLower.rfind(pLower, startPos);
    }

    if (actual != expected) {
      std::cerr << "FIND MISMATCH at iteration " << i << ": pattern='" << pat
                << "' start=" << startPos << " fwd=" << forward
                << " case=" << matchCase << " expected=" << expected
                << " actual=" << actual << std::endl;
      exit(1);
    }
  }
  std::cout << "  PASSED" << std::endl;
}

// ──────────────────────────────────────────────────────────────
// 6. Shell history stress: bounds never violated, navigation consistent
// ──────────────────────────────────────────────────────────────
void StressShellHistory(int iterations) {
  std::cout << "StressShellHistory (" << iterations << " iterations)..."
            << std::endl;
  Buffer buf;
  buf.SetShell(true);
  buf.Insert(0, "$ ");
  buf.SetInputStart(2);
  buf.SetCaretPos(2);

  const char *cmds[] = {"ls", "cd ..", "dir", "pwd", "echo hello", "git status"};
  std::mt19937 rng(6006);

  for (int i = 0; i < iterations; ++i) {
    int op = rng() % 3;
    if (op == 0) {
      buf.AddShellHistory(cmds[rng() % 6]);
    } else if (op == 1) {
      buf.ShellHistoryUp();
    } else {
      buf.ShellHistoryDown();
    }

    // Caret must remain in valid range
    size_t caret = buf.GetCaretPos();
    size_t len = buf.GetTotalLength();
    VERIFY(caret <= len, "ShellHistory: caret out of bounds");
    // Caret must not move before input start
    VERIFY(caret >= buf.GetInputStart(), "ShellHistory: caret before input start");
  }
  std::cout << "  PASSED" << std::endl;
}

int main() {
  try {
    StressInsertDelete(5000);
    StressUndoRedo(20, 100);
    StressCaretSelection(10000);
    StressFolding(5000);
    StressFind(3000);
    StressShellHistory(2000);
    std::cout << "=== ALL BUFFER STRESS TESTS PASSED ===" << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "Exception: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
