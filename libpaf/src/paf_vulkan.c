#define LIBPAF_EXPORTS
#include "paf_vulkan.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ── Platform dynamic loading ─────────────────────────────────────────────────

#if defined(_WIN32) && !defined(__ANDROID__) && !defined(__linux__)
#include <windows.h>
typedef HMODULE lib_handle_t;
#define lib_open(n)      LoadLibraryA(n)
#define lib_sym(h,s)     (void*)(uintptr_t)GetProcAddress((HMODULE)(h), s)
#define lib_close(h)     FreeLibrary((HMODULE)(h))
static const char* VK_LIB = "vulkan-1.dll";
#else
#include <dlfcn.h>
typedef void* lib_handle_t;
#define lib_open(n)      dlopen(n, RTLD_NOW | RTLD_LOCAL)
#define lib_sym(h,s)     dlsym(h, s)
#define lib_close(h)     dlclose(h)
static const char* VK_LIB = "libvulkan.so.1";
#endif

// ── Minimal Vulkan type definitions (no vulkan.h dependency) ─────────────────

typedef uint64_t VkInstance_T;
typedef uint64_t VkPhysicalDevice_T;
typedef uint64_t VkDevice_T;
typedef uint64_t VkQueue_T;
typedef uint64_t VkCommandPool_T;
typedef uint64_t VkCommandBuffer_T;
typedef uint64_t VkDescriptorSetLayout_T;
typedef uint64_t VkDescriptorPool_T;
typedef uint64_t VkDescriptorSet_T;
typedef uint64_t VkPipelineLayout_T;
typedef uint64_t VkShaderModule_T;
typedef uint64_t VkPipeline_T;
typedef uint64_t VkBuffer_T;
typedef uint64_t VkDeviceMemory_T;
typedef uint64_t VkFence_T;

typedef VkInstance_T*           VkInstance;
typedef VkPhysicalDevice_T*     VkPhysicalDevice;
typedef VkDevice_T*             VkDevice;
typedef VkQueue_T*              VkQueue;
typedef VkCommandPool_T*        VkCommandPool;
typedef VkCommandBuffer_T*      VkCommandBuffer;
typedef VkDescriptorSetLayout_T* VkDescriptorSetLayout;
typedef VkDescriptorPool_T*     VkDescriptorPool;
typedef VkDescriptorSet_T*      VkDescriptorSet;
typedef VkPipelineLayout_T*     VkPipelineLayout;
typedef VkShaderModule_T*       VkShaderModule;
typedef VkPipeline_T*           VkPipeline;
typedef VkBuffer_T*             VkBuffer;
typedef VkDeviceMemory_T*       VkDeviceMemory;
typedef VkFence_T*              VkFence;

typedef int32_t  VkResult;
typedef uint32_t VkFlags;
typedef uint32_t VkBool32;
typedef uint64_t VkDeviceSize;

#define VK_SUCCESS                0
#define VK_NULL_HANDLE            NULL
#define VK_WHOLE_SIZE             (~(uint64_t)0)
#define VK_STRUCTURE_TYPE_APPLICATION_INFO                   0
#define VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO               1
#define VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO           2
#define VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO                 3
#define VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO          16
#define VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO       29
#define VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO  18
#define VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO        30
#define VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO  32
#define VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO        33
#define VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO       34
#define VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET               35
#define VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO                 12
#define VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO               5
#define VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO           39
#define VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO       40
#define VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO          42
#define VK_STRUCTURE_TYPE_SUBMIT_INFO                        4
#define VK_STRUCTURE_TYPE_FENCE_CREATE_INFO                  8

#define VK_QUEUE_COMPUTE_BIT              0x02
#define VK_BUFFER_USAGE_STORAGE_BUFFER_BIT 0x20
#define VK_BUFFER_USAGE_TRANSFER_DST_BIT   0x02
#define VK_BUFFER_USAGE_TRANSFER_SRC_BIT   0x01
#define VK_SHARING_MODE_EXCLUSIVE          0
#define VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT   0x02
#define VK_MEMORY_PROPERTY_HOST_COHERENT_BIT  0x04
#define VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT   0x01
#define VK_DESCRIPTOR_TYPE_STORAGE_BUFFER     7
#define VK_SHADER_STAGE_COMPUTE_BIT           0x20
#define VK_PIPELINE_BIND_POINT_COMPUTE        1
#define VK_COMMAND_BUFFER_LEVEL_PRIMARY       0
#define VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT 0x01
#define VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT  0x00000800
#define VK_ACCESS_SHADER_WRITE_BIT            0x00000040

