#pragma once
#include "RuntimePluginABI.h"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace kairo::player::detail
{
    struct RuntimePluginManifest{std::string ID,Library;};
    // Simple bounded manifest; one basename only, no traversal or command
    // arguments. An incompatible declared ABI rejects before library loading.
    inline RuntimePluginManifest ReadRuntimePluginManifest(const std::filesystem::path& path)
    {
        if(!std::filesystem::is_regular_file(path)||std::filesystem::is_symlink(path))
            throw std::invalid_argument("Plugin manifest must be a regular file.");
        std::ifstream file(path,std::ios::binary);char bytes[4097]{};
        file.read(bytes,sizeof(bytes));const auto count=file.gcount();
        if(file.bad()||count<=0||count>4096)throw std::invalid_argument("Plugin manifest size/read failure.");
        std::istringstream in(std::string(bytes,static_cast<std::size_t>(count)));
        std::string header,version,idKey,abiKey,abi,libraryKey,trailing;
        RuntimePluginManifest manifest;
        if(!(in>>header>>version>>idKey>>manifest.ID>>abiKey>>abi>>libraryKey>>manifest.Library)||in>>trailing||
            header!="kairo-plugin"||version!="1"||idKey!="id"||abiKey!="abi"||abi!="1"||libraryKey!="library")
            throw std::invalid_argument("Invalid plugin manifest or ABI.");
        const auto safe=[](unsigned char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.';};
        if(manifest.ID.empty()||manifest.ID.size()>=64||manifest.Library.empty()||manifest.Library.size()>128||
            manifest.Library.front()=='.'||!std::all_of(manifest.ID.begin(),manifest.ID.end(),safe)||!std::all_of(manifest.Library.begin(),manifest.Library.end(),safe))
            throw std::invalid_argument("Invalid plugin ID or library basename.");
        return manifest;
    }
    // Explicit trust boundary: caller selects an installed native plugin.
    // Do not automatically scan/download/load libraries from asset packages.
    // Move/copy disabled so the callback can never outlive its loaded library.
    class RuntimeNativePlugin
    {
    public:
        explicit RuntimeNativePlugin(const std::filesystem::path& manifestPath)
        {
            const auto manifest=ReadRuntimePluginManifest(manifestPath);
            const auto library=std::filesystem::absolute(manifestPath).parent_path()/manifest.Library;
            if(!std::filesystem::is_regular_file(library)||std::filesystem::is_symlink(library))
                throw std::invalid_argument("Plugin library must be a regular file.");
#ifdef _WIN32
            m_Handle=LoadLibraryExW(library.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
#else
            m_Handle=dlopen(library.c_str(),RTLD_NOW|RTLD_LOCAL);
#endif
            if(!m_Handle)throw std::runtime_error("Plugin library load failed.");
            try
            {
#ifdef _WIN32
                const auto symbol=GetProcAddress(m_Handle,"kairo_plugin_query");
#else
                const auto symbol=dlsym(m_Handle,"kairo_plugin_query");
#endif
                if(!symbol)throw std::runtime_error("Plugin query symbol missing.");
                KairoPluginQuery query=nullptr;
                static_assert(sizeof(query)==sizeof(symbol));
                std::memcpy(&query,&symbol,sizeof(query));
                if(query(KAIRO_PLUGIN_ABI_VERSION,sizeof(m_API),&m_API)!=0||m_API.struct_size!=sizeof(m_API)||
                    m_API.abi_version!=KAIRO_PLUGIN_ABI_VERSION||!m_API.action)
                    throw std::runtime_error("Plugin rejected ABI or returned invalid descriptor.");
                const auto end=std::find(std::begin(m_API.id),std::end(m_API.id),'\0');
                if(end==std::end(m_API.id)||std::string(std::begin(m_API.id),end)!=manifest.ID)
                    throw std::runtime_error("Plugin manifest/descriptor identity mismatch.");
                m_ID=manifest.ID;
            }
            catch(...){Close();throw;}
        }
        ~RuntimeNativePlugin(){Close();}
        RuntimeNativePlugin(const RuntimeNativePlugin&)=delete;
        RuntimeNativePlugin& operator=(const RuntimeNativePlugin&)=delete;
        [[nodiscard]] const std::string& ID() const noexcept{return m_ID;}
        // A narrow extension action: return a scalar through caller-owned
        // storage. Nonzero result becomes a host error, not a partial result.
        [[nodiscard]] std::int32_t Invoke(std::int32_t input) const
        {
            std::int32_t output=0;
            if(m_API.action(input,&output)!=0)throw std::runtime_error("Plugin action rejected input.");
            return output;
        }
    private:
#ifdef _WIN32
        HMODULE m_Handle=nullptr;
#else
        void* m_Handle=nullptr;
#endif
        KairoPluginV1 m_API{};std::string m_ID;
        void Close() noexcept
        {
            if(!m_Handle)return;
#ifdef _WIN32
            FreeLibrary(m_Handle);
#else
            dlclose(m_Handle);
#endif
            m_Handle=nullptr;m_API={};
        }
    };
}
