#include "corvus/files/static_resource_file.hpp"
#include "corvus/log.hpp"

namespace Corvus::Core {
StaticResourceFile::StaticResourceFile(const std::string& fileName) : fileName(fileName) {
    file = PHYSFS_openRead(fileName.c_str());
    if (!file) {
        CORVUS_CORE_ERROR("Failed to open file: {}", fileName);
        throw std::runtime_error("Failed to open file: " + fileName);
    }

    CORVUS_CORE_INFO("Loaded static resource: {}", fileName);
}

StaticResourceFile::~StaticResourceFile() {
    if (file) {
        PHYSFS_close(file);
        CORVUS_CORE_INFO("Unloaded static resource: {}", fileName);
    }
}

std::vector<char> StaticResourceFile::readBytes(size_t byteCount) const {
    std::vector<char> buffer(byteCount);
    PHYSFS_readBytes(file, buffer.data(), byteCount);
    return buffer;
}

std::vector<char> StaticResourceFile::readAllBytes() const {
    const PHYSFS_sint64 fileLength = PHYSFS_fileLength(file);
    std::vector<char>   buffer(fileLength + 1);
    PHYSFS_readBytes(file, buffer.data(), fileLength);
    buffer[fileLength] = '\0';
    return buffer;
}

std::string StaticResourceFile::getFileName() { return fileName; }
}
