//
// Created by root on 1/11/23.
//

#include <string.h>
#include <ctype.h>
#include "file_reader.h"
#include "tested_declarations.h"
#include "rdebug.h"
#include "tested_declarations.h"
#include "rdebug.h"
#include "tested_declarations.h"
#include "rdebug.h"
#include "tested_declarations.h"
#include "rdebug.h"
struct disk_t* disk_open_from_file(const char* volume_file_name)
{
    if(!volume_file_name)
    {
        errno = EFAULT;
        return NULL;
    }
    struct disk_t *file = calloc(1,sizeof(struct disk_t));
    if(!file)
    {
        errno = ENOMEM;
        return NULL;
    }

    file->f = fopen(volume_file_name,"rb");
    if(!file->f)
    {
        free(file);
        file = NULL;
        errno = ENONET;
        return NULL;
    }
    return file;
}

int disk_read(struct disk_t* pdisk, int32_t first_sector, void* buffer, int32_t sectors_to_read)
{
    if(!pdisk || !buffer)
    {
        errno = EFAULT;
        return -1;
    }

    int spr1 = fseek(pdisk->f,first_sector*bytesPerSector,SEEK_SET);
    if(spr1<0)
    {
        errno = ERANGE;
        return -1;
    }

    if((int)fread(buffer,bytesPerSector,sectors_to_read,pdisk->f) != (int)sectors_to_read)
    {
        errno = ERANGE;
        return -1;
    }


    return (int)sectors_to_read;
}

int disk_close(struct disk_t* pdisk)
{
    if(!pdisk)
    {
        errno = EFAULT;
        return -1;
    }
    fclose(pdisk->f);
    free(pdisk);
    pdisk = NULL;
    return 0;
}

/////////////////////////////////////////////////////

struct volume_t* fat_open(struct disk_t* pdisk, uint32_t first_sector)
{
    if(!pdisk)
    {
        errno = EFAULT;
        return NULL;
    }

    uint32_t volumeStartZero = 0;
    uint8_t *dataTable = calloc(1,bytesPerSector);
    if(!dataTable)
    {
        errno = ENOMEM;
        return NULL;
    }
    int output = disk_read(pdisk, (int32_t)first_sector, dataTable, 1);
    if(output == -1)
    {
        errno = EINVAL;
        free(dataTable);
        return NULL;
    }
    struct fat_super_t *superSector = (struct fat_super_t *)dataTable;
    if(superSector->bytesPerSectorValue == 0)
    {
        errno = EINVAL;
        free(dataTable);
        return NULL;
    }
    struct volume_t* volumeStruct = calloc(1,sizeof(struct volume_t));
    if(!volumeStruct)
    {
        errno = ENOMEM;
        free(dataTable);
        return NULL;
    }
    volumeStruct->superSector = calloc(1,sizeof(struct fat_super_t));
    if(!volumeStruct->superSector)
    {
        errno = ENOMEM;
        free(dataTable);
        free(volumeStruct);
        return NULL;
    }
    volumeStruct->firstFat1Position = volumeStartZero + superSector->sectorsReserved;
    volumeStruct->secondFat2Position = volumeStartZero + superSector->sectorsReserved + superSector->sectorsPerFat;
    volumeStruct->rootDirectoryPosition = volumeStartZero  + superSector->sectorsReserved + superSector->fatCount * superSector->sectorsPerFat;
    volumeStruct->sectorsPerRootDirectory = (superSector->rootDirectoryCapacity * sizeof(struct fat_entry_t)) / superSector->bytesPerSectorValue;
    if ((superSector->rootDirectoryCapacity * sizeof(struct fat_entry_t)) % superSector->bytesPerSectorValue != 0)
    {
        volumeStruct->sectorsPerRootDirectory++;
    }

    volumeStruct->secondCluster2Position = volumeStruct->rootDirectoryPosition + volumeStruct->sectorsPerRootDirectory;

    volumeStruct->volumeSize = superSector->logicalSectors16 == 0 ? superSector->logicalSectors32 : superSector->logicalSectors16;
    volumeStruct->userSize = volumeStruct->volumeSize - (superSector->fatCount * superSector->sectorsPerFat) - superSector->sectorsReserved - volumeStruct->sectorsPerRootDirectory;
    volumeStruct->numberOfClusterPerVolume = volumeStruct->userSize / superSector->sectorsPerCluster;

    if(memcmp(dataTable+volumeStruct->firstFat1Position,dataTable+volumeStruct->secondFat2Position, volumeStruct->volumeSize * bytesPerSector) == 0
    || superSector->magicLastVal != 0xaa55)
    {
        errno = EINVAL;
        free(dataTable);
        free(volumeStruct);
        return NULL;
    }

