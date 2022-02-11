#ifndef __CPU_H
#define __CPU_H

// Detected CPU capabilities - used as input to the GetCPUCaps() function
enum CPUCAPS{
    // Synthesized values
   CPU_MFG,        // Manufacturer (returns enum CPU_MFGS)
   CPU_TYPE,       // CPU type (return enum CPU_TYPES)

    // Processor Features - returned as boolean values
   HAS_CPUID,      // Supports CPUID instruction
   HAS_FPU,        // FPU present
   HAS_VME,        // Virtual Mode Extensions
   HAS_DEBUG,      // Debug extensions
   HAS_PSE,        // Page Size Extensions
   HAS_TSC,        // Time Stamp Counter
   HAS_MSR,        // Model Specific Registers
   HAS_MCE,        // Machine Check Extensions
   HAS_CMPXCHG8,   // CMPXCHG8 instruction
   HAS_MMX,        // MMX support
   HAS_3DNOW,      // 3DNow! support
   HAS_SSE_MMX,    // SSE MMX support
   HAS_SSE_FP,     // SSE FP support
   HAS_SIMD,       // SIMD support
};

//----------------------------
// Detected CPU Manufacturers - returned by GetCPUCaps (CPU_MFG);
enum CPU_MFGS{
   MFG_UNKNOWN,
   MFG_AMD,
   MFG_INTEL,
   MFG_CYRIX,
   MFG_CENTAUR
};

//----------------------------

// Detected CPU models - returned by GetCPUCaps (CPU_TYPE);
enum CPU_TYPES{
    UNKNOWN,
    AMD_Am486,
    AMD_K5,
    AMD_K6_MMX,
    AMD_K6_2,
    AMD_K6_3,
    AMD_K7,

    INTEL_486DX,
    INTEL_486SX,
    INTEL_486DX2,
    INTEL_486SL,
    INTEL_486SX2,
    INTEL_486DX2E,
    INTEL_486DX4,
    INTEL_Pentium,
    INTEL_Pentium_MMX,
    INTEL_Pentium_Pro,
    INTEL_Pentium_II,
    INTEL_Celeron,
    INTEL_Pentium_III,
};

/******************************************************************************
 Routine:   GetCPUCaps
 Input:     Which capability to query (see enum CPUCAPS for an exhaustive list)
 Returns:   Depends on input, either a boolean or an enumeration value.
            CPU_TYPE - enum CPU_TYPES
            CPU_MFG  - enum CPU_MFGS
 Comment:   This function returns information about the capabilies of the
            CPU on which it is called.  The input enumeration covers both
            processor feature bits (the HAS_* values) and "synthesized"
            information.
            
            THE HAS_* QUERIES SHOULD ALWAYS BE USED IN PREFERENCE TO DIRECTLY 
            CHECKING THE CPU TYPE WHEN LOOKING FOR FEATURES.  For instance,
            it is *always* better to check for HAS_3DNOW directly, rather
            than rely on checking for a K6_2, K6_3, or K7.  Likewise,
            HAS_MMX should always be used in preference to other methods
            of checking for MMX instructions.

            The features bits are checked against either the base feature
            bits (CPUID function 1, edx) or the extended feature bits
            (CPUID function 0x80000001, edx), as appropriate.  The return
            value is 1 for feature present or 0 for feature not present,

            The synthesized information is created by interpreting the CPUID
            results in some way.

            The CPUCAPS currently implemented are not exhaustive by any means,
            but they cover the basic features and 3DNow! extensions, which
            are the majority of the used bits.  In future revisions, more
            feature bits and synthesized values will be added.

            Note that this routine caches the feature bits when first called,
            so checking multiple features is relatively efficient after the
            first invocation.  However, tt is not recommended practice to
            use GetCPUCaps() inside time-critical code.

******************************************************************************/
dword GetCPUCaps(CPUCAPS);

//----------------------------

#endif