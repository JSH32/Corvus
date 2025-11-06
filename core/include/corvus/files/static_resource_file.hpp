#pragma once

#include <memory>
#include <physfs.h>
#include <stdexcept>
#include <vector>

namespace Corvus::Core {
class StaticResourceFile {
    PHYSFS_File* file;
    std::string  fileName;

public:
    /**
     * @brief Create a shared reference to a static file.
     *
     * @param fileName name of the file to open.
     * @return std::shared_ptr<StaticResourceFile> A shared cloneable resource handle to the file.
     */
    static std::shared_ptr<StaticResourceFile> create(const std::string& fileName) {
        return std::make_shared<StaticResourceFile>(fileName);
    }

    /**
     * @warning DO NOT USE!!
     * Use create
     */
    explicit StaticResourceFile(const std::string& fileName);
    ~StaticResourceFile();

    StaticResourceFile& operator=(const StaticResourceFile&) = delete;

    /**
     * @brief Read a specified number of bytes from the file.
     *
     * @param byteCount The number of bytes to read.
     * @return std::vector<char> Buffer with the read bytes.
     */
    std::vector<char> readBytes(size_t byteCount) const;

    /**
     * @brief Get the name of the file.
     *
     * @return std::string The name of the file.
     */
    std::string getFileName();

    /**
     * @brief Read all the bytes in the file.
     *
     * @return std::vector<char> Buffer with all the bytes from the file.
     */
    std::vector<char> readAllBytes() const;
};
}
