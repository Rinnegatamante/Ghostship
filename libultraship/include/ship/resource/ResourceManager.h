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

    /**
     * @brief Initializes the ResourceManager, mounting archives and starting the thread pool.
     * @param archivePaths        Paths to OTR/O2R archive files or directories containing them.
     * @param validHashes         Set of acceptable game-version hash values; empty = all accepted.
     * @param reservedThreadCount Number of OS threads to reserve outside the resource thread pool.
     */
    void Init(const std::vector<std::string>& archivePaths, const std::unordered_set<uint32_t>& validHashes,
              int32_t reservedThreadCount = 1);
    ~ResourceManager();

    /**
     * @brief Returns true once Init() has completed successfully.
     * @return true if the manager is ready to load resources.
     */
    bool IsLoaded();

    /** @brief Returns the ArchiveManager that manages the mounted archives. */
    std::shared_ptr<ArchiveManager> GetArchiveManager();
    /** @brief Returns the ResourceLoader responsible for deserializing file data. */
    std::shared_ptr<ResourceLoader> GetResourceLoader();

    /**
     * @brief Returns a resource from the cache if present, without loading from disk.
     * @param filePath  Virtual path of the resource.
     * @param loadExact If true, skips alt-asset path resolution and uses the exact path.
     * @return Cached IResource, or nullptr if not found in the cache.
     */
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

    /**
     * @brief Writes raw data into an archive and optionally evicts the stale cache entry.
     * @param identifier Identifier of the resource to write.
     * @param data       Raw bytes to write.
     * @param unloadFile If true, removes the old cache entry after writing.
     * @return true on success.
     */
    bool WriteResource(const ResourceIdentifier& identifier, const std::vector<uint8_t>& data, bool unloadFile);

    /**
     * @brief Loads all resources whose paths match the given glob mask.
     * @param searchMask Glob pattern (e.g. @c "textures/ui/\*" ).
     * @return Pointer to a vector of loaded IResource objects.
     */
    std::shared_ptr<std::vector<std::shared_ptr<IResource>>> LoadResources(const std::string& searchMask);
    std::shared_ptr<std::vector<std::shared_ptr<IResource>>> LoadResourcesAsync(const std::string& searchMask);

    /**
     * @brief Marks as dirty all cached resources whose paths match a glob mask.
     *
     * Dirty resources remain in the cache but are flagged for reload on next access.
     *
     * @param searchMask Glob pattern.
     */
    void DirtyResources(const std::string& searchMask);

    void UnloadResources(const std::string& searchMask);
    void UnloadResourcesAsync(const std::string& searchMask);

    /**
     * @brief Checks whether a file is a valid OTR archive by reading its header signature.
     * @param fileName Path to the file to inspect.
     * @return true if the file starts with a valid OTR signature.
     */
    bool OtrSignatureCheck(const char* fileName);

    /**
     * @brief Returns whether alt-asset (mod) loading is currently enabled.
     * @return true if alternate assets are active.
     */
    bool IsAltAssetsEnabled();

    /**
     * @brief Enables or disables alt-asset (mod) loading.
     * @param isEnabled true to enable, false to disable.
     */
    void SetAltAssetsEnabled(bool isEnabled);

	std::shared_ptr<File> LoadFileProcess(const std::string& filePath);
	
    size_t GetResourceSize(std::shared_ptr<IResource> resource);

    /**
     * @brief Returns the byte size of the payload of a resource identified by path.
     * @param name Virtual path of the resource.
     * @return Size in bytes, or 0 if not found.
     */
    size_t GetResourceSize(const char* name);

    /**
     * @brief Returns the byte size of the payload of a resource identified by CRC.
     * @param crc 64-bit content hash.
     * @return Size in bytes, or 0 if not found.
     */
    size_t GetResourceSize(uint64_t crc);

    /**
     * @brief Returns true if the given resource originated from an alt-assets override.
     * @param resource Shared pointer to the resource.
     */
    bool GetResourceIsCustom(std::shared_ptr<IResource> resource);

    /**
     * @brief Returns true if the resource at the given path is an alt-asset override.
     * @param name Virtual path of the resource.
     */
    bool GetResourceIsCustom(const char* name);

    /**
     * @brief Returns true if the resource identified by CRC is an alt-asset override.
     * @param crc 64-bit content hash.
     */
    bool GetResourceIsCustom(uint64_t crc);

    /**
     * @brief Returns a type-erased raw pointer to the resource payload.
     * @param resource Shared pointer to the resource.
     * @return Void pointer to the payload, or nullptr if the resource is null.
     */
    void* GetResourceRawPointer(std::shared_ptr<IResource> resource);

    /**
     * @brief Returns a type-erased raw pointer to the payload of a resource by path.
     * @param name Virtual path of the resource.
     * @return Void pointer to the payload, or nullptr if not found.
     */
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
