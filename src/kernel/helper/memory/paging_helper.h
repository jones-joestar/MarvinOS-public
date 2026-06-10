#pragma once
#include "../common.h"

#define HIGHER_HALF_OFFSET 0xFFFFFFFF80000000ULL
#define PHYSICAL_OFFSET 0xFFFF888000000000ULL
#define GOP_VIRT_BASE 0xFFFFFE8000000000ULL
#define GOP_USERSPACE 0x400000000ULL
#define HEAP_VIRT_BASE 0xFFFFC90000000000ULL
#define STACK_VIRT_BASE 0xFFFFFF0000000000ULL

#define KERNEL_PHYS(virtual_addr) (((uint64_t)(virtual_addr) - HIGHER_HALF_OFFSET))
#define PHYS_TO_VIRT(physical_addr) ((uint64_t*)((uint64_t)(physical_addr) + PHYSICAL_OFFSET))
#define VIRT_TO_PHYS(virtual_addr) ((uint64_t*)((uint64_t)(virtual_addr) - PHYSICAL_OFFSET))

#define PAGE_SIZE 4096
#define TABLE_ENTRIES 512
#define TWO_MEGABYTE (PAGE_SIZE * TABLE_ENTRIES)
#define ONE_GIGABYTE (PAGE_SIZE * TABLE_ENTRIES * TABLE_ENTRIES)

// page alignment
#define PAGE_ROUNDUP(sz) (((sz)+PAGE_SIZE-1) & ~(PAGE_SIZE-1))
#define PAGE_ROUNDDOWN(a) (((a)) & ~(PAGE_SIZE-1))

// page flags
#define PAGE_PRESENT  (1ULL << 0) // access will produce page fault if 0
#define PAGE_WRITE   (1ULL << 1) // enables write access
#define PAGE_USER_MODE     (1ULL << 2) // enables user mode access
#define PAGE_WRITE_THROUGH (1ULL << 3)
#define PAGE_CACHE_DISABLE (1ULL << 4)
#define PAGE_HUGE          (1ULL << 7)
// PAT bit for 2MB huge pages (bit 12 of PDE selects PAT entry 4, 5, 6, or 7)
#define PAGE_PAT_LARGE     (1ULL << 12)
#define PAGE_EXECUTE_DISABLE (1ULL << 63) // disables execution

#define PAGE_ADDR_MASK 0x000FFFFFFFFFF000ULL

typedef uint64_t pte_t;

void init_paging();
void* bump_alloc();
bool map_address(void* phys, void* virt, uint64_t flags, bool allocate);
void load_page_table(pte_t* addr);
bool unmap_address_from_pml4(pte_t* pml4, void* virt);
bool unmap_address(void* virt);
void unmap_user_gop(pte_t* pml4);

pte_t* create_page_table();
void destroy_page_table(pte_t* pml4);
pte_t* get_kernel_pml4();

extern uint64_t GOP_NUM_PAGES;