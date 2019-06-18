# MSE - Implementacion de Manejadores de Dispositivos

## Consignas
1. Compilar kernel con soporte para BeableBoneBlack.
2. Agregar nuevo dispositivo al device tree.
3. Realizar un modulo de kernel que maneje un dispositivo I2C a elección.
4. Interfaz al dispositivo mediante char device.
5. Programa en userspace que interactue con el módulo a través del char device.

## Entregables
- Device Tree modificado: [am335x-customboneblack.dts](https://github.com/royconejo/mse-linux-imd/blob/master/am335x-customboneblack.dts)
- Modulo manejador de dispositivos: [retrociaa_eeprom.c](https://github.com/royconejo/mse-linux-imd/blob/master/retrociaa_eeprom.c)
- Aplicación en userspace: [rce_userspace.c](https://github.com/royconejo/mse-linux-imd/blob/master/rce_userspace.c)

## Dispositivo a manejar
Este modulo maneja la EEPROM [M24M01-RDW6TP](https://www.st.com/resource/en/datasheet/m24m01-r.pdf) de 128 Kb presente en la placa 
[RETRO-CIAA](http://www.retro-ciaa.com).

El ultimo bit de la direccion a acceder se indica en el ultimo bit de la 
direccion I2C, de ahi que se especifiquen 2 dispositivos en el device tree
ambos de 64 Kb y con direcciones contiguas. Por este motivo se exponen dos
character devices, uno para cada area de 64 Kb.

El driver [ya existe](https://github.com/torvalds/linux/blob/master/drivers/misc/eeprom/at24.c), pero 
como ejercicio se implementara uno mas simple, adaptado a las necesidades
del proyecto. 

En particular, al firmware del proyecto RETRO-CIAA no le interesa acceder a 
la EEPROM de a bytes sino de a paginas. En consecuencia, la forma de leer y
escribir estos devices es exclusivamente de a una pagina entera a la vez.
Se asume que el buffer pasado a las funciones "read" o "write" desde 
userspace SIEMPRE tiene una capacidad de al menos STORAGE_PAGESIZE. Y se
hace una interpretacion especial del parametro "len" para que indique no la
cantidad de bytes sino el indice a la pagina deseada.

## Instrucciones
(Partiendo de un kernel ya configurado y compilado segun linux-kernel-labs)
1. Clonar este proyecto en ~/linux-kernel-labs/modules/nfsroot/root
2. Copiar am335x-customboneblack.dts a ~/linux-kernel-labs/src/linux/arch/arm/boot/dtb/
3. Agregar custom dtb a Makefile en ese directorio
4. $ export ARCH=arm
5. $ export CROSS_COMPILE=arm-linux-gnueabi-
6. Compilar kernel con $make dtbs
7. Copiar zImage y custom dtb al servidor TFTP
8. En el directorio de este módulo
   - Para compilar el kernel module: $ make
   - Para compilar la aplicacion de prueba en userspace: $ arm-linux-gnueabi-gcc rce_userspace.c -o rce_userspace
9. Bootear la BeagleBoneBlack con zImage, am335x-customboneblack.dts y filesystem por NFS. Los comandos de u-boot se encuentran en https://github.com/royconejo/mse-linux-imd/tree/master/notes


## Pruebas en BeagleBoneBlack
### Existencia de los nuevos dispositivos definidos en el device tree
```ShellSession
# find /sys/firmware/devicetree -name "*retrociaa*"
/sys/firmware/devicetree/base/ocp/i2c@4802a000/retrociaa_eeprom0@56
/sys/firmware/devicetree/base/ocp/i2c@4802a000/retrociaa_eeprom1@57
```

### Definición de los dispositivos
```ShellSession
# dtc -I fs /sys/firmware/devicetree/base/ | grep retrociaa
Warning (status_is_string): "status" property in /ocp/timer@48040000 is not a string
Warning (status_is_string): "status" property in /ocp/timer@44e31000 is not a string
			retrociaa_eeprom0@56 {
				compatible = "retrociaa,eeprom";
			retrociaa_eeprom1@57 {
				compatible = "retrociaa,eeprom";
```

### Habilitación del adaptador i2c-1
```ShellSession
# i2cdetect -l
i2c-1	i2c       	OMAP I2C adapter                	I2C adapter
i2c-2	i2c       	OMAP I2C adapter                	I2C adapter
i2c-0	i2c       	OMAP I2C adapter                	I2C adapter
```

### Detección de la EEPROM de la RETRO-CIAA
```ShellSession
# i2cdetect -r 1
WARNING! This program can confuse your I2C bus, cause data loss and worse!
I will probe file /dev/i2c-1 using read byte commands.
I will probe address range 0x03-0x77.
Continue? [Y/n] Y
     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
00:          -- -- -- -- -- -- -- -- -- -- -- -- -- 
10: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
20: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
30: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
40: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
50: -- -- -- -- -- -- 56 57 -- -- -- -- -- -- -- -- 
60: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
70: -- -- -- -- -- -- -- --                         
```

### Inserción del modulo en el kernel
Notese el parametro **verbose=1**
```ShellSession
# cd /root/retrociaa_eeprom
# insmod retrociaa_eeprom.ko verbose=1
[ 7183.778217] retrociaa_eeprom: [in rce_init] Initializing...
[ 7183.784362] retrociaa_eeprom: [in rce_init] Char device class 'retrociaa', major number '246'.
[ 7183.793315] retrociaa_eeprom: [in rce_init] Char device class 'retrociaa' has been created.
[ 7183.807160] retrociaa_eeprom: [in rce_probe] I2C device 'eeprom' at address '0x56' seems to be working.
[ 7183.818577] retrociaa_eeprom: [in rce_probe] Char device 'retrociaa_eeprom0' has been created.
[ 7183.833293] retrociaa_eeprom: [in rce_probe] I2C device 'eeprom' at address '0x57' seems to be working.
[ 7183.844427] retrociaa_eeprom: [in rce_probe] Char device 'retrociaa_eeprom1' has been created.
[ 7183.853749] retrociaa_eeprom: [in rce_init] Initialized.
```

### Existencia de los nuevos char devices en su clase correspondiente
```ShellSession
# cd /sys/class/retrociaa/
# ls
retrociaa_eeprom0  retrociaa_eeprom1
```

### Char devices listados en /dev
```ShellSession
# cd /dev
# ls | grep retrociaa
retrociaa_eeprom0
retrociaa_eeprom1
```

### Acceso a los char devices
```ShellSession
# cat /dev/retrociaa_eeprom1 1>& /dev/null
[11695.053589] retrociaa_eeprom: [in chd_open] Params inodep '1fef571d', filep '101698d8'.
[11695.062015] retrociaa_eeprom: [in get_client_by_file] File access granted for client '1', name 'eeprom'.
[11695.073309] retrociaa_eeprom: [in chd_read] Params filep '101698d8', buffer '795ff0cd', len '4096', offset 'aa8f3a2e'.
[11695.084471] retrociaa_eeprom: [in get_client_by_file] File access granted for client '1', name 'eeprom'.
[11695.094172] retrociaa_eeprom: [in chd_read] Param len (accessed page) must be lower than '256'.
[11695.105300] retrociaa_eeprom: [in chd_release] Params inodep '1fef571d', filep '101698d8'.
[11695.113920] retrociaa_eeprom: [in get_client_by_file] File access granted for client '1', name 'eeprom'.
```

### Remoción del modulo del kernel
```ShellSession
# rmmod retrociaa_eeprom.ko
[ 7351.152496] retrociaa_eeprom: [in rce_exit] Shutting down...
[ 7351.164925] retrociaa_eeprom: [in rce_exit] Goodbye.
```

### Nueva inserción del modulo
Notar la falta del parametro **verbose**
```ShellSession
# cd /root/retrociaa_eeprom
# insmod retrociaa_eeprom.ko
```

### Prueba de acceso a los device char desde aplicación en userspace
```ShellSession
# cd /root/retrociaa_eeprom
# ./rce_userspace
Userland INFO: Opening char device at '/dev/retrociaa_eeprom0'...
Userland INFO: Char device opened, got handle '3'.
Userland INFO: Reading current, unmodified contents of page '0'...
Userland INFO: Reading page '0' from file handle '3'...
Userland INFO: Page has been read.
Userland INFO: Displaying read contents:
  0000  75 EF 7C B4 A5 B7 C0 0A 8D 8F 65 B2 67 03 AD 11 
  0010  22 2D CD 7F 1C C1 48 EB 62 43 77 1B E9 9F C6 AC 
  0020  2A EC F1 A5 4A F3 E2 56 67 3A AB AB FC A4 CD F1 
  0030  F5 73 75 1A 02 AD 3E C2 9F 40 1C 84 22 73 F5 86 
  0040  75 8D D5 C5 8E 0F A8 EE EC E2 CE F6 29 46 F2 C7 
  0050  EE 37 83 DE A3 FA C7 51 24 52 8D 6A 0B 43 42 D5 
  0060  1D 19 F9 FC 7D 12 82 9F 0B 35 38 29 E0 46 48 32 
  0070  74 61 10 78 3B 7A F2 33 EF 50 ED 57 95 A5 A1 A9 
  0080  BC B4 9D 61 4D C7 C1 3B 72 1E 18 0C 67 92 B7 DE 
  0090  84 4E 8F 75 3E 46 7A 74 C8 DE 7F 6B 32 AA 1F 4E 
  00A0  3F F0 BC 12 F6 5B C5 EA 4C 1F F4 39 E0 EE A3 1A 
  00B0  73 3C BE 27 4D B2 11 87 64 FE B0 7B E5 69 72 4A 
  00C0  E2 18 BC E9 F6 57 66 F1 33 8B A6 1D 25 70 3C 9A 
  00D0  14 F2 2A 87 19 CD 17 E5 64 DA 74 58 7D 68 21 48 
  00E0  6E C7 72 38 F2 4E 9B DB EB 03 67 D9 6F 01 B7 98 
  00F0  34 C0 CF AA 23 4B FF 41 43 E7 00 67 8F 3B 19 AA 

Userland INFO: Generating random contents...
Userland INFO: Displaying new random buffer:
  0000  4E 14 27 DA 9A 32 FA 92 7A 66 A1 E0 87 22 68 C9 
  0010  D7 E0 BA 77 A1 0A 7E B6 73 16 3D DD 44 94 0E 63 
  0020  8C 8A 36 3D A5 97 31 26 C3 A7 14 2A BA D9 8C C3 
  0030  71 C1 90 D6 BF 31 1A 8F 80 79 A1 9D 30 A6 C0 37 
  0040  A8 4E 23 05 26 43 82 AD 40 15 F7 7E C0 61 51 07 
  0050  39 51 1D 23 BF AB A1 FB 02 B9 F5 9A 6F 13 DB 70 
  0060  C5 75 1B DA 8B 67 6B 00 A4 B7 08 A6 D2 F4 22 84 
  0070  90 55 65 ED CA CD 96 37 01 44 B2 8E B8 20 34 BF 
  0080  74 55 56 AA 13 39 E1 64 0F 4D 35 AB 98 38 72 9A 
  0090  78 07 E4 4C F4 F8 F2 5C 98 BF 67 5A 51 71 5C F8 
  00A0  67 15 2E 0A 1A 83 2E 54 E6 AD 9E B5 B7 F0 4B 84 
  00B0  CE E3 5A 73 7A 26 D9 D3 04 86 53 15 AA DE 4B 6B 
  00C0  26 47 5A AD 6E 08 95 D3 58 60 8C 6D 95 4F DC AB 
  00D0  3C ED 60 7A 50 25 98 77 A2 C4 58 34 67 EE 63 82 
  00E0  5A 9D A9 59 E8 7B 2A F9 0F D1 ED E6 DC 52 FD 5A 
  00F0  86 A1 2C 66 DA 71 46 5F DC 6E 14 7F ED F3 CC CD 

Userland INFO: Writing new contents to page '0'...
Userland INFO: Writing page '0' to file handle '3'...
Userland INFO: Page has been written.
Userland INFO: Waiting some time...
Userland INFO: Reading new contents of page '0'...
Userland INFO: Reading page '0' from file handle '3'...
Userland INFO: Page has been read.
Userland INFO: Displaying new read contents:
  0000  4E 14 27 DA 9A 32 FA 92 7A 66 A1 E0 87 22 68 C9 
  0010  D7 E0 BA 77 A1 0A 7E B6 73 16 3D DD 44 94 0E 63 
  0020  8C 8A 36 3D A5 97 31 26 C3 A7 14 2A BA D9 8C C3 
  0030  71 C1 90 D6 BF 31 1A 8F 80 79 A1 9D 30 A6 C0 37 
  0040  A8 4E 23 05 26 43 82 AD 40 15 F7 7E C0 61 51 07 
  0050  39 51 1D 23 BF AB A1 FB 02 B9 F5 9A 6F 13 DB 70 
  0060  C5 75 1B DA 8B 67 6B 00 A4 B7 08 A6 D2 F4 22 84 
  0070  90 55 65 ED CA CD 96 37 01 44 B2 8E B8 20 34 BF 
  0080  74 55 56 AA 13 39 E1 64 0F 4D 35 AB 98 38 72 9A 
  0090  78 07 E4 4C F4 F8 F2 5C 98 BF 67 5A 51 71 5C F8 
  00A0  67 15 2E 0A 1A 83 2E 54 E6 AD 9E B5 B7 F0 4B 84 
  00B0  CE E3 5A 73 7A 26 D9 D3 04 86 53 15 AA DE 4B 6B 
  00C0  26 47 5A AD 6E 08 95 D3 58 60 8C 6D 95 4F DC AB 
  00D0  3C ED 60 7A 50 25 98 77 A2 C4 58 34 67 EE 63 82 
  00E0  5A 9D A9 59 E8 7B 2A F9 0F D1 ED E6 DC 52 FD 5A 
  00F0  86 A1 2C 66 DA 71 46 5F DC 6E 14 7F ED F3 CC CD 

Userland INFO: Comparing contents:
  0000  == == == == == == == == == == == == == == == == 
  0010  == == == == == == == == == == == == == == == == 
  0020  == == == == == == == == == == == == == == == == 
  0030  == == == == == == == == == == == == == == == == 
  0040  == == == == == == == == == == == == == == == == 
  0050  == == == == == == == == == == == == == == == == 
  0060  == == == == == == == == == == == == == == == == 
  0070  == == == == == == == == == == == == == == == == 
  0080  == == == == == == == == == == == == == == == == 
  0090  == == == == == == == == == == == == == == == == 
  00A0  == == == == == == == == == == == == == == == == 
  00B0  == == == == == == == == == == == == == == == == 
  00C0  == == == == == == == == == == == == == == == == 
  00D0  == == == == == == == == == == == == == == == == 
  00E0  == == == == == == == == == == == == == == == == 
  00F0  == == == == == == == == == == == == == == == == 

Userland INFO: >>>> OK! Buffers have equal contents!
Userland INFO: Closing file handle '3'...
Userland INFO: File handle closed.
```