typedef struct { uint32_t sType; const void* pNext; uint32_t apiVersion; } VkApplicationInfo;
typedef struct { uint32_t sType; const void* pNext; VkFlags flags;
                 const VkApplicationInfo* pApplicationInfo;
                 uint32_t enabledLayerCount; const char* const* ppEnabledLayerNames;
                 uint32_t enabledExtensionCount; const char* const* ppEnabledExtensionNames; } VkInstanceCreateInfo;
typedef struct { uint32_t sType; const void* pNext; VkFlags flags; uint32_t queueFamilyIndex;
                 uint32_t queueCount; const float* pQueuePriorities; } VkDeviceQueueCreateInfo;
typedef struct { uint32_t sType; const void* pNext; VkFlags flags;
                 uint32_t queueCreateInfoCount; const VkDeviceQueueCreateInfo* pQueueCreateInfos;
                 uint32_t enabledLayerCount; const char* const* ppEnabledLayerNames;
                 uint32_t enabledExtensionCount; const char* const* ppEnabledExtensionNames;
                 const void* pEnabledFeatures; } VkDeviceCreateInfo;
typedef struct { uint32_t sType; const void* pNext; VkFlags flags;
                 size_t codeSize; const uint32_t* pCode; } VkShaderModuleCreateInfo;
typedef struct { uint32_t sType; const void* pNext; VkFlags flags;
                 uint32_t bindingCount; const void* pBindings; } VkDescriptorSetLayoutCreateInfo;
typedef struct { uint32_t binding; uint32_t descriptorType; uint32_t descriptorCount;
                 VkFlags stageFlags; const void* pImmutableSamplers; } VkDescriptorSetLayoutBinding;
typedef struct { uint32_t sType; const void* pNext; VkFlags flags;
                 uint32_t setLayoutCount; const VkDescriptorSetLayout* pSetLayouts;
                 uint32_t pushConstantRangeCount; const void* pPushConstantRanges; } VkPipelineLayoutCreateInfo;
typedef struct { VkFlags stageFlags; uint32_t offset; uint32_t size; } VkPushConstantRange;
typedef struct { uint32_t sType; const void* pNext; VkFlags flags;
                 uint32_t stage; VkShaderModule module; const char* pName;
                 const void* pSpecializationInfo; } VkPipelineShaderStageCreateInfo;
typedef struct { uint32_t sType; const void* pNext; VkFlags flags;
                 VkPipelineShaderStageCreateInfo stage; VkPipelineLayout layout;
                 VkPipeline basePipelineHandle; int32_t basePipelineIndex; } VkComputePipelineCreateInfo;
typedef struct { uint32_t sType; const void* pNext; VkFlags flags;
                 uint32_t maxSets; uint32_t poolSizeCount; const void* pPoolSizes; } VkDescriptorPoolCreateInfo;
typedef struct { uint32_t type; uint32_t descriptorCount; } VkDescriptorPoolSize;
typedef struct { uint32_t sType; const void* pNext;
                 VkDescriptorPool descriptorPool;
                 uint32_t descriptorSetCount; const VkDescriptorSetLayout* pSetLayouts; } VkDescriptorSetAllocateInfo;
typedef struct { VkBuffer buffer; VkDeviceSize offset; VkDeviceSize range; } VkDescriptorBufferInfo;
typedef struct { uint32_t sType; const void* pNext; VkDescriptorSet dstSet;
                 uint32_t dstBinding; uint32_t dstArrayElement;
                 uint32_t descriptorCount; uint32_t descriptorType;
                 const void* pImageInfo; const VkDescriptorBufferInfo* pBufferInfo;
                 const void* pTexelBufferView; } VkWriteDescriptorSet;
typedef struct { uint32_t sType; const void* pNext; VkFlags flags;
                 VkDeviceSize size; VkFlags usage; uint32_t sharingMode;
                 uint32_t queueFamilyIndexCount; const uint32_t* pQueueFamilyIndices; } VkBufferCreateInfo;
typedef struct { VkDeviceSize size; VkDeviceSize alignment; uint32_t memoryTypeBits; } VkMemoryRequirements;
typedef struct { uint32_t sType; const void* pNext;
                 VkDeviceSize allocationSize; uint32_t memoryTypeIndex; } VkMemoryAllocateInfo;
