#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

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
}
bool check_addr(char* addr){
	if(!is_user_vaddr(addr)|| !pml4_get_page(thread_current()->pml4,addr))
		return false;
	return true;
}
/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.
	// printf ("system call!\n");
	// printf("%d\n",check_addr(f->R.rsi));
	// printf("%d\n",f->R.rax);
	switch (f->R.rax)
	{
	case SYS_HALT:
		halt();
		break;
	case SYS_EXIT:
		exit(f->R.rdi);
	case SYS_FORK:
		// fork();
	case SYS_EXEC:
	case SYS_WAIT:
		wait(f->R.rdi);
	case SYS_CREATE:
		create(f->R.rdi,f->R.rsi);
	case SYS_REMOVE:
		remove(f->R.rdi);
	case SYS_OPEN:
	case SYS_FILESIZE:
	case SYS_READ:
	case SYS_WRITE:
		write(f->R.rdi,f->R.rsi,f->R.rdx);
	case SYS_SEEK:
	case SYS_TELL:
	case SYS_CLOSE:
		close(f->R.rdi);
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
	thread_current()->exit_status = status;
	thread_exit();
}
pid_t fork (const char *thread_name){
	if(thread_current()->name != thread_name)
		return thread_create(thread_name,PRI_DEFAULT,pml4_for_each,thread_current());
	else
		return 0;
}
int exec (const char *file){

}
int wait (pid_t child_tid){
	return process_wait(child_tid);
}
bool create (const char *file, unsigned initial_size){
	return filesys_create(file,initial_size);
}
bool remove (const char *file){
	return filesys_remove(file);
}
int open (const char *file){
	struct file *open_file = filesys_open(file);
}
int filesize (int fd);
int read (int fd, void *buffer, unsigned length){

}
int write (int fd, const void *buffer, unsigned length){
	putbuf(buffer,length);
	return fd;
}
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd){
	
}

int dup2(int oldfd, int newfd);