    volumeStruct->file = pdisk;
    memcpy(volumeStruct->superSector, superSector, bytesPerSector);
    free(dataTable);

    return volumeStruct;
}

int fat_close(struct volume_t* pvolume)
{
    if(!pvolume)
    {
        errno = EFAULT;
        return -1;
    }
    if(pvolume->superSector)
    {
        free(pvolume->superSector);
        pvolume->superSector = NULL;
    }
    if(pvolume)
    {
        free(pvolume);
        pvolume = NULL;
    }

    return 0;
}


///////////////////////////////////////////////////////////////

struct clusters_chain_t *get_chain_fat16(const void * const buffer, size_t size, uint16_t first_cluster)
{
    struct clusters_chain_t *chain = calloc(1,sizeof(struct clusters_chain_t));
    if(!chain)
    {
        errno = EFAULT;
        return NULL;
    }


    chain->clusters = malloc(sizeof(uint16_t));
    if (chain->clusters == NULL) {
        errno = ENOMEM;
        return NULL;
    }

    chain->clusters[0] = first_cluster;
    chain->size++;
    uint16_t *pomoc = (uint16_t*) buffer;

    uint16_t current_cluster = first_cluster;
    while (current_cluster != 0) {

        uint16_t next_cluster = pomoc[current_cluster];
        if(next_cluster>4607)
            break;
        uint16_t *pom = realloc(chain->clusters, (chain->size + 1) * sizeof(uint16_t));
        if (pom == NULL) {
            errno = ENOMEM;
            return NULL;
        }
        chain->clusters = pom;
        chain->clusters[chain->size] = next_cluster;
        chain->size++;

        current_cluster = next_cluster;
    }
    return chain;
}

int whichIndex(const char * s)
{
    int i = 0;
    for (i = 0; i < (int)strlen(s); ++i) {
        if(!isalpha(*(s+i)))
            return i;
    }
    return i;
}

struct file_t* file_open(struct volume_t* pvolume, const char* file_name)
{
    if(!pvolume || !file_name)
    {
        errno = EFAULT;
        return NULL;
    }

    uint16_t sectorsPerFatLocal = pvolume->superSector->sectorsPerFat;
    char *temp = calloc(pvolume->sectorsPerRootDirectory,bytesPerSector);
    if(!temp)
    {
        errno = ENOMEM;
        return NULL;
    }
    int output = disk_read(pvolume->file, (uint16_t)(pvolume->secondFat2Position + sectorsPerFatLocal),
                           temp, (uint16_t)pvolume->sectorsPerRootDirectory);
    if(output == -1)
    {
        errno = EISDIR;
        free(temp);
        return NULL;
    }


