#include <iostream>
#include <string>
#include <optional>
#include <memory>
#include <array>
#include <vector>
#include <iomanip>
#include <algorithm>

#include <Windows.h>
#include <ntddscsi.h>

#include "Utils.hpp"

#define openDrive(path) CreateFileA(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr)

std::array<std::string, 16> busTypes{
  "Unknown", "SCSI", "ATAPI", "ATA",
  "1394", "SSA", "Fibre", "USB",
  "RAID", "ISCSI", "SAS", "SATA",
  "SD", "MMC", "Vitrual", "File backed virtual"
};

struct DeviceInfo {
  std::string vendor{"N/A"};
  std::string model{"N/A"};
  std::string firmware_version{"N/A"};
  std::string bus_type{"N/A"};
  std::string serial_number{"N/A"};

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
    return o;
  }
};

struct Volume {
  char letter;
  DWORD deviceNumber;
  SizeValue totalSpace{0};
  SizeValue freeSpace{0};
  SizeValue busySpace{0};
  std::vector<std::string> errors { };
  std::vector<std::string> ataSupport{ };
};

auto acquireDeviceInfo(HANDLE driveHandle, STORAGE_PROPERTY_QUERY& query) {
  STORAGE_DESCRIPTOR_HEADER header;
  if (!DeviceIoControl(driveHandle, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), &header, sizeof(header), nullptr, nullptr)) {
    std::cerr << "acquireDeviceInfo: " << driveHandle << " failed to acquire header :: " << GetLastError() << "\n";
    return std::optional<DeviceInfo>();
  }
  DWORD bufferSize = header.Size;
  auto buffer = std::make_unique<char[]>(bufferSize);

  if (!DeviceIoControl(driveHandle, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), buffer.get(), bufferSize, nullptr, nullptr)) {
    std::cerr << "acquireDeviceInfo: " << driveHandle << " failed to acquire buffer :: " << GetLastError() << "\n";
    return std::optional<DeviceInfo>();
  }

  const STORAGE_DEVICE_DESCRIPTOR* descriptor = reinterpret_cast<const STORAGE_DEVICE_DESCRIPTOR*>(buffer.get());
  DeviceInfo info;
  if (descriptor->VendorIdOffset != 0)
    info.vendor = trim(std::string(buffer.get() + descriptor->VendorIdOffset));
  if (descriptor->ProductIdOffset != 0)
    info.model = trim(std::string(buffer.get() + descriptor->ProductIdOffset));
  if (descriptor->ProductRevisionOffset != 0)
    info.firmware_version = std::string(buffer.get() + descriptor->ProductRevisionOffset);
  if (descriptor->SerialNumberOffset != 0)
    info.serial_number = trim(std::string(buffer.get() + descriptor->SerialNumberOffset));
  if (descriptor->BusType > 0 && descriptor->BusType < busTypes.size())
    info.bus_type = busTypes[descriptor->BusType];
  else info.bus_type = "(" + std::to_string(descriptor->BusType) + ")";

  auto identitySize = 512;
  auto identityBuffer = std::make_unique<unsigned char[]>(identitySize + sizeof(ATA_PASS_THROUGH_EX));
  auto ataPass = reinterpret_cast<ATA_PASS_THROUGH_EX*>(identityBuffer.get());
  ataPass->Length = sizeof(ataPass);
  ataPass->TimeOutValue = 10;
  ataPass->DataTransferLength = identitySize;
  ataPass->DataBufferOffset = sizeof(ATA_PASS_THROUGH_EX);

  auto ideRegs = reinterpret_cast<IDEREGS*>(ataPass->CurrentTaskFile);
  ideRegs->bCommandReg = 0xec;
  ideRegs->bSectorCountReg = 1;

  ataPass->AtaFlags = ATA_FLAGS_DATA_IN | ATA_FLAGS_DRDY_REQUIRED;

  if (!DeviceIoControl(driveHandle, IOCTL_ATA_PASS_THROUGH, ataPass, identitySize + sizeof(ATA_PASS_THROUGH_EX), ataPass, identitySize + sizeof(ATA_PASS_THROUGH_EX), nullptr, nullptr)) {
    std::cerr << "acquireDeviceInfo: DeviceIoControl IOCTL_ATA_PASS_THROUGH (" << info.model << ") :: " << GetLastError() << "\n";
    return std::optional<DeviceInfo>();
  }
  
  auto* data = reinterpret_cast<WORD*>(identityBuffer.get() + sizeof(ATA_PASS_THROUGH_EX));
  uint16_t ataSupport = data[80];

  struct {
    uint8_t : 7;
    uint8_t ata8_acs : 1;
    uint8_t ata_atapi_7 : 1;
    uint8_t ata_atapi_6 : 1;
    uint8_t ata_atapi_5 : 1;
    uint8_t ata_atapi_4 : 1;
    uint8_t : 4;
  } ataMask;
  std::memcpy(&ataMask, &ataSupport, 2);
  _asm nop;
  return std::optional(info);
}

