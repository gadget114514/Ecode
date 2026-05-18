# MFT Reader Internals

This document details the internal logic of the `FastFileSearch` MFT Reader.

## Overview
The application bypasses the Windows File System API (FindFirstFile/FindNextFile) for speed by reading the **Master File Table (MFT)** ($MFT) directly from the NTFS volume.

## Core Components

### 1. Initialization (`Initialize`)
- Opens a handle to the raw volume (e.g., `\\.\D:`) using `CreateFile`.
- Queries volume metadata via `FSCTL_GET_NTFS_VOLUME_DATA`.
- **Critical Fix**: Correctly determines proper `RecordSize`. Early versions heavily relied on `ClustersPerFileRecordSegment`. The current implementation prefers `BytesPerFileRecordSegment` to avoid signed/unsigned interpretation errors (e.g., 0xF6 representing 1024 bytes).

### 2. MFT Location
- The MFT itself is a file. Its location is defined in the Volume Boot Record.
- We read the start LCN (Logical Cluster Number) of $MFT.
- We read the first record of the MFT (Record 0) to find the **DATA attribute** (0x80) of the MFT itself.
- This DATA attribute contains "Data Runs" (fragments) describing where the MFT physically resides on the disk.

### 3. Scanning (`Scan`)
- The scanner iterates through the Data Runs of the MFT.
- It reads 1MB chunks of raw disk data.
- It iterates through each `RecordSize` (usually 1024 KB) block in the buffer.
- **Parsing**: `ParseRecord` validates the `FILE` signature (0x454C4946).
- **Attributes**: It walks the attributes to find:
  - `0x10` ($STANDARD_INFORMATION): For file timestamps.
  - `0x30` ($FILE_NAME): For filename and parent directory reference.

### 4. Search & Filtering
- **Prefix Matching**: To support "Folder Search", we check if a file's full path starts with the filtered prefix (case-insensitive).
- **Limit**: Search results are capped (default 50,000) to prevent UI thread hangs.

## Debugging Flags
The `test_console.exe` tool supports:
- **`-v` (Verbose)**: Prints every valid MFT record found. Implemented via `scanDebugCallback` in `MFTReader::Scan`.
- **`-t` (Trace)**: Traces high-level stages. Use this if `Scan` appears to hang or exit silently. It logs volume initialization parameters and run list decoding.

## Known Limitations
- **Resident Files**: Currently optimized for typical files. Highly fragmented MFTs or complex resident attributes might be simplified.
- **Admin Rights**: Strictly required. The app cannot function without them.
