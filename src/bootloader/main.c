#include <efi.h>
#include <efilib.h>
#include "../kernel/bootinfo.h"


 // Entry point for the UEFI bootloader
 // long print statements can cause crashes
EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    InitializeLib(ImageHandle, SystemTable);
    Print(L"ImageHandle: %lx\r\n", ImageHandle);
    Print(L"Booting...\r\n");

    EFI_INPUT_KEY Key;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
    EFI_STATUS Status;

    // #### initializing GOP ####

    EFI_GUID gopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;

    Print(L"searching GOP...\r\n");

    Status = uefi_call_wrapper(BS->LocateProtocol, 3, &gopGuid, NULL, (VOID **)&gop);

    Print(L"LocateProtocol: %r\r\n", Status);

    Print(L"setting new gop so we got some nice res of 640x400\r\n");

    UINT32 bestMode = 0;
    UINT32 bestRes = 0xFFFFFFFF;

    // pick the lowest resolution mode available
    for (UINTN i = 0; i < gop->Mode->MaxMode; i++) {
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
        UINTN size;
        uefi_call_wrapper(gop->QueryMode, 4, gop, i, &size, &info);

        UINT32 res = info->HorizontalResolution * info->VerticalResolution;
        if (res < bestRes) {
            bestRes = res;
            bestMode = i;
        }
    }
    Print(L"bestMode: %d\r\n", bestMode);
    // switch to the selected mode
    Status = uefi_call_wrapper(gop->SetMode, 2, gop, bestMode);

    if(EFI_ERROR(Status)) {
        Print(L"Unable to set mode %d\r\n", Status);
        while (1) {}
    }

    Print(L"Framebuffer address %lx\r\n", gop->Mode->FrameBufferBase);
    Print(L"Framebuffer size %ld\r\n", gop->Mode->FrameBufferSize);
    Print(L"Width %d Height %d\r\n", gop->Mode->Info->HorizontalResolution, gop->Mode->Info->VerticalResolution);
    Print(L"PixelsPerScanLine %d\r\n", gop->Mode->Info->PixelsPerScanLine);
    Print(L"PixelFormat %d\r\n", gop->Mode->Info->PixelFormat);

    // stop if no linear framebuffer, we need it for direct pixel writes
    if (gop->Mode->Info->PixelFormat == PixelBltOnly) {
        Print(L"PixelBltOnly: no linear framebuffer on this machine, cannot boot\r\n");
        while (1) {}
    }

    //#### initializing kernel ####

    Print(L"BS: %lx\r\n", BS);

    // need LoadedImage to find which device we booted from
    Print(L"Loading device handle from bootloader:  ");
    EFI_LOADED_IMAGE *LoadedImage;
    EFI_GUID lipGuid = LOADED_IMAGE_PROTOCOL;
    Status = uefi_call_wrapper(BS->OpenProtocol, 6,
                               ImageHandle, &lipGuid, (VOID **)&LoadedImage,
                               ImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    if (EFI_ERROR(Status)) {
        Print(L"Error: %r\r\n", Status);
        while (1) {}
    }
    Print(L"ok\r\n");

    // get the filesystem protocol from the boot device
    Print(L"Getting file:  ");
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Fs;
    EFI_GUID fsGuid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    Status = uefi_call_wrapper(BS->HandleProtocol, 3, LoadedImage->DeviceHandle, &fsGuid, (VOID **)&Fs);
    if (EFI_ERROR(Status)) {
        Print(L"Error: %r\r\n", Status);
        while (1) {}
    }
    Print(L"ok\r\n");

    // open root volume then look for kernel.bin
    Print(L"open kernel.bin:  ");
    EFI_FILE_HANDLE Root, KernelFile;
    Status = uefi_call_wrapper(Fs->OpenVolume, 2, Fs, &Root);
    if (EFI_ERROR(Status)) {
        Print(L"Error opening root volume: %r\r\n", Status);
        while (1) {}
    }

    Status = uefi_call_wrapper(Root->Open, 5, Root, &KernelFile, L"kernel.bin", EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status)) {
        Print(L"Error: kernel.bin not found! %r\r\n", Status);
        while (1) {}
    }

    // query file size so we know how many pages to allocate
    Print(L"getting kernel.bin size:   ");
    EFI_FILE_INFO *FileInfo = LibFileInfo(KernelFile);
    UINTN KernelSize = FileInfo->FileSize;
    Print(L"Kernel size: %d bytes\r\n", KernelSize);
    FreePool(FileInfo);

    Print(L"reserve size for kernel.bin on the RAM at 0x200000:  ");
    EFI_PHYSICAL_ADDRESS KernelAddr = 0x200000;
    Status = uefi_call_wrapper(BS->AllocatePages, 4,
                               AllocateAddress, EfiLoaderData,
                               256, &KernelAddr);
    if (EFI_ERROR(Status)) {
        Print(L"AllocatePages failed: %r\r\n", Status);
        while (1) {}
    }
    Print(L"ok\r\n");

    // read kernel into memory and close the file handles
    Print(L"reading and closing kernel file:  ");
    uefi_call_wrapper(KernelFile->Read, 3,
                      KernelFile, &KernelSize, (VOID *)KernelAddr);
    uefi_call_wrapper(KernelFile->Close, 1, KernelFile);
    Print(L"ok\r\n");
    Print(L"Kernel loaded at 0x%lx\r\n", KernelAddr);

    // fill in bootinfo so the kernel knows where everything landed
    Print(L"creating bootinfo struct:   ");
    static BootInfo gBootInfo;
    gBootInfo.framebuffer_base = gop->Mode->FrameBufferBase;
    gBootInfo.width            = gop->Mode->Info->HorizontalResolution;
    gBootInfo.height           = gop->Mode->Info->VerticalResolution;
    gBootInfo.pitch            = gop->Mode->Info->PixelsPerScanLine;
    gBootInfo.pixel_format     = gop->Mode->Info->PixelFormat;
    gBootInfo.pixel_red_mask   = gop->Mode->Info->PixelInformation.RedMask;
    gBootInfo.pixel_green_mask = gop->Mode->Info->PixelInformation.GreenMask;
    gBootInfo.pixel_blue_mask  = gop->Mode->Info->PixelInformation.BlueMask;
    Print(L"ok\r\n");

    // Preload user binaries before fetching the memory map.
    CHAR16 *uefi_paths[]   = { L"bin\\sh", L"bin\\test", L"bin\\doom", L"doom1.wad" };
    char   *kernel_names[] = { "/bin/sh",  "/bin/test",  "/bin/doom",  "doom1.wad"  };
    UINTN   num_bins       = 4;

    gBootInfo.file_count = 0;
    for (UINTN fi = 0; fi < num_bins; fi++) {
        EFI_FILE_HANDLE BinFile;
        Status = uefi_call_wrapper(Root->Open, 5, Root, &BinFile,
                                   uefi_paths[fi], EFI_FILE_MODE_READ, 0);
        if (EFI_ERROR(Status)) {
            Print(L"[WARN] %s not found on ESP, skipping\r\n", uefi_paths[fi]);
            continue;
        }

        EFI_FILE_INFO *BinInfo = LibFileInfo(BinFile);
        UINTN BinSize = BinInfo->FileSize;
        FreePool(BinInfo);

        UINTN pages = (BinSize + 0xFFF) / 0x1000;
        EFI_PHYSICAL_ADDRESS BinAddr = 0;
        Status = uefi_call_wrapper(BS->AllocatePages, 4,
                                   AllocateAnyPages, EfiLoaderData, pages, &BinAddr);
        if (EFI_ERROR(Status)) {
            Print(L"[WARN] AllocatePages failed for %s\r\n", uefi_paths[fi]);
            uefi_call_wrapper(BinFile->Close, 1, BinFile);
            continue;
        }

        uefi_call_wrapper(BinFile->Read, 3, BinFile, &BinSize, (VOID *)BinAddr);
        uefi_call_wrapper(BinFile->Close, 1, BinFile);

        UINTN idx = gBootInfo.file_count;
        for (int k = 0; k < BOOT_MAX_FILENAME - 1 && kernel_names[fi][k]; k++)
            gBootInfo.files[idx].name[k] = kernel_names[fi][k];
        gBootInfo.files[idx].data = (UINT64)BinAddr;
        gBootInfo.files[idx].size = (UINT32)BinSize;
        gBootInfo.file_count++;
        Print(L"Loaded %s: %lu bytes at 0x%lx\r\n", uefi_paths[fi], BinSize, BinAddr);
    }

    uefi_call_wrapper(Root->Close, 1, Root);

    Print(L"Get Map memory:   ");
    UINTN MapSize = 0, MapKey, DescSize;
    UINT32 DescVer;
    EFI_MEMORY_DESCRIPTOR *MemMap = NULL;

    // query how large the memory map actually is
    uefi_call_wrapper(BS->GetMemoryMap, 5,
                      &MapSize, MemMap, &MapKey, &DescSize, &DescVer);

    Print(L"ok\r\n");

    // add padding for any descriptors the firmware may add on ExitBootServices
    MapSize += 8 * DescSize;
    uefi_call_wrapper(BS->AllocatePool, 3,
                      EfiLoaderData, MapSize, (VOID **)&MemMap);

    // get the actual memory map and the key we need for ExitBootServices
    Status = uefi_call_wrapper(BS->GetMemoryMap, 5,
                               &MapSize, MemMap, &MapKey, &DescSize, &DescVer);
    if (EFI_ERROR(Status)) {
        Print(L"GetMemoryMap failed: %r\r\n", Status);
        while (1) {}
    }

    gBootInfo.mmap_addr      = (uint64_t)MemMap;
    gBootInfo.mmap_size      = (uint64_t)MapSize;
    gBootInfo.mmap_desc_size = (uint64_t)DescSize;
    gBootInfo.mmap_desc_ver  = DescVer;

    Print(L"DescSize: %d bytes\r\n", DescSize);
    Print(L"%ld\n", &gBootInfo);
    Print(L"nothing we can do now... try to reboot maybe.\r\n");
    Print(L"ExitBootServices now:   ");
    Status = uefi_call_wrapper(BS->ExitBootServices, 2, ImageHandle, MapKey);
    if (EFI_ERROR(Status)) {
        uefi_call_wrapper(BS->GetMemoryMap, 5,
                          &MapSize, MemMap, &MapKey, &DescSize, &DescVer);
        Status = uefi_call_wrapper(BS->ExitBootServices, 2, ImageHandle, MapKey);
        if (EFI_ERROR(Status)) {
            // if we reach this point, it's time to give up, memory map is probably a mess and we can't trust anything anymore
            while (1) {}
        }
    }

    // Explicitly load both RCX and RDI with the
    // BootInfo pointer before jumping, so kernel_entry finds it
    // regardless of which register it checks first.
    __asm__ __volatile__ (
        "mov %0, %%rcx\n\t"
        "mov %0, %%rdi\n\t"
        "jmp *%1"
        : : "r"(&gBootInfo), "r"(KernelAddr) : "rcx", "rdi"
    );


    while (1) {}
    return EFI_SUCCESS;
}
