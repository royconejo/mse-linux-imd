// SPDX-License-Identifier: GPL-2.0+
/*
    Copyright (C) 2019 Santiago Germino

    Este modulo maneja la EEPROM M24M01-RDW6TP de 128 Kb presente en la placa 
    RETRO-CIAA.

    El ultimo bit de la direccion a acceder se indica en el ultimo bit de la 
    direccion I2C, de ahi que se especifiquen 2 dispositivos en el device tree
    ambos de 64 Kb y con direcciones contiguas. Por este motivo se exponen dos
    character devices, uno para cada area de 64 Kb.

    Basicamente el driver ya existe en linux/drivers/misc/eeprom/at24.c, pero 
    como ejercicio se implementara uno mas simple, adaptado a las necesidades
    del proyecto.

    En particular, al firmware del proyecto RETRO-CIAA no le interesa acceder a 
    la EEPROM de a bytes sino de a paginas. En consecuencia, la forma de leer y
    escribir estos devices es exclusivamente de a una pagina entera a la vez.

    Se asume que el buffer pasado a las funciones "read" o "write" desde 
    userspace SIEMPRE tiene una capacidad de al menos STORAGE_PAGESIZE. Y se
    hace una interpretacion especial del parametro "len" para que indique no la
    cantidad de bytes sino el indice a la pagina deseada.

    El modulo se compila como un ejemplo mas en "linux-kernel-labs".

    1) $export ARCH=arm
    2) Compilar kernel con configuracion para beablebone_black
       - make omap2plus_defconfig
       - linux-kernel-labs.pdf pagina 14
       - make menuconfig
    3) $export CROSS_COMPILE=arm-linux-gnueabi-
    4) make -j 4
    5) Copiar am335x-customboneblack.dts a /src/linux/arch/arm/boot/dtb/
    6) Agregar dtb a Makefile en ese directorio
    7) Compilar kernel con make dtbs
    8) "make" en el directorio de este modulo

    PD: No estoy usando el coding style del kernel.
*/
#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/i2c.h>
#include <linux/string.h>


MODULE_LICENSE      ("GPL");
MODULE_DESCRIPTION  ("RETRO-CIAA EEPROM");
MODULE_AUTHOR       ("El Santiago Germino");
MODULE_VERSION      ("1.0");


