#pragma once
#include <cstdint>
#include <windows.h>

#pragma pack(push, 1)

// FAT BIOS Parameter Block (BPB) for FAT16
struct FAT16_BPB {
  uint8_t Jump[3];
  uint8_t OemID[8];
  uint16_t BytesPerSector;
  uint8_t SectorsPerCluster;
  uint16_t ReservedSectors;
  uint8_t Fats;
  uint16_t RootEntries;
  uint16_t SmallSectors;
  uint8_t MediaDescriptor;
  uint16_t SectorsPerFat;
  uint16_t SectorsPerTrack;
  uint16_t NumberOfHeads;
  uint32_t HiddenSectors;
  uint32_t LargeSectors;
  uint8_t PhysicalDriveNumber;
  uint8_t CurrentHead;
  uint8_t Signature;
  uint32_t VolumeSerialNumber;
  uint8_t VolumeLabel[11];
  uint8_t SystemID[8];
};

// FAT BIOS Parameter Block (BPB) for FAT32
struct FAT32_BPB {
  uint8_t Jump[3];
  uint8_t OemID[8];
  uint16_t BytesPerSector;
  uint8_t SectorsPerCluster;
  uint16_t ReservedSectors;
  uint8_t Fats;
  uint16_t RootEntries;
  uint16_t SmallSectors;
  uint8_t MediaDescriptor;
  uint16_t SectorsPerFat16;
  uint16_t SectorsPerTrack;
  uint16_t NumberOfHeads;
  uint32_t HiddenSectors;
  uint32_t LargeSectors;
  // FAT32 Extended fields
  uint32_t SectorsPerFat32;
  uint16_t ExtFlags;
  uint16_t FSVersion;
  uint32_t RootCluster;
  uint16_t FSInfo;
  uint16_t BkBootSec;
  uint8_t Reserved[12];
  uint8_t PhysicalDriveNumber;
  uint8_t CurrentHead;
  uint8_t Signature;
  uint32_t VolumeSerialNumber;
  uint8_t VolumeLabel[11];
  uint8_t SystemID[8];
};

// Standard Directory Entry (SFN)
struct FAT_DIRECTORY_ENTRY {
  uint8_t Name[11]; // 8.3 format
  uint8_t Attributes;
  uint8_t Reserved;
  uint8_t CreateTimeTenth;
  uint16_t CreateTime;
  uint16_t CreateDate;
  uint16_t LastAccessDate;
  uint16_t FirstClusterHigh;
  uint16_t WriteTime;
  uint16_t WriteDate;
  uint16_t FirstClusterLow;
  uint32_t FileSize;
};

// Long Filename Entry (LFN)
struct FAT_LFN_ENTRY {
  uint8_t SequenceNumber;
  wchar_t Name1[5];
  uint8_t Attributes; // Always 0x0F
  uint8_t Type;       // Always 0x00
  uint8_t Checksum;
  wchar_t Name2[6];
  uint16_t FirstCluster; // Always 0x0000
  wchar_t Name3[2];
};

#pragma pack(pop)

// Attributes
const uint8_t FAT_ATTR_READ_ONLY = 0x01;
const uint8_t FAT_ATTR_HIDDEN = 0x02;
const uint8_t FAT_ATTR_SYSTEM = 0x04;
const uint8_t FAT_ATTR_VOLUME_ID = 0x08;
const uint8_t FAT_ATTR_DIRECTORY = 0x10;
const uint8_t FAT_ATTR_ARCHIVE = 0x20;
const uint8_t FAT_ATTR_LFN = 0x0F;
