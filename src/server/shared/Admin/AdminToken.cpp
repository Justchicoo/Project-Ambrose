/*
 * Project Ambrose by Imjustchico
 * Takes the admin API token from Admin.Token or its token file, generating 32 random bytes into a file it creates itself so the permissions are always the ones built here, keeping that file in the data folder or, where the machine names none, in the folder the config file came from, putting those permissions back on a file that already exists, and refusing a token that is too short or holds anything but printable characters.
 */

#include "AdminToken.h"
#include "ListenerSettings.h"
#include "ConfigMgr.h"
#include "CryptoRandom.h"
#include "StringUtil.h"
#include "Types.h"

#include <fmt/format.h>

#include <cstddef>
#include <fstream>
#include <iterator>
#include <sstream>
#include <system_error>
#include <vector>

#ifdef _WIN32
#include <windows.h>

#include <aclapi.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace
{
    constexpr std::size_t TokenBytes = 32;

#ifdef _WIN32
    std::string LastError()
    {
        return std::error_code(static_cast<int>(::GetLastError()), std::system_category()).message();
    }

    std::string ErrorText(DWORD result)
    {
        return std::error_code(static_cast<int>(result), std::system_category()).message();
    }

    bool BuildOwnerOnlyAcl(PACL& list, std::string& error)
    {
        HANDLE processToken = nullptr;
        if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &processToken))
        {
            error = LastError();
            return false;
        }
        DWORD needed = 0;
        ::GetTokenInformation(processToken, TokenUser, nullptr, 0, &needed);
        std::vector<uint8> buffer(needed != 0 ? needed : 1);
        if (!::GetTokenInformation(processToken, TokenUser, buffer.data(), static_cast<DWORD>(buffer.size()), &needed))
        {
            error = LastError();
            ::CloseHandle(processToken);
            return false;
        }
        ::CloseHandle(processToken);

        EXPLICIT_ACCESS_W access{};
        access.grfAccessPermissions = FILE_ALL_ACCESS;
        access.grfAccessMode = SET_ACCESS;
        access.grfInheritance = NO_INHERITANCE;
        access.Trustee.TrusteeForm = TRUSTEE_IS_SID;
        access.Trustee.TrusteeType = TRUSTEE_IS_USER;
        access.Trustee.ptstrName = static_cast<LPWSTR>(reinterpret_cast<TOKEN_USER const*>(buffer.data())->User.Sid);

        list = nullptr;
        if (DWORD const result = ::SetEntriesInAclW(1, &access, nullptr, &list); result != ERROR_SUCCESS)
        {
            error = ErrorText(result);
            return false;
        }
        return true;
    }

    bool SecureOwnerOnly(std::filesystem::path const& file, std::string& error)
    {
        PACL list = nullptr;
        if (!BuildOwnerOnlyAcl(list, error))
            return false;
        std::wstring name = file.wstring();
        DWORD const result = ::SetNamedSecurityInfoW(name.data(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION, nullptr, nullptr, list, nullptr);
        ::LocalFree(list);
        if (result != ERROR_SUCCESS)
        {
            error = ErrorText(result);
            return false;
        }
        return true;
    }

    bool WriteOwnerOnly(std::filesystem::path const& file, std::string_view text, std::string& error)
    {
        PACL list = nullptr;
        if (!BuildOwnerOnlyAcl(list, error))
            return false;
        SECURITY_DESCRIPTOR descriptor{};
        bool prepared = ::InitializeSecurityDescriptor(&descriptor, SECURITY_DESCRIPTOR_REVISION) != FALSE;
        prepared = prepared && ::SetSecurityDescriptorDacl(&descriptor, TRUE, list, FALSE) != FALSE;
        prepared = prepared && ::SetSecurityDescriptorControl(&descriptor, SE_DACL_PROTECTED, SE_DACL_PROTECTED) != FALSE;
        if (!prepared)
        {
            error = LastError();
            ::LocalFree(list);
            return false;
        }

        SECURITY_ATTRIBUTES attributes{ sizeof(SECURITY_ATTRIBUTES), &descriptor, FALSE };
        HANDLE const handle = ::CreateFileW(file.c_str(), GENERIC_WRITE, 0, &attributes, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        ::LocalFree(list);
        if (handle == INVALID_HANDLE_VALUE)
        {
            error = LastError();
            return false;
        }
        std::size_t written = 0;
        while (written < text.size())
        {
            DWORD chunk = 0;
            if (!::WriteFile(handle, text.data() + written, static_cast<DWORD>(text.size() - written), &chunk, nullptr) || chunk == 0)
            {
                error = LastError();
                ::CloseHandle(handle);
                return false;
            }
            written += chunk;
        }
        ::CloseHandle(handle);
        return true;
    }
#else
    bool SecureOwnerOnly(std::filesystem::path const& file, std::string& error)
    {
        struct stat status{};
        if (::stat(file.c_str(), &status) != 0)
        {
            error = std::error_code(errno, std::generic_category()).message();
            return false;
        }
        if ((status.st_mode & (S_IRWXG | S_IRWXO)) == 0)
            return true;
        if (::chmod(file.c_str(), S_IRUSR | S_IWUSR) != 0)
        {
            error = std::error_code(errno, std::generic_category()).message();
            return false;
        }
        return true;
    }

    bool WriteOwnerOnly(std::filesystem::path const& file, std::string_view text, std::string& error)
    {
        int const descriptor = ::open(file.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, S_IRUSR | S_IWUSR);
        if (descriptor < 0)
        {
            error = std::error_code(errno, std::generic_category()).message();
            return false;
        }
        if (::fchmod(descriptor, S_IRUSR | S_IWUSR) != 0)
        {
            error = std::error_code(errno, std::generic_category()).message();
            ::close(descriptor);
            return false;
        }
        std::size_t written = 0;
        while (written < text.size())
        {
            ssize_t const chunk = ::write(descriptor, text.data() + written, text.size() - written);
            if (chunk <= 0)
            {
                if (errno == EINTR)
                    continue;
                error = std::error_code(errno, std::generic_category()).message();
                ::close(descriptor);
                return false;
            }
            written += static_cast<std::size_t>(chunk);
        }
        if (::close(descriptor) != 0)
        {
            error = std::error_code(errno, std::generic_category()).message();
            return false;
        }
        return true;
    }
#endif
}

std::string AdminToken::Generate()
{
    std::vector<uint8> const bytes = Ambrose::Crypto::GetRandomBytes(TokenBytes);
    std::string token;
    token.reserve(bytes.size() * 2);
    for (uint8 const byte : bytes)
        fmt::format_to(std::back_inserter(token), "{:02x}", byte);
    return token;
}

std::optional<std::string> AdminToken::Validate(std::string_view token)
{
    if (token.size() < ListenerSettings::MinTokenLength)
        return fmt::format("is shorter than {} characters", ListenerSettings::MinTokenLength);
    if (token.size() > ListenerSettings::MaxTokenLength)
        return fmt::format("is longer than {} characters", ListenerSettings::MaxTokenLength);
    for (char const character : token)
        if (static_cast<unsigned char>(character) <= 0x20 || static_cast<unsigned char>(character) >= 0x7F)
            return std::string("holds a character that is not printable ASCII");
    return std::nullopt;
}

std::filesystem::path AdminToken::DefaultFile(std::string const& appName, std::filesystem::path const& dataFolder, std::filesystem::path const& fallbackFolder)
{
    std::filesystem::path const& folder = dataFolder.empty() ? fallbackFolder : dataFolder;
    if (folder.empty())
        return {};
    return folder / "admin" / (appName + ".token");
}

bool AdminToken::WriteSecretFile(std::filesystem::path const& file, std::string_view text, std::string& error)
{
    std::error_code code;
    std::filesystem::path const folder = file.parent_path();
    if (!folder.empty() && !std::filesystem::exists(folder, code))
    {
        std::filesystem::create_directories(folder, code);
        if (code)
        {
            error = code.message();
            return false;
        }
    }
    std::filesystem::remove(file, code);
    if (code)
    {
        error = code.message();
        return false;
    }
    return WriteOwnerOnly(file, text, error);
}

bool AdminToken::SecureFile(std::filesystem::path const& file, std::string& error)
{
    return SecureOwnerOnly(file, error);
}

AdminTokenResult AdminToken::Resolve(ListenerSettings const& settings, std::string const& appName, std::filesystem::path const& dataFolder, std::filesystem::path const& fallbackFolder)
{
    AdminTokenResult result;
    if (!settings.Token.empty())
    {
        if (std::optional<std::string> const problem = Validate(settings.Token))
        {
            result.Error = fmt::format("Admin.Token {}", *problem);
            return result;
        }
        result.Token = settings.Token;
        result.Source = "Admin.Token";
        return result;
    }

    result.File = settings.TokenFile.empty() ? DefaultFile(appName, dataFolder, fallbackFolder) : settings.TokenFile;
    result.Source = "Admin.TokenFile";
    if (result.File.empty())
    {
        result.Error = "the admin API has no token and no file to keep one in: set Admin.Token or Admin.TokenFile";
        return result;
    }

    std::string const path = ConfigMgr::PathToUtf8(result.File);
    std::error_code code;
    if (std::filesystem::is_regular_file(result.File, code))
    {
        std::ifstream stream(result.File, std::ios::binary);
        if (!stream)
        {
            result.Error = fmt::format("the admin API token file {} cannot be read", path);
            return result;
        }
        std::ostringstream contents;
        contents << stream.rdbuf();
        std::string const token(Ambrose::Trim(contents.str()));
        if (std::optional<std::string> const problem = Validate(token))
        {
            result.Error = fmt::format("the admin API token in {} {}; delete the file to have a new token generated", path, *problem);
            return result;
        }
        result.Token = token;
        std::string secured;
        if (!SecureFile(result.File, secured))
            result.Warning = fmt::format("the admin API token file {} cannot be made readable only by this user: {}", path, secured);
        return result;
    }

    result.Token = Generate();
    std::string error;
    if (!WriteSecretFile(result.File, result.Token, error))
    {
        result.Token.clear();
        result.Error = fmt::format("the admin API token cannot be written to {}: {}", path, error);
        return result;
    }
    result.Generated = true;
    return result;
}
