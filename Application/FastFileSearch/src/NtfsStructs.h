#pragma once
#include <windows.h>
#include <cstdint>

#pragma pack(push, 1)

// NTFS Boot Sector (partial, for validation or info)
struct BOOT_SECTOR {
    uint8_t Jump[3];
    uint8_t OemID[8];
    uint16_t BytesPerSector;
    uint8_t SectorsPerCluster;
    uint16_t ReversedReservedSectors;
    uint8_t AlwaysZero1[3];
    uint16_t AlwaysZero2;
    uint8_t MediaDescriptor;
    uint16_t AlwaysZero3;
    uint16_t SectorsPerTrack;
    uint16_t NumberOfHeads;
    uint32_t HiddenSectors;
    uint32_t NotUsed;
    uint32_t NotUsed2;
    uint64_t TotalSectors;
    uint64_t MftStartLcn;
    uint64_t Mft2StartLcn;
    uint32_t ClustersPerFileRecord;
    uint32_t ClustersPerIndexBlock;
    uint64_t VolumeSerialNumber;
    uint32_t Checksum;
};

// File Record Header (MFT Record)
struct FILE_RECORD_HEADER {
    uint32_t Magic;              // "FILE"
    uint16_t UpdateSequenceOffset;
    uint16_t UpdateSequenceSize; // Size in words
    uint64_t LogSequenceNumber;
    uint16_t SequenceNumber;
    uint16_t HardLinkCount;
    uint16_t AttributeOffset;
    uint16_t Flags;              // 0x01 = InUse, 0x02 = Directory
    uint32_t RealSize;           // Used size of MFT record
    uint32_t AllocatedSize;      // Allocated size of MFT record
    uint64_t BaseFileRecord;     // Ref to base record (if this is extension)
    uint16_t NextAttributeID;
    uint16_t Align;
    uint32_t MFTRecordNumber;    // (XP and later)
};

// General Attribute Header
struct ATTRIBUTE_HEADER {
    uint32_t TypeID;
    uint32_t Length;
    uint8_t NonResidentFlag;
    uint8_t NameLength;
    uint16_t NameOffset;
    uint16_t Flags;
    uint16_t AttributeID;
};

// Resident Attribute Header
struct RESIDENT_ATTRIBUTE_HEADER {
    ATTRIBUTE_HEADER Header;
    uint32_t ValueLength;
    uint16_t ValueOffset;
    uint8_t IndexedFlag;
    uint8_t Padding;
};

// Standard Information Attribute (0x10)
struct STANDARD_INFORMATION {
    uint64_t CreationTime;
    uint64_t FileChangeTime;
    uint64_t MftChangeTime;
    uint64_t AccessTime;
    uint32_t FileAttributes;
    uint32_t MaxNumVersions;
    uint32_t VersionNumber;
    uint32_t ClassID;
    uint32_t OwnerID;
    uint32_t SecurityID;
    uint64_t QuotaCharge;
    uint64_t USN;
};

// Filename Attribute (0x30)
struct FILE_NAME_ATTRIBUTE {
    uint64_t ParentDirectoryRef; // File reference to the parent directory
    uint64_t CreationTime;
    uint64_t ChangeTime;
    uint64_t LastWriteTime;
    uint64_t LastAccessTime;
    uint64_t AllocatedSize;
    uint64_t DataSize;
    uint32_t FileAttributes;
    uint32_t AlignmentOrReserved;
    uint8_t NameLength;
    uint8_t NameType; // 0x01 = Long, 0x02 = Short, 0x03 = Both
    wchar_t Name[1];
};

#pragma pack(pop)

// Attribute Types
const uint32_t AttributeStandardInformation = 0x10;
const uint32_t AttributeAttributeList = 0x20;
const uint32_t AttributeFileName = 0x30;
const uint32_t AttributeObjectId = 0x40;
const uint32_t AttributeSecurityDescriptor = 0x50;
const uint32_t AttributeVolumeName = 0x60;
const uint32_t AttributeVolumeInformation = 0x70;
const uint32_t AttributeData = 0x80;
const uint32_t AttributeIndexRoot = 0x90;
const uint32_t AttributeIndexAllocation = 0xA0;
const uint32_t AttributeBitmap = 0xB0;
const uint32_t AttributeReparsePoint = 0xC0;
const uint32_t AttributeEAInformation = 0xD0;
const uint32_t AttributeEA = 0xE0;
const uint32_t AttributePropertySet = 0xF0;
const uint32_t AttributeLoggedUtilityStream = 0x100;
const uint32_t AttributeEnd = 0xFFFFFFFF;
