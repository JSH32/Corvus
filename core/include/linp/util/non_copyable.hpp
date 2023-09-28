/**
 * @class NonCopyable
 *
 * @brief A class designed to prevent its derived classes from being copied or assigned to other variables.
 *
 * This class uses the delete keyword to disable the copy constructor and copy assignment operator which are normally
 * created implicitly by the C++ compiler. Any attempt to copy or assign a class that inherits from NonCopyable
 * will lead to a compile-time error. It is typically used to ensure derived objects have unique ownership of resources
 * or to prevent costly or insecure deep copies.
 */
class NonCopyable {
public:
    NonCopyable(NonCopyable const&)            = delete;
    NonCopyable& operator=(NonCopyable const&) = delete;
    NonCopyable() { }
};