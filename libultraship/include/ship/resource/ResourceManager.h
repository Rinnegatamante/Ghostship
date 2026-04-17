#pragma once

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <list>
#include <vector>
#include <mutex>
#include <queue>
#include <variant>
#include "ship/resource/Resource.h"
#include "ship/resource/ResourceLoader.h"
#include "ship/resource/archive/Archive.h"
#include "ship/resource/archive/ArchiveManager.h"
#include "robin_hood.h"

namespace Ship {
struct File;

class ResourceManager {
    friend class ResourceLoader;
    typedef enum class ResourceLoadError { None, NotCached, NotFound } ResourceLoadError;

  public:
    ResourceManager();
    void Init(const std::vector<std::string>& archivePaths, const std::unordered_set<uint32_t>& validHashes,
              int32_t reservedThreadCount = 1);
    ~ResourceManager();

    bool IsLoaded();

    std::shared_ptr<ArchiveManager> GetArchiveManager();
    std::shared_ptr<ResourceLoader> GetResourceLoader();

    std::shared_ptr<IResource> GetCachedResource(const std::string& filePath, bool loadExact = false);
    std::shared_ptr<IResource> GetCachedResource(uint64_t hash, bool loadExact = false);
    std::shared_ptr<IResource> LoadResource(const std::string& filePath, bool loadExact = false,
                                            std::shared_ptr<ResourceInitData> initData = nullptr);
    std::shared_ptr<IResource> LoadResource(uint64_t crc, bool loadExact = false,
                                            std::shared_ptr<ResourceInitData> initData = nullptr);
	std::shared_ptr<IResource> LoadResourceFromCStr(const char *filePath, bool loadExact = false,
                                                         std::shared_ptr<ResourceInitData> initData = nullptr);
    std::shared_ptr<IResource> LoadResourceProcess(const std::string& filePath, bool loadExact = false,
                                                   std::shared_ptr<ResourceInitData> initData = nullptr,
                                                   uint64_t hash = 0);
	std::shared_ptr<IResource> LoadResourceProcessFast(const char *filePath);
    std::shared_ptr<IResource>
    LoadResourceAsync(const std::string& filePath, bool loadExact = false,
                      std::shared_ptr<ResourceInitData> initData = nullptr);
    std::shared_ptr<IResource>
    LoadResourceAsync(const char *filePath, bool loadExact,
                      std::shared_ptr<ResourceInitData> initData, size_t sz);
    size_t UnloadResource(uint64_t hash);
    size_t UnloadResource(const std::string& filePath);

    std::shared_ptr<std::vector<std::shared_ptr<IResource>>> LoadResources(const std::string& searchMask);
    std::shared_ptr<std::vector<std::shared_ptr<IResource>>> LoadResourcesAsync(const std::string& searchMask);

    void DirtyResources(const std::string& searchMask);
    void UnloadResources(const std::string& searchMask);
    void UnloadResourcesAsync(const std::string& searchMask);

    bool OtrSignatureCheck(const char* fileName);
    bool IsAltAssetsEnabled();
    void SetAltAssetsEnabled(bool isEnabled);
	std::shared_ptr<File> LoadFileProcess(const std::string& filePath);
	
    size_t GetResourceSize(std::shared_ptr<IResource> resource);
    size_t GetResourceSize(const char* name);
    size_t GetResourceSize(uint64_t crc);

    bool GetResourceIsCustom(std::shared_ptr<IResource> resource);
    bool GetResourceIsCustom(const char* name);
    bool GetResourceIsCustom(uint64_t crc);

    void* GetResourceRawPointer(std::shared_ptr<IResource> resource);
    void* GetResourceRawPointer(const char* name);
	void* GetOtrResourceRawPointer(const char* name);
    void* GetResourceRawPointer(uint64_t crc);

  protected:
    std::shared_ptr<std::vector<std::shared_ptr<IResource>>> LoadResourcesProcess(const std::string& searchMask);
    void UnloadResourcesProcess(const std::string& searchMask);
    std::shared_ptr<IResource> CheckCache(const std::string& filePath, bool loadExact = false);
    std::shared_ptr<IResource> CheckCache(uint64_t hash, bool loadExact = false);
  private:
    robin_hood::unordered_map<uint64_t, std::shared_ptr<IResource>> mResourceCache;
    std::shared_ptr<ResourceLoader> mResourceLoader;
    std::shared_ptr<ArchiveManager> mArchiveManager;
    bool mAltAssetsEnabled = false;
    // Private information for which owner and archive are default.
    uintptr_t mDefaultCacheOwner = 0;
    std::shared_ptr<Archive> mDefaultCacheArchive = nullptr;
};
} // namespace Ship
