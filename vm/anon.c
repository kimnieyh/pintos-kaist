/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"
#include "lib/kernel/bitmap.h"
#include "threads/synch.h"
/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

struct bitmap *swap_map;
struct lock swap_lock;

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	swap_disk = disk_get(1,1); // swap disk 
	size_t swap_size = disk_size(swap_disk) / SECTORS_PER_PAGE;
	swap_map = bitmap_create(swap_size);
	lock_init(&swap_lock);
}

/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &anon_ops;
	struct anon_page *anon_page = &page->anon;
	anon_page->type = type;

	// 아직 swap_map에 들어가지 않은 상태
	anon_page->swap_idx = -1;

	return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	// printf("[START] anon_swap_in {%p}\n",page->va);
	struct anon_page *anon_page = &page->anon;
	
	int page_no = anon_page->swap_idx;

	// 유효한 swap map 인지 확인
	if(bitmap_test(swap_map,page_no) == false){
		return false;
	}

	// 한 페이지의 sector의 개수만큼 sector에서 read
	for (int i = 0 ; i < SECTORS_PER_PAGE ; i ++)
	{
		lock_acquire(&swap_lock);
		disk_read(swap_disk, page_no * SECTORS_PER_PAGE + i , page->va + DISK_SECTOR_SIZE * i );
		lock_release(&swap_lock);
	}

	// 사용 가능한 swap map으로 변경
	bitmap_set(swap_map,page_no,false);
	// printf("[END] anon_swap_in {%p}\n",page->va);
	return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	// printf("[START] anon_swap_out {%p}\n",page->va);
	struct anon_page *anon_page = &page->anon;

	// 빈 swap slot 찾기
	int page_no = bitmap_scan(swap_map,0,1,false);

	// 한 페이지의 sector의 개수만큼 sector에 write
	for (int i = 0 ; i < SECTORS_PER_PAGE ; i ++)
	{
		lock_acquire(&swap_lock);
		disk_write(swap_disk, page_no * SECTORS_PER_PAGE + i , page->va + DISK_SECTOR_SIZE * i );
		lock_release(&swap_lock);
	}

	// swap_map 셋팅
	bitmap_set(swap_map, page_no, true);

	//clear page
	pml4_clear_page(thread_current()->pml4,page->va);

	anon_page->swap_idx = page_no;
	// printf("[END] anon_swap_out {%p}\n",page->va);
	return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
}
