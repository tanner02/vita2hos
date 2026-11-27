# Commercial libc startup readiness

Audit source baseline: 676e06b (2026-11-27). Owned dependency module startup,
main TLS, function imports and writable variable aliases passed physically on
runtime 006d5c4. That evidence does not establish commercial libc compatibility.

The user-owned PCSF00516 libc has 45 imports: 31 function imports available in
runtime source, 11 provided by bundled module exports, and one process variable
provided conditionally by the loader. Two function imports remain unresolved:

| Library | NID | Symbol |
| --- | --- | --- |
| SceLibKernel | 0x15e2a45d | sceKernelCallModuleExit |
| SceProcessmgr | 0xeb6e50bb | sceKernelCallAbortHandler |

The variable (library 0x5ad9c136, NID 0xdf084dfa) is `__sce_libcparam`.
`load_process_parameters_register` publishes the main executable's guest libc
parameter address after validating its size and mapped range. It is not a
missing HLE variable. An audit that only scans EXPORT macros misses this case.
The pinned Vita3K kernel source independently identifies the variable NID.
Existing host load_exports tests exercise publication and guest address identity.

Static inspection used the production relocation code on a private ELF copy,
then Capstone scans of relocated code. No ARM instructions were executed. The
scan found one direct call candidate for CallModuleExit with argument 1 and
eight direct call candidates for CallAbortHandler, with observed immediate
reason values in 0x102 through 0x109 and a second argument of zero. These are
candidate call-site observations, not complete control-flow or API contracts.
The pinned Vita3K implementations leave both APIs unimplemented. Returning
success blindly would hide unimplemented termination behavior.

Before a commercial libc startup test:

- Establish both helper contracts and implement appropriate termination/module
  lifecycle behavior, including failure and reentrant paths.
- Verify the executable's actual libc process parameters and the selected
  dependency set, rather than using import counts alone.
- Bind actual dependencies and use only reviewed, source-bound TP patch plans.
- Collect console startup and exit reports; a host binding replay cannot prove
  ARM execution or successful commercial initialization.

The current source availability audit still finds 388 missing main imports,
23 FIOS imports and 14 Ult imports. Availability is not semantic compatibility.
No proprietary code, disassembly, assets or keys are stored in this document.
