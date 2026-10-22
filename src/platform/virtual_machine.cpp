#include "virtual_machine.h"
#include <cstring>
#include <intrin.h>

namespace Platform {

namespace {

enum CpuidRegister {
    REGISTER_EAX,
    REGISTER_EBX,
    REGISTER_ECX,
    REGISTER_EDX,
    CPUID_REGISTER_COUNT
};

constexpr int CPU_FEATURES_LEAF = 1;
constexpr unsigned HYPERVISOR_PRESENT_BIT = 1u << 31;

constexpr int HYPERVISOR_VENDOR_LEAF = 0x40000000;
constexpr int HYPERV_PRIVILEGES_LEAF = 0x40000003;
constexpr unsigned CREATE_PARTITIONS_PRIVILEGE = 1u << 0;
constexpr char HYPERV_VENDOR[] = "Microsoft Hv";
constexpr size_t VENDOR_REGISTER_BYTES = 4;
constexpr size_t VENDOR_LENGTH = 3 * VENDOR_REGISTER_BYTES;

struct CpuidResult {
    unsigned registers[CPUID_REGISTER_COUNT];
};

CpuidResult Cpuid(int leaf) {
    int registers[CPUID_REGISTER_COUNT];
    __cpuid(registers, leaf);
    CpuidResult result;
    for (int i = 0; i < CPUID_REGISTER_COUNT; i++)
        result.registers[i] = static_cast<unsigned>(registers[i]);
    return result;
}

bool HypervisorBitSet() {
    return (Cpuid(CPU_FEATURES_LEAF).registers[REGISTER_ECX] & HYPERVISOR_PRESENT_BIT) != 0;
}

// Windows itself runs on Hyper-V when Memory Integrity, WSL2 or Hyper-V is on
bool IsHyperVHost() {
    const CpuidResult vendorLeaf = Cpuid(HYPERVISOR_VENDOR_LEAF);
    if (vendorLeaf.registers[REGISTER_EAX] < static_cast<unsigned>(HYPERV_PRIVILEGES_LEAF))
        return false;

    char vendor[VENDOR_LENGTH];
    std::memcpy(vendor, &vendorLeaf.registers[REGISTER_EBX], VENDOR_REGISTER_BYTES);
    std::memcpy(vendor + VENDOR_REGISTER_BYTES, &vendorLeaf.registers[REGISTER_ECX],
                VENDOR_REGISTER_BYTES);
    std::memcpy(vendor + 2 * VENDOR_REGISTER_BYTES, &vendorLeaf.registers[REGISTER_EDX],
                VENDOR_REGISTER_BYTES);
    if (std::memcmp(vendor, HYPERV_VENDOR, VENDOR_LENGTH) != 0)
        return false;

    return (Cpuid(HYPERV_PRIVILEGES_LEAF).registers[REGISTER_EBX] &
            CREATE_PARTITIONS_PRIVILEGE) != 0;
}

}

bool IsRunningInVirtualMachine() {
    static const bool runningInVirtualMachine = HypervisorBitSet() && !IsHyperVHost();
    return runningInVirtualMachine;
}

}