typedef struct { uint32_t sType; const void* pNext; VkFlags flags;
                 uint32_t queueFamilyIndex; } VkCommandPoolCreateInfo;
typedef struct { uint32_t sType; const void* pNext; VkCommandPool commandPool;
                 uint32_t level; uint32_t commandBufferCount; } VkCommandBufferAllocateInfo;
typedef struct { uint32_t sType; const void* pNext; VkFlags flags;
                 const void* pInheritanceInfo; } VkCommandBufferBeginInfo;
typedef struct { uint32_t sType; const void* pNext; uint32_t waitSemaphoreCount;
                 const void* pWaitSemaphores; const VkFlags* pWaitDstStageMask;
                 uint32_t commandBufferCount; const VkCommandBuffer* pCommandBuffers;
                 uint32_t signalSemaphoreCount; const void* pSignalSemaphores; } VkSubmitInfo;
typedef struct { uint32_t sType; const void* pNext; VkFlags flags; } VkFenceCreateInfo;
typedef struct { uint32_t memoryTypeCount;
                 struct { VkFlags propertyFlags; uint32_t heapIndex; } memoryTypes[32];
                 uint32_t memoryHeapCount;
                 struct { VkDeviceSize size; VkFlags flags; } memoryHeaps[16]; } VkPhysicalDeviceMemoryProperties;
typedef struct { uint32_t queueFlags; uint32_t queueCount;
                 uint32_t timestampValidBits; uint32_t minImageTransferGranularityW; } VkQueueFamilyProperties;

// ── Vulkan function pointer types ─────────────────────────────────────────────

