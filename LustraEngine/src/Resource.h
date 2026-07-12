#pragma once

#include "LustraLib/Assert.h"
#include "LustraVulkan.h"
#include "vma/vk_mem_alloc.h"

#include <concepts>

struct ResourceTag
{
	// This is a struct that all resources should inherit from.
	// Its purpose is to be able to identify if a type is a resource or not to differentiate them from other normal data
	// structures.
};

// Concept that is to be checked by anything that is tied to templated usage of resources.
template <typename T>
concept ResourceType = std::derived_from<T, ResourceTag>;

template <ResourceType T>
struct Handle // Lightweight versioned resource handle
{
	uint32_t index;
	uint32_t generation;

	T* Get();
	const T* Get() const;
	void Release();
};

template <ResourceType T>
struct ResourcePool
{
	static constexpr uint32_t kMaxResourcesPerPool = 256u;
	static constexpr uint32_t kEndIndexSentinel    = UINT32_MAX;

	struct ResourceSlot
	{
		union
		{
			T object;
			uint32_t nextFree; // When slot is free, the memory can be used to point to next free slot.
		};

		uint32_t refCount   = 0;
		uint32_t generation = 0;

		ResourceSlot() : nextFree(0) {}

		~ResourceSlot()
		{
			// Deliberator NOOP.
			// Destruction is the responsibility of the pool as it
			// needs to evaluate if an object is allocated or not.
		}

		ResourceSlot(const ResourceSlot&)            = delete;
		ResourceSlot(ResourceSlot&&)                 = delete;
		ResourceSlot& operator=(const ResourceSlot&) = delete;
		ResourceSlot& operator=(ResourceSlot&&)      = delete;
	};

	std::array<ResourceSlot, kMaxResourcesPerPool> pool;
	uint32_t freeEntryHead;

	ResourcePool()
	{
		for (uint32_t i = 0; i < pool.size(); i++)
		{
			pool[i].nextFree   = i + 1;
			pool[i].generation = 1; // Start at 1 so default initialized handles are never valid.
		}

		pool.back().nextFree = kEndIndexSentinel;

		freeEntryHead = 0u;
	}

	std::vector<uint32_t> GetIndicesOfAliveObjects()
	{
		std::vector<uint32_t> aliveObjects = {};

		std::vector<bool> allocatedSlot(pool.size(), true);
		uint32_t currentFreeHead = freeEntryHead;
		while (currentFreeHead != kEndIndexSentinel)
		{
			allocatedSlot[currentFreeHead] = false;
			currentFreeHead                = pool[currentFreeHead].nextFree;
		}

		for (uint32_t i = 0; i < allocatedSlot.size(); i++)
		{
			if (allocatedSlot[i])
			{
				aliveObjects.push_back(i);
			}
		}

		return aliveObjects;
	}

	// Manually calls destructors on all allocated slots.
	// NOTE: Deallocation of GPU resources is done separately during a Destroy call.
	~ResourcePool()
	{
		const std::vector<uint32_t> aliveObjectIndices = GetIndicesOfAliveObjects();

		for (const uint32_t aliveObjectIndex : aliveObjectIndices)
		{
			ResourceSlot& aliveSlot = pool[aliveObjectIndex];

			if (aliveSlot.refCount > 0)
			{
				PRINT_WARNING("Deleting object with {} references left", aliveSlot.refCount);
			}

			aliveSlot.object.~T();
		}
	}

	T* Get(const Handle<T> handle)
	{
		// Only checks range, NOT generation as that case is handled below.
		ENSURE(IsHandleInRange(handle));

		ResourceSlot& slot = pool[handle.index];

		// Outdated generation, return nullptr
		if (slot.generation != handle.generation)
		{
			return nullptr;
		}

		return &slot.object;
	}

	// Initial allocation of object (refcount = 1)
	Handle<T> Allocate(T&& object = {})
	{
		ENSURE_EX(
		    freeEntryHead != kEndIndexSentinel, "Resource pool is full. Make sure you are allocating responsibly."
		);

		ResourceSlot& freeSlot   = pool[freeEntryHead];
		const uint32_t slotIndex = freeEntryHead;
		freeEntryHead            = freeSlot.nextFree;

		// Placement new: orders a new object to be constructed in memory that is already allocated
		// A regular move assignment might (assuming non-trivial destructor) have the freeSlot.object try to first
		// deallocate its resources before consuming the rvalue object. However, Pool is doing manual lifetime
		// management and T is never constructed until this point and might try to deallocate resources it never had, so
		// an in memory construction is required.
		new (&freeSlot.object) T(std::move(object));
		freeSlot.refCount = 1;

		return Handle<T>{.index = slotIndex, .generation = freeSlot.generation};
	}

	// Secondary lookups that want to claim shared ownership.
	// Every call to this function needs to be matched with a Release call.
	void AddRef(Handle<T> handle)
	{
		ENSURE(IsHandleValid(handle));

		pool[handle.index].refCount++;
	}

	void Release(Handle<T> handle)
	{
		ENSURE(IsHandleValid(handle));

		pool[handle.index].refCount--;
	}

	// Free when memory is to be reclaimed.
	void Free(Handle<T> handle)
	{
		ENSURE(IsHandleValid(handle));

		ResourceSlot& slot = pool[handle.index];

		slot.object.~T();              // Destroy the object.
		slot.generation++;             // Invalidate all dangling handles.
		slot.nextFree = freeEntryHead; // Point to the previous free head.

		freeEntryHead = handle.index;
	}

	ResourcePool(const ResourcePool&)            = delete;
	ResourcePool(ResourcePool&&)                 = delete;
	ResourcePool& operator=(const ResourcePool&) = delete;
	ResourcePool& operator=(ResourcePool&&)      = delete;

  private:
	static bool IsHandleInRange(const Handle<T>& handle)
	{
		return handle.index < kMaxResourcesPerPool;
	}

	bool IsHandleValid(const Handle<T>& handle)
	{
		return IsHandleInRange(handle) && (handle.generation == pool[handle.index].generation);
	}
};

namespace Resource
{
	template <ResourceType T>
	inline ResourcePool<T>& PoolInstance()
	{
		// Each pool is a single static instance of the type it stores.
		// Remains until the lifetime of the program.
		static ResourcePool<T> poolInstance;

		return poolInstance;
	}
} // namespace Resource

template <ResourceType T>
T* Handle<T>::Get()
{
	return Resource::PoolInstance<T>().Get(*this);
}

template <ResourceType T>
const T* Handle<T>::Get() const
{
	return Resource::PoolInstance<T>().Get(*this);
}

template <ResourceType T>
void Handle<T>::Release()
{
	Resource::PoolInstance<T>().Release(*this);

	// Invalidate handle
	index      = UINT32_MAX;
	generation = 0;
}
