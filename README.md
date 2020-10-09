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
