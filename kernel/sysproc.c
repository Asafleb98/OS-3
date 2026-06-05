#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
//My Code*******************************************
// sys_flip_display: zero-copy page flip.
//
// Syscall argument 0: user virtual address of a page-aligned buffer
// that is exactly GPU_FB_PAGES (300) * PGSIZE bytes (i.e. 640x480x4 =
// 1,228,800 bytes).  The buffer must already be fully mapped in the
// calling process's address space.
//
// Returns 0 on success, (uint64)-1 on failure (bad alignment, page not
// mapped, missing user permission, etc.).
uint64
sys_flip_display(void)
{
  uint64 addr;
  argaddr(0, &addr);

  // NULL is meaningless here — the user must provide a real buffer.
  // Page alignment is also validated again inside virtio_gpu_flip,
  // but checking here lets us fail fast without acquiring gpu_lock.
  if (addr == 0 || addr % PGSIZE != 0)
    return -1;

  struct proc *p = myproc();

  // virtio_gpu_flip does the per-page walk, permission check, and the
  // actual DETACH/ATTACH commands.  On failure it leaves the device's
  // current backing untouched.
  if (virtio_gpu_flip(p->pagetable, addr) != 0)
    return -1;

  return 0;
}

// sys_map_display: map the GPU's kernel framebuffer pages (fb[]) directly
// into the calling process's address space with PTE_U|PTE_R|PTE_W.
//
// Syscall argument 0: desired user virtual address (must be page-aligned).
//   Pass 0 to let the kernel auto-select the next available VA above p->sz.
//
// Returns the mapped virtual address on success, (uint64)-1 on failure.
//
// TODO: Students implement this syscall.
// sys_map_display: map the GPU's kernel framebuffer pages (fb[]) directly
// into the calling process's address space with PTE_U|PTE_R|PTE_W.
//
// Syscall argument 0: desired user virtual address (must be page-aligned).
//   Pass 0 to let the kernel auto-select the next available VA above p->sz.
//
// Returns the mapped virtual address on success, (uint64)-1 on failure.
uint64
sys_map_display(void)
{
  uint64 addr;
  argaddr(0, &addr);

  struct proc *p = myproc();

  // One mapping per process.  If this process already has the FB mapped,
  // refuse — the caller can keep using its previous pointer.  (Re-mapping
  // would either leak the old mapping or surprise other code holding the
  // old pointer.)
  if (p->fb_va != 0)
    return -1;

  uint64 va;
  if (addr == 0) {
    // Auto-pick: first page-aligned VA at or above p->sz.
    va = PGROUNDUP(p->sz);
  } else {
    // Caller-supplied VA: must be page-aligned.
    if (addr % PGSIZE != 0)
      return -1;
    va = addr;
  }

  // virtio_gpu_map_user does the collision check, the actual mapping,
  // and rolls back on partial failure.
  if (virtio_gpu_map_user(p->pagetable, va) != 0)
    return -1;

  // Remember where we mapped so freeproc()/exec() can tear it down
  // without freeing the (kernel-owned) physical pages.
  p->fb_va = va;
  return va;
}