typedef VkResult (*PFN_vkCreateInstance)(const VkInstanceCreateInfo*, const void*, VkInstance*);
typedef void     (*PFN_vkDestroyInstance)(VkInstance, const void*);
typedef VkResult (*PFN_vkEnumeratePhysicalDevices)(VkInstance, uint32_t*, VkPhysicalDevice*);
typedef void     (*PFN_vkGetPhysicalDeviceQueueFamilyProperties)(VkPhysicalDevice, uint32_t*, VkQueueFamilyProperties*);
typedef void     (*PFN_vkGetPhysicalDeviceMemoryProperties)(VkPhysicalDevice, VkPhysicalDeviceMemoryProperties*);
typedef VkResult (*PFN_vkCreateDevice)(VkPhysicalDevice, const VkDeviceCreateInfo*, const void*, VkDevice*);
typedef void     (*PFN_vkDestroyDevice)(VkDevice, const void*);
typedef void     (*PFN_vkGetDeviceQueue)(VkDevice, uint32_t, uint32_t, VkQueue*);
typedef VkResult (*PFN_vkCreateShaderModule)(VkDevice, const VkShaderModuleCreateInfo*, const void*, VkShaderModule*);
typedef void     (*PFN_vkDestroyShaderModule)(VkDevice, VkShaderModule, const void*);
typedef VkResult (*PFN_vkCreateDescriptorSetLayout)(VkDevice, const VkDescriptorSetLayoutCreateInfo*, const void*, VkDescriptorSetLayout*);
typedef void     (*PFN_vkDestroyDescriptorSetLayout)(VkDevice, VkDescriptorSetLayout, const void*);
typedef VkResult (*PFN_vkCreatePipelineLayout)(VkDevice, const VkPipelineLayoutCreateInfo*, const void*, VkPipelineLayout*);
typedef void     (*PFN_vkDestroyPipelineLayout)(VkDevice, VkPipelineLayout, const void*);
typedef VkResult (*PFN_vkCreateComputePipelines)(VkDevice, void*, uint32_t, const VkComputePipelineCreateInfo*, const void*, VkPipeline*);
typedef void     (*PFN_vkDestroyPipeline)(VkDevice, VkPipeline, const void*);
typedef VkResult (*PFN_vkCreateDescriptorPool)(VkDevice, const VkDescriptorPoolCreateInfo*, const void*, VkDescriptorPool*);
typedef void     (*PFN_vkDestroyDescriptorPool)(VkDevice, VkDescriptorPool, const void*);
typedef VkResult (*PFN_vkAllocateDescriptorSets)(VkDevice, const VkDescriptorSetAllocateInfo*, VkDescriptorSet*);
typedef void     (*PFN_vkUpdateDescriptorSets)(VkDevice, uint32_t, const VkWriteDescriptorSet*, uint32_t, const void*);
typedef VkResult (*PFN_vkCreateBuffer)(VkDevice, const VkBufferCreateInfo*, const void*, VkBuffer*);
typedef void     (*PFN_vkDestroyBuffer)(VkDevice, VkBuffer, const void*);
typedef void     (*PFN_vkGetBufferMemoryRequirements)(VkDevice, VkBuffer, VkMemoryRequirements*);
typedef VkResult (*PFN_vkAllocateMemory)(VkDevice, const VkMemoryAllocateInfo*, const void*, VkDeviceMemory*);
typedef void     (*PFN_vkFreeMemory)(VkDevice, VkDeviceMemory, const void*);
typedef VkResult (*PFN_vkBindBufferMemory)(VkDevice, VkBuffer, VkDeviceMemory, VkDeviceSize);
typedef VkResult (*PFN_vkMapMemory)(VkDevice, VkDeviceMemory, VkDeviceSize, VkDeviceSize, VkFlags, void**);
typedef void     (*PFN_vkUnmapMemory)(VkDevice, VkDeviceMemory);
typedef VkResult (*PFN_vkCreateCommandPool)(VkDevice, const VkCommandPoolCreateInfo*, const void*, VkCommandPool*);
typedef void     (*PFN_vkDestroyCommandPool)(VkDevice, VkCommandPool, const void*);
typedef VkResult (*PFN_vkAllocateCommandBuffers)(VkDevice, const VkCommandBufferAllocateInfo*, VkCommandBuffer*);
typedef void     (*PFN_vkFreeCommandBuffers)(VkDevice, VkCommandPool, uint32_t, const VkCommandBuffer*);
typedef VkResult (*PFN_vkBeginCommandBuffer)(VkCommandBuffer, const VkCommandBufferBeginInfo*);
typedef VkResult (*PFN_vkEndCommandBuffer)(VkCommandBuffer);
typedef void     (*PFN_vkCmdBindPipeline)(VkCommandBuffer, uint32_t, VkPipeline);
typedef void     (*PFN_vkCmdBindDescriptorSets)(VkCommandBuffer, uint32_t, VkPipelineLayout, uint32_t, uint32_t, const VkDescriptorSet*, uint32_t, const uint32_t*);
typedef void     (*PFN_vkCmdPushConstants)(VkCommandBuffer, VkPipelineLayout, VkFlags, uint32_t, uint32_t, const void*);
typedef void     (*PFN_vkCmdDispatch)(VkCommandBuffer, uint32_t, uint32_t, uint32_t);
typedef VkResult (*PFN_vkQueueSubmit)(VkQueue, uint32_t, const VkSubmitInfo*, VkFence);
typedef VkResult (*PFN_vkQueueWaitIdle)(VkQueue);
typedef VkResult (*PFN_vkCreateFence)(VkDevice, const VkFenceCreateInfo*, const void*, VkFence*);
typedef void     (*PFN_vkDestroyFence)(VkDevice, VkFence, const void*);
typedef VkResult (*PFN_vkWaitForFences)(VkDevice, uint32_t, const VkFence*, VkBool32, uint64_t);
typedef VkResult (*PFN_vkResetFences)(VkDevice, uint32_t, const VkFence*);
typedef VkResult (*PFN_vkResetCommandPool)(VkDevice, VkCommandPool, VkFlags);

// ── State ─────────────────────────────────────────────────────────────────────

static lib_handle_t    s_lib            = NULL;
static int             s_avail          = -1;  /* -1=uninitialised */
static VkInstance      s_instance       = VK_NULL_HANDLE;
static VkPhysicalDevice s_phys          = VK_NULL_HANDLE;
static VkDevice        s_device         = VK_NULL_HANDLE;
static VkQueue         s_queue          = VK_NULL_HANDLE;
static VkCommandPool   s_cmd_pool       = VK_NULL_HANDLE;
static VkDescriptorSetLayout s_dsl      = VK_NULL_HANDLE;
static VkPipelineLayout s_pipe_layout   = VK_NULL_HANDLE;
static VkPipeline      s_pipeline       = VK_NULL_HANDLE;
static VkDescriptorPool s_desc_pool     = VK_NULL_HANDLE;
static uint32_t        s_queue_family   = 0;
static VkPhysicalDeviceMemoryProperties s_mem_props;

