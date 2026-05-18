#pragma once
#include <cstdint>
#include <windows.h>


#pragma pack(push, 1)

// exFAT Main Boot Region
struct EXFAT_BOOT_SECTOR {
  uint8_t Jump[3];
  uint8_t FileSystemName[8]; // "EXFAT   "
  uint8_t MustBeZero[53];
  uint64_t PartitionOffset;
  uint64_t VolumeLength;
  uint32_t FatOffset;
  uint32_t FatLength;
  uint32_t ClusterHeapOffset;
  uint32_t ClusterCount;
  uint32_t RootDirectoryCluster;
  uint32_t VolumeSerialNumber;
  uint16_t FileSystemRevision;
  uint16_t VolumeFlags;
  uint8_t BytesPerSectorShift;
  uint8_t SectorsPerClusterShift;
  uint8_t NumberOfFats;
  uint8_t DriveSelect;
  uint8_t PercentInUse;
  uint8_t Reserved[7];
  uint8_t BootCode[390];
  uint16_t BootSignature;
};

// Generic Directory Entry
struct EXFAT_DIRECTORY_ENTRY {
  uint8_t EntryType;
  uint8_t Custom[31];
};

// File Directory Entry (0x85)
struct EXFAT_FILE_ENTRY {
  uint8_t EntryType; // 0x85
  uint8_t SecondaryCount;
  uint16_t SetChecksum;
  uint16_t FileAttributes;
  uint16_t Reserved1;
  uint32_t CreateTimestamp;
  uint32_t LastModifiedTimestamp;
  uint32_t LastAccessedTimestamp;
  uint8_t Create10msIncrement;
  uint8_t LastModified10msIncrement;
  uint8_t CreateUtcOffset;
  uint8_t LastModifiedUtcOffset;
  uint8_t LastAccessedUtcOffset;
  uint8_t Reserved2[7];
};

// Stream Extension Directory Entry (0xC0)
struct EXFAT_STREAM_EXTENSION_ENTRY {
  uint8_t EntryType; // 0xC0
  uint8_t GeneralSecondaryFlags;
  uint8_t Reserved1;
  uint8_t NameLength;
  uint16_t NameHash;
  uint16_t Reserved2;
  uint64_t ValidDataLength;
  uint32_t Reserved3;
  uint32_t FirstCluster;
  uint64_t DataLength;
};

// File Name Directory Entry (0xC1)
struct EXFAT_FILENAME_ENTRY {
  uint8_t EntryType; // 0xC1
  uint8_t GeneralSecondaryFlags;
  wchar_t FileName[15];
};

#pragma pack(pop)

// Entry Types
const uint8_t EXFAT_ENTRY_TYPE_END = 0x00;
const uint8_t EXFAT_ENTRY_TYPE_ALLOC_BITMAP = 0x81;
const uint8_t EXFAT_ENTRY_TYPE_UPCASE_TABLE = 0x82;
const uint8_t EXFAT_ENTRY_TYPE_VOLUME_LABEL = 0x83;
const uint8_t EXFAT_ENTRY_TYPE_FILE = 0x85;
const uint8_t EXFAT_ENTRY_TYPE_STREAM_EXT = 0xC0;
const uint8_t EXFAT_ENTRY_TYPE_FILENAME = 0xC1;

// Flags
const uint8_t EXFAT_FLAG_ALLOCATION_POSSIBLE = 0x01;
const uint8_t EXFAT_FLAG_NO_FAT_CHAIN = 0x02;
