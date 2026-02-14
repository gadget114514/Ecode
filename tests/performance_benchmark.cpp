#include "../include/Buffer.h"
#include "../include/PieceTable.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>

class Timer {
public:
  Timer(const std::string &name)
      : m_name(name), m_start(std::chrono::high_resolution_clock::now()) {}
  ~Timer() {
    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - m_start)
            .count();
    std::cout << std::left << std::setw(40) << m_name << ": " << std::right
              << std::setw(10) << duration << " us" << std::endl;
  }

private:
  std::string m_name;
  std::chrono::time_point<std::chrono::high_resolution_clock> m_start;
};

void BenchmarkLineIndex() {
  std::cout << "\n--- Line Index Benchmarks ---" << std::endl;

  PieceTable pt;
  std::string largeText;
  for (int i = 0; i < 100000; ++i) {
    largeText += "Line " + std::to_string(i) + "\n";
  }
  pt.LoadOriginal(largeText.c_str(), largeText.length());

  {
    Timer t("GetLineOffset (1000 random looks - Cached)");
    for (int i = 0; i < 1000; ++i) {
      pt.GetLineOffset(rand() % 100000);
    }
  }

  // To show speedup, we could invalidate cache and measure first rebuild
  pt.InvalidateLineCache();
  {
    Timer t("RebuildLineCache (100k lines - SIMD)");
    pt.GetLineOffset(0); // Triggers rebuild
  }
}

void BenchmarkCompaction() {
  std::cout << "\n--- Piece Table Compaction Benchmarks ---" << std::endl;

  PieceTable pt;
  pt.LoadOriginal("Start\n", 6);

  {
    Timer t("1000 Sequential Inserts (Fragmentation)");
    for (int i = 0; i < 1000; ++i) {
      pt.Insert(pt.GetTotalLength(), "a");
    }
  }

  std::cout << "Pieces before manual compaction: " << pt.GetPieceCount()
            << std::endl;
  {
    Timer t("Manual Compaction");
    pt.CompactPieces();
  }
  std::cout << "Pieces after manual compaction:  " << pt.GetPieceCount()
            << std::endl;
}

void BenchmarkViewport() {
  std::cout << "\n--- Viewport Extraction Benchmarks ---" << std::endl;

  Buffer buf;
  std::string massiveData;
  for (int i = 0; i < 1000000; ++i) {
    massiveData += "This is line " + std::to_string(i) + "\n";
  }

  // We need a path for Buffer::OpenFile or we can just use a hacky way to
  // populate it Since Buffer doesn't have a direct "LoadString" (it uses
  // MemoryMappedFile) We'll test PieceTable's part of viewport extraction logic
  // or just use a small mock

  PieceTable pt;
  pt.LoadOriginal(massiveData.data(), massiveData.length());

  {
    Timer t("GetText (100 lines from middle - O(log N))");
    size_t startLine = 500000;
    size_t startOff = pt.GetLineOffset(startLine);
    size_t endOff = pt.GetLineOffset(startLine + 100);
    std::string text = pt.GetText(startOff, endOff - startOff);
  }
}

void BenchmarkSearch() {
  std::cout << "\n--- Search Benchmarks ---" << std::endl;

  Buffer buf;
  // Create a 10MB buffer with a needle at the end
  std::string largeData;
  largeData.reserve(10 * 1024 * 1024);
  for (int i = 0; i < 100000; ++i) {
    largeData += "Some data line " + std::to_string(i) + "\n";
  }
  std::string needle = "TARGET_NEEDLE_123";
  largeData += needle;

  PieceTable pt;
  pt.LoadOriginal(largeData.data(), largeData.length());
  // Since Buffer::Find uses m_pieceTable which is private, we'll just test the
  // logic here Or we could add a test method to Buffer for searching. For now,
  // let's keep it simple.
}

int main() {
  std::cout << "Ecode Performance Optimization Benchmarks" << std::endl;
  std::cout << "==========================================" << std::endl;

  BenchmarkLineIndex();
  BenchmarkCompaction();
  BenchmarkViewport();
  // BenchmarkSearch();

  std::cout << "\nBenchmarks completed." << std::endl;
  return 0;
}