#define LOAD_VK(name) \
    PFN_##name vk_##name = (PFN_##name)lib_sym(s_lib, #name); \
    if (!vk_##name) goto fail;

// ── SPIR-V loader ─────────────────────────────────────────────────────────────

static uint32_t* load_spv(const char* filename, size_t* out_size) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) return NULL;
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    rewind(fp);
    if (sz <= 0 || (sz & 3) != 0) { fclose(fp); return NULL; }
    uint32_t* buf = (uint32_t*)malloc((size_t)sz);
    if (!buf) { fclose(fp); return NULL; }
    if (fread(buf, 1, (size_t)sz, fp) != (size_t)sz) { free(buf); fclose(fp); return NULL; }
    fclose(fp);
    *out_size = (size_t)sz;
    return buf;
}

// ── Memory helper ─────────────────────────────────────────────────────────────

static uint32_t find_memory_type(uint32_t type_bits, VkFlags required_props) {
    for (uint32_t i = 0; i < s_mem_props.memoryTypeCount; i++) {
        if ((type_bits & (1u << i)) &&
            (s_mem_props.memoryTypes[i].propertyFlags & required_props) == required_props)
            return i;
    }
    return UINT32_MAX;
}

// ── Public API ────────────────────────────────────────────────────────────────

int paf_vulkan_init(void) {
    if (s_avail >= 0) return s_avail;
    s_avail = 0;

    s_lib = lib_open(VK_LIB);
    if (!s_lib) return 0;

    LOAD_VK(vkCreateInstance)
    LOAD_VK(vkEnumeratePhysicalDevices)
    LOAD_VK(vkGetPhysicalDeviceQueueFamilyProperties)
    LOAD_VK(vkGetPhysicalDeviceMemoryProperties)
    LOAD_VK(vkCreateDevice)
    LOAD_VK(vkGetDeviceQueue)

    // Create instance
    VkApplicationInfo ai = { VK_STRUCTURE_TYPE_APPLICATION_INFO, NULL, 1 };
    VkInstanceCreateInfo ici = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, NULL, 0,
                                  &ai, 0, NULL, 0, NULL };
    if (vk_vkCreateInstance(&ici, NULL, &s_instance) != VK_SUCCESS) goto fail;

    // Pick first physical device with a compute queue
    {
        uint32_t cnt = 0;
        vk_vkEnumeratePhysicalDevices(s_instance, &cnt, NULL);
        if (cnt == 0) goto fail;
        VkPhysicalDevice* devs = (VkPhysicalDevice*)malloc(cnt * sizeof(VkPhysicalDevice));
        if (!devs) goto fail;
        vk_vkEnumeratePhysicalDevices(s_instance, &cnt, devs);

        for (uint32_t d = 0; d < cnt; d++) {
            uint32_t qcnt = 0;
            vk_vkGetPhysicalDeviceQueueFamilyProperties(devs[d], &qcnt, NULL);
            VkQueueFamilyProperties* qprops =
                (VkQueueFamilyProperties*)malloc(qcnt * sizeof(VkQueueFamilyProperties));
            if (!qprops) { free(devs); goto fail; }
            vk_vkGetPhysicalDeviceQueueFamilyProperties(devs[d], &qcnt, qprops);
            for (uint32_t q = 0; q < qcnt; q++) {
                if (qprops[q].queueFlags & VK_QUEUE_COMPUTE_BIT) {
                    s_phys = devs[d];
                    s_queue_family = q;
                    break;
                }
            }
            free(qprops);
            if (s_phys != VK_NULL_HANDLE) break;
        }
        free(devs);
        if (s_phys == VK_NULL_HANDLE) goto fail;
    }

    vk_vkGetPhysicalDeviceMemoryProperties(s_phys, &s_mem_props);

    // Create logical device
    {
        float prio = 1.0f;
        VkDeviceQueueCreateInfo qi = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, NULL,
                                        0, s_queue_family, 1, &prio };
        VkDeviceCreateInfo di = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, NULL, 0,
                                   1, &qi, 0, NULL, 0, NULL, NULL };
        if (vk_vkCreateDevice(s_phys, &di, NULL, &s_device) != VK_SUCCESS) goto fail;
        vk_vkGetDeviceQueue(s_device, s_queue_family, 0, &s_queue);
    }

    // Load remaining device functions
    LOAD_VK(vkDestroyDevice)
    LOAD_VK(vkDestroyInstance)
    LOAD_VK(vkCreateShaderModule)
    LOAD_VK(vkDestroyShaderModule)
    LOAD_VK(vkCreateDescriptorSetLayout)
    LOAD_VK(vkDestroyDescriptorSetLayout)
    LOAD_VK(vkCreatePipelineLayout)
    LOAD_VK(vkDestroyPipelineLayout)
    LOAD_VK(vkCreateComputePipelines)
    LOAD_VK(vkDestroyPipeline)
    LOAD_VK(vkCreateDescriptorPool)
    LOAD_VK(vkDestroyDescriptorPool)
    LOAD_VK(vkAllocateDescriptorSets)
    LOAD_VK(vkUpdateDescriptorSets)
    LOAD_VK(vkCreateBuffer)
    LOAD_VK(vkDestroyBuffer)
    LOAD_VK(vkGetBufferMemoryRequirements)
    LOAD_VK(vkAllocateMemory)
    LOAD_VK(vkFreeMemory)
    LOAD_VK(vkBindBufferMemory)
    LOAD_VK(vkMapMemory)
    LOAD_VK(vkUnmapMemory)
    LOAD_VK(vkCreateCommandPool)
    LOAD_VK(vkDestroyCommandPool)
    LOAD_VK(vkAllocateCommandBuffers)
    LOAD_VK(vkFreeCommandBuffers)
    LOAD_VK(vkBeginCommandBuffer)
    LOAD_VK(vkEndCommandBuffer)
    LOAD_VK(vkCmdBindPipeline)
    LOAD_VK(vkCmdBindDescriptorSets)
    LOAD_VK(vkCmdPushConstants)
    LOAD_VK(vkCmdDispatch)
    LOAD_VK(vkQueueSubmit)
    LOAD_VK(vkQueueWaitIdle)
    LOAD_VK(vkCreateFence)
    LOAD_VK(vkDestroyFence)
    LOAD_VK(vkWaitForFences)
    LOAD_VK(vkResetFences)
    LOAD_VK(vkResetCommandPool)

    // Load paf_sha256.spv from the working directory or /usr/lib/paf/
    size_t spv_size = 0;
    uint32_t* spv = load_spv("paf_sha256.spv", &spv_size);
    if (!spv) spv = load_spv("/usr/lib/paf/paf_sha256.spv", &spv_size);
    if (!spv) goto fail;

    // Build shader module, descriptor set layout, pipeline layout, pipeline
    {
        VkShaderModule shader = VK_NULL_HANDLE;
        VkShaderModuleCreateInfo smci = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                           NULL, 0, spv_size, spv };
        VkResult r = vk_vkCreateShaderModule(s_device, &smci, NULL, &shader);
        free(spv);
        if (r != VK_SUCCESS) goto fail;

        // Descriptor set layout: 4 storage buffers (data, offsets, sizes, hashes)
        VkDescriptorSetLayoutBinding bindings[4];
        for (int i = 0; i < 4; i++) {
            bindings[i].binding           = (uint32_t)i;
            bindings[i].descriptorType    = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            bindings[i].descriptorCount   = 1;
            bindings[i].stageFlags        = VK_SHADER_STAGE_COMPUTE_BIT;
            bindings[i].pImmutableSamplers = NULL;
        }
        VkDescriptorSetLayoutCreateInfo dslci = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            NULL, 0, 4, bindings };
        vk_vkCreateDescriptorSetLayout(s_device, &dslci, NULL, &s_dsl);

        VkPushConstantRange pcr = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t) };
        VkPipelineLayoutCreateInfo plci = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            NULL, 0, 1, &s_dsl, 1, &pcr };
        vk_vkCreatePipelineLayout(s_device, &plci, NULL, &s_pipe_layout);

        VkPipelineShaderStageCreateInfo stage = {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            NULL, 0, VK_SHADER_STAGE_COMPUTE_BIT, shader, "main", NULL };
        VkComputePipelineCreateInfo cpci = {
            VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            NULL, 0, stage, s_pipe_layout, VK_NULL_HANDLE, 0 };
        vk_vkCreateComputePipelines(s_device, VK_NULL_HANDLE, 1, &cpci, NULL, &s_pipeline);
        vk_vkDestroyShaderModule(s_device, shader, NULL);

        if (!s_pipeline) goto fail;
    }

    // Descriptor pool (up to 4 sets, 4 bindings each)
    {
        VkDescriptorPoolSize ps = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4 };
        VkDescriptorPoolCreateInfo dpci = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, NULL, 0, 1, 1, &ps };
        vk_vkCreateDescriptorPool(s_device, &dpci, NULL, &s_desc_pool);
    }

    // Command pool
    {
        VkCommandPoolCreateInfo cpci = {
            VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, NULL, 0, s_queue_family };
        vk_vkCreateCommandPool(s_device, &cpci, NULL, &s_cmd_pool);
    }

    s_avail = 1;
    return 1;

fail:
    s_avail = 0;
    return 0;
}

int paf_vulkan_is_available(void) {
    if (s_avail < 0) paf_vulkan_init();
    return s_avail;
}

// ── Hash implementation ───────────────────────────────────────────────────────

int paf_vulkan_hash_flat(const uint8_t* host_buf, const uint64_t* offsets,
                          const uint64_t* sizes, uint32_t count,
                          uint8_t* out_hashes) {
    if (!paf_vulkan_is_available() || count == 0) return -1;

    LOAD_VK(vkCreateBuffer) LOAD_VK(vkDestroyBuffer)
    LOAD_VK(vkGetBufferMemoryRequirements) LOAD_VK(vkAllocateMemory) LOAD_VK(vkFreeMemory)
    LOAD_VK(vkBindBufferMemory) LOAD_VK(vkMapMemory) LOAD_VK(vkUnmapMemory)
    LOAD_VK(vkAllocateDescriptorSets) LOAD_VK(vkUpdateDescriptorSets)
    LOAD_VK(vkAllocateCommandBuffers) LOAD_VK(vkFreeCommandBuffers)
    LOAD_VK(vkBeginCommandBuffer) LOAD_VK(vkEndCommandBuffer)
    LOAD_VK(vkCmdBindPipeline) LOAD_VK(vkCmdBindDescriptorSets)
    LOAD_VK(vkCmdPushConstants) LOAD_VK(vkCmdDispatch)
    LOAD_VK(vkQueueSubmit) LOAD_VK(vkQueueWaitIdle)
    LOAD_VK(vkCreateFence) LOAD_VK(vkDestroyFence)
    LOAD_VK(vkWaitForFences) LOAD_VK(vkResetFences)
    LOAD_VK(vkResetCommandPool)

    // Calculate total data size
    uint64_t total_data = offsets[count - 1] + sizes[count - 1];
    VkDeviceSize hash_size   = (VkDeviceSize)count * 32;
    VkDeviceSize offset_size = (VkDeviceSize)count * sizeof(uint64_t);

    // Create host-visible buffers for data + offsets + sizes + hashes
    VkBuffer     bufs[4] = { VK_NULL_HANDLE, VK_NULL_HANDLE,
                              VK_NULL_HANDLE, VK_NULL_HANDLE };
    VkDeviceMemory mems[4] = { VK_NULL_HANDLE, VK_NULL_HANDLE,
                                VK_NULL_HANDLE, VK_NULL_HANDLE };
    VkDeviceSize byte_sizes[4] = {
        (VkDeviceSize)total_data,
        offset_size, offset_size, hash_size
    };
    VkFlags host_props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    for (int i = 0; i < 4; i++) {
        VkBufferCreateInfo bci = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0,
            byte_sizes[i],
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_SHARING_MODE_EXCLUSIVE, 0, NULL };
        if (vk_vkCreateBuffer(s_device, &bci, NULL, &bufs[i]) != VK_SUCCESS) goto cleanup;

        VkMemoryRequirements req;
        vk_vkGetBufferMemoryRequirements(s_device, bufs[i], &req);
        uint32_t mt = find_memory_type(req.memoryTypeBits, host_props);
        if (mt == UINT32_MAX) goto cleanup;

        VkMemoryAllocateInfo mai = {
            VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, req.size, mt };
        if (vk_vkAllocateMemory(s_device, &mai, NULL, &mems[i]) != VK_SUCCESS) goto cleanup;
        vk_vkBindBufferMemory(s_device, bufs[i], mems[i], 0);
    }

    // Upload input data
    { void* p; vk_vkMapMemory(s_device, mems[0], 0, VK_WHOLE_SIZE, 0, &p);
      memcpy(p, host_buf, (size_t)total_data); vk_vkUnmapMemory(s_device, mems[0]); }
    { void* p; vk_vkMapMemory(s_device, mems[1], 0, VK_WHOLE_SIZE, 0, &p);
      memcpy(p, offsets, (size_t)offset_size); vk_vkUnmapMemory(s_device, mems[1]); }
    { void* p; vk_vkMapMemory(s_device, mems[2], 0, VK_WHOLE_SIZE, 0, &p);
      memcpy(p, sizes, (size_t)offset_size); vk_vkUnmapMemory(s_device, mems[2]); }

    // Allocate descriptor set and bind buffers
    {
        VkDescriptorSet ds = VK_NULL_HANDLE;
        VkDescriptorSetAllocateInfo dsai = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, NULL,
            s_desc_pool, 1, &s_dsl };
        vk_vkAllocateDescriptorSets(s_device, &dsai, &ds);

        VkDescriptorBufferInfo bi[4];
        VkWriteDescriptorSet wr[4];
        for (int i = 0; i < 4; i++) {
            bi[i].buffer = bufs[i]; bi[i].offset = 0; bi[i].range = VK_WHOLE_SIZE;
            wr[i].sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            wr[i].pNext            = NULL;
            wr[i].dstSet           = ds;
            wr[i].dstBinding       = (uint32_t)i;
            wr[i].dstArrayElement  = 0;
            wr[i].descriptorCount  = 1;
            wr[i].descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            wr[i].pImageInfo       = NULL;
            wr[i].pBufferInfo      = &bi[i];
            wr[i].pTexelBufferView = NULL;
        }
        vk_vkUpdateDescriptorSets(s_device, 4, wr, 0, NULL);

        // Record and submit command buffer
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VkCommandBufferAllocateInfo cbai = {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, NULL,
            s_cmd_pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1 };
        vk_vkAllocateCommandBuffers(s_device, &cbai, &cmd);

        VkCommandBufferBeginInfo cbi = {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, NULL };
        vk_vkBeginCommandBuffer(cmd, &cbi);
        vk_vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, s_pipeline);
        vk_vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                    s_pipe_layout, 0, 1, &ds, 0, NULL);
        vk_vkCmdPushConstants(cmd, s_pipe_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                               0, sizeof(uint32_t), &count);
        uint32_t groups = (count + 255u) / 256u;
        vk_vkCmdDispatch(cmd, groups, 1, 1);
        vk_vkEndCommandBuffer(cmd);

        VkFenceCreateInfo fci = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, NULL, 0 };
        VkFence fence = VK_NULL_HANDLE;
        vk_vkCreateFence(s_device, &fci, NULL, &fence);

        VkSubmitInfo si = { VK_STRUCTURE_TYPE_SUBMIT_INFO, NULL, 0, NULL, NULL,
                             1, &cmd, 0, NULL };
        vk_vkQueueSubmit(s_queue, 1, &si, fence);
        vk_vkWaitForFences(s_device, 1, &fence, 1, UINT64_MAX);
        vk_vkDestroyFence(s_device, fence, NULL);
        vk_vkFreeCommandBuffers(s_device, s_cmd_pool, 1, &cmd);
        vk_vkResetCommandPool(s_device, s_cmd_pool, 0);
    }

    // Read back hashes
    { void* p; vk_vkMapMemory(s_device, mems[3], 0, VK_WHOLE_SIZE, 0, &p);
      memcpy(out_hashes, p, (size_t)hash_size); vk_vkUnmapMemory(s_device, mems[3]); }

cleanup:
    for (int i = 0; i < 4; i++) {
        if (bufs[i] != VK_NULL_HANDLE) vk_vkDestroyBuffer(s_device, bufs[i], NULL);
        if (mems[i] != VK_NULL_HANDLE) vk_vkFreeMemory(s_device, mems[i], NULL);
    }
    return s_avail ? 0 : -1;

fail:
    return -1;
}

void paf_vulkan_cleanup(void) {
    if (!s_lib) return;
    // Resources released on device/instance destroy; omit individual teardown
    // to keep the cleanup path simple. The OS reclaims GPU resources on process exit.
    lib_close(s_lib);
    s_lib   = NULL;
    s_avail = -1;
}
