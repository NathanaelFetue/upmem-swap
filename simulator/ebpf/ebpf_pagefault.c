#include <uapi/linux/ptrace.h>
#include <linux/sched.h>
#include <uapi/linux/bpf.h>
#include <bcc/proto.h>

/*
 * eBPF program that hooks do_page_fault() to intercept page faults
 * and redirect them to user-space handler via eBPF maps
 * 
 * Inspired by LightSwap architecture
 */

BPF_PERF_OUTPUT(events);
BPF_TABLE("hash", uint64_t, uint64_t, pf_map, 10000);  /* Store page fault info */

/* Page fault event structure */
struct page_fault_event {
    uint32_t tid;
    uint32_t pid;
    uint64_t faulted_addr;
    uint64_t instruction_pointer;
    uint64_t timestamp;
    uint32_t is_write;
};

/* Hook do_page_fault() */
int trace_page_fault(struct pt_regs *ctx, unsigned long address,
                    struct pt_regs *regs)
{
    struct page_fault_event event = {};
    
    event.tid = bpf_get_current_pid_tgid() & 0xFFFFFFFF;
    event.pid = bpf_get_current_pid_tgid() >> 32;
    event.faulted_addr = address;
    event.instruction_pointer = PT_REGS_IP(regs);
    event.timestamp = bpf_ktime_get_ns();
    
    /* Heuristic: if error_code & 2, it's a write fault */
    event.is_write = ctx->si >> 1 & 1;
    
    /* Send to user-space */
    events.perf_submit(ctx, &event, sizeof(event));
    
    return 0;
}

/*
 * Alternative: hook do_handle_mm_fault() - more specific
 * This is called for each page fault after initial handling
 */
int trace_handle_mm_fault(struct pt_regs *ctx, struct vm_area_struct *vma,
                          unsigned long address, unsigned int flags)
{
    struct page_fault_event event = {};
    
    event.tid = bpf_get_current_pid_tgid() & 0xFFFFFFFF;
    event.pid = bpf_get_current_pid_tgid() >> 32;
    event.faulted_addr = address;
    event.timestamp = bpf_ktime_get_ns();
    event.is_write = (flags & 1) ? 1 : 0;  /* FAULT_FLAG_WRITE */
    
    /* Send to user-space */
    events.perf_submit(ctx, &event, sizeof(event));
    
    return 0;
}
