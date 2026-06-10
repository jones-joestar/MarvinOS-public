#include "loader.h"
#include "elf.h"
#include "../memory/paging_helper.h"
#include "../memory/kalloc.h"
#include "../memory/malloc.h"
#include "../disk/fat32.h"
#include "../common.h"
#include "../console/console_helper.h"
#include "../../bootinfo.h"

extern BootInfo *bootInfo;

static void map_segment_pages(uint64_t vaddr, uint64_t memsz, uint32_t pflags) {
    uint64_t start = PAGE_ROUNDDOWN(vaddr);
    uint64_t end   = PAGE_ROUNDUP(vaddr + memsz);
    
    uint64_t flags = PAGE_USER_MODE | PAGE_PRESENT | PAGE_WRITE;
    if (!(pflags & PF_X)) flags |= PAGE_EXECUTE_DISABLE;

    for (uint64_t addr = start; addr < end; addr += PAGE_SIZE) {
        void *phys = kalloc();
        map_address(phys, (void *)addr, flags, true);
    }
}

static user_program_t load_elf_from_buf(const uint8_t *buf) {
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)buf;
    if (*(uint32_t *)ehdr->e_ident != ELF_MAGIC ||
        ehdr->e_type    != ET_EXEC  ||
        ehdr->e_machine != EM_X86_64)
        return (user_program_t){ .valid = 0 }; //checks validity of ELF header with magic number, type, and architecture

    uint64_t max_vaddr = 0;
    Elf64_Phdr *phdrs = (Elf64_Phdr *)(buf + ehdr->e_phoff);
    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        Elf64_Phdr *ph = &phdrs[i];
        if (ph->p_type != PT_LOAD || ph->p_memsz == 0)
            continue;

        if (ph->p_vaddr >= HIGHER_HALF_OFFSET)
            return (user_program_t){ .valid = 0 };

        map_segment_pages(ph->p_vaddr, ph->p_memsz, ph->p_flags);
        memset((void *)ph->p_vaddr, 0, ph->p_memsz);
        if (ph->p_filesz)
            memcpy((void *)ph->p_vaddr, buf + ph->p_offset, ph->p_filesz);

        uint64_t end = ph->p_vaddr + ph->p_memsz;
        if (end > max_vaddr)
            max_vaddr = end;
    }

    map_stack_pages();
    return (user_program_t){
        .entry     = (void *)(uint64_t)ehdr->e_entry,
        .stack_top = USER_STACK_TOP,
        .heap_base = (void *)PAGE_ROUNDUP(max_vaddr),
        .valid     = 1
    };
}

user_program_t load_elf_from_disk(const char *path) {
    // Check files pre-loaded by the bootloader from the ESP (works on any hardware)
    if (bootInfo) {
        for (uint32_t i = 0; i < bootInfo->file_count; i++) {
            const char *p1 = path,  *p2 = bootInfo->files[i].name;
            if (*p1 == '/') p1++;
            if (*p2 == '/') p2++;
            if (strcmp(p1, p2) == 0)
                return load_elf_from_buf((const uint8_t *)PHYS_TO_VIRT(bootInfo->files[i].data));
        }
    }

    
    fat32_file_t f = fat32_open(path);
    if (!f.valid)
        return (user_program_t){ .valid = 0 };

    uint8_t *buf = malloc(f.size);
    if (!buf)
        return (user_program_t){ .valid = 0 };

    fat32_read(&f, buf, f.size);
    user_program_t result = load_elf_from_buf(buf);
    free(buf);
    return result;
}
