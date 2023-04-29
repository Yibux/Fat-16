//
// Created by root on 1/11/23.
//

#ifndef PROJEKT_FAT16_FILE_READER_H
#define PROJEKT_FAT16_FILE_READER_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>

#define bytesPerSector 512


struct disk_t{
    FILE *f;
};
struct disk_t* disk_open_from_file(const char* volume_file_name);
int disk_read(struct disk_t* pdisk, int32_t first_sector, void* buffer, int32_t sectors_to_read);
int disk_close(struct disk_t* pdisk);

struct fat_super_t {
    uint8_t jumpCode[3];
    char oemName[8];

    uint16_t bytesPerSectorValue;
    uint8_t sectorsPerCluster;
    uint16_t sectorsReserved;
    uint8_t fatCount;
    uint16_t rootDirectoryCapacity;
    uint16_t logicalSectors16;
    uint8_t  reservedData;
    uint16_t sectorsPerFat;

    uint32_t reserved2Data;

    uint32_t hiddenSectors;
    uint32_t logicalSectors32;

    uint16_t reserved3Data;
    uint8_t reserved4Data;

    uint32_t serialNumberValue;

    char label[11];
    char fsid[8];

    uint8_t bootCode[448];
    uint16_t magicLastVal; // 55 aa
} __attribute__(( packed ));

struct volume_t{
    uint32_t firstFat1Position;
    uint32_t secondFat2Position;
    uint32_t rootDirectoryPosition;
    uint32_t sectorsPerRootDirectory;
    uint32_t secondCluster2Position;
    uint32_t volumeSize;
    uint32_t userSize;
    uint32_t numberOfClusterPerVolume;
    struct disk_t *file;
    struct fat_super_t *superSector;
};

struct volume_t* fat_open(struct disk_t* pdisk, uint32_t first_sector);
int fat_close(struct volume_t* pvolume);




struct fat_entry_t {
    char name[8];
    char extenstion[3];
    uint8_t attributes;
    uint8_t some_data[6]; // todo: do wczytania później
    uint16_t some_data2;
    uint16_t high_cluster_index;
    uint16_t some_data3[2];
    uint16_t low_cluster_index;
    uint32_t file_size;
} __attribute__(( packed ));

struct file_t
{
    struct volume_t* volumePointer;
    uint16_t filePosition;
    uint32_t fileSize;
    struct clusters_chain_t *chains;
};
struct clusters_chain_t {
    uint16_t *clusters;
    size_t size;
};
struct file_t* file_open(struct volume_t* pvolume, const char* file_name);
int file_close(struct file_t* stream);
size_t file_read(void *ptr, size_t size, size_t nmemb, struct file_t *stream);
int32_t file_seek(struct file_t* stream, int32_t offset, int whence);
struct clusters_chain_t *get_chain_fat16(const void * const buffer, size_t size, uint16_t first_cluster);
int whichIndex(const char * s);


struct dir_t{
    struct volume_t *volumeStruct;
    bool wasOpened;
    uint16_t filePosition;
};
struct creation_date{
    unsigned int day, month, year;
};

struct creation_time{
    unsigned int hour, minute, second;
};

struct dir_entry_t{
    char name[13];
    uint32_t size;
    bool is_readonly; // 1
    bool is_hidden; // 2
    bool is_system;
    bool is_archived;
    bool is_directory;
    struct creation_date creationDate;
    struct creation_time creationTime;
}__attribute__(( packed ));

struct dir_t* dir_open(struct volume_t* pvolume, const char* dir_path);
int dir_read(struct dir_t* pdir, struct dir_entry_t* pentry);
int dir_close(struct dir_t* pdir);

#endif //PROJEKT_FAT16_FILE_READER_H
