// SPDX-License-Identifier: GPL-2.0+
/*
    Copyright (C) 2019 Santiago Germino

    Programa de demostracion del modulo retrociaa_eeprom.

    Compilar con:
        $arm-linux-gnueabi-gcc rce_userspace.c -o rce_userspace
*/

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>


#define     STORAGE_CAPACITY        (64 * 1024)
#define     STORAGE_PAGESIZE        (256)
#define     STORAGE_PAGES           (STORAGE_CAPACITY / STORAGE_PAGESIZE)

#define     DEV_EEPROM0             "/dev/retrociaa_eeprom0"
#define     DEV_EEPROM1             "/dev/retrociaa_eeprom1"

#define     PAGE_PRINT_COLS         16

#define     PRINT_PREFIX_INFO       "Userland INFO: "
#define     PRINT_PREFIX_ALERT      "Userland ERROR: "
#define     PRINT(t,s,...)          printf (PRINT_PREFIX_##t s "\n", \
                                        ##__VA_ARGS__)


static char bufferA [STORAGE_PAGESIZE];
static char bufferB [STORAGE_PAGESIZE];


static int dev_open (const char* path)
{
    int file;

    PRINT (INFO, "Opening char device at '%s'...", path);

    if ((file = open (path, O_RDWR)) < 0)
    {
        PRINT (ALERT, "Cannot open char device at '%s', error '%d'.",
                path, file);
    }
    else
    {
        PRINT (INFO, "Char device opened, got handle '%d'.", file);
    }

    return file;
}


static int dev_close (int file)
{
    if (file >= 0)
    {
        PRINT (INFO, "Closing file handle '%d'...", file);
        close (file);
        PRINT (INFO, "File handle closed.");
    }
    else
    {
        PRINT (ALERT, "Invalid file handle '%d'.", file);
    }

    return 0;
}


static int dev_page_read (int file, size_t page, char *buffer)
{
    int err;

    memset (buffer, 0, STORAGE_PAGESIZE);

    PRINT (INFO, "Reading page '%d' from file handle '%d'...",
            page, file);

    if ((err = read (file, buffer, page)) != STORAGE_PAGESIZE)
    {
        PRINT (ALERT, "Cannot read page '%d', error '%d'.",
                page, err);
    }

    PRINT (INFO, "Page has been read.");
    return 0;
}


static int dev_page_write (int file, size_t page, char *buffer)
{
    int err;

    PRINT (INFO, "Writing page '%d' to file handle '%d'...",
            page, file);

    if ((err = write (file, buffer, page)) != STORAGE_PAGESIZE)
    {
        PRINT (ALERT, "Cannot write page '%d', error '%d'.",
                page, err);
    }

    PRINT (INFO, "Page has been written.");
    return 0;
}


static int dev_page_random (char *buffer)
{
    int file;
    int err;

    if ((file = open ("/dev/urandom", O_RDONLY)) < 0)
    {
        PRINT (ALERT, "Random device open error '%d'.", file);
        return file;
    }

    if ((err = read (file, buffer, STORAGE_PAGESIZE)) < 0)
    {
        PRINT (ALERT, "Page random fill error '%d'.", err);
    }
    else if (err != STORAGE_PAGESIZE)
    {
        PRINT (ALERT, "Page random fill, '%d' of '%d' bytes generated.",
            err, STORAGE_PAGESIZE);
        err = -EIO;
        return -EIO;
    }
    else 
    {
        err = 0;
    }

    close (file);
    return err;
}


static void dev_page_print (char *buffer)
{
    int x;
    int y;

    for (y = 0; y < (STORAGE_PAGESIZE / PAGE_PRINT_COLS); ++y)
    {
        printf ("  %04X  ", y * PAGE_PRINT_COLS);

        for (x = 0; x < PAGE_PRINT_COLS; ++x)
        {
            printf ("%02X ", buffer[x + y * PAGE_PRINT_COLS]);
        }

        printf ("\n");
    }

    printf ("\n");
}


static int dev_page_compare (char *bufferA, char *bufferB)
{
    int x;
    int y;
    int offs;
    int diff = 0;

    for (y = 0; y < (STORAGE_PAGESIZE / PAGE_PRINT_COLS); ++y)
    {
        printf ("  %04X  ", y * PAGE_PRINT_COLS);

        for (x = 0; x < PAGE_PRINT_COLS; ++x)
        {
            offs = x + y * PAGE_PRINT_COLS;

            if (bufferA[offs] == bufferB[offs])
            {
                printf ("== ");
            }
            else 
            {
                printf ("EE ");
                ++ diff;
            }
        }

        printf ("\n");
    }

    printf ("\n");

    return diff;
}


int main ()
{
    int file;
    int err;
    int page = 0;

    if ((file = dev_open (DEV_EEPROM0)) < 0)
    {
        goto exit_error;
    }

    PRINT (INFO, "Reading current, unmodified contents of page '%d'...", page);

    if ((err = dev_page_read (file, page, bufferA)))
    {
        goto exit_error;
    }

    PRINT (INFO, "Displaying read contents:");

    dev_page_print (bufferA);


    PRINT (INFO, "Generating random contents...");

    if ((err = dev_page_random (bufferA)))
    {
        goto exit_error;
    }


    PRINT (INFO, "Displaying new random buffer:");

    dev_page_print (bufferA);


    PRINT (INFO, "Writing new contents to page '%d'...", page);
    
    if ((err = dev_page_write (file, page, bufferA)))
    {
        goto exit_error;
    }

    PRINT (INFO, "Waiting some time...");

    sleep (2);

    PRINT (INFO, "Reading new contents of page '%d'...", page);

    if ((err = dev_page_read (file, page, bufferB)))
    {
        goto exit_error;
    }

    PRINT (INFO, "Displaying new read contents:");

    dev_page_print (bufferB);


    PRINT (INFO, "Comparing contents:");

    if ((err = dev_page_compare (bufferA, bufferB)))
    {
        PRINT (ALERT, "Found '%d' differences.", err);
        goto exit_error;
    }
    else
    {
        PRINT (INFO, ">>>> OK! Buffers have equal contents!");
    }

    dev_close (file);
    
    return 0;

exit_error:
    PRINT (ALERT, "Exiting with error.");
    dev_close (file);

    return 1;
}