    int size = (int)(pvolume->sectorsPerRootDirectory * bytesPerSector);
    struct fat_entry_t *toFileName;
    int iterator = 0;
    int index = whichIndex(file_name);
    do {
        if(iterator>=size)
            break;
        toFileName = (struct fat_entry_t*)(temp + iterator);

        if(strncmp(toFileName->name,file_name, index) == 0)
        {
            if(*(file_name + index) == '.')
            {
                if(strncmp(toFileName->name ,file_name + index + 1, whichIndex(file_name + index)) == 0)
                    break;
            }
            else
                break;
        }
        iterator+=32;
    }while(1);
    if(iterator >= size)
    {
        errno = ENOENT;
        free(temp);
        return NULL;
    }
    if(toFileName->file_size == 0)
    {
        errno = ENOENT;
        free(temp);
        return NULL;
    }
    struct file_t *file = calloc(1,sizeof(struct file_t));
    if(!file)
    {
        errno = ENOMEM;
        free(temp);
        return NULL;
    }
    char *fatTable = calloc(pvolume->superSector->sectorsPerFat,bytesPerSector);
    if(!fatTable)
    {
        errno = ENOMEM;
        free(temp);
        free(file);
        return NULL;
    }
    output = disk_read(pvolume->file, (uint16_t)(pvolume->firstFat1Position),
                       fatTable, (uint16_t)pvolume->superSector->sectorsPerFat);
    if(output == -1)
    {
        errno = EISDIR;
        free(file);
        free(temp);
        free(fatTable);
        return NULL;
    }
    file->chains = get_chain_fat16(fatTable,(uint16_t)(pvolume->superSector->sectorsPerFat * bytesPerSector), toFileName->low_cluster_index);
    file->volumePointer = pvolume;
    file->fileSize = toFileName->file_size;
    free(temp);
    free(fatTable);
    return file;
}
int file_close(struct file_t* stream)
{
    if(!stream)
    {
        errno = EFAULT;
        return 1;
    }
    if(stream->volumePointer->superSector)
    {
        free(stream->volumePointer->superSector);
        stream->volumePointer->superSector = NULL;
    }
    if(stream->chains->clusters)
    {
        free(stream->chains->clusters);
        stream->chains->clusters = NULL;
    }
    if(stream->chains)
    {
        free(stream->chains);
        stream->chains = NULL;
    }
    if(stream)
    {
        free(stream);
        stream = NULL;
    }
    return 0;
}
size_t file_read(void *ptr, size_t size, size_t nmemb, struct file_t *stream)
{
    if(!ptr || !stream)
    {
        errno = EFAULT;
        return -1;
    }
    size_t readElements = 0;
    int flag = 0;
    const int arraySize = (int)(stream->volumePointer->superSector->sectorsPerCluster * bytesPerSector);
    char *cluster = malloc(arraySize);
    if(!cluster)
    {
        errno = ENOMEM;
        return -1;
    }
    size_t whichClaster = stream->filePosition/(stream->volumePointer->superSector->sectorsPerCluster * bytesPerSector);
    for (size_t j = whichClaster; j < stream->chains->size ; ++j) {
        flag = 0;
        disk_read(stream->volumePointer->file, (int32_t)(stream->volumePointer->secondCluster2Position + (stream->chains->clusters[j] - 2) * stream->volumePointer->superSector->sectorsPerCluster),
                  cluster, stream->volumePointer->superSector->sectorsPerCluster);
        if(readElements >= stream->fileSize)
            break;

        for (size_t i = 0; i < bytesPerSector*stream->volumePointer->superSector->sectorsPerCluster; ++i, ++readElements, stream->filePosition++) {
            if(stream->filePosition == 2047)
            {
                j-=2;
                j+=2;
            }
            if(readElements >= size*nmemb || stream->filePosition >= stream->fileSize)
                break;
            size_t currentCluster = stream->filePosition/(stream->volumePointer->superSector->sectorsPerCluster * bytesPerSector);
            if(currentCluster > j)
            {
                flag = 1;
                break;
            }


            *((char*)ptr+readElements) = *(cluster + stream->filePosition - (stream->volumePointer->superSector->sectorsPerCluster * bytesPerSector*j));
        }
        if(flag)
            continue;
        if(readElements >= size*nmemb || stream->filePosition >= stream->fileSize)
            break;
    }

    free(cluster);
    return readElements/size;
}

int checkIfInRange(struct file_t* file, uint32_t value)
{
    if(file->fileSize < value)
        return 0;
    return 1;
}

int32_t file_seek(struct file_t* stream, int32_t offset, int whence)
{
    if(!stream)
    {
        errno = EFAULT;
        return -1;
    }


    switch (whence) {
        case SEEK_SET:
            if(!checkIfInRange(stream, offset))
            {
                errno = EFAULT;
                return -1;
            }
            stream->filePosition = offset;
            break;
        case SEEK_CUR:
            if(!checkIfInRange(stream, stream->filePosition + offset))
            {
                errno = EFAULT;
                return -1;
            }
            stream->filePosition += offset;
            break;
        case SEEK_END:
            if(offset<0)
                offset*=-1;
            if(!checkIfInRange(stream, stream->fileSize - offset))
            {
                errno = EFAULT;
                return -1;
            }
            stream->filePosition = stream->fileSize - offset;
            break;
        default:
        {
            errno = EINVAL;
            return -1;
        }
    }
    return 1;
}

///////////////////////////////////////////////////////////

struct dir_t* dir_open(struct volume_t* pvolume, const char* dir_path)
{
    if(!pvolume || !dir_path)
    {
        errno = EFAULT;
        return NULL;
    }

    uint16_t sectorsPerFatLocal = pvolume->superSector->sectorsPerFat;
    char *temp = calloc(pvolume->sectorsPerRootDirectory,bytesPerSector);
    if(!temp)
    {
        errno = ENOMEM;
        return NULL;
    }
    int output = disk_read(pvolume->file, (uint16_t)(pvolume->secondFat2Position + sectorsPerFatLocal),
                           temp, (uint16_t)pvolume->sectorsPerRootDirectory);
    if(output == -1)
    {
        errno = EISDIR;
        free(temp);
        return NULL;
    }

    struct dir_t *directoryStruct = calloc(1,sizeof(struct dir_t));
    if(!directoryStruct)
    {
        errno = ENOMEM;
        free(temp);
        return NULL;
    }

