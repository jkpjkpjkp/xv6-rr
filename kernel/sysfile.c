//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "spinlock.h"
#include "proc.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "fcntl.h"
#include "buf.h"

// Fetch the nth word-sized system call argument as a file descriptor
// and return both the descriptor and the corresponding struct file.
static int
argfd(int n, int *pfd, struct file **pf)
{
  int fd;
  struct file *f;

  argint(n, &fd);
  if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == 0)
    return -1;
  if(pfd)
    *pfd = fd;
  if(pf)
    *pf = f;
  return 0;
}

// Allocate a file descriptor for the given file.
// Takes over file reference from caller on success.
static int
fdalloc(struct file *f)
{
  int fd;
  struct proc *p = myproc();

  for(fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd] == 0){
      p->ofile[fd] = f;
      return fd;
    }
  }
  return -1;
}

// static int
// fdalloc3(struct file *f, int fd)
// {
//   if(fd < 0 || fd >= NOFILE)
//     return -1;
//   struct proc *p = myproc();

//   if(p->ofile[fd] == 0){
//     p->ofile[fd] = f;
//     return fd;
//   }else{
//     return -1;
//   }
// }

// returns strlen(buf) on success
int 
pwd(char *buf)
{
  struct inode *ip;
  struct proc *p = myproc();
  ip = p->cwd;
  
  struct inode *next;
  char fullpath[MAXPATH] = "";
  char temppath[MAXPATH] = "";

  // Special case for root directory
  if(ip->inum == ROOTINO) {
    buf = "/";
    return 1;
  }

  // Travel up to root while building path
  while(ip->inum != ROOTINO) {
    ilock(ip);
    // Look for ".." entry to get parent directory
    if((next = dirlookup(ip, "..", 0)) == 0) {
      iunlockput(ip);
      panic("[pwd] .. not found in non-/ dir");
    }
    
    ilock(next);
    // Search through parent directory for current directory's name
    struct dirent de;
    for(int off = 0; off < next->size; off += sizeof(de)) {
      if(readi(next, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
        continue;
      if(de.inum == ip->inum) {
        // Prepend this directory name to path
        memmove(temppath, "/", 1);
        memmove(temppath + 1, de.name, DIRSIZ);
        memmove(temppath + strlen(temppath), fullpath, strlen(fullpath));
        memmove(fullpath, temppath, MAXPATH);
        break;
      }
    }
    
    iunlockput(ip);
    ip = next;
  }
  iunlockput(ip);

  // If path is empty (shouldn't happen), return root
  if(strlen(fullpath) == 0) {
    panic("[pwd] path is empty");
  }

  int ret = strlen(fullpath);
  safestrcpy(buf, fullpath, ret);
  return ret;
}

uint64
mount(char *source, char *target, const char *fstype, unsigned long flags)
{
  struct inode *ip1, *ip2;
  char name[DIRSIZ];
  uint dev;
  begin_op();
  
  if((ip1 = namei(source)) == 0){
    end_op();
    return -1;
  }

  ilock(ip1);
  if(ip1->inum != ROOTINO){
    iunlock(ip1);
    end_op();
    return -1;
  }

  dev = ip1->dev;
  iunlock(ip1);

  ip2 = nameiparent(target, name);
  if(ip2 == 0){
    end_op();
    return -1;
  }

  ilock(ip2);
  if(ip2->type != T_DIR){
    iunlockput(ip2);
    end_op();
    return -1;
  }

  if(dirlink(ip2, name, ip1->inum) < 0){
    iunlockput(ip2);
    end_op();
    return -1;
  }

  iunlockput(ip2);
  end_op();
  return dev;
}

uint64
sys_dup(void)
{
  struct file *f;
  int fd;

  if(argfd(0, 0, &f) < 0)
    return -1;

  if((fd=fdalloc(f)) < 0)
    return -1;
  filedup(f);
  return fd;
}

uint64
sys_dup3(void)
{
  struct file *f;
  int fd;

  if(argfd(0, 0, &f) < 0)
    return -1;
  if((fd=fdalloc(f)) < 0)
    return -1;
  filedup(f);
  return fd;
}

uint64
sys_read(void)
{
  struct file *f;
  int n;
  uint64 p;

  argaddr(1, &p);
  argint(2, &n);
  if(argfd(0, 0, &f) < 0)
    return -1;
  return fileread(f, p, n);
}

uint64
sys_write(void)
{
  struct file *f;
  int n;
  uint64 p;
  
  argaddr(1, &p);
  argint(2, &n);
  if(argfd(0, 0, &f) < 0)
    return -1;

  return filewrite(f, p, n);
}

uint64
sys_close(void)
{
  int fd;
  struct file *f;

  if(argfd(0, &fd, &f) < 0)
    return -1;
  myproc()->ofile[fd] = 0;
  fileclose(f);
  return 0;
}

uint64
sys_fstat(void)
{
  struct file *f;
  uint64 st; // user pointer to struct stat

  argaddr(1, &st);
  if(argfd(0, 0, &f) < 0)
    return -1;
  return filestat(f, st);
}

// Create the path new as a link to the same inode as old.
uint64
sys_link(void)
{
  char name[DIRSIZ], new[MAXPATH], old[MAXPATH];
  struct inode *dp, *ip;

  if(argstr(0, old, MAXPATH) < 0 || argstr(1, new, MAXPATH) < 0)
    return -1;

  begin_op();
  if((ip = namei(old)) == 0){
    end_op();
    return -1;
  }

  ilock(ip);
  if(ip->type == T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }

  ip->nlink++;
  iupdate(ip);
  iunlock(ip);

  if((dp = nameiparent(new, name)) == 0)
    goto bad;
  ilock(dp);
  if(dp->dev != ip->dev || dirlink(dp, name, ip->inum) < 0){
    iunlockput(dp);
    goto bad;
  }
  iunlockput(dp);
  iput(ip);

  end_op();

  return 0;

bad:
  ilock(ip);
  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);
  end_op();
  return -1;
}

uint64
sys_linkat(void) // UNDONE! name[] not populated.
{
  char name[DIRSIZ], new[MAXPATH], old[MAXPATH];
  int olddirfd, newdirfd, flags;
  struct inode *dp, *ip;
  argint(0, &olddirfd);
  argint(2, &newdirfd);
  argint(4, &flags);

  if(argstr(1, old, MAXPATH) < 0 || argstr(3, new, MAXPATH) < 0)
    return -1;

  begin_op();
  if((ip = namefd(olddirfd, 0, old, 0)) == 0){
    end_op();
    return -1;
  }

  ilock(ip);
  if(ip->type == T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }

  ip->nlink++;
  iupdate(ip);
  iunlock(ip);

  if((dp = namefd(newdirfd, 1, new, name)) == 0)
    goto bad;
  ilock(dp);
  if(dp->dev != ip->dev || dirlink(dp, name, ip->inum) < 0){
    iunlockput(dp);
    goto bad;
  }

  iunlockput(dp);
  iput(ip);

  end_op();

  return 0;

bad:
  ilock(ip);
  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);
  end_op();
  return -1;
}

// Is the directory dp empty except for "." and ".." ?
static int
isdirempty(struct inode *dp)
{
  int off;
  struct dirent de;

  for(off=2*sizeof(de); off<dp->size; off+=sizeof(de)){
    if(readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
      panic("isdirempty: readi");
    if(de.inum != 0)
      return 0;
  }
  return 1;
}

uint64
sys_unlink(void)
{
  struct inode *ip, *dp;
  struct dirent de;
  char name[DIRSIZ], path[MAXPATH];
  uint off;

  if(argstr(0, path, MAXPATH) < 0)
    return -1;

  begin_op();
  if((dp = nameiparent(path, name)) == 0){
    end_op();
    return -1;
  }

  ilock(dp);

  // Cannot unlink "." or "..".
  if(namecmp(name, ".") == 0 || namecmp(name, "..") == 0)
    goto bad;

  if((ip = dirlookup(dp, name, &off)) == 0)
    goto bad;
  ilock(ip);

  if(ip->nlink < 1)
    panic("unlink: nlink < 1");
  if(ip->type == T_DIR && !isdirempty(ip)){
    iunlockput(ip);
    goto bad;
  }

  memset(&de, 0, sizeof(de));
  if(writei(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
    panic("unlink: writei");
  if(ip->type == T_DIR){
    dp->nlink--;
    iupdate(dp);
  }
  iunlockput(dp);

  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);

  end_op();

  return 0;

bad:
  iunlockput(dp);
  end_op();
  return -1;
}

uint64
sys_unlinkat(void)
{
  int dirfd;
  struct inode *ip, *dp;
  struct dirent de;
  char name[DIRSIZ], path[MAXPATH];
  uint off;

  argint(0, &dirfd);
  if(argstr(1, path, MAXPATH) < 0)
    return -1;

  begin_op();
  if((dp = namefd(dirfd, 1, path, name)) == 0){
    end_op();
    return -1;
  }

  // hereon copied from sys_unlink and unmodified. 
  ilock(dp);

  // Cannot unlink "." or "..".
  if(namecmp(name, ".") == 0 || namecmp(name, "..") == 0)
    goto bad;

  if((ip = dirlookup(dp, name, &off)) == 0)
    goto bad;
  ilock(ip);

  if(ip->nlink < 1)
    panic("unlink: nlink < 1");
  if(ip->type == T_DIR && !isdirempty(ip)){
    iunlockput(ip);
    goto bad;
  }

  memset(&de, 0, sizeof(de));
  if(writei(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
    panic("unlink: writei");
  if(ip->type == T_DIR){
    dp->nlink--;
    iupdate(dp);
  }
  iunlockput(dp);

  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);

  end_op();

  return 0;

bad:
  iunlockput(dp);
  end_op();
  return -1;
}

struct inode*
create(char *path, short type, short major, short minor, int fd)
{
  printf("[create] starting\n");
  struct inode *ip, *dp;
  char name[DIRSIZ];

  if(fd > 0)
    dp = namefd(fd, 1, path, name);
  else
    dp = nameiparent(path, name);

  if(dp == 0)
    return 0;

  printf("[create] locking dir ip\n");
  ilock(dp);

  if((ip = dirlookup(dp, name, 0)) != 0){
    iunlockput(dp);
    ilock(ip);
    if(type == T_FILE && (ip->type == T_FILE || ip->type == T_DEVICE))
      return ip;
    iunlockput(ip);
    return 0;
  }

  printf("[create] dirlookup done\n");

  if((ip = ialloc(dp->dev, type)) == 0){
    iunlockput(dp);
    return 0;
  }

  ilock(ip);
  ip->major = major;
  ip->minor = minor;
  ip->nlink = 1;
  iupdate(ip);

  if(type == T_DIR){  // Create . and .. entries.
    // No ip->nlink++ for ".": avoid cyclic ref count.
    if(dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
      goto fail;
  }

  if(dirlink(dp, name, ip->inum) < 0)
    goto fail;

  if(type == T_DIR){
    // now that success is guaranteed:
    dp->nlink++;  // for ".."
    iupdate(dp);
  }

  iunlockput(dp);

  return ip;

 fail:
  // something went wrong. de-allocate ip.
  ip->nlink = 0;
  iupdate(ip);
  iunlockput(ip);
  iunlockput(dp);
  return 0;
}

uint64
sys_open(void)
{
  char path[MAXPATH];
  int fd, omode;
  struct file *f;
  struct inode *ip;
  int n;

  argint(1, &omode);
  if((n = argstr(0, path, MAXPATH)) < 0)
    return -1;

  begin_op();

  if(omode & O_CREATE){
    ip = create(path, T_FILE, 0, 0, 0);
    if(ip == 0){
      end_op();
      return -1;
    }
  } else {
    if((ip = namei(path)) == 0){
      end_op();
      return -1;
    }
    ilock(ip);
    if(ip->type == T_DIR && omode != O_RDONLY){
      iunlockput(ip);
      end_op();
      return -1;
    }
  }

  if(ip->type == T_DEVICE && (ip->major < 0 || ip->major >= NDEV)){
    iunlockput(ip);
    end_op();
    return -1;
  }

  if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0){
    if(f)
      fileclose(f);
    iunlockput(ip);
    end_op();
    return -1;
  }

  if(ip->type == T_DEVICE){
    f->type = FD_DEVICE;
    f->major = ip->major;
  } else {
    f->type = FD_INODE;
    f->off = 0;
  }
  f->ip = ip;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);

  if((omode & O_TRUNC) && ip->type == T_FILE){
    itrunc(ip);
  }

  iunlock(ip);
  end_op();

  return fd;
}

uint64
sys_openat(void) // TODO: double check the O_CREATE is set as always on
{
  int fd, n;
  char path[MAXPATH], fullpath[MAXPATH];
  struct file *f;

  argint(0, &fd);
  if(argstr(1, path, MAXPATH) < 0)
    return -1;

  if(path[0] == '/' || fd == -100) {
    struct proc *p = myproc();
    p->trapframe->a0 = p->trapframe->a1;
    p->trapframe->a1 = p->trapframe->a2;
    return sys_open();
  }

  if(fd < 0 || fd >= NOFILE || (f = myproc()->ofile[fd]) == 0)
    return -1;

  if(f->type != FD_INODE || !(f->ip->type == T_DIR))
    return -1;

  n = pwd(fullpath);
  strncpy(fullpath + n, path, MAXPATH - n);

  safestrcpy(path, fullpath, MAXPATH);
  struct proc *p = myproc();
  p->trapframe->a0 = p->trapframe->a1;
  p->trapframe->a1 = p->trapframe->a2;
  return sys_open();
}

uint64
sys_mkdir(void)
{
  char path[MAXPATH];
  struct inode *ip;

  begin_op();
  if(argstr(0, path, MAXPATH) < 0 || (ip = create(path, T_DIR, 0, 0, 0)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

uint64
sys_mkdirat(void)
{
  int dirfd;
  char path[MAXPATH];
  struct inode *ip;


  argint(0, &dirfd);
  if(argstr(1, path, MAXPATH) < 0)
    return -1;
  
  begin_op();
  if((ip = create(path, T_DIR, 0, 0, dirfd)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

uint64
sys_mknod(void)
{
  struct inode *ip;
  char path[MAXPATH];
  int major, minor;

  begin_op();
  argint(1, &major);
  argint(2, &minor);
  if((argstr(0, path, MAXPATH)) < 0 ||
     (ip = create(path, T_DEVICE, major, minor, 0)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

uint64
sys_chdir(void)
{
  char path[MAXPATH];
  struct inode *ip;
  struct proc *p = myproc();
  
  begin_op();
  if(argstr(0, path, MAXPATH) < 0 || (ip = namei(path)) == 0){
    end_op();
    return -1;
  }
  ilock(ip);
  if(ip->type != T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }
  iunlock(ip);
  iput(p->cwd);
  end_op();
  p->cwd = ip;
  return 0;
}

uint64
sys_getcwd(void)
{
  int n;
  uint64 buf;
  argaddr(0, &buf);
  char fullpath[MAXPATH] = "";
  n = pwd(fullpath);

  struct proc *p = myproc();
  if(copyout(p->pagetable, buf, fullpath, n+1) < 0)
    return -1;
  return 0;
}

struct sys_getdents64_dirent {
  uint64 d_ino;	// 索引结点号
  long d_off;	// 到下一个sys_getdents64_dirent的偏移
  unsigned short d_reclen;	// 当前sys_getdents64_dirent的长度
  unsigned char d_type;	// 文件类型
  char d_name[DIRSIZ];	//文件名
};

uint64
sys_getdents64(void)
{
  // by o1 pro, modified by jkp
  int fd;
  struct file *f;
  uint64 buf;
  int len, cur_len;
  uint off;
  struct dirent de;
  struct sys_getdents64_dirent ret;
  struct inode *ip;
  struct proc *p;

  argint(0, &fd);
  argaddr(1, &buf);
  argint(2, &len);

  p = myproc();
  if (fd < 0 || fd >= NOFILE || (f = p->ofile[fd]) == 0)
    return -1;

  if (f->type != FD_INODE || !f->readable)
    return -1;

  struct inode *dp = f->ip;
  ilock(dp);
  if (dp->type != T_DIR) {
    iunlock(dp);
    return -1;
  }

  p = myproc();
  for(off = 0, cur_len = 0; off < dp->size; off += sizeof(de), cur_len += sizeof(ret)) {
    if(readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
      panic("sys_getdents64 read");
    if(de.inum == 0)
      continue;
    ret.d_ino = de.inum;
    safestrcpy(ret.d_name, de.name, DIRSIZ);
    ip = iget(dp->dev, de.inum);
    ret.d_type = ip->type;
    ret.d_off = sizeof(ret); // todo: should we put special value to indicate it is the last ret?
    ret.d_reclen = sizeof(ret) - sizeof(ret.d_name) + strlen(ret.d_name);
    if (cur_len + sizeof(ret) > len) {
      iunlock(dp);
      return -1;
    }
    copyout(p->pagetable, buf + cur_len, (char *) &ret, sizeof(ret));
  }
  iunlock(dp);
  return cur_len;
}

uint64
sys_exec(void)
{
  printf("[kernel/sysfile.c:sys_exec] starting\n");
  char path[MAXPATH], *argv[MAXARG];
  int i;
  uint64 uargv, uarg;

  argaddr(1, &uargv);
  printf("[kernel/sysfile.c:sys_exec] why\n");
  if(argstr(0, path, MAXPATH) < 0) {
    return -1;
  }
  printf("[kernel/sysfile.c:sys_exec] args\n");
  memset(argv, 0, sizeof(argv));
  for(i=0;; i++){
    if(i >= NELEM(argv)){
      goto bad;
    }
    if(fetchaddr(uargv+sizeof(uint64)*i, (uint64*)&uarg) < 0){
      goto bad;
    }
    if(uarg == 0){
      argv[i] = 0;
      break;
    }
    argv[i] = kalloc();
    if(argv[i] == 0)
      goto bad;
    if(fetchstr(uarg, argv[i], PGSIZE) < 0)
      goto bad;
  }

  printf("[kernel/sysfile.c:sys_exec] exec\n");
  int ret = exec(path, argv);

  for(i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    kfree(argv[i]);

  return ret;

 bad:
  for(i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    kfree(argv[i]);
  return -1;
}

uint64
sys_pipe(void)
{
  uint64 fdarray; // user pointer to array of two integers
  struct file *rf, *wf;
  int fd0, fd1;
  struct proc *p = myproc();

  argaddr(0, &fdarray);
  if(pipealloc(&rf, &wf) < 0)
    return -1;
  fd0 = -1;
  if((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0){
    if(fd0 >= 0)
      p->ofile[fd0] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  if(copyout(p->pagetable, fdarray, (char*)&fd0, sizeof(fd0)) < 0 ||
     copyout(p->pagetable, fdarray+sizeof(fd0), (char *)&fd1, sizeof(fd1)) < 0){
    p->ofile[fd0] = 0;
    p->ofile[fd1] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  return 0;
}

uint64
sys_mmap(void)
{
  return -1;  // Dummy implementation
}

uint64
sys_munmap(void)
{
  return -1;  // Dummy implementation
}

uint64
sys_virtiodiskrw(void)
{
  uint64 buf_addr;
  int write, dev, blockno;
  struct buf b;
  
  argaddr(0, &buf_addr);
  argint(1, &write);
  argint(2, &dev);
  argint(3, &blockno);

  initsleeplock(&b.lock, "virtio_disk_rw buf");
  b.valid = 0;
  b.disk = 0;
  b.dev = dev;
  b.blockno = blockno;

  if(copyin(myproc()->pagetable, (char*)b.data, buf_addr, BSIZE) < 0)
    return -1;

  virtio_disk_rw(&b, write, dev);

  if(!write) {
    if(copyout(myproc()->pagetable, buf_addr, (char*)b.data, BSIZE) < 0)
      return -1;
  }

  return 0;
}