#include "ship/resource/ResourceManager.h"
#include <spdlog/spdlog.h>
#include "ship/resource/File.h"
#include "ship/resource/archive/Archive.h"
#include <algorithm>
#include <thread>
#include "ship/utils/StringHelper.h"
#include "ship/utils/Utils.h"
#include "ship/config/ConsoleVariable.h"
#include "ship/Context.h"

#define XXH_STATIC_LINKING_ONLY
#define XXH_IMPLEMENTATION
#include "xxhash_utils.h"

namespace Ship {

ResourceManager::ResourceManager() {
}

void ResourceManager::Init(const std::vector<std::string>& archivePaths,
                           const std::unordered_set<uint32_t>& validHashes, int32_t reservedThreadCount) {
    mResourceLoader = std::make_shared<ResourceLoader>();
    mArchiveManager = std::make_shared<ArchiveManager>();
    GetArchiveManager()->Init(archivePaths, validHashes);
}

ResourceManager::~ResourceManager() {
    SPDLOG_INFO("destruct ResourceManager");
}

bool ResourceManager::IsLoaded() {
    return mArchiveManager != nullptr && mArchiveManager->IsLoaded();
}

std::shared_ptr<File> ResourceManager::LoadFileProcess(const std::string& filePath) {
    auto file = mArchiveManager->LoadFile(filePath);
    if (file != nullptr) {
        SPDLOG_TRACE("Loaded File {} on ResourceManager", filePath);
    } else {
        SPDLOG_TRACE("Could not load File {} in ResourceManager", filePath);
    }
    return file;
}

std::shared_ptr<IResource> ResourceManager::LoadResourceProcess(const std::string& filePath, bool loadExact,
                                                                std::shared_ptr<ResourceInitData> initData, uint64_t hash) {
    // Check for and remove the OTR signature
    if (!hash && OtrSignatureCheck(filePath.c_str())) {
        const auto newFilePath = filePath.substr(7);
        return LoadResourceProcess(newFilePath, false, initData);
    }
#ifndef __vita__
    // Attempt to load the alternate version of the asset, if we fail then we continue trying to load the standard
    // asset.
    if (!loadExact && mAltAssetsEnabled && !filePath.starts_with(IResource::gAltAssetPrefix)) {
        const auto altPath = IResource::gAltAssetPrefix + filePath;
        auto altResource = LoadResourceProcess(altPath, loadExact, initData);

        if (altResource != nullptr) {
            return altResource;
        }
    }
#endif

    if (!hash)
        hash = XXH3_64bits(filePath.c_str(), filePath.size());

#ifndef __vita__
    // Check for resource load errors which can indicate an alternate asset.
    // If we are attempting to load an alternate asset, we can return null
    if (!loadExact && mAltAssetsEnabled && filePath.starts_with(IResource::gAltAssetPrefix)) {
        if (std::holds_alternative<ResourceLoadError>(cacheLine)) {
            try {
                // If we have attempted to cache an alternate asset, but failed, we return nullptr and rely on the
                // calling function to return a regular asset. If we have NOT attempted load already, attempt the load.
                auto loadError = std::get<ResourceLoadError>(cacheLine);
                if (loadError != ResourceLoadError::NotCached) {
                    return nullptr;
                }
            } catch (std::bad_variant_access const& e) {
                // Ignore the exception. This should never happen. The last check should've returned the resource.
            }
        }
    }
#endif

    // Get the file from the OTR
    auto file = LoadFileProcess(filePath);
    if (file == nullptr) {
        SPDLOG_TRACE("Failed to load resource file at path {}", filePath);
        mResourceCache[hash] = ResourceLoadError::NotFound;
        return nullptr;
    }

    // Transform the raw data into a resource
    auto resource = GetResourceLoader()->LoadResource(filePath, file, initData);

    {
        // Set the cache to the loaded resource
        if (resource != nullptr) {
            mResourceCache[hash] = resource;
        } else {
            mResourceCache[hash] = ResourceLoadError::NotFound;
        }
    }

    if (resource != nullptr) {
        SPDLOG_TRACE("Loaded Resource {} on ResourceManager", filePath);
    } else {
        SPDLOG_TRACE("Resource load FAILED {} on ResourceManager", filePath);
    }

    return resource;
}

std::shared_ptr<IResource> ResourceManager::LoadResourceProcess(const char *filePath,
                                                                std::shared_ptr<ResourceInitData> initData, uint64_t hash) {
    // Get the file from the OTR
    auto file = LoadFileProcess(filePath);
    if (file == nullptr) {
        SPDLOG_TRACE("Failed to load resource file at path {}", filePath);
        mResourceCache[hash] = ResourceLoadError::NotFound;
        return nullptr;
    }

    // Transform the raw data into a resource
    auto resource = GetResourceLoader()->LoadResource(filePath, file, initData);

    {
        // Set the cache to the loaded resource
        if (resource != nullptr) {
            mResourceCache[hash] = resource;
        } else {
            mResourceCache[hash] = ResourceLoadError::NotFound;
        }
    }

    if (resource != nullptr) {
        SPDLOG_TRACE("Loaded Resource {} on ResourceManager", filePath);
    } else {
        SPDLOG_TRACE("Resource load FAILED {} on ResourceManager", filePath);
    }

    return resource;
}

std::shared_ptr<IResource>
ResourceManager::LoadResourceAsync(const char *filePath, bool loadExact,
                                   std::shared_ptr<ResourceInitData> initData, size_t sz) {
	uint64_t hash = XXH3_64bits(filePath, sz);

    // Check the cache before queueing the job.
    auto cacheCheck = GetCachedResource(hash, loadExact);
    if (cacheCheck) {
        return cacheCheck;
    }

    return LoadResourceProcess(filePath, initData, hash);
}

std::shared_ptr<IResource>
ResourceManager::LoadResourceAsync(const std::string& filePath, bool loadExact,
                                   std::shared_ptr<ResourceInitData> initData) {
    // Check for and remove the OTR signature
    if (OtrSignatureCheck(filePath.c_str())) {
        return LoadResourceAsync(&filePath.c_str()[7], loadExact, initData, filePath.size() - 7);
    }
    
    uint64_t hash = XXH3_64bits(filePath.c_str(), filePath.size());

    // Check the cache before queueing the job.
    auto cacheCheck = GetCachedResource(hash, loadExact);
    if (cacheCheck) {
        return cacheCheck;
    }

    return LoadResourceProcess(filePath, loadExact, initData, hash);
}

std::shared_ptr<IResource> ResourceManager::LoadResource(const std::string& filePath, bool loadExact,
                                                         std::shared_ptr<ResourceInitData> initData) {
    return LoadResourceAsync(filePath, loadExact, initData);
}

std::shared_ptr<IResource> ResourceManager::LoadResource(uint64_t crc, bool loadExact,
                                                         std::shared_ptr<ResourceInitData> initData) {
    const std::string* hashStr = GetArchiveManager()->HashToString(crc);
    if (hashStr == nullptr || hashStr->length() == 0) {
        SPDLOG_TRACE("ResourceLoad: Unknown crc {}\n", crc);
        return nullptr;
    }

    return LoadResource(*hashStr, loadExact, initData);
}

std::variant<ResourceManager::ResourceLoadError, std::shared_ptr<IResource>>
ResourceManager::CheckCache(const std::string& filePath, bool loadExact) {
#ifndef __vita__
    if (!loadExact && mAltAssetsEnabled && !filePath.starts_with(IResource::gAltAssetPrefix)) {
        const auto altPath = IResource::gAltAssetPrefix + filePath;
        auto altCacheResult = CheckCache(altPath, loadExact);

        // If the type held at this cache index is a resource, then we return it.
        // Else we attempt to load standard definition assets.
        if (std::holds_alternative<std::shared_ptr<IResource>>(altCacheResult)) {
            return altCacheResult;
        }
    }
#endif
    uint64_t hash = XXH3_64bits(filePath.c_str(), filePath.size());
    auto cacheFind = mResourceCache.find(hash);
    if (cacheFind == mResourceCache.end()) {
        return ResourceLoadError::NotCached;
    }

    return cacheFind->second;
}

std::variant<ResourceManager::ResourceLoadError, std::shared_ptr<IResource>>
ResourceManager::CheckCache(uint64_t hash, bool loadExact) {
    auto cacheFind = mResourceCache.find(hash);
    if (cacheFind == mResourceCache.end()) {
        return ResourceLoadError::NotCached;
    }

    return cacheFind->second;
}

std::shared_ptr<IResource> ResourceManager::GetCachedResource(const std::string& filePath, bool loadExact) {
    // Gets the cached resource based on filePath.
    return GetCachedResource(CheckCache(filePath, loadExact));
}

std::shared_ptr<IResource> ResourceManager::GetCachedResource(uint64_t hash, bool loadExact) {
    // Gets the cached resource based on filePath.
    return GetCachedResource(CheckCache(hash, loadExact));
}

std::shared_ptr<IResource>
ResourceManager::GetCachedResource(std::variant<ResourceLoadError, std::shared_ptr<IResource>> cacheLine) {
    // Gets the cached resource based on a cache line std::variant from the cache map.
    if (std::holds_alternative<std::shared_ptr<IResource>>(cacheLine)) {
        try {
            auto resource = std::get<std::shared_ptr<IResource>>(cacheLine);

            if (resource.use_count() <= 0) {
                return nullptr;
            }

            if (resource->IsDirty()) {
                return nullptr;
            }

            return resource;
        } catch (std::bad_variant_access const& e) {
            // Ignore the exception
        }
    }

    return nullptr;
}

std::shared_ptr<std::vector<std::shared_ptr<IResource>>>
ResourceManager::LoadResourcesProcess(const std::string& searchMask) {
    auto loadedList = std::make_shared<std::vector<std::shared_ptr<IResource>>>();
    auto fileList = GetArchiveManager()->ListFiles(searchMask);
    loadedList->reserve(fileList->size());

    for (size_t i = 0; i < fileList->size(); i++) {
        auto fileName = std::string(fileList->operator[](i));
        auto resource = LoadResource(fileName);
        loadedList->push_back(resource);
    }

    return loadedList;
}

std::shared_ptr<std::vector<std::shared_ptr<IResource>>>
ResourceManager::LoadResourcesAsync(const std::string& searchMask) {
    return LoadResourcesProcess(searchMask);
}

std::shared_ptr<std::vector<std::shared_ptr<IResource>>> ResourceManager::LoadResources(const std::string& searchMask) {
    return LoadResourcesAsync(searchMask);
}

void ResourceManager::DirtyResources(const std::string& searchMask) {
    auto list = GetArchiveManager()->ListFiles(searchMask);
    for (const auto& key : *list.get()) {
        uint64_t hash = XXH3_64bits(key.c_str(), key.size());
        auto resource = GetCachedResource(hash);
        // If it's a resource, we will set the dirty flag, else we will just unload it.
        if (resource != nullptr) {
            resource->Dirty();
        } else {
            UnloadResource(hash);
        }
    }
}

void ResourceManager::UnloadResourcesAsync(const std::string& searchMask) {
    UnloadResourcesProcess(searchMask);
}

void ResourceManager::UnloadResources(const std::string& searchMask) {
    UnloadResourcesProcess(searchMask);
}

void ResourceManager::UnloadResourcesProcess(const std::string& searchMask) {
    auto list = GetArchiveManager()->ListFiles(searchMask);

    for (const auto& key : *list.get()) {
        UnloadResource(key);
    }
}

std::shared_ptr<ArchiveManager> ResourceManager::GetArchiveManager() {
    return mArchiveManager;
}

std::shared_ptr<ResourceLoader> ResourceManager::GetResourceLoader() {
    return mResourceLoader;
}

size_t ResourceManager::UnloadResource(const std::string& searchMask) {
    // Store a shared pointer here so that erase doesn't destruct the resource.
    // The resource will attempt to load other resources on the destructor, and this will fail because we already hold
    // the mutex.
    std::variant<ResourceLoadError, std::shared_ptr<IResource>> value = nullptr;
    size_t ret = 0;
    // We can only erase the resource if we have any resources for that owner.
    uint64_t hash = XXH3_64bits(searchMask.c_str(), searchMask.size());
    if (mResourceCache.contains(hash)) {
        mResourceCache.erase(hash);
    }

    return ret;
}

size_t ResourceManager::UnloadResource(uint64_t hash) {
    // Store a shared pointer here so that erase doesn't destruct the resource.
    // The resource will attempt to load other resources on the destructor, and this will fail because we already hold
    // the mutex.
    std::variant<ResourceLoadError, std::shared_ptr<IResource>> value = nullptr;
    size_t ret = 0;
    // We can only erase the resource if we have any resources for that owner.
    if (mResourceCache.contains(hash)) {
        mResourceCache.erase(hash);
    }

    return ret;    
}

bool ResourceManager::OtrSignatureCheck(const char* fileName) {
	return fileName[0] == '_';
}

bool ResourceManager::IsAltAssetsEnabled() {
    return mAltAssetsEnabled;
}

void ResourceManager::SetAltAssetsEnabled(bool isEnabled) {
    mAltAssetsEnabled = isEnabled;
}

size_t ResourceManager::GetResourceSize(std::shared_ptr<IResource> resource) {
    if (resource == nullptr) {
        return 0;
    }

    return resource->GetPointerSize();
}

size_t ResourceManager::GetResourceSize(const char* name) {
    auto resource = LoadResource(name);

    return GetResourceSize(resource);
}

size_t ResourceManager::GetResourceSize(uint64_t crc) {
    auto resource = LoadResource(crc);

    return GetResourceSize(resource);
}

bool ResourceManager::GetResourceIsCustom(std::shared_ptr<IResource> resource) {
    if (resource == nullptr) {
        return false;
    }

    return resource->GetInitData()->IsCustom;
}

bool ResourceManager::GetResourceIsCustom(const char* name) {
    auto resource = LoadResource(name);

    return GetResourceIsCustom(resource);
}

bool ResourceManager::GetResourceIsCustom(uint64_t crc) {
    auto resource = LoadResource(crc);

    return GetResourceIsCustom(resource);
}

void* ResourceManager::GetResourceRawPointer(std::shared_ptr<IResource> resource) {
    if (resource == nullptr) {
        return nullptr;
    }

    return resource->GetRawPointer();
}

void* ResourceManager::GetResourceRawPointer(const char* name) {
    auto resource = LoadResource(name);

    return GetResourceRawPointer(resource);
}

void* ResourceManager::GetResourceRawPointer(uint64_t crc) {
    auto resource = LoadResource(crc);

    return GetResourceRawPointer(resource);
}

} // namespace Ship
