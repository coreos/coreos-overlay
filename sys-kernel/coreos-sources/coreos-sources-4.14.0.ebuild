# Copyright 2014 CoreOS, Inc.
# Distributed under the terms of the GNU General Public License v2

EAPI="5"
ETYPE="sources"

# -rc releases should be versioned L.M_rcN
# Final releases should be versioned L.M.N, even for N == 0

# Only needed for RCs
K_BASE_VER="4.14"

inherit kernel-2
detect_version

DESCRIPTION="Full sources for the CoreOS Linux kernel"
HOMEPAGE="http://www.kernel.org"
if [[ "${PV%%_rc*}" != "${PV}" ]]; then
	SRC_URI="https://git.kernel.org/torvalds/p/v${KV%-coreos}/v${OKV} -> patch-${KV%-coreos}.patch ${KERNEL_BASE_URI}/linux-${OKV}.tar.xz"
	PATCH_DIR="${FILESDIR}/${KV_MAJOR}.${KV_PATCH}"
else
	SRC_URI="${KERNEL_URI}"
	PATCH_DIR="${FILESDIR}/${KV_MAJOR}.${KV_MINOR}"
fi

KEYWORDS="amd64 arm64"
IUSE=""

# For 4.14 >= 4.14.10
src_install() {
	kernel-2_src_install
	local r="${PR#r0}"
	local script="${ED}/usr/src/linux-${PV/_rc/-rc}-coreos${r:+-${r}}/tools/objtool/sync-check.sh"
	if [[ -e "${script}" ]]; then
		chmod +x "${script}" || die
	fi
}

