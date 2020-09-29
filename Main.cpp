#include <iostream>
#include <string>
#include <optional>
#include <memory>
#include <array>

#include <Windows.h>

#define openDrive(path) CreateFileA(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr)

struct DeviceInfo {
  std::string vendor;
  std::string model;
  std::string firmware_version;
  std::string bus_type;
  std::string serial_number;
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

int main() {
  STORAGE_PROPERTY_QUERY query;
  std::memset(&query, 0, sizeof(query));

  query.PropertyId = StorageDeviceProperty;
  query.QueryType = PropertyStandardQuery;

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
      }
    } else break;
  }


}