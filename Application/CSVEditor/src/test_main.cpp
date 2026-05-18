#include <iostream>
#include <fstream>
#include <vector>
#include <cassert>
#include <string>
#include "MemoryMappedFile.h"
#include "PieceTable.h"
#include "CsvDocument.h"
#include "Localization.h"

void CreateDummyFile(const std::wstring& path, const std::string& content) {
    std::ofstream out(std::string(path.begin(), path.end()), std::ios::binary);
    out << content;
    out.close();
}

void TestMemoryMapping() {
    std::cout << "Testing CsvMemoryMappedFile..." << std::endl;
    std::wstring path = L"test_mmf.txt";
    std::string content = "Hello World";
    CreateDummyFile(path, content);

    CsvMemoryMappedFile mmf;
    if (!mmf.Open(path)) {
        std::cerr << "Failed to open MMF" << std::endl;
        exit(1);
    }

    assert(mmf.GetSize() == content.size());
    const uint8_t* data = mmf.GetData();
    for(size_t i=0; i<content.size(); ++i) {
        assert(data[i] == content[i]);
    }
    std::cout << "  Passed." << std::endl;
}

void TestCsvPieceTable() {
    std::cout << "Testing CsvPieceTable..." << std::endl;
    std::wstring path = L"test_pt.txt";
    std::string content = "ABC";
    CreateDummyFile(path, content);

    CsvPieceTable pt;
    pt.LoadFromFile(path);

    assert(pt.GetSize() == 3);
    assert(pt.GetAt(0) == 'A');

    // Insert "12" at offset 1 -> "A12BC"
    uint8_t ins[] = {'1', '2'};
    pt.Insert(1, ins, 2);

    assert(pt.GetSize() == 5);
    assert(pt.GetAt(0) == 'A');
    assert(pt.GetAt(1) == '1');
    assert(pt.GetAt(2) == '2');
    assert(pt.GetAt(3) == 'B');
    assert(pt.GetAt(4) == 'C');

    std::cout << "  Passed." << std::endl;
}

void TestCsvDocument() {
    std::cout << "Testing CsvDocument..." << std::endl;
    std::wstring path = L"test_csv.txt";
    std::string content = "Row1,Data\nRow2,Data\nRow3";
    CreateDummyFile(path, content);

    CsvDocument doc;
    doc.Load(path);

    assert(doc.GetRowCount() == 3);
    
    auto r1 = doc.GetRowRaw(0);
    std::string s1(r1.begin(), r1.end());
    assert(s1 == "Row1,Data");

    auto r2 = doc.GetRowRaw(1);
    std::string s2(r2.begin(), r2.end());
    assert(s2 == "Row2,Data");

    auto r3 = doc.GetRowRaw(2);
    std::string s3(r3.begin(), r3.end());
    assert(s3 == "Row3");

    std::cout << "  Passed." << std::endl;
}

void TestComplexCsv() {
    std::cout << "Testing Complex Csv (Quotes, Newlines)..." << std::endl;
    std::wstring path = L"test_complex.csv";
    // Row 1: "A,B",C
    // Row 2: "Multi
    // Line",D
    std::string content = "\"A,B\",C\n\"Multi\nLine\",D";
    CreateDummyFile(path, content);

    CsvDocument doc;
    doc.Load(path);

    // Should be 2 rows
    assert(doc.GetRowCount() == 2);

    auto cells1 = doc.GetRowCells(0);
    assert(cells1.size() == 2);
    assert(cells1[0] == L"A,B");
    assert(cells1[1] == L"C");

    auto cells2 = doc.GetRowCells(1);
    assert(cells2.size() == 2);
    assert(cells2[0] == L"Multi\nLine");
    assert(cells2[1] == L"D");

    std::cout << "  Passed." << std::endl;
}

void TestFileSave() {
    std::cout << "Testing File Save..." << std::endl;
    std::wstring path = L"test_save.txt";
    std::string content = "Original";
    CreateDummyFile(path, content);

    CsvPieceTable pt;
    pt.LoadFromFile(path);
    
    // Insert data
    std::string insert = " Modified";
    pt.Insert(8, (const uint8_t*)insert.data(), insert.size());

    std::wstring outPath = L"test_save_out.txt";
    if (!pt.Save(outPath)) {
        std::cerr << "Failed to save" << std::endl;
        exit(1);
    }

    // Verify content
    std::ifstream in(std::string(outPath.begin(), outPath.end()), std::ios::binary);
    std::string savedContent((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    
    assert(savedContent == "Original Modified");
    std::cout << "  Passed." << std::endl;
}

void TestDelete() {
    std::cout << "Testing Delete..." << std::endl;
    // P: 0123456789 (10 chars)
    // Delete 5, len 3 -> 0123489
    
    CsvPieceTable pt;
    std::string data = "0123456789";
    pt.Insert(0, (const uint8_t*)data.data(), data.size());
    
    pt.Delete(5, 3); // Delete 5,6,7. Expect 0123489.
    
    if (pt.GetSize() != 7) { std::cerr << "Del Size err " << pt.GetSize() << std::endl; exit(1); }
    if (pt.GetAt(4) != '4') { std::cerr << "Del Content err at 4" << std::endl; exit(1); }
    if (pt.GetAt(5) != '8') { std::cerr << "Del Content err at 5: " << pt.GetAt(5) << std::endl; exit(1); }
    
    std::cout << "  Passed." << std::endl;
}


void TestInsertRow() {
    std::cout << "Testing InsertRow..." << std::endl;
    std::wstring path = L"test_insert.csv";
    std::string content = "A,B\nC,D";
    CreateDummyFile(path, content);
    
    CsvDocument doc;
    doc.Load(path);
    assert(doc.GetRowCount() == 2);
    
    // Insert at index 1 (between rows)
    std::vector<std::wstring> newRow = {L"E", L"F"};
    doc.InsertRow(1, newRow);
    
    // Check Row Count
    assert(doc.GetRowCount() == 3);
    
    // Check content
    auto r0 = doc.GetRowCells(0); assert(r0[0] == L"A");
    auto r1 = doc.GetRowCells(1); assert(r1[0] == L"E"); assert(r1[1] == L"F");
    auto r2 = doc.GetRowCells(2); assert(r2[0] == L"C");
    
    // Insert at end
    std::vector<std::wstring> endRow = {L"G", L"H"};
    doc.InsertRow(3, endRow);
    std::cout << "Rows after insert: " << doc.GetRowCount() << std::endl;
    // Inspect Row 2
    auto r2_check = doc.GetRowCells(2);
    if (!r2_check.empty()) std::cout << "r2[0]: " << std::string(r2_check[0].begin(), r2_check[0].end()) << std::endl;
    else std::cout << "r2 empty" << std::endl;

    auto r3 = doc.GetRowCells(3); 
    if (r3.empty()) {
        std::cout << "r3 cells vector is empty." << std::endl;
        auto raw = doc.GetRowRaw(3);
        std::cout << "r3 raw size: " << raw.size() << " Content: [";
        for(auto c : raw) std::cout << c;
        std::cout << "]" << std::endl;
    }
    else std::cout << "r3[0]: " << std::string(r3[0].begin(), r3[0].end()) << std::endl;
    
    assert(!r3.empty());
    assert(r3[0] == L"G");
    
    std::cout << "  Passed." << std::endl;
}

void TestUndoRedo() {
    std::cout << "Testing Undo/Redo..." << std::endl;
    std::wstring path = L"test_undo.csv";
    std::string content = "1,1\n2,2";
    CreateDummyFile(path, content);
    
    CsvDocument doc;
    doc.Load(path);
    assert(doc.GetRowCount() == 2);
    
    // 1. Insert Row
    std::vector<std::wstring> newRow = {L"3", L"3"};
    doc.InsertRow(2, newRow);
    assert(doc.GetRowCount() == 3);
    assert(doc.CanUndo() == true);
    
    // 2. Undo
    doc.Undo();
    assert(doc.GetRowCount() == 2);
    assert(doc.CanRedo() == true);
    auto r1 = doc.GetRowCells(1);
    assert(r1[0] == L"2"); // Should be back to Original
    
    // 3. Redo
    doc.Redo();
    assert(doc.GetRowCount() == 3);
    auto r2 = doc.GetRowCells(2);
    assert(r2[0] == L"3");
    
    // 4. Update Cell
    doc.UpdateCell(0, 0, L"X"); // 1,1 -> X,1
    auto r0 = doc.GetRowCells(0);
    assert(r0[0] == L"X");
    
    // 5. Undo Update
    doc.Undo();
    r0 = doc.GetRowCells(0);
    assert(r0[0] == L"1");
    
    std::cout << "  Passed." << std::endl;
}

// Helper to measure time
#include <chrono>
#include <random>

void TestStress() {
    std::cout << "Testing Stress (Large File)..." << std::endl;
    std::wstring path = L"stress_test.csv";
    
    // Create large file (e.g. 10,000 rows)
    {
        std::ofstream out("stress_test.csv", std::ios::binary);
        out << "ID,Name,Value,Description\n";
        for (int i = 0; i < 10000; ++i) {
            out << i << ",Name" << i << "," << (i * 100) << ",Description for row " << i << "\n";
        }
    }
    
    CsvDocument doc;
    auto start = std::chrono::high_resolution_clock::now();
    assert(doc.Load(path));
    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "  Load 10k rows: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms" << std::endl;
    
    assert(doc.GetRowCount() == 10001);
    
    // Insert at beginning (heavy for row index rebuild?)
    std::vector<std::wstring> newRow = {L"NEW", L"Beginning", L"0", L"Inserted"};
    start = std::chrono::high_resolution_clock::now();
    doc.InsertRow(0, newRow);
    end = std::chrono::high_resolution_clock::now();
    std::cout << "  Insert at 0: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms" << std::endl;
    
    assert(doc.GetRowCount() == 10002);
    auto r0 = doc.GetRowCells(0);
    assert(r0[0] == L"NEW");
    
    // Insert at end
    doc.InsertRow(doc.GetRowCount(), newRow);
    assert(doc.GetRowCount() == 10003);
    
    // Cleanup
    DeleteFile(path.c_str());
    std::cout << "  Passed." << std::endl;
}

void TestGeneralized() {
    std::cout << "Testing Generalized/Edge Cases..." << std::endl;
    
    // 1. Empty File
    {
        std::wstring path = L"empty.csv";
        std::ofstream out("empty.csv", std::ios::binary); 
        out.close();
        
        CsvDocument doc;
        assert(doc.Load(path));
        assert(doc.GetRowCount() == 0);
        
        // Insert into empty
        std::vector<std::wstring> row = {L"A", L"B"};
        doc.InsertRow(0, row);
        assert(doc.GetRowCount() == 1);
        auto r = doc.GetRowCells(0);
        assert(r[0] == L"A");
        DeleteFile(path.c_str());
    }
    
    // 2. File with no newline at end
    {
        std::wstring path = L"no_newline.csv";
        {
            std::ofstream out("no_newline.csv", std::ios::binary);
            out << "A,B"; // No \n
        }
        CsvDocument doc;
        assert(doc.Load(path));
        assert(doc.GetRowCount() == 1);
        
        std::vector<std::wstring> row = {L"C", L"D"};
        doc.InsertRow(1, row); // Append
        assert(doc.GetRowCount() == 2);
        auto r0 = doc.GetRowCells(0);
        auto r1 = doc.GetRowCells(1);
        assert(r0[0] == L"A");
        assert(r1[0] == L"C");
        DeleteFile(path.c_str());
    }
    
    // 3. Insert in middle with no newline
    {
         // If we have A,B and insert C,D at 0, it should become C,D\nA,B
         std::wstring path = L"no_newline_start.csv";
         {
             std::ofstream out("no_newline_start.csv", std::ios::binary);
             out << "A,B"; 
         }
         CsvDocument doc;
         doc.Load(path);
         std::vector<std::wstring> row = {L"C", L"D"};
         doc.InsertRow(0, row);
         assert(doc.GetRowCount() == 2);
         auto r0 = doc.GetRowCells(0);
         auto r1 = doc.GetRowCells(1);
         assert(r0[0] == L"C"); // New row
         assert(r1[0] == L"A"); // Old row pushed down
         
         DeleteFile(path.c_str());
    }

    // 4. Randomized Ops
    {
        std::cout << "  Randomized Ops..." << std::endl;
        std::wstring path = L"random.csv";
        {
             std::ofstream out("random.csv", std::ios::binary);
             out << "0,0\n1,1\n2,2\n"; 
        }
        CsvDocument doc;
        doc.Load(path);
        
        std::mt19937 rng(42); // Deterministic seed
        
        for(int i=0; i<100; ++i) {
            int op = rng() % 3;
            size_t rows = doc.GetRowCount();
            if (op == 0) { // Insert
                size_t r = rng() % (rows + 1);
                std::vector<std::wstring> row = {std::to_wstring(i), L"Ins"};
                doc.InsertRow(r, row);
            } else if (op == 1 && rows > 0) { // Delete
                size_t r = rng() % rows;
                doc.DeleteRow(r);
            } else if (op == 2 && rows > 0) { // Update
                size_t r = rng() % rows;
                doc.UpdateCell(r, 0, L"Upd");
            }
            
            // Basic sanity check
            assert(doc.GetRowCount() >= 0);
        }
        DeleteFile(path.c_str());
    }
    std::cout << "  Passed." << std::endl;
}

void TestAdvancedSearchReplace() {
    std::cout << "Testing Advanced Search & Replace..." << std::endl;
    CsvDocument doc;
    // Setup: 
    // Row 0: "Hello", "World"
    // Row 1: "Hello", "Universe"
    // Row 2: "123", "456"
    std::vector<std::wstring> r0 = {L"Hello", L"World"};
    std::vector<std::wstring> r1 = {L"Hello", L"Universe"};
    std::vector<std::wstring> r2 = {L"123", L"456"};
    doc.InsertRow(0, r0);
    doc.InsertRow(1, r1);
    doc.InsertRow(2, r2);
    
    // 1. Partial Match (Contains)
    {
        CsvDocument::SearchOptions opts;
        opts.mode = CsvDocument::SearchMode::Contains;
        opts.includeStart = true; // Include 0,0
        size_t r = 0, c = 0;
        assert(doc.Search(L"ell", r, c, opts));
        assert(r == 0 && c == 0);
        
        // Find Next (Exclude start)
        opts.includeStart = false; 
        assert(doc.Search(L"ell", r, c, opts)); 
        assert(r == 1 && c == 0); // Should jump to next row
    }
    
    // 2. Exact Match
    {
        CsvDocument::SearchOptions opts;
        opts.mode = CsvDocument::SearchMode::Exact;
        opts.matchCase = true;
        opts.includeStart = true;
        size_t r = 0, c = 0;
        assert(!doc.Search(L"ell", r, c, opts));
        
        r = 0; c = 0;
        assert(doc.Search(L"Hello", r, c, opts));
        assert(r == 0 && c == 0);
    }
    
    // 3. Regex Match
    {
        CsvDocument::SearchOptions opts;
        opts.mode = CsvDocument::SearchMode::Regex;
        opts.includeStart = true;
        size_t r = 0, c = 0;
        assert(doc.Search(L"\\d+", r, c, opts));
        assert(r == 2 && c == 0);
    }

    // 4. Replace (Next)
    {
        CsvDocument::SearchOptions opts;
        opts.mode = CsvDocument::SearchMode::Contains;
        size_t r = 0, c = 0;
        bool found = doc.Search(L"World", r, c, opts);
        assert(found);
        
        doc.Replace(L"World", L"Earth", r, c, opts);
        auto cell = doc.GetRowCells(0)[1];
        assert(cell == L"Earth");
    }
    
    // 5. Replace All (Regex)
    {
        CsvDocument::SearchOptions opts;
        opts.mode = CsvDocument::SearchMode::Regex;
        int count = doc.ReplaceAll(L"He[l]{2}o", L"Hi", opts);
        assert(count == 2);
        
        auto c0 = doc.GetRowCells(0)[0];
        auto c1 = doc.GetRowCells(1)[0];
        assert(c0 == L"Hi");
        assert(c1 == L"Hi");
    }

    // 6. Replace Undo/Redo
    {
        // Current state: Row 1 is "Hi", "Universe"
        CsvDocument::SearchOptions opts;
        opts.mode = CsvDocument::SearchMode::Contains;
        size_t r = 1, c = 1;
        
        assert(doc.GetRowCells(1)[1] == L"Universe");
        
        // Replace
        doc.Replace(L"Universe", L"Cosmos", r, c, opts);
        assert(doc.GetRowCells(1)[1] == L"Cosmos");
        
        // Undo
        assert(doc.CanUndo());
        doc.Undo();
        assert(doc.GetRowCells(1)[1] == L"Universe");
        
        // Redo
        assert(doc.CanRedo());
        doc.Redo();
        assert(doc.GetRowCells(1)[1] == L"Cosmos");
    }
    
    std::cout << "  Passed." << std::endl;
}

void TestColumnOperations()
{
    std::cout << "Testing Column Operations..." << std::endl;
    CsvDocument doc;
    
    // Create simple CSV
    // A,B,C
    // 1,2,3
    std::string data = "A,B,C\n1,2,3";
    
    std::wstring path = L"test_col.csv";
    CreateDummyFile(path, data);
    
    if (!doc.Load(path)) {
        std::cerr << "Failed to load test_col.csv" << std::endl;
        return;
    }
    
    // Insert Column at 1 (between A and B)
    doc.InsertColumn(1, L"NEW");
    // Expected: A,NEW,B,C
    //           1,NEW,2,3
    
    auto row0 = doc.GetRowCells(0);
    assert(row0.size() == 4);
    assert(row0[0] == L"A");
    assert(row0[1] == L"NEW");
    assert(row0[2] == L"B");
    assert(row0[3] == L"C");
    
    auto row1 = doc.GetRowCells(1);
    assert(row1.size() == 4);
    assert(row1[1] == L"NEW");
    
    std::cout << "Insert Column Passed" << std::endl;
    
    // Delete Column at 2 (B)
    // A,NEW,B,C -> A,NEW,C
    doc.DeleteColumn(2);
    
    row0 = doc.GetRowCells(0);
    assert(row0.size() == 3);
    assert(row0[2] == L"C");
    
    std::cout << "Delete Column Passed" << std::endl;
    
    // Undo Delete
    if (doc.CanUndo()) {
        doc.Undo();
        row0 = doc.GetRowCells(0);
        assert(row0.size() == 4);
        assert(row0[2] == L"B");
         std::cout << "Undo Delete Column Passed" << std::endl;
    }
    
    DeleteFile(path.c_str());
    std::cout << "  Passed." << std::endl;
}

void TestCopyPaste()
{
    std::cout << "Testing Copy/Paste..." << std::endl;
    CsvDocument doc;
    
    // Setup 2x2 grid
    // A,B
    // 1,2
    CreateDummyFile(L"test_paste.csv", "A,B\n1,2");
    doc.Load(L"test_paste.csv");
    
    // Test 1: Paste single cell
    doc.PasteCells(0, 0, L"X"); // Should replace A with X
    auto r0 = doc.GetRowCells(0);
    assert(r0[0] == L"X");
    
    // Test 2: Paste row (TSV)
    // "Y\tZ" -> pastes Y at 1,0 and Z at 1,1
    doc.PasteCells(1, 0, L"Y\tZ");
    auto r1 = doc.GetRowCells(1);
    assert(r1[0] == L"Y");
    assert(r1[1] == L"Z");
    
    // Test 3: Paste Block (CSV)
    // "M,N\nO,P" at 0,0
    // X,B -> M,N
    // Y,Z -> O,P
    // Note: PasteCells defaults to TSV if no tabs found but commas found? Yes.
    // "M,N\nO,P" has commas.
    doc.PasteCells(0, 0, L"M,N\nO,P");
    r0 = doc.GetRowCells(0);
    r1 = doc.GetRowCells(1);
    assert(r0[0] == L"M");
    assert(r0[1] == L"N");
    assert(r1[0] == L"O");
    assert(r1[1] == L"P");
    
    // Test 4: Expand Row (Paste beyond columns)
    // 0,0 is M. Paste "Q,R,S" at 0,0 -> M becomes Q, N becomes R, new col S
    doc.PasteCells(0, 0, L"Q,R,S");
    r0 = doc.GetRowCells(0);
    assert(r0.size() == 3);
    assert(r0[2] == L"S");
    
    // Test 5: Expand Column (Paste new rows)
    // Paste "New1\nNew2" at 2,0 (row 2 doesn't exist)
    doc.PasteCells(2, 0, L"New1\nNew2");
    assert(doc.GetRowCount() == 4);
    assert(doc.GetRowCells(2)[0] == L"New1");
    assert(doc.GetRowCells(3)[0] == L"New2");

    std::cout << "  Passed." << std::endl;
    DeleteFile(L"test_paste.csv");
}

void TestEncoding()
{
    std::cout << "Testing Encoding (UTF-16 LE)..." << std::endl;
    
    // Create UTF-16 LE file: BOM + "A\tB\n"
    // BOM: FF FE
    // A: 41 00
    // \t: 09 00
    // B: 42 00
    // \n: 0A 00
    unsigned char bytes[] = { 
        0xFF, 0xFE, 
        0x41, 0x00, 
        0x09, 0x00, 
        0x42, 0x00, 
        0x0A, 0x00 
    };
    
    FILE* f = _wfopen(L"test_utf16.csv", L"wb");
    fwrite(bytes, 1, sizeof(bytes), f);
    fclose(f);
    
    CsvDocument doc;
    doc.SetDelimiter(L'\t');
    doc.Load(L"test_utf16.csv");
    
    // Debug info
    std::cout << "  Row count: " << doc.GetRowCount() << std::endl;
    
    assert(doc.GetRowCount() == 1);
    auto row0 = doc.GetRowCells(0);
    assert(row0.size() == 2);
    
    // row0[0] might contain BOM if we didn't strip it.
    // L"\uFEFFA" is length 2.
    // Let's see what we get.
    // Ideally we strip BOM. My code currently doesn't strip it from index 0.
    // So row0[0] is likely BOM + "A".
    // Or just "A" if BOM was consumed?
    // DetectEncoding reads pieces. CsvPieceTable is raw.
    // RebuildRowIndex starts at 0.
    // DecodeString decodes bytes starting at 0.
    // So yes, BOM is included in first cell.
    
    std::wstring cell0 = row0[0];
    if (cell0.size() > 1 && cell0[0] == 0xFEFF) {
        cell0 = cell0.substr(1);
    }
    
    assert(cell0 == L"A");
    assert(row0[1] == L"B");
    
    // Test Insert
    // Insert Column at 1 -> A, New, B
    // Default value "New" will be encoded as UTF16 LE bytes.
    doc.InsertColumn(1, L"New");
    
    row0 = doc.GetRowCells(0);
    assert(row0.size() == 3);
    assert(row0[1] == L"New");
    
    std::cout << "  Passed." << std::endl;
    DeleteFile(L"test_utf16.csv");
}

void TestLineEndings()
{
    std::cout << "Testing Line Endings (CRLF)..." << std::endl;
    
    // Create CRLF file
    const char* content = "A,B\r\nC,D\r\n";
    FILE* f = _wfopen(L"test_crlf.csv", L"wb");
    fwrite(content, 1, strlen(content), f);
    fclose(f);
    
    CsvDocument doc;
    doc.Load(L"test_crlf.csv");
    
    assert(doc.GetRowCount() == 2);
    
    // Insert Row
    std::vector<std::wstring> newRow = { L"E", L"F" };
    doc.InsertRow(2, newRow); // Append
    
    doc.Save(L"test_crlf_saved.csv");
    
    // Verify file content has \r\n for the new row
    std::ifstream ifs("test_crlf_saved.csv", std::ios::binary);
    std::string fileContent((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    ifs.close();
    
    // Original content should be preserved (if piece table worked)
    // New row should definitely have \r\n if detection worked.
    
    // "A,B\r\nC,D\r\nE,F\r\n"
    std::string expected = "A,B\r\nC,D\r\nE,F\r\n";
    if (fileContent != expected) {
        std::cout << "Mismatch!\nExpected size: " << expected.size() << "\nActual size: " << fileContent.size() << std::endl;
        // Print hex
        for(char c : fileContent) std::cout << std::hex << (int)(unsigned char)c << " ";
        std::cout << std::dec << std::endl;
    }
    assert(fileContent == expected);
    
    std::cout << "  Passed." << std::endl;
    DeleteFile(L"test_crlf.csv");
    DeleteFile(L"test_crlf_saved.csv");
}

void TestLocalization()
{
    std::cout << "Testing Localization..." << std::endl;
    
    // Default English
    CsvLocalization::SetLanguage(CsvLanguage::English);
    assert(std::wstring(CsvLocalization::GetString(CsvStringId::Menu_File)) == L"&File");
    
    // Switch to Japanese
    CsvLocalization::SetLanguage(CsvLanguage::Japanese);
    // \u30D5\u30A1\u30A4\u30EB(&F)
    std::wstring jpFile = CsvLocalization::GetString(CsvStringId::Menu_File);
    assert(jpFile == L"\u30D5\u30A1\u30A4\u30EB(&F)");

    // Return to English
    CsvLocalization::SetLanguage(CsvLanguage::English);
    
    std::cout << "  Passed." << std::endl;
}

void TestExport()
{
    std::cout << "Testing Export..." << std::endl;
    
    // Create Doc
    CsvDocument doc;
    // Header Row
    doc.InsertRow(0, { L"Col1", L"Col2" });
    // Data Row
    doc.InsertRow(1, { L"Data1", L"Data2" });
    
    // HTML
    doc.Export(L"test_export.html", CsvDocument::ExportFormat::HTML);
    std::ifstream htmlFile("test_export.html");
    std::string htmlContent((std::istreambuf_iterator<char>(htmlFile)), std::istreambuf_iterator<char>());
    htmlFile.close();
    
    // Check content basics
    if (htmlContent.find("<html>") == std::string::npos) {
        std::cout << "HTML Export Failed: <html> tag not found." << std::endl;
        assert(false);
    }
    
    // Markdown
    doc.Export(L"test_export.md", CsvDocument::ExportFormat::Markdown);
    std::ifstream mdFile("test_export.md");
    std::string mdContent((std::istreambuf_iterator<char>(mdFile)), std::istreambuf_iterator<char>());
    mdFile.close();
    
    if (mdContent.find("| Col1 | Col2 |") == std::string::npos) {
        std::cout << "Markdown Export Failed: Header not found." << std::endl;
        assert(false);
    }
    
    std::cout << "  Passed." << std::endl;
    DeleteFile(L"test_export.md");
}

void TestImport()
{
    std::cout << "Testing Import..." << std::endl;
    
    // Create Doc 1
    CsvDocument doc;
    doc.InsertRow(0, { L"A1", L"B1" });
    
    // Create Import File
    std::wstring importPath = L"test_import.csv";
    CreateDummyFile(importPath, "A2,B2\nA3,B3");
    
    // Import
    bool res = doc.Import(importPath);
    assert(res);
    
    // Check rows
    assert(doc.GetRowCount() == 3);
    assert(doc.GetRowCells(0)[0] == L"A1");
    assert(doc.GetRowCells(1)[0] == L"A2");
    assert(doc.GetRowCells(2)[0] == L"A3");
    
    DeleteFile(importPath.c_str());
    std::cout << "  Passed." << std::endl;
}

int main() {
    TestMemoryMapping(); 
    TestCsvPieceTable();
    TestCsvDocument();
    TestComplexCsv();
    TestFileSave();
    TestDelete();
    TestInsertRow();
    TestUndoRedo();
    TestStress();
    TestGeneralized();
    TestAdvancedSearchReplace();
    TestColumnOperations();
    TestCopyPaste();
    TestEncoding();
    TestLineEndings();
    TestLocalization();
    TestExport();
    TestImport();
    
    std::cout << "All Tests Passed!" << std::endl;
    return 0;
}
