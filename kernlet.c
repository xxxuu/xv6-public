#include "types.h"
#include "kernel.h"
#include "spinlock.h"
#include "condvar.h"
#include "cpu.h"
#include "proc.h"
#include "vm.h"
#include "fs.h"
#include "file.h"
#include "wq.h"
#include "ipc.h"

static void
pread_work(struct work *w, void *a0, void *a1, void *a2, void *a3)
{
  struct inode *ip = a0;
  void *kshared = a1;
  struct ipcctl *ipc = kshared;
  size_t count = (uptr)a2;
  off_t off = (uptr)a3;
  int r;

  if (count > KSHAREDSIZE-PGSIZE)
    panic("pread_work");

  //cprintf("1: %p %p %lu %lu\n", ip, buf, count, off);

  ilock(ip, 0);
  r = readi(ip, kshared+PGSIZE, off, count);
  iunlock(ip);
  
  ipc->result = r;
  barrier();
  ipc->done = 1;
}

static struct work *
pread_allocwork(struct inode *ip, void *buf, size_t count, off_t off)
{
  struct work *w = allocwork();
  if (w == NULL)
    return NULL;

  //cprintf("0: %p %p %lu %lu\n", ip, buf, count, off);

  w->rip = pread_work;
  w->arg0 = ip;
  w->arg1 = buf;
  w->arg2 = (void*)count;
  w->arg3 = (void*)off;
  
  return w;
}

long
sys_kernlet(int fd, size_t count, off_t off)
{
  struct file *f;
  struct work *w;

  if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == 0)
    return -1;
  if(f->type != FD_INODE)
    return -1;

  w = pread_allocwork(f->ip, myproc()->vmap->kshared, count, off);
  if (w == NULL)
    return -1;
  if (wq_push(w) < 0) {
    freework(w);
    return -1;
  }
  return 0;
}
