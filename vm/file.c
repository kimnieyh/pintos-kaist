/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/pte.h"
static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);
void hash_print_func (struct hash_elem *e, void *aux){
	struct page *p = hash_entry(e,struct page,hash_elem);
	
	printf("-------------------------------------------------------------\n");
	printf("  %12p  |  %12p  |  %12p    |    %d    \n",p->va,p->frame->kva,p->file.file,
	pg_ofs(p->va)& PTE_W);
}
void 
print_spt(){ 
	printf("       VA       |       KV       |        FILE      | writable \n");
	hash_apply(&thread_current()->spt,hash_print_func);
	printf("=============================================================\n");
}

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
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
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
	uint32_t read_bytes, zero_bytes;

	read_bytes = length > file_size ? offset + length : offset + file_size;
	zero_bytes = (ROUND_UP (read_bytes, PGSIZE)
								- read_bytes);
	
	void *ret = addr;
	while ( read_bytes > 0 || zero_bytes > 0 ) {
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		struct file_info *file_info = calloc(sizeof(struct file_info),1);
		file_info->file = file;
		file_info->offset = offset;
		file_info->length = length;
		file_info->bytes = page_read_bytes;
		if(!vm_alloc_page_with_initializer(VM_FILE, addr,
					writable, lazy_load_segment,file_info))
		{
			// printf("[FAIL] do_mmap.vm_alloc_page_with_initializer\n");
			free(file_info);
			return NULL;
		}
		offset += page_read_bytes;
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
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