    int size = (int)(pvolume->sectorsPerRootDirectory * bytesPerSector);
    struct fat_entry_t *toFileName;
    int index = whichIndex(dir_path);
    do {
        if(directoryStruct->filePosition>=size || index == 0 )
            break;
        toFileName = (struct fat_entry_t*)(temp + directoryStruct->filePosition);
        if(toFileName->file_size != 0)
        {
            errno = ENOTDIR;
            free(temp);
            free(directoryStruct);
            return NULL;
        }
        if(strncmp(toFileName->name,dir_path, index) == 0)
        {
            if(*(dir_path + index) == '.')
            {
                if(toFileName->file_size == 0)
                    break;
            }
            else
                break;
        }
        directoryStruct->filePosition+=32;
    }while(1);

    if(directoryStruct->filePosition == 0 && strcmp(dir_path,"\\") == 0)
        directoryStruct->filePosition = 0;
    else
        directoryStruct->filePosition = directoryStruct->filePosition;
    directoryStruct->wasOpened = false;
    directoryStruct->volumeStruct = pvolume;
    free(temp);

    return directoryStruct;
}


unsigned int sthIndex(const char *string, unsigned int max)
{
    for (unsigned int i = 0; i < max; ++i) {
        if(!isalpha(*(string + i)))
        {
            return i;
        }
    }
    return max;
}

int dir_read(struct dir_t* pdir, struct dir_entry_t* pentry)
{
    uint16_t sectorsPerFatLocal = pdir->volumeStruct->superSector->sectorsPerFat;
    char *temp = calloc(pdir->volumeStruct->sectorsPerRootDirectory,bytesPerSector);
    if(!temp)
    {
        errno = ENOMEM;
        return -1;
    }
    int output = disk_read(pdir->volumeStruct->file, (uint16_t)(pdir->volumeStruct->secondFat2Position + sectorsPerFatLocal),
                           temp, (uint16_t)pdir->volumeStruct->sectorsPerRootDirectory);
    if(output == -1)
    {
        errno = EISDIR;
        free(temp);
        return -1;
    }


    int size = (int)(pdir->volumeStruct->sectorsPerRootDirectory * bytesPerSector);
    struct fat_entry_t *toFileName;
    int flag = 0;
   // int index = whichIndex(pentry->name);
    do {
        if(pdir->filePosition>=size)
            break;
        toFileName = (struct fat_entry_t*)(temp + pdir->filePosition);
        if(*(char*)toFileName == 0)
        {
            free(temp);
            return 1;
        }
        if(*(char*)toFileName == -27)
        {
            pdir->filePosition+=32;
            continue;
        }
        memset(pentry->name,0,13);
        int j;
        for (j = 0; j < 8; ++j) {
            if(toFileName->name[j] == ' '){
                toFileName->name[j] = 0;
                break;
            }
        }
        int extLen = 0;
        for (int k = 0; k < 3; ++k, ++extLen) {
            if(toFileName->extenstion[k] == ' '){
                toFileName->extenstion[k] = 0;

                break;
            }
        }
        strncpy(pentry->name, toFileName->name,j);
        unsigned int idName = sthIndex(toFileName->name, 8);
        unsigned int idExt = sthIndex(toFileName->extenstion, 2);
        if(idExt > 0)
        {
            pentry->name[idName] = '.';
            strncpy(pentry->name + idName + 1,toFileName->extenstion, extLen);
        }
        pdir->filePosition+=32;
        flag = 1;
        break;
    }while(1);
    if(pdir->filePosition >= size || !flag)
    {
        free(temp);
        return 1;
    }

    int atrybuty = (int)toFileName->attributes;

    for (int i = 4; atrybuty > 0 ; --i) {
        if(i==0 && atrybuty >= 1)
        {
            pentry->is_readonly = true;
            atrybuty--;
        }
        else if(i==1 && atrybuty >= 2)
        {
            pentry->is_hidden = true;
            atrybuty-=2;
        }
        else if(i==2 && atrybuty >= 4)
        {
            pentry->is_system = true;
            atrybuty-=4;
        }
        else if(i==3 && atrybuty >= 16)
        {
            pentry->is_directory = true;
            atrybuty-=16;
        }
        else if(i==4 && atrybuty >= 32)
        {
            pentry->is_archived = true;
            atrybuty-=32;
        }

    }

    free(temp);
    return 0;
}
int dir_close(struct dir_t* pdir)
{
    if(!pdir)
    {
        errno = EFAULT;
        return -1;
    }

    if(pdir)
    {
        free(pdir);
        pdir = NULL;
    }

    return 0;
}



