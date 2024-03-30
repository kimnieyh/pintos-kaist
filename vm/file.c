/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/pte.h"
static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);


/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init (void) {

}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;
	page->file.type = type;
	return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
	void *addr = page->va;

	struct file *file = file_page->file;
	size_t length = file_length(file);
	off_t offset = file_page->offset;
	if(file_read_at(file,kva,length,offset)!= length){
		PANIC("todo");
		return false;
	}
	if(length < PGSIZE){
		memset(kva+offset,0,PGSIZE-length);
	}
	return true;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
	void *addr = page->va;
	struct thread *t = thread_current();

	if(IS_WRITABLE(page->file.type))
	{	
		struct file *file = page->file.file;
		int length = page->file.length;

		file_write_at (file,page->frame->kva,length,page->file.offset);
	}

	pml4_clear_page(t->pml4,addr);
	page->frame->page = NULL;
	page->frame = NULL;

	return true;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
	// printf("[START]do_mmap!\n");

	// 이미 할당된 페이지 인지 확인 
	struct page check_p;
	check_p.va = addr;
	if(spt_find_page(&thread_current()->spt, &check_p))
		return NULL;

	size_t file_size = file_length(file);
	size_t read_bytes, zero_bytes;

	read_bytes = length > file_size - offset ? length : file_size - offset;
	zero_bytes = (ROUND_UP (read_bytes, PGSIZE)
								- read_bytes);
	enum vm_type type = writable ? (VM_FILE | IS_WRITABLE) : VM_FILE;
	void *ret = addr;
	while ( read_bytes > 0 || zero_bytes > 0 ) {
		// printf("file_size : %d\n",file_size);
		// printf("addr : %p\n",addr);
		// printf("offset :%d \n",offset);
		size_t page_read_bytes = read_bytes > PGSIZE ? PGSIZE : read_bytes;
		size_t file_read_bytes = file_size > 0 ? file_size : 0;
		file_read_bytes = file_size > PGSIZE ? PGSIZE : file_size;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;
		// printf("file_read_bytes : %d\n",file_read_bytes);
		struct file_info *file_info = calloc(sizeof(struct file_info),1);
		file_info->file = file;
		file_info->offset = offset;
		file_info->length = length;
		// 실제 파일 바이트 
		file_info->bytes = file_read_bytes;
		if(!vm_alloc_page_with_initializer(type, addr,
					writable, lazy_load_segment,file_info))
		{
			printf("[FAIL] do_mmap.vm_alloc_page_with_initializer\n");
			free(file_info);
			return NULL;
		}
		offset += page_read_bytes;
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		file_size -= file_read_bytes;
		addr += PGSIZE;
	}

	return ret;
}

/* Do the munmap */
void
do_munmap (void *addr) {
// 	printf("[START]do_munmap start\n");
	struct thread *t = thread_current();
	struct page *page = spt_find_page(&t->spt,addr);
	struct file *file = page->file.file;
	int length = page->file.length;

	if(IS_WRITABLE(page->file.type))
	{	
		struct inode *inode = file_get_inode(file);
		inode_write_at (inode,page->frame->kva,length,page->file.offset);
	}	
	
	int page_cnt = ( length -1 ) / PGSIZE +1;
	for (int i = 0 ; i < page_cnt ; i ++ ){
		page = spt_find_page(&t->spt,addr+(PGSIZE * i));
		if (page == NULL){
			break;
		}
		spt_remove_page(&t->spt,page);
	}
	file_close(file);
	// printf("[END]do_munmap end\n");
}