# XXX: Note we must prefix the patch filenames with "z" to ensure they are
# applied _after_ a potential patch-${KV}.patch file, present when building a
# patchlevel revision.  We mustn't apply our patches first, it fails when the
# local patches overlap with the upstream patch.
UNIPATCH_LIST="
	${PATCH_DIR}/z0001-mm-x86-mm-Fix-performance-regression-in-get_user_pag.patch \
	${PATCH_DIR}/z0002-x86-asm-Remove-unnecessary-n-t-in-front-of-CC_SET-fr.patch \
	${PATCH_DIR}/z0003-objtool-Don-t-report-end-of-section-error-after-an-e.patch \
	${PATCH_DIR}/z0004-x86-head-Remove-confusing-comment.patch \
	${PATCH_DIR}/z0005-x86-head-Remove-unused-bad_address-code.patch \
	${PATCH_DIR}/z0006-x86-head-Fix-head-ELF-function-annotations.patch \
	${PATCH_DIR}/z0007-x86-boot-Annotate-verify_cpu-as-a-callable-function.patch \
	${PATCH_DIR}/z0008-x86-xen-Fix-xen-head-ELF-annotations.patch \
	${PATCH_DIR}/z0009-x86-xen-Add-unwind-hint-annotations.patch \
	${PATCH_DIR}/z0010-x86-head-Add-unwind-hint-annotations.patch \
	${PATCH_DIR}/z0011-ACPI-APEI-adjust-a-local-variable-type-in-ghes_iorem.patch \
	${PATCH_DIR}/z0012-x86-unwinder-Make-CONFIG_UNWINDER_ORC-y-the-default-.patch \
	${PATCH_DIR}/z0013-x86-fpu-debug-Remove-unused-x86_fpu_state-and-x86_fp.patch \
	${PATCH_DIR}/z0014-x86-unwind-Rename-unwinder-config-options-to-CONFIG_.patch \
	${PATCH_DIR}/z0015-x86-unwind-Make-CONFIG_UNWINDER_ORC-y-the-default-in.patch \
	${PATCH_DIR}/z0016-bitops-Add-clear-set_bit32-to-linux-bitops.h.patch \
	${PATCH_DIR}/z0017-x86-cpuid-Add-generic-table-for-CPUID-dependencies.patch \
	${PATCH_DIR}/z0018-x86-fpu-Parse-clearcpuid-as-early-XSAVE-argument.patch \
	${PATCH_DIR}/z0019-x86-fpu-Make-XSAVE-check-the-base-CPUID-features-bef.patch \
	${PATCH_DIR}/z0020-x86-fpu-Remove-the-explicit-clearing-of-XSAVE-depend.patch \
	${PATCH_DIR}/z0021-x86-platform-UV-Convert-timers-to-use-timer_setup.patch \
	${PATCH_DIR}/z0022-objtool-Print-top-level-commands-on-incorrect-usage.patch \
	${PATCH_DIR}/z0023-x86-cpuid-Prevent-out-of-bound-access-in-do_clear_cp.patch \
	${PATCH_DIR}/z0024-x86-entry-Use-SYSCALL_DEFINE-macros-for-sys_modify_l.patch \
	${PATCH_DIR}/z0025-mm-sparsemem-Allocate-mem_section-at-runtime-for-CON.patch \
	${PATCH_DIR}/z0026-x86-kasan-Use-the-same-shadow-offset-for-4-and-5-lev.patch \
	${PATCH_DIR}/z0027-x86-xen-Provide-pre-built-page-tables-only-for-CONFI.patch \
	${PATCH_DIR}/z0028-x86-xen-Drop-5-level-paging-support-code-from-the-XE.patch \
	${PATCH_DIR}/z0029-ACPI-APEI-remove-the-unused-dead-code-for-SEA-NMI-no.patch \
	${PATCH_DIR}/z0030-x86-asm-Don-t-use-the-confusing-.ifeq-directive.patch \
	${PATCH_DIR}/z0031-x86-build-Beautify-build-log-of-syscall-headers.patch \
	${PATCH_DIR}/z0032-x86-mm-64-Rename-the-register_page_bootmem_memmap-si.patch \
	${PATCH_DIR}/z0033-x86-cpufeatures-Enable-new-SSE-AVX-AVX512-CPU-featur.patch \
	${PATCH_DIR}/z0034-x86-mm-Relocate-page-fault-error-codes-to-traps.h.patch \
	${PATCH_DIR}/z0035-x86-boot-Relocate-definition-of-the-initial-state-of.patch \
	${PATCH_DIR}/z0036-ptrace-x86-Make-user_64bit_mode-available-to-32-bit-.patch \
	${PATCH_DIR}/z0037-x86-entry-64-Remove-the-restore_c_regs_and_iret-labe.patch \
	${PATCH_DIR}/z0038-x86-entry-64-Split-the-IRET-to-user-and-IRET-to-kern.patch \
	${PATCH_DIR}/z0039-x86-entry-64-Move-SWAPGS-into-the-common-IRET-to-use.patch \
	${PATCH_DIR}/z0040-x86-entry-64-Simplify-reg-restore-code-in-the-standa.patch \
	${PATCH_DIR}/z0041-x86-entry-64-Shrink-paranoid_exit_restore-and-make-l.patch \
	${PATCH_DIR}/z0042-x86-entry-64-Use-pop-instead-of-movq-in-syscall_retu.patch \
	${PATCH_DIR}/z0043-x86-entry-64-Merge-the-fast-and-slow-SYSRET-paths.patch \
	${PATCH_DIR}/z0044-x86-entry-64-Use-POP-instead-of-MOV-to-restore-regs-.patch \
	${PATCH_DIR}/z0045-x86-entry-64-Remove-the-RESTORE_._REGS-infrastructur.patch \
	${PATCH_DIR}/z0046-xen-x86-entry-64-Add-xen-NMI-trap-entry.patch \
	${PATCH_DIR}/z0047-x86-entry-64-De-Xen-ify-our-NMI-code.patch \
	${PATCH_DIR}/z0048-x86-entry-32-Pull-the-MSR_IA32_SYSENTER_CS-update-co.patch \
	${PATCH_DIR}/z0049-x86-entry-64-Pass-SP0-directly-to-load_sp0.patch \
	${PATCH_DIR}/z0050-x86-entry-Add-task_top_of_stack-to-find-the-top-of-a.patch \
	${PATCH_DIR}/z0051-x86-xen-64-x86-entry-64-Clean-up-SP-code-in-cpu_init.patch \
	${PATCH_DIR}/z0052-x86-entry-64-Stop-initializing-TSS.sp0-at-boot.patch \
	${PATCH_DIR}/z0053-x86-entry-64-Remove-all-remaining-direct-thread_stru.patch \
	${PATCH_DIR}/z0054-x86-entry-32-Fix-cpu_current_top_of_stack-initializa.patch \
	${PATCH_DIR}/z0055-x86-entry-64-Remove-thread_struct-sp0.patch \
	${PATCH_DIR}/z0056-x86-traps-Use-a-new-on_thread_stack-helper-to-clean-.patch \
	${PATCH_DIR}/z0057-x86-entry-64-Shorten-TEST-instructions.patch \
	${PATCH_DIR}/z0058-x86-cpuid-Replace-set-clear_bit32.patch \
	${PATCH_DIR}/z0059-bitops-Revert-cbe96375025e-bitops-Add-clear-set_bit3.patch \
	${PATCH_DIR}/z0060-x86-mm-Define-_PAGE_TABLE-using-_KERNPG_TABLE.patch \
	${PATCH_DIR}/z0061-x86-cpufeatures-Re-tabulate-the-X86_FEATURE-definiti.patch \
	${PATCH_DIR}/z0062-x86-cpufeatures-Fix-various-details-in-the-feature-d.patch \
	${PATCH_DIR}/z0063-selftests-x86-protection_keys-Fix-syscall-NR-redefin.patch \
	${PATCH_DIR}/z0064-selftests-x86-ldt_gdt-Robustify-against-set_thread_a.patch \
	${PATCH_DIR}/z0065-selftests-x86-ldt_gdt-Add-infrastructure-to-test-set.patch \
	${PATCH_DIR}/z0066-selftests-x86-ldt_gdt-Run-most-existing-LDT-test-cas.patch \
	${PATCH_DIR}/z0067-selftests-x86-ldt_get-Add-a-few-additional-tests-for.patch \
	${PATCH_DIR}/z0068-ACPI-APEI-Replace-ioremap_page_range-with-fixmap.patch \
	${PATCH_DIR}/z0069-x86-virt-x86-platform-Merge-struct-x86_hyper-into-st.patch \
	${PATCH_DIR}/z0070-x86-virt-Add-enum-for-hypervisors-to-replace-x86_hyp.patch \
	${PATCH_DIR}/z0071-kbuild-derive-relative-path-for-KBUILD_SRC-from-CURD.patch \
	${PATCH_DIR}/z0072-Add-arm64-coreos-verity-hash.patch \
	${PATCH_DIR}/z0073-drivers-misc-intel-pti-Rename-the-header-file-to-fre.patch \
	${PATCH_DIR}/z0074-x86-cpufeature-Add-User-Mode-Instruction-Prevention-.patch \
	${PATCH_DIR}/z0075-x86-Make-X86_BUG_FXSAVE_LEAK-detectable-in-CPUID-on-.patch \
	${PATCH_DIR}/z0076-perf-x86-Enable-free-running-PEBS-for-REGS_USER-INTR.patch \
	${PATCH_DIR}/z0077-bpf-fix-build-issues-on-um-due-to-mising-bpf_perf_ev.patch \
	${PATCH_DIR}/z0078-locking-barriers-Add-implicit-smp_read_barrier_depen.patch \
	${PATCH_DIR}/z0079-locking-barriers-Convert-users-of-lockless_dereferen.patch \
	${PATCH_DIR}/z0080-x86-mm-kasan-Don-t-use-vmemmap_populate-to-initializ.patch \
	${PATCH_DIR}/z0081-x86-entry-64-paravirt-Use-paravirt-safe-macro-to-acc.patch \
	${PATCH_DIR}/z0082-x86-unwinder-orc-Dont-bail-on-stack-overflow.patch \
	${PATCH_DIR}/z0083-x86-unwinder-Handle-stack-overflows-more-gracefully.patch \
	${PATCH_DIR}/z0084-x86-irq-Remove-an-old-outdated-comment-about-context.patch \
	${PATCH_DIR}/z0085-x86-irq-64-Print-the-offending-IP-in-the-stack-overf.patch \
	${PATCH_DIR}/z0086-x86-entry-64-Allocate-and-enable-the-SYSENTER-stack.patch \
	${PATCH_DIR}/z0087-x86-dumpstack-Add-get_stack_info-support-for-the-SYS.patch \
	${PATCH_DIR}/z0088-x86-entry-gdt-Put-per-CPU-GDT-remaps-in-ascending-or.patch \
	${PATCH_DIR}/z0089-x86-mm-fixmap-Generalize-the-GDT-fixmap-mechanism-in.patch \
	${PATCH_DIR}/z0090-x86-kasan-64-Teach-KASAN-about-the-cpu_entry_area.patch \
	${PATCH_DIR}/z0091-x86-entry-Fix-assumptions-that-the-HW-TSS-is-at-the-.patch \
	${PATCH_DIR}/z0092-x86-dumpstack-Handle-stack-overflow-on-all-stacks.patch \
	${PATCH_DIR}/z0093-x86-entry-Move-SYSENTER_stack-to-the-beginning-of-st.patch \
	${PATCH_DIR}/z0094-x86-entry-Remap-the-TSS-into-the-CPU-entry-area.patch \
	${PATCH_DIR}/z0095-x86-entry-64-Separate-cpu_current_top_of_stack-from-.patch \
	${PATCH_DIR}/z0096-x86-espfix-64-Stop-assuming-that-pt_regs-is-on-the-e.patch \
	${PATCH_DIR}/z0097-x86-entry-64-Use-a-per-CPU-trampoline-stack-for-IDT-.patch \
	${PATCH_DIR}/z0098-x86-entry-64-Return-to-userspace-from-the-trampoline.patch \
	${PATCH_DIR}/z0099-x86-entry-64-Create-a-per-CPU-SYSCALL-entry-trampoli.patch \
	${PATCH_DIR}/z0100-x86-entry-64-Move-the-IST-stacks-into-struct-cpu_ent.patch \
	${PATCH_DIR}/z0101-x86-entry-64-Remove-the-SYSENTER-stack-canary.patch \
	${PATCH_DIR}/z0102-x86-entry-Clean-up-the-SYSENTER_stack-code.patch \
	${PATCH_DIR}/z0103-x86-entry-64-Make-cpu_entry_area.tss-read-only.patch \
	${PATCH_DIR}/z0104-x86-paravirt-Dont-patch-flush_tlb_single.patch \
	${PATCH_DIR}/z0105-x86-paravirt-Provide-a-way-to-check-for-hypervisors.patch \
	${PATCH_DIR}/z0106-x86-cpufeatures-Make-CPU-bugs-sticky.patch \
	${PATCH_DIR}/z0107-x86-Kconfig-Limit-NR_CPUS-on-32-bit-to-a-sane-amount.patch \
	${PATCH_DIR}/z0108-x86-mm-dump_pagetables-Check-PAGE_PRESENT-for-real.patch \
	${PATCH_DIR}/z0109-x86-mm-dump_pagetables-Make-the-address-hints-correc.patch \
	${PATCH_DIR}/z0110-x86-vsyscall-64-Explicitly-set-_PAGE_USER-in-the-pag.patch \
	${PATCH_DIR}/z0111-x86-vsyscall-64-Warn-and-fail-vsyscall-emulation-in-.patch \
	${PATCH_DIR}/z0112-arch-mm-Allow-arch_dup_mmap-to-fail.patch \
	${PATCH_DIR}/z0113-x86-ldt-Rework-locking.patch \
	${PATCH_DIR}/z0114-x86-ldt-Prevent-LDT-inheritance-on-exec.patch \
	${PATCH_DIR}/z0115-x86-mm-64-Improve-the-memory-map-documentation.patch \
	${PATCH_DIR}/z0116-x86-doc-Remove-obvious-weirdnesses-from-the-x86-MM-l.patch \
	${PATCH_DIR}/z0117-x86-entry-Rename-SYSENTER_stack-to-CPU_ENTRY_AREA_en.patch \
	${PATCH_DIR}/z0118-x86-uv-Use-the-right-TLB-flush-API.patch \
	${PATCH_DIR}/z0119-x86-microcode-Dont-abuse-the-TLB-flush-interface.patch \
	${PATCH_DIR}/z0120-x86-mm-Use-__flush_tlb_one-for-kernel-memory.patch \
	${PATCH_DIR}/z0121-x86-mm-Remove-superfluous-barriers.patch \
	${PATCH_DIR}/z0122-x86-mm-Add-comments-to-clarify-which-TLB-flush-funct.patch \
	${PATCH_DIR}/z0123-x86-mm-Move-the-CR3-construction-functions-to-tlbflu.patch \
	${PATCH_DIR}/z0124-x86-mm-Remove-hard-coded-ASID-limit-checks.patch \
	${PATCH_DIR}/z0125-x86-mm-Put-MMU-to-hardware-ASID-translation-in-one-p.patch \
	${PATCH_DIR}/z0126-x86-mm-Create-asm-invpcid.h.patch \
	${PATCH_DIR}/z0127-x86-cpu_entry_area-Move-it-to-a-separate-unit.patch \
	${PATCH_DIR}/z0128-x86-cpu_entry_area-Move-it-out-of-the-fixmap.patch \
	${PATCH_DIR}/z0129-init-Invoke-init_espfix_bsp-from-mm_init.patch \
	${PATCH_DIR}/z0130-x86-cpu_entry_area-Prevent-wraparound-in-setup_cpu_e.patch \
	${PATCH_DIR}/z0131-x86-cpufeatures-Add-X86_BUG_CPU_INSECURE.patch \
	${PATCH_DIR}/z0132-x86-mm-pti-Disable-global-pages-if-PAGE_TABLE_ISOLAT.patch \
	${PATCH_DIR}/z0133-x86-mm-pti-Prepare-the-x86-entry-assembly-code-for-e.patch \
	${PATCH_DIR}/z0134-x86-mm-pti-Add-infrastructure-for-page-table-isolati.patch \
	${PATCH_DIR}/z0135-x86-pti-Add-the-pti-cmdline-option-and-documentation.patch \
	${PATCH_DIR}/z0136-x86-mm-pti-Add-mapping-helper-functions.patch \
	${PATCH_DIR}/z0137-x86-mm-pti-Allow-NX-poison-to-be-set-in-p4d-pgd.patch \
	${PATCH_DIR}/z0138-x86-mm-pti-Allocate-a-separate-user-PGD.patch \
	${PATCH_DIR}/z0139-x86-mm-pti-Populate-user-PGD.patch \
	${PATCH_DIR}/z0140-x86-mm-pti-Add-functions-to-clone-kernel-PMDs.patch \
	${PATCH_DIR}/z0141-x86-mm-pti-Force-entry-through-trampoline-when-PTI-a.patch \
	${PATCH_DIR}/z0142-x86-mm-pti-Share-cpu_entry_area-with-user-space-page.patch \
	${PATCH_DIR}/z0143-x86-entry-Align-entry-text-section-to-PMD-boundary.patch \
	${PATCH_DIR}/z0144-x86-mm-pti-Share-entry-text-PMD.patch \
	${PATCH_DIR}/z0145-x86-mm-pti-Map-ESPFIX-into-user-space.patch \
	${PATCH_DIR}/z0146-x86-cpu_entry_area-Add-debugstore-entries-to-cpu_ent.patch \
	${PATCH_DIR}/z0147-x86-events-intel-ds-Map-debug-buffers-in-cpu_entry_a.patch \
	${PATCH_DIR}/z0148-x86-mm-64-Make-a-full-PGD-entry-size-hole-in-the-mem.patch \
	${PATCH_DIR}/z0149-x86-pti-Put-the-LDT-in-its-own-PGD-if-PTI-is-on.patch \
	${PATCH_DIR}/z0150-x86-pti-Map-the-vsyscall-page-if-needed.patch \
	${PATCH_DIR}/z0151-x86-mm-Allow-flushing-for-future-ASID-switches.patch \
	${PATCH_DIR}/z0152-x86-mm-Abstract-switching-CR3.patch \
	${PATCH_DIR}/z0153-x86-mm-Use-Fix-PCID-to-optimize-user-kernel-switches.patch \
	${PATCH_DIR}/z0154-x86-mm-Optimize-RESTORE_CR3.patch \
	${PATCH_DIR}/z0155-x86-mm-Use-INVPCID-for-__native_flush_tlb_single.patch \
	${PATCH_DIR}/z0156-x86-mm-Clarify-the-whole-ASID-kernel-PCID-user-PCID-.patch \
	${PATCH_DIR}/z0157-x86-dumpstack-Indicate-in-Oops-whether-PTI-is-config.patch \
	${PATCH_DIR}/z0158-x86-mm-pti-Add-Kconfig.patch \
	${PATCH_DIR}/z0159-x86-mm-dump_pagetables-Add-page-table-directory-to-t.patch \
	${PATCH_DIR}/z0160-x86-mm-dump_pagetables-Check-user-space-page-table-f.patch \
	${PATCH_DIR}/z0161-x86-mm-dump_pagetables-Allow-dumping-current-pagetab.patch \
	${PATCH_DIR}/z0162-x86-ldt-Make-the-LDT-mapping-RO.patch \
	${PATCH_DIR}/z0163-x86-smpboot-Remove-stale-TLB-flush-invocations.patch \
	${PATCH_DIR}/z0164-x86-mm-Remove-preempt_disable-enable-from-__native_f.patch \
	${PATCH_DIR}/z0165-x86-ldt-Plug-memory-leak-in-error-path.patch \
	${PATCH_DIR}/z0166-x86-ldt-Make-LDT-pgtable-free-conditional.patch \
	${PATCH_DIR}/z0167-x86-pti-Enable-PTI-by-default.patch \
	${PATCH_DIR}/z0168-x86-cpu-x86-pti-Do-not-enable-PTI-on-AMD-processors.patch \
	${PATCH_DIR}/z0169-x86-pti-Make-sure-the-user-kernel-PTEs-match.patch \
	${PATCH_DIR}/z0170-x86-dumpstack-Fix-partial-register-dumps.patch \
	${PATCH_DIR}/z0171-x86-dumpstack-Print-registers-for-first-stack-frame.patch \
"