auto indexVolumes(STORAGE_PROPERTY_QUERY& query) {
  std::vector<Volume> index;
  auto lettersMask = GetLogicalDrives();

  for (char letter = 'A'; letter <= 'Z'; letter++) {
    if ((lettersMask >> (letter - 'A')) & 1) {
      std::string path = std::string(1, letter) + ":\\";
      Volume vol{letter};
      ULARGE_INTEGER total;
      ULARGE_INTEGER free;
      if (!GetDiskFreeSpaceExA(path.c_str(), 0, &total, &free)) {
        auto error = GetLastError();
        vol.errors.push_back(getErrorMessage(GetLastError()));
      } else {
        vol.totalSpace = SizeValue(total.QuadPart);
        vol.freeSpace = SizeValue(free.QuadPart);
        vol.busySpace = SizeValue(total.QuadPart - free.QuadPart);
      }

      path = std::string("\\\\.\\") + std::string(1, letter) + ":";

      auto volumeHandle = CreateFileA(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
      if (volumeHandle == INVALID_HANDLE_VALUE) {
        std::cerr << "indexVolumes: CreateFileA is INVALID_HANDLE_VALUE :: (" << letter << ") :: " << GetLastError() << "\n";
        continue;
      }

      auto buffer = std::make_unique<char[]>(12);
      if (!DeviceIoControl(volumeHandle, IOCTL_STORAGE_GET_DEVICE_NUMBER, &query, sizeof(query), buffer.get(), 12, nullptr, nullptr)) {
        std::cerr << "indexVolumes: DeviceIoControl (" << letter << ") :: " << GetLastError() << "\n";
        continue;
      }

      const auto* deviceNumber = reinterpret_cast<_STORAGE_DEVICE_NUMBER*>(buffer.get());
      vol.deviceNumber = deviceNumber->DeviceNumber;
      index.push_back(vol);
    }
  }
  return index;
}

int main() {
  STORAGE_PROPERTY_QUERY query;
  std::memset(&query, 0, sizeof(query));
  query.PropertyId = StorageDeviceProperty;
  query.QueryType = PropertyStandardQuery;

  auto volumeIndex = indexVolumes(query);

  for (uint32_t index = 0; true; index++) {
    std::string drivePath = "\\\\.\\PhysicalDrive" + std::to_string(index);
    if (auto driveHandle = openDrive(drivePath); driveHandle != INVALID_HANDLE_VALUE) {
      if (auto result = acquireDeviceInfo(driveHandle, query); result) {
        auto info = result.value();
        std::cout << info << "\n";

        for (const auto vol : volumeIndex) {
          if (vol.deviceNumber == index) {
            std::cout << "Volume " << vol.letter << ":\n";

            auto hasSize = vol.totalSpace.bytes() != 0;
            auto hasErrors = !vol.errors.empty();

            if (hasSize) {
              std::cout << "\t" << vol.busySpace << " / " << vol.totalSpace << " (" << vol.freeSpace << " free)\n";
              if (hasErrors)
                std::cout << "\tErrors: \n";
            } 
            if (hasErrors) {
              for (const auto& error : vol.errors) {
                std::cout << "\t" << error << "\n";
              }
            }
          }
        }
      }
    } else break;
    std::cout << "\n";
  }
  getchar();
}