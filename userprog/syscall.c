#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/palloc.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */
#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */
/* An open file. */
struct file {
	struct inode *inode;        /* File's inode. */
	off_t pos;                  /* Current position. */
	bool deny_write;            /* Has file_deny_write() been called? */
};
void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
	lock_init(&filesys_lock);
}
void check_addr(char* addr){
	//세팅은 안됐지만, 페이지는 존재할 때 !!
#ifdef VM
	if(!is_user_vaddr(addr)){
		exit(-1);}
	struct page *page = spt_find_page(&thread_current()->spt,addr);
	if(!page){
		exit(-1);}
	else{
		return;	
	}
	
#endif
	if(!is_user_vaddr(addr)|| !pml4_get_page(thread_current()->pml4,addr))
		exit(-1); 
}

void check_page(char * addr){
	#ifdef VM
	struct page *page = spt_find_page(&thread_current()->spt,addr);
	if(page ==NULL)
		exit(-1);

	switch (page->operations->type)
	{
	case VM_UNINIT:
		// if(!IS_WRITABLE(page->uninit.type))
		// 	exit(-1);
		break;
	case VM_ANON:
		if(!IS_WRITABLE(page->anon.type))
			exit(-1);
		break;
	case VM_FILE:
		if(!IS_WRITABLE(page->file.type))
			exit(-1);
		break;
	}
	#endif
}
/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	switch (f->R.rax)
	{
	case SYS_HALT:
		halt();
		break;
	case SYS_EXIT:
		exit(f->R.rdi);
		break;
	case SYS_FORK:
		f->R.rax = fork(f->R.rdi,f);
		break;
	case SYS_EXEC:  
		f->R.rax = exec(f->R.rdi);
	case SYS_WAIT:
		f->R.rax = wait(f->R.rdi);
		break;
	case SYS_CREATE:
		f->R.rax = create(f->R.rdi,f->R.rsi);
		break;
	case SYS_REMOVE:
		remove(f->R.rdi);
		break;
	case SYS_OPEN:
		f->R.rax = open(f->R.rdi);
		break;
	case SYS_FILESIZE:
		f->R.rax = filesize(f->R.rdi);
		break;
	case SYS_READ:
		f->R.rax = read(f->R.rdi,f->R.rsi,f->R.rdx);
		break;
	case SYS_WRITE:
		f->R.rax = write(f->R.rdi,f->R.rsi,f->R.rdx);
		break;
	case SYS_SEEK:
		seek(f->R.rdi,f->R.rsi);
		break;
	case SYS_TELL:
		f->R.rax = tell(f->R.rdi);
		break;
	case SYS_CLOSE:
		close(f->R.rdi);
		break;
	case SYS_DUP2:
		f->R.rax = dup2(f->R.rdi,f->R.rsi);
		break;
	case SYS_MMAP:
		f->R.rax = (uint64_t)mmap(f->R.rdi,f->R.rsi,f->R.rdx,f->R.r10,f->R.r8);
		break;
	case SYS_MUNMAP:
		munmap(f->R.rdi);
		break;
	default:
		break;
	}
}

void halt (void) //NO_RETURN
{
	power_off();
}

void exit (int status)// NO_RETURN
{
	struct thread *curr = thread_current ();
	curr->exit_status = status;
	printf("%s: exit(%d)\n",curr->name,curr->exit_status);
	thread_exit();
}

pid_t fork (const char *thread_name,const struct intr_frame *f){
	return process_fork(thread_name,f);
}

int exec (const char *file){
	// printf("exec start\n");
	check_addr(file);
	char *f_copy = palloc_get_page(0);
	if(f_copy == NULL)
		exit(-1);
	strlcpy(f_copy,file,PGSIZE);
	int result = process_exec(f_copy);
	if(result == -1)
		exit(-1);
	return result;
}

int wait (pid_t child_tid){
	return process_wait(child_tid);
}

int create_fd(struct file *file){
	struct thread *curr = thread_current();
	struct file **fdt = curr->files;
	while(curr->fd_idx < FDT_COUNT_LIMIT && fdt[curr->fd_idx])
		curr->fd_idx++;
	if(curr->fd_idx >= FDT_COUNT_LIMIT)
		return -1;
	fdt[curr->fd_idx] = file;

	return curr->fd_idx;
}

struct file* find_file_by_fd(int fd){
	struct thread *curr = thread_current();
	if(fd >= FDT_COUNT_LIMIT || fd <0)
		return NULL;
	return curr->files[fd];
}

void del_fd(int fd){
	struct thread *curr = thread_current();
	curr->files[fd] = NULL;
}

bool create (const char *file, unsigned initial_size){
	// printf("create start\n");
	check_addr(file);
	lock_acquire(&filesys_lock);
	bool success = filesys_create(file,initial_size);
	lock_release(&filesys_lock);
	return success;
}

bool remove (const char *file){
	return filesys_remove(file);
}

int open (const char *file){
	// printf("open start\n");
	check_addr(file);

	struct file *open_file = filesys_open(file);

	if(open_file == NULL)
		return -1;

	int fd = create_fd(open_file);
	if (fd == -1)
		file_close(open_file);
	return fd;
}

int filesize (int fd){
	struct file *file = find_file_by_fd(fd);
	if(file < 3)
		return -1;
	return file_length(file);
}
void pos_update(struct file *file){
	int i;
	struct thread *t = thread_current();
	for (i = 0 ; i < FDT_COUNT_LIMIT; i ++){
		if(t->files[i] > 2)
			if(t->files[i]->inode == file->inode)
				t->files[i]->pos = file_tell(file);
	}
}
int read (int fd, void *buffer, unsigned length){
	check_addr(buffer);
	check_page(buffer);
	struct file *file = find_file_by_fd(fd);
	int bytes_read = 0;
	char *ptr = (char *)buffer;
	if (file == 1){
		for(int i = 0 ; i < length; i++){
			char ch = input_getc();
			if (ch == '\n')
				break;
			*ptr = ch;
			ptr ++;
			bytes_read ++;
		}
	}else{
		if (file <3)
			return -1;

		lock_acquire(&filesys_lock);
		// printf("file_read !!!!\n");
		bytes_read = file_read(file,buffer,length);
		// printf("file_read done!!!!%d\n",bytes_read);
		lock_release(&filesys_lock);
		pos_update(file);	
	}
	// printf("read done\n");
	return bytes_read;
}

int write (int fd, const void *buffer, unsigned length){
	check_addr(buffer);
	struct file *file = find_file_by_fd(fd);

	int byte_write = 0;
	if (file == 2){ 
		putbuf(buffer,length);
		byte_write = length;
	}
	else
	{
		if (file < 3)
			return -1;
		lock_acquire(&filesys_lock);
		byte_write = file_write(file,buffer,length);
		lock_release(&filesys_lock);
		pos_update(file);
	}
	return byte_write;
}
// 파일 편집 위치 변경
void seek (int fd, unsigned position){
	struct file *file = find_file_by_fd(fd);
	if(file < 3)
		return;
	file_seek(file,position);
	pos_update(file);
}
// 파일 위치 반환
unsigned tell (int fd){
	struct file *file = find_file_by_fd(fd);
	if(file < 3)
		return;
	return file_tell(file); 
}

void close (int fd){
	struct file *file = find_file_by_fd(fd);
	if (file > 0 ){
		if (file > 2)
		{	
			file_close(file);
		}
		del_fd(fd);
	}
}

int dup2(int oldfd, int newfd){
	struct file *old_file = find_file_by_fd(oldfd);
	struct file *new_file = find_file_by_fd(newfd);
	// printf("old:%d, new:%d\n",oldfd,newfd);
	if(old_file == NULL)
		return -1;
	if (old_file == new_file)
		return newfd;
	
	if (old_file > 2 ){
		close(newfd);
		lock_acquire(&filesys_lock);
		thread_current()->files[newfd] = file_duplicate(old_file);
		lock_release(&filesys_lock);
	}else{
		close(newfd);
		thread_current()->files[newfd] = old_file;
	}
	return newfd;
}

void *
mmap (void *addr, size_t length, int writable, int fd, unsigned int offset) {
	struct file *file = find_file_by_fd(fd);
	struct file *new_file = file_duplicate(file);
	if( file < 3 ){
		// printf("[FAIL]file < 3\n");
		return NULL;}
	if( length == NULL ){
		// printf("[FAIL]length is NULL\n");
		return NULL;}
	if( addr == NULL ){
		// printf("[FAIL]addr is NULL\n");
		return NULL;}
	if( pg_ofs(addr) ){
		// printf("[FAIL]addr is not pg_ofs %p\n ",addr);
		return NULL;}
	if( !file_length(file) ){
		// printf("[FAIL]file_length is NULL\n");
		return NULL;
	}
	if(!is_user_vaddr(addr)){
		return NULL;
	}
	if(pg_ofs(offset)) {
		return NULL;
	}

	return do_mmap(addr,length,writable,new_file,offset);
}

void
munmap (void *addr) {
	if(addr == NULL)
		return;
	do_munmap(addr);
}
