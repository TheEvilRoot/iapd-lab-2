# Interfaces and peripheral devices | Lab 2

Task: get information about drives installed in the system such as:
 
 - Model
 - Vendor
 - Firmware version
 - Mounted volumes
 - Interface type
 - Supported ATA standarts (if any)
 - DMA, UltraDMA support
 - Supported PIO modes

## Notes

Windows Drivers API provides 512 bytes structure by `ata.h` header. I do not have one and need to parse this data by myself.

By direct ATA command `0xec` (`ATA_PASS_THROUGH`) WinAPI provides these 512 bytes.

Offsets in that structure are provided by [this paper](http://www.t13.org/documents/uploadeddocuments/docs2007/d1699r4a-ata8-acs.pdf). *Page 111*

## Sample output

```

--------------
WL1000GSA1654G
--------------

  Vendor: N/A
   Model: WL1000GSA1654G
Firmware: 51.0AB51
Bus type: SATA
Serial #: WOCL25001113432

DMA Support: Yes
  Ultra DMA: 6 or below
PIO Support: PIO Mode 1
             PIO Mode 2
ATA Standarts:
    ATA8-ACS: Yes
 ATA/ATAPI-7: Yes
 ATA/ATAPI-6: No
 ATA/ATAPI-5: No
 ATA/ATAPI-4: No

Volume E:
        469 Gb / 593 Gb (123 Gb free)
Volume K:
        71 Gb / 136 Gb (65 Gb free)
Volume W:
        4 Gb / 126 Gb (122 Gb free)

--------------------------
GIGABYTE GP-GSTFS31480GNTD
--------------------------

  Vendor: N/A
   Model: GIGABYTE GP-GSTFS31480GNTD
Firmware: SBFM61.3
Bus type: SATA
Serial #: SN192208955422

DMA Support: Yes
  Ultra DMA: 6 or below
PIO Support: PIO Mode 1
             PIO Mode 2
ATA Standarts:
    ATA8-ACS: Yes
 ATA/ATAPI-7: Yes
 ATA/ATAPI-6: Yes
 ATA/ATAPI-5: Yes
 ATA/ATAPI-4: Yes

Volume C:
        93 Gb / 119 Gb (26 Gb free)

-----------
M3 Portable
-----------

  Vendor: Seagate
   Model: M3 Portable
Firmware: 0708
Bus type: USB
Serial #: NM13EM50

Volume F:
        206 Gb / 238 Gb (31 Gb free)

```