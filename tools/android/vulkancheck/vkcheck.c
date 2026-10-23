/* What this phone's Vulkan driver offers, against what NativeSmash's graphics need: its Direct3D 11 renderer runs on DXVK,
 * which (doitsujin/dxvk, "Driver support") needs Vulkan 1.3 and VK_EXT_robustness2 with nullDescriptor and robustBufferAccess2;
 * Direct3D 11's stream output needs VK_EXT_transform_feedback. The report is a text, shown by MainActivity. */
#include <jni.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <vulkan/vulkan.h>

#define OUT(...) do { off += snprintf(buf + off, sizeof buf - off, __VA_ARGS__); if (off > (int)sizeof buf - 1) off = (int)sizeof buf - 1; } while (0)

static int has_ext(const VkExtensionProperties *e, uint32_t n, const char *name)
{
    for (uint32_t i = 0; i < n; i++) if (!strcmp(e[i].extensionName, name)) return 1;
    return 0;
}

static const char *yn(int v) { return v ? "yes" : "NO"; }

JNIEXPORT jstring JNICALL Java_com_nativesmash_vulkancheck_MainActivity_report(JNIEnv *env, jclass cls)
{
    static char buf[32768];
    int off = 0;
    (void)cls;
    OUT("NativeSmash - Vulkan check\n\n");
    VkApplicationInfo app = { VK_STRUCTURE_TYPE_APPLICATION_INFO, 0, "vkcheck", 1, "none", 1, VK_API_VERSION_1_1 };
    VkInstanceCreateInfo ici = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, 0, 0, &app };
    VkInstance inst;
    if (vkCreateInstance(&ici, 0, &inst) != VK_SUCCESS) {
        OUT("No Vulkan instance could be created: this phone cannot run the game's graphics.\n");
        return (*env)->NewStringUTF(env, buf);
    }
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(inst, &count, 0);
    if (!count) {
        OUT("No Vulkan device.\n");
        return (*env)->NewStringUTF(env, buf);
    }
    VkPhysicalDevice devs[8];
    if (count > 8) count = 8;
    vkEnumeratePhysicalDevices(inst, &count, devs);
    for (uint32_t d = 0; d < count; d++) {
        VkPhysicalDeviceProperties p;
        vkGetPhysicalDeviceProperties(devs[d], &p);
        uint32_t ne = 0;
        vkEnumerateDeviceExtensionProperties(devs[d], 0, &ne, 0);
        VkExtensionProperties *ext = calloc(ne ? ne : 1, sizeof *ext);
        vkEnumerateDeviceExtensionProperties(devs[d], 0, &ne, ext);

        VkPhysicalDeviceRobustness2FeaturesEXT rob2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT };
        VkPhysicalDeviceTransformFeedbackFeaturesEXT tf = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT };
        VkPhysicalDeviceCustomBorderColorFeaturesEXT cbc = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT };
        int e_rob2 = has_ext(ext, ne, "VK_EXT_robustness2"), e_tf = has_ext(ext, ne, "VK_EXT_transform_feedback");
        int e_cbc = has_ext(ext, ne, "VK_EXT_custom_border_color");
        int api13 = p.apiVersion >= VK_API_VERSION_1_3;
        VkPhysicalDeviceFeatures2 f2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
        void *chain = 0;
        if (e_rob2) { rob2.pNext = chain; chain = &rob2; }
        if (e_tf) { tf.pNext = chain; chain = &tf; }
        if (e_cbc) { cbc.pNext = chain; chain = &cbc; }
        f2.pNext = chain;
        PFN_vkGetPhysicalDeviceFeatures2 get2 = (PFN_vkGetPhysicalDeviceFeatures2)vkGetInstanceProcAddr(inst, "vkGetPhysicalDeviceFeatures2");
        if (get2) get2(devs[d], &f2); else vkGetPhysicalDeviceFeatures(devs[d], &f2.features);
        const VkPhysicalDeviceFeatures *f = &f2.features;

        OUT("Device %u: %s\n", d, p.deviceName);
        OUT("Vulkan %u.%u.%u   driver 0x%x   vendor 0x%x\n", VK_VERSION_MAJOR(p.apiVersion), VK_VERSION_MINOR(p.apiVersion),
            VK_VERSION_PATCH(p.apiVersion), p.driverVersion, p.vendorID);
        OUT("%u device extensions\n\n", ne);
        OUT("What DXVK needs (its Driver support page):\n");
        OUT("  Vulkan 1.3 ........................ %s\n", yn(api13));
        OUT("  VK_EXT_robustness2 ................ %s\n", yn(e_rob2));
        OUT("    nullDescriptor .................. %s\n", yn(e_rob2 && rob2.nullDescriptor));
        OUT("    robustBufferAccess2 ............. %s\n", yn(e_rob2 && rob2.robustBufferAccess2));
        OUT("    robustImageAccess2 .............. %s\n", yn(e_rob2 && rob2.robustImageAccess2));
        OUT("  VK_EXT_transform_feedback ......... %s (Direct3D 11 stream output)\n", yn(e_tf && tf.transformFeedback));
        OUT("  VK_EXT_custom_border_color ........ %s\n", yn(e_cbc && cbc.customBorderColors));
        OUT("  geometryShader / tessellation ..... %s / %s\n", yn(f->geometryShader), yn(f->tessellationShader));
        OUT("  depthClamp, depthBiasClamp ........ %s, %s\n", yn(f->depthClamp), yn(f->depthBiasClamp));
        OUT("  shaderClipDistance / CullDistance . %s / %s\n", yn(f->shaderClipDistance), yn(f->shaderCullDistance));
        OUT("  fillModeNonSolid, wideLines ....... %s, %s\n", yn(f->fillModeNonSolid), yn(f->wideLines));
        OUT("  multiDrawIndirect ................. %s\n", yn(f->multiDrawIndirect));
        OUT("  fragmentStoresAndAtomics .......... %s\n", yn(f->fragmentStoresAndAtomics));
        OUT("  imageCubeArray, independentBlend .. %s, %s\n", yn(f->imageCubeArray), yn(f->independentBlend));
        OUT("  maxPushConstantsSize (>= 256) ..... %u\n", p.limits.maxPushConstantsSize);
        int must = api13 && e_rob2 && rob2.nullDescriptor && rob2.robustBufferAccess2;
        int tfok = e_tf && tf.transformFeedback;
        OUT("\nVerdict: ");
        if (must && tfok) OUT("this GPU has what DXVK asks for. The graphics route through DXVK looks possible.\n");
        else if (must) OUT("DXVK's hard requirements are met, but not transform feedback: Direct3D 11 stream output would need a workaround.\n");
        else OUT("this GPU lacks something DXVK needs (the lines marked NO above). Running the game here through DXVK would need workarounds, or a Vulkan renderer of NativeSmash's own.\n");
        OUT("\nAll extensions:\n");
        for (uint32_t i = 0; i < ne; i++) OUT("  %s\n", ext[i].extensionName);
        OUT("\n");
        free(ext);
    }
    vkDestroyInstance(inst, 0);
    return (*env)->NewStringUTF(env, buf);
}
