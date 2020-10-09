#ifndef __LAB_2_DEVICE_INFO
#define __LAB_2_DEVICE_INFO

#include <iostream>
#include <string>

#include "Utils.hpp"

struct DeviceInfo {
  std::string vendor{ "N/A" };
  std::string model{ "N/A" };
  std::string firmware_version{ "N/A" };
  std::string bus_type{ "N/A" };
  std::string serial_number{ "N/A" };

  bool is_info_filled{ false };
  bool dma_suport{ false };
  bool pio_support[8];

  int ultra_dma_mode_supported{ -1 };

  struct {
    uint8_t : 7;

    uint8_t ata8_acs : 1;
    uint8_t atapi_7 : 1;
    uint8_t atapi_6 : 1;
    uint8_t atapi_5 : 1;
    uint8_t atapi_4 : 1;

    uint8_t : 4;
  } ata_data;

  friend std::ostream& operator<<(std::ostream& o, const DeviceInfo& i) {
    std::cout << std::string(i.model.size(), '-') << "\n";
    std::cout << i.model << "\n";
    std::cout << std::string(i.model.size(), '-') << "\n\n";


    std::cout << std::setfill(' ') << std::right;
    std::cout << std::setw(10) << "Vendor: " << i.vendor << "\n";
    std::cout << std::setw(10) << "Model: " << i.model << "\n";
    std::cout << std::setw(10) << "Firmware: " << i.firmware_version << "\n";
    std::cout << std::setw(10) << "Bus type: " << i.bus_type << "\n";
    std::cout << std::setw(10) << "Serial #: " << i.serial_number << "\n";

    if (i.is_info_filled) {
      std::cout << "\n";

      std::cout << std::setfill(' ') << std::right;
      std::cout << std::setw(13) << "DMA Support: " << stringifyBool(i.dma_suport) << "\n";
      std::cout << std::setw(13) << "Ultra DMA: " << i.ultra_dma_mode_supported << " or below\n";
      std::cout << std::setw(13) << "PIO Support: ";

      if (!all_of<bool>(i.pio_support, 8, [](bool b) { return !b; })) {
        for (auto k = 0; k < 8; k++) {
          if (i.pio_support[k]) {
            std::cout << (k > 0 ? std::string(13, ' ') : "") << "PIO Mode " << (k + 1) << "\n";
          }
        }
      } else std::cout << "None\n";

      std::cout << "ATA Standarts: \n";
      std::cout << std::setfill(' ') << std::right;
      std::cout << std::setw(14) << " ATA8-ACS: " << stringifyBool(i.ata_data.ata8_acs) << "\n";
      std::cout << std::setw(14) << " ATA/ATAPI-7: " << stringifyBool(i.ata_data.atapi_7) << "\n";
      std::cout << std::setw(14) << " ATA/ATAPI-6: " << stringifyBool(i.ata_data.atapi_6) << "\n";
      std::cout << std::setw(14) << " ATA/ATAPI-5: " << stringifyBool(i.ata_data.atapi_5) << "\n";
      std::cout << std::setw(14) << " ATA/ATAPI-4: " << stringifyBool(i.ata_data.atapi_4) << "\n";
    }

    return o;
  }
};

#endif