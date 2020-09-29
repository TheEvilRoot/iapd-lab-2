#include <iostream>
#include <string>
#include <optional>
#include <memory>
#include <array>
#include <vector>

#include <Windows.h>

#define openDrive(path) CreateFileA(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr)

struct DeviceInfo {
  std::string vendor;
  std::string model;
  std::string firmware_version;
  std::string bus_type;
  std::string serial_number;
};

struct Volume {
  char letter;
  DWORD deviceNumber;
  _ULARGE_INTEGER totalSpace;
  _ULARGE_INTEGER freeSpace;
  _ULARGE_INTEGER busySpace;
};

std::array<std::string, 16> busTypes{
  "Unknown", "SCSI", "ATAPI", "ATA",
  "1394", "SSA", "Fibre", "USB",
  "RAID", "ISCSI", "SAS", "SATA",
  "SD", "MMC", "Vitrual", "File backed virtual"
};

std::optional<DeviceInfo> acquireDeviceInfo(HANDLE driveHandle, STORAGE_PROPERTY_QUERY& query) {
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
    info.vendor = std::string(buffer.get() + descriptor->VendorIdOffset);
  if (descriptor->ProductIdOffset != 0)
    info.model = std::string(buffer.get() + descriptor->ProductIdOffset);
  if (descriptor->ProductRevisionOffset != 0)
    info.firmware_version = std::string(buffer.get() + descriptor->ProductRevisionOffset);
  if (descriptor->SerialNumberOffset != 0)
    info.serial_number = std::string(buffer.get() + descriptor->SerialNumberOffset);
  if (descriptor->BusType > 0 && descriptor->BusType < busTypes.size())
    info.bus_type = busTypes[descriptor->BusType];
  else info.bus_type = "(" + std::to_string(descriptor->BusType) + ")";
  return info;
}

auto indexVolumes(STORAGE_PROPERTY_QUERY& query) {
  std::vector<Volume> index;
  auto logicalDrivesCount = GetLogicalDrives();

  for (char letter = 'A'; letter <= 'Z'; letter++) {
    if ((logicalDrivesCount >> letter - 'A') & 1 && letter) { // what the fuck?
      std::string path = letter + ":\\";
      Volume vol;
      vol.letter = letter;
      GetDiskFreeSpaceExA(path.c_str(), 0, &vol.totalSpace, &vol.freeSpace);

      path = std::string("\\\\.\\") + std::string(1, letter) + ":";

      auto volumeHandle = CreateFileA(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
      if (volumeHandle == INVALID_HANDLE_VALUE) {
        std::cerr << "indexVolumes: " << letter << ": " << GetLastError() << "\n";
      }

      auto buffer = std::make_unique<char[]>(12);
      if (!DeviceIoControl(volumeHandle, IOCTL_STORAGE_GET_DEVICE_NUMBER, &query, sizeof(query), buffer.get(), 12, nullptr, nullptr)) {
        continue;
      }

      const _STORAGE_DEVICE_NUMBER* deviceNumber = reinterpret_cast<_STORAGE_DEVICE_NUMBER*>(buffer.get());
      vol.deviceNumber = deviceNumber->DeviceNumber;
      vol.busySpace.QuadPart = vol.totalSpace.QuadPart - vol.freeSpace.QuadPart;
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

  HANDLE driveHandle{ INVALID_HANDLE_VALUE };
  for (uint32_t index = 0; true; index++) {
    std::string drivePath = "\\\\.\\PhysicalDrive" + std::to_string(index);
    if (driveHandle = openDrive(drivePath); driveHandle != INVALID_HANDLE_VALUE) {
      if (auto info = acquireDeviceInfo(driveHandle, query); info) {
        auto i = info.value();
        std::cout << "Vendor: " << i.vendor << "\n";
        std::cout << "Model: " << i.model << "\n";
        std::cout << "Firmware: " << i.firmware_version << "\n";
        std::cout << "Bus type: " << i.bus_type << "\n";
        std::cout << "Serial number: " << i.serial_number << "\n";

        std::cout << " Volumes: \n";
        for (const auto vol : volumeIndex) {
          if (vol.deviceNumber == index) {
            std::cout << "  " << vol.letter << ": \n   Total: " << vol.totalSpace.QuadPart << "\n   Free: " << vol.freeSpace.QuadPart << "\n   Busy: " << vol.busySpace.QuadPart << "\n\n";
          }
        }
      }
    } else break;
  }

}