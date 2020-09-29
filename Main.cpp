#include <iostream>
#include <string>
#include <optional>
#include <memory>
#include <array>
#include <vector>
#include <iomanip>
#include <algorithm>

#include <Windows.h>

#define openDrive(path) CreateFileA(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr)

std::array<std::string, 16> busTypes{
  "Unknown", "SCSI", "ATAPI", "ATA",
  "1394", "SSA", "Fibre", "USB",
  "RAID", "ISCSI", "SAS", "SATA",
  "SD", "MMC", "Vitrual", "File backed virtual"
};

std::array<std::string, 5> sizeSuffixes{
  "B", "Kb", "Mb", "Gb", "Tb"
};

struct SizeValue {
private:
  ULONGLONG _bytes;
  ULONGLONG _value{ 0 };
  std::string _suffix{ };

  size_t applySuffix() {
    for (auto sIdx = 0; sIdx < sizeSuffixes.size(); sIdx++) {
      if (_value < 1024) {
        return sIdx;
      }
      _value /= 1024;
    }
    return sizeSuffixes.size() - 1;
  }
public:
  explicit SizeValue(ULONGLONG v) :
    _bytes{ v }, _value{ v }, _suffix{ sizeSuffixes[applySuffix()] } { }

  auto bytes() const { return _bytes; }
  auto value() const { return _value; }
  auto suffix() const { return _suffix; }

};

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
  SizeValue totalSpace{0};
  SizeValue freeSpace{0};
  SizeValue busySpace{0};
  std::vector<std::string> errors { };
};

auto getErrorMessage(DWORD errorCode) {
  if (errorCode == ERROR_SUCCESS) return std::string();
  LPSTR messageBuffer{ nullptr };
  auto size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
    NULL, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPSTR>(&messageBuffer), 0, NULL);
  std::string message(messageBuffer, size);
  for (auto i = 0; i < message.size(); i++)
    if (message[i] == 0xa || message[i] == 0xd) message[i] = ' ';
  LocalFree(messageBuffer);
  return message;
}

auto trim(const std::string& str) {
  auto first = str.find_first_not_of(' ');
  if (first == std::string::npos) {
    return str;
  }
  auto last = str.find_last_not_of(' ');
  return str.substr(first, (last - first + 1));
}



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
    info.vendor = std::string(buffer.get() + descriptor->VendorIdOffset);
  if (descriptor->ProductIdOffset != 0)
    info.model = std::string(buffer.get() + descriptor->ProductIdOffset);
  if (descriptor->ProductRevisionOffset != 0)
    info.firmware_version = std::string(buffer.get() + descriptor->ProductRevisionOffset);
  if (descriptor->SerialNumberOffset != 0)
    info.serial_number = trim(std::string(buffer.get() + descriptor->SerialNumberOffset));
  if (descriptor->BusType > 0 && descriptor->BusType < busTypes.size())
    info.bus_type = busTypes[descriptor->BusType];
  else info.bus_type = "(" + std::to_string(descriptor->BusType) + ")";
  return info;
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

      const _STORAGE_DEVICE_NUMBER* deviceNumber = reinterpret_cast<_STORAGE_DEVICE_NUMBER*>(buffer.get());
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
            std::cout << "  " << vol.letter << ":\n";
            auto hasSize = vol.totalSpace.bytes() != 0;
            auto hasErrors = !vol.errors.empty();
            if (hasSize) {
              std::cout << "   Total: " << vol.totalSpace.value() << vol.totalSpace.suffix() << "\n";
              std::cout << "   Free:  " << vol.freeSpace.value() << vol.freeSpace.suffix() << "\n";
              std::cout << "   Busy:  " << vol.busySpace.value() << vol.busySpace.suffix() << "\n";
            }
            if (hasSize && hasErrors)
              std::cout << "   Errors: \n";
            if (hasErrors) {
              for (const auto& error : vol.errors) {
                std::cout << "    " << error << "\n";
              }
            }
          }
        }
      }
    } else break;
    std::cout << std::setw(20) << std::setfill('-') << "\n";
  }

}