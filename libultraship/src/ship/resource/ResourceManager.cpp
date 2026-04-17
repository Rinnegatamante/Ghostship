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

std::shared_ptr<IResource> ResourceManager::LoadResourceProcessFast(const char *filePath) {
	uint64_t hash = XXH3_64bits(filePath, strlen(filePath));
	
	// While waiting in the queue, another thread could have loaded the resource.
    // In a last attempt to avoid doing work that will be discarded, let's check if the cached version exists.
    auto cachedResource = CheckCache(hash, false);
    if (cachedResource != nullptr) {
        return cachedResource;
    }

    // Get the file from the OTR
    auto file = LoadFileProcess(filePath);
    if (file == nullptr) {
        SPDLOG_TRACE("Failed to load resource file at path {}", filePath);
        return nullptr;
    }

    // Transform the raw data into a resource
    auto resource = GetResourceLoader()->LoadResource(filePath, file, nullptr);

    // Set the cache to the loaded resource
    if (resource != nullptr) {
        mResourceCache[hash] = resource;
    }

    if (resource != nullptr) {
        SPDLOG_TRACE("Loaded Resource {} on ResourceManager", filePath);
    } else {
        SPDLOG_TRACE("Resource load FAILED {} on ResourceManager", filePath);
    }

    return resource;
}

std::shared_ptr<IResource> ResourceManager::LoadResourceProcess(const std::string& filePath, bool loadExact,
                                                                std::shared_ptr<ResourceInitData> initData, uint64_t hash) {
    // Check for and remove the OTR signature
    if (!hash) {
		if (OtrSignatureCheck(filePath.c_str())) {
            const auto newFilePath = filePath.substr(7);
			hash = XXH3_64bits(newFilePath.c_str(), newFilePath.size());
            return LoadResourceProcess(newFilePath, loadExact, initData, hash);
		}
		
        hash = XXH3_64bits(filePath.c_str(), filePath.size());
	}
	
	// While waiting in the queue, another thread could have loaded the resource.
    // In a last attempt to avoid doing work that will be discarded, let's check if the cached version exists.
    auto cachedResource = CheckCache(hash, loadExact);
    if (cachedResource != nullptr) {
        return cachedResource;
    }

    // Get the file from the OTR
    auto file = LoadFileProcess(filePath);
    if (file == nullptr) {
        SPDLOG_TRACE("Failed to load resource file at path {}", filePath);
        return nullptr;
    }

    // Transform the raw data into a resource
    auto resource = GetResourceLoader()->LoadResource(filePath, file, initData);

    // Set the cache to the loaded resource
    if (resource != nullptr) {
        mResourceCache[hash] = resource;
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

    return LoadResourceProcess(filePath, loadExact, initData, hash);
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
	auto cacheCheck = GetCachedResource(crc, loadExact);
    if (cacheCheck) {
        return cacheCheck;
    }														 
															 
    const std::string* hashStr = GetArchiveManager()->HashToString(crc);
    if (hashStr == nullptr || hashStr->length() == 0) {
        SPDLOG_TRACE("ResourceLoad: Unknown crc {}\n", crc);
        return nullptr;
    }

    return LoadResourceProcess(*hashStr, loadExact, initData, crc);
}

std::shared_ptr<IResource> ResourceManager::CheckCache(const std::string& filePath, bool loadExact) {
    uint64_t hash = XXH3_64bits(filePath.c_str(), filePath.size());
    auto cacheFind = mResourceCache.find(hash);
    if (cacheFind == mResourceCache.end()) {
        return nullptr;
    }

    return cacheFind->second;
}

std::shared_ptr<IResource> ResourceManager::CheckCache(uint64_t hash, bool loadExact) {
    auto cacheFind = mResourceCache.find(hash);
    if (cacheFind == mResourceCache.end()) {
        return nullptr;
    }

    return cacheFind->second;
}

std::shared_ptr<IResource> ResourceManager::GetCachedResource(const std::string& filePath, bool loadExact) {
    // Gets the cached resource based on filePath.
    return CheckCache(filePath, loadExact);
}

std::shared_ptr<IResource> ResourceManager::GetCachedResource(uint64_t hash, bool loadExact) {
    // Gets the cached resource based on filePath.
    return CheckCache(hash, loadExact);
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
    std::shared_ptr<IResource> value = nullptr;
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
    std::shared_ptr<IResource> value = nullptr;
    size_t ret = 0;
    // We can only erase the resource if we have any resources for that owner.
    if (mResourceCache.contains(hash)) {
        mResourceCache.erase(hash);
    }

    return ret;    
}

bool ResourceManager::OtrSignatureCheck(const char* fileName) {
    static const char* sOtrSignature = "__OTR__";
    return strncmp(fileName, sOtrSignature, strlen(sOtrSignature)) == 0;
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

void* ResourceManager::GetResourceRawPointer(const char* filePath) {
	// Check for and remove the OTR signature
    size_t sz = strlen(filePath);
    if (OtrSignatureCheck(filePath)) {
        filePath = &filePath[7];
		sz -= 7;
    }
	
	uint64_t hash = XXH3_64bits(filePath, sz);

    // Check the cache before queueing the job.
    auto resource = GetCachedResource(hash, false);
    if (!resource) {
        resource = LoadResourceProcess(filePath, false, nullptr, hash);
	}

    return GetResourceRawPointer(resource);
}

void* ResourceManager::GetOtrResourceRawPointer(const char* filePath) {
	filePath = &filePath[7];
	uint64_t hash = XXH3_64bits(filePath, strlen(filePath));

    // Check the cache before queueing the job.
    auto resource = GetCachedResource(hash, false);
    if (!resource) {
        resource = LoadResourceProcess(filePath, false, nullptr, hash);
	}

    return GetResourceRawPointer(resource);
}

void* ResourceManager::GetResourceRawPointer(uint64_t crc) {
    auto resource = LoadResource(crc);

    return GetResourceRawPointer(resource);
}

} // namespace Ship