#define     STORAGE_CAPACITY        (64 * 1024)
#define     STORAGE_PAGESIZE        (256)
#define     STORAGE_PAGES           (STORAGE_CAPACITY / STORAGE_PAGESIZE)
#define     I2C_ADDRESS_BYTES       2
#define     MAX_CLIENTS             2
#define     CLASS_NAME              "retrociaa"
#define     PRINT_PREFIX            "%s: " 
#define     PRINT_PREFIX_INFO       KERN_INFO PRINT_PREFIX
#define     PRINT_PREFIX_ALERT      KERN_ALERT PRINT_PREFIX
#define     PRINT(t,s,...)          if (verbose) \
                                        printk (PRINT_PREFIX_##t "[in %s] " s \
                                        "\n", THIS_MODULE->name, \
                                        __FUNCTION__, ##__VA_ARGS__)

struct rce_client
{
    struct device       *device;
    struct i2c_client   *client;
};


static struct class         *devClass;
static int                  classMajor;
static int                  clientsCount;
static struct rce_client    clients     [MAX_CLIENTS];
static char                 pageBuffer  [I2C_ADDRESS_BYTES + STORAGE_PAGESIZE];
static int                  verbose;

module_param (verbose, int, 0);


static const struct i2c_device_id rce_ids[] =
{
    { "retrociaa_eeprom", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, rce_ids);


#ifdef CONFIG_OF
static const struct of_device_id rce_of_match[] =
{
    { .compatible = "retrociaa,eeprom" },
    { }
};
MODULE_DEVICE_TABLE(of, rce_of_match);
#endif


static int i2c_get_address_from_page (size_t page, char *addr)
{
    const int PageAddr = page * STORAGE_PAGESIZE;
    addr[0] = (PageAddr >> 8) & 0xFF;
    addr[1] = PageAddr & 0xFF;

    return PageAddr;
}


static ssize_t i2c_address_select (struct i2c_client *client, size_t page)
{
    char        byteAddr[2];
    const int   PageAddr = i2c_get_address_from_page (page, byteAddr);
    ssize_t     err;

    PRINT (INFO, "Accesing page '%d' from address bytes '%x' and '%x' "
                 "(offset %d)...", page, byteAddr[0], byteAddr[1], PageAddr);

    if ((err = i2c_master_send (client, byteAddr, 2)) < 0)
    {
        PRINT (ALERT, "I2C address send FAILED with error '%d'.", err);
    }
    else if (err != 2)
    {
        PRINT (ALERT, "I2C address send FAILED, '%d' of '%d bytes written.",
                err, 2);
        return -EIO;
    }

    return 0;
}


static struct i2c_client * get_client_by_file (struct file *filep)
{
    const unsigned Major = imajor (filep->f_inode);
    const unsigned Minor = iminor (filep->f_inode);

    if (Major != classMajor)
    {
        PRINT (ALERT, "File major number '%d' is different than major of this "
                      "class '%d'.", Major, classMajor);
        return NULL;
    }

    if (Minor >= clientsCount)
    {
        PRINT (ALERT, "File minor number '%d' is invalid. "
                      "Registered clients: '%d'.", Minor, clientsCount);
        return NULL;
    }

    PRINT (INFO, "File access granted for client '%d', name '%s'.",
            Minor, clients[Minor].client->name);

    return clients[Minor].client;
}


static int chd_open (struct inode *inodep, struct file *filep) 
{
    static struct i2c_client *client;

    PRINT (INFO, "Params inodep '%p', filep '%p'.", inodep, filep);

    if (!(client = get_client_by_file (filep)))
    {
        PRINT (ALERT, "File access denied.");
        return -EINVAL;
    }

    return 0;
}


static ssize_t chd_read (struct file *filep, char *buffer, size_t len,
                            loff_t *offset)
{
    int                         err;
    static struct i2c_client    *client;

    PRINT (INFO, "Params filep '%p', buffer '%p', len '%d', offset '%p'.", 
            filep, buffer, len, offset);

    if (!(client = get_client_by_file (filep)))
    {
        PRINT (ALERT, "File access denied.");
        return -EINVAL;
    }

    if (!filep || !buffer)
    {
        PRINT (ALERT, "Params filep and/or buffer cannot be NULL.");
        return -EINVAL;
    }

    if (len >= STORAGE_PAGES)
    {
        PRINT (ALERT, "Param len (accessed page) must be lower than '%d'.",
                STORAGE_PAGES);
        return -EINVAL;
    }

    // Storage address select
    if ((err = i2c_address_select (client, len)))
    {
        return err;
    }

    // Whole page read
    if ((err = i2c_master_recv (client, pageBuffer, STORAGE_PAGESIZE)) < 0)
    {
        PRINT (ALERT, "I2C page read FAILED with error '%d'.", err);
        return err;
    }
    else if (err != STORAGE_PAGESIZE)
    {
        PRINT (ALERT, "I2C page read FAILED, '%d' of '%d bytes read.",
                err, STORAGE_PAGESIZE);
        return -EIO;
    }

    if ((err = copy_to_user (buffer, pageBuffer, STORAGE_PAGESIZE)) < 0)
    {
        PRINT (ALERT, "Page data copy_to_user() FAILED with error '%d'.", err);
        return err;
    }
    else if (err && err != STORAGE_PAGESIZE)
    {
        PRINT (ALERT, "Page data copy_to_user() FAILED, "
                      "'%d' of '%d bytes copied.",
                        err, STORAGE_PAGESIZE);
        return -EIO;
    }

    return STORAGE_PAGESIZE;
}


static ssize_t chd_write (struct file *filep, const char *buffer, size_t len,
                            loff_t *offset)
{
    int                         err;
    static struct i2c_client    *client;

    PRINT (INFO, "Params filep '%p', buffer '%p', len '%d', offset '%p'.", 
            filep, buffer, len, offset);

    if (!(client = get_client_by_file (filep)))
    {
        PRINT (ALERT, "File access denied.");
        return -EINVAL;
    }

    if (!filep || !buffer)
    {
        PRINT (ALERT, "Params filep and/or buffer cannot be NULL.");
        return -EINVAL;
    }

    if (len >= STORAGE_PAGES)
    {
        PRINT (ALERT, "Param len (accessed page) must be lower than '%d'.", 
                STORAGE_PAGES);
        return -EINVAL;
    }

    if ((err = copy_from_user (&pageBuffer[I2C_ADDRESS_BYTES], buffer, 
                                STORAGE_PAGESIZE)) < 0)
    {
        PRINT (ALERT, "Page data copy_from_user() FAILED with error '%d'.", 
                err);
        return err;
    }
    else if (err && err != STORAGE_PAGESIZE)
    {
        PRINT (ALERT, "Page data copy_from_user() FAILED, "
                      "'%d' of '%d bytes copied.",
                        err, STORAGE_PAGESIZE);
        return -EIO;
    }

    // Storage address select
    i2c_get_address_from_page (len, pageBuffer);

    // Address + whole page write
    if ((err = i2c_master_send (client, pageBuffer, 
                            I2C_ADDRESS_BYTES + STORAGE_PAGESIZE)) < 0)
    {
        PRINT (ALERT, "I2C page write FAILED with error '%d'.", err);
        return err;
    }
    else if (err != I2C_ADDRESS_BYTES + STORAGE_PAGESIZE)
    {
        PRINT (ALERT, "I2C page write FAILED, '%d' of '%d bytes written.",
                err, I2C_ADDRESS_BYTES + STORAGE_PAGESIZE);
        return -EIO;
    }

    return STORAGE_PAGESIZE;
}
 

static int chd_release (struct inode *inodep, struct file *filep)
{
    static struct i2c_client *client;

    PRINT (INFO, "Params inodep '%p', filep '%p'.", inodep, filep);

    if (!(client = get_client_by_file (filep)))
    {
        PRINT (ALERT, "File access denied.");
        return -EINVAL;
    }

    return 0;
}


static struct file_operations rce_fops =
{
   .open    = chd_open,
   .read    = chd_read,
   .write   = chd_write,
   .release = chd_release,
};


static void shutdown (void)
{
    int i;

    for (i = 0; i < clientsCount; ++i)
    {
        if (clients[i].device && !IS_ERR (clients[i].device))
        {
            device_destroy (devClass, MKDEV(classMajor, i));
        }
    }

    if (devClass && !IS_ERR (devClass))
    {
        class_destroy (devClass);
    }

    if (classMajor >= 0)
    {
        unregister_chrdev (classMajor, CLASS_NAME);
    }
}


static int rce_probe (struct i2c_client *client)
{
    int                 err;
    struct rce_client   *cs;
    char                deviceName[I2C_NAME_SIZE * 2];

    if (clientsCount >= MAX_CLIENTS)
    {
        PRINT (ALERT, "Maximum number of I2C devices reached at '%d'.",
                clientsCount);
        return -ENOMEM;
    }

    #if 0
    // Check device tree parameters
    if (device_property_read_u32 (reDevice, "size", &storageCapacity))
    {
        PRINT (ALERT, "Parameter 'size' not set.");
        return -EINVAL;
    }
    #endif

    // Test hardware presence
    if ((err = i2c_master_recv (client, pageBuffer, 1)) != 1)
    {
        PRINT (ALERT, "I2C device test at address '0x%x' FAILED "
                      "with error '%d'.", client->addr, err);
        return -ENODEV;
    }

    PRINT (INFO, "I2C device '%s' at address '0x%x' seems to be working.",
                    client->name, client->addr);

    cs = &clients[clientsCount];
    memset (cs, 0, sizeof(struct rce_client));

    snprintf (deviceName, sizeof(deviceName), "%s_%s%d", 
                CLASS_NAME, client->name, clientsCount);

    // New char device for this client
    if (IS_ERR (cs->device = device_create (devClass, NULL, 
                        MKDEV(classMajor, clientsCount), NULL, deviceName)))
    {
        PRINT (ALERT, "Char device '%s' creation FAILED with error '%d'.", 
                deviceName, (int) PTR_ERR(cs->device));
        shutdown ();
        return PTR_ERR (cs->device);
    }

    PRINT (INFO, "Char device '%s' has been created.", deviceName);

    cs->client = client;
    ++ clientsCount;

    return 0;
}


static int rce_remove (struct i2c_client *client)
{
    return 0;
}


static struct i2c_driver rce_driver = 
{
    .probe_new  = rce_probe,
    .remove     = rce_remove,
    .id_table   = rce_ids,
    .driver     = 
    {
        .name           = "retrociaa_eeeprom",
        .owner          = THIS_MODULE,
        .of_match_table = rce_of_match,
    },
};


static int __init rce_init (void)
{
    int err;

    PRINT (INFO, "Initializing...");

    clientsCount    = 0;
    classMajor      = -1;
    devClass        = NULL;
 
    // Major number dynamic allocation
    if ((classMajor = register_chrdev (0, CLASS_NAME, &rce_fops))
            < 0)
    {
        PRINT (ALERT, "Char device class '" CLASS_NAME "' major number "
                      "registration FAILED with error '%d'.", classMajor);
        shutdown ();
        return classMajor;
    }

    PRINT (INFO, "Char device class '" CLASS_NAME "', major number '%d'.",
                    classMajor);
 
    // Char device class
    if (IS_ERR (devClass = class_create (THIS_MODULE, CLASS_NAME)))
    {
        PRINT (ALERT, "Char device class '" CLASS_NAME "' creation FAILED "
                      "with error '%d'.", (int) PTR_ERR(devClass));
        shutdown ();
        return PTR_ERR (devClass);
    }

    PRINT (INFO, "Char device class '" CLASS_NAME "' has been created.");
 
    err = i2c_add_driver (&rce_driver);
    if (err)
    {
        PRINT (ALERT, "I2C add driver FAILED with error '%d'.", err);
        shutdown ();
        return err;
    }

    PRINT (INFO, "Initialized.");
    return 0;
}
module_init (rce_init);


static void __exit rce_exit (void)
{
    PRINT (INFO, "Shutting down...");
    i2c_del_driver (&rce_driver);
    shutdown ();
    PRINT (INFO, "Goodbye.");
}
module_exit (rce_exit);

// EOM :)
