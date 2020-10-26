/* SPDX-License-Identifier: BSD-2-Clause */

#include <tilck/common/page_size.h>

#include "defs.h"
#include "utils.h"

#define EFI_MBI_MAX_ADDR (64 * KB)

EFI_MEMORY_DESCRIPTOR mmap[512];
UINTN mmap_size;
UINTN desc_size;

multiboot_info_t *mbi;
multiboot_memory_map_t *multiboot_mmap;
UINT32 mmap_elems_count;

EFI_STATUS
AllocateMbi(void)
{
   EFI_STATUS status = EFI_SUCCESS;
   EFI_PHYSICAL_ADDRESS multiboot_buffer = EFI_MBI_MAX_ADDR;

   status = BS->AllocatePages(AllocateMaxAddress,
                              EfiLoaderData,
                              1,
                              &multiboot_buffer);
   HANDLE_EFI_ERROR("AllocatePages");

   BS->SetMem(TO_PTR(multiboot_buffer), 1 * PAGE_SIZE, 0);
   mbi = TO_PTR(multiboot_buffer);

end:
   return status;
}

void
MbiSetFramebufferInfo(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *mode_info,
                      UINTN fb_addr)
{
   mbi->flags |= MULTIBOOT_INFO_FRAMEBUFFER_INFO;
   mbi->framebuffer_addr = fb_addr;
   mbi->framebuffer_pitch =
      mode_info->PixelsPerScanLine * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL);
   mbi->framebuffer_width = mode_info->HorizontalResolution,
   mbi->framebuffer_height = mode_info->VerticalResolution,
   mbi->framebuffer_bpp = sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL) * 8;
   mbi->framebuffer_type = MULTIBOOT_FRAMEBUFFER_TYPE_RGB;

   if (mode_info->PixelFormat == PixelBlueGreenRedReserved8BitPerColor) {
      mbi->framebuffer_red_field_position = 16;
      mbi->framebuffer_green_field_position = 8;
      mbi->framebuffer_blue_field_position = 0;
   } else if (mode_info->PixelFormat == PixelRedGreenBlueReserved8BitPerColor) {
      mbi->framebuffer_red_field_position = 0;
      mbi->framebuffer_green_field_position = 8;
      mbi->framebuffer_blue_field_position = 16;
   }

   mbi->framebuffer_red_mask_size = 8;
   mbi->framebuffer_green_mask_size = 8;
   mbi->framebuffer_blue_mask_size = 8;
}

static UINT32 EfiToMultibootMemType(UINT32 type)
{
   switch (type) {

      case EfiReservedMemoryType:
      case EfiRuntimeServicesCode:
      case EfiRuntimeServicesData:
         return MULTIBOOT_MEMORY_RESERVED;

      case EfiLoaderCode:
      case EfiLoaderData:
      case EfiBootServicesCode:
      case EfiBootServicesData:
      case EfiConventionalMemory:
         return MULTIBOOT_MEMORY_AVAILABLE;

      case EfiUnusableMemory:
         return MULTIBOOT_MEMORY_BADRAM;

      case EfiACPIReclaimMemory:
         return MULTIBOOT_MEMORY_ACPI_RECLAIMABLE;

      case EfiACPIMemoryNVS:
         return MULTIBOOT_MEMORY_NVS;

      case EfiMemoryMappedIO:
      case EfiMemoryMappedIOPortSpace:
      case EfiPalCode:
         return MULTIBOOT_MEMORY_RESERVED;

      default:
         /* Let's just be conservative */
         return MULTIBOOT_MEMORY_BADRAM;
   }
}

static void AddMemoryRegion(UINT64 start, UINT64 end, UINT32 type)
{
   mbi->flags |= MULTIBOOT_INFO_MEMORY;

   if (type == MULTIBOOT_MEMORY_AVAILABLE) {
      if (start < mbi->mem_lower * KB)
         mbi->mem_lower = start / KB;

      if (end > mbi->mem_upper * KB)
         mbi->mem_upper = end / KB;
   }

   multiboot_mmap[mmap_elems_count++] = (multiboot_memory_map_t) {
      .size = sizeof(multiboot_memory_map_t) - sizeof(u32),
      .addr = (multiboot_uint64_t)start,
      .len = (multiboot_uint64_t)(end - start),
      .type = type,
   };
}

EFI_STATUS
MultibootSaveMemoryMap(UINTN *mapkey)
{
   EFI_MEMORY_DESCRIPTOR *desc = NULL;
   EFI_STATUS status = EFI_SUCCESS;
   UINT32 last_type = (UINT32) -1;
   UINT64 last_start = 0;
   UINT64 last_end = 0;

   EFI_PHYSICAL_ADDRESS multiboot_mmap_paddr = EFI_MBI_MAX_ADDR;

   status = BS->AllocatePages(AllocateMaxAddress,
                              EfiLoaderData,
                              1,
                              &multiboot_mmap_paddr);
   HANDLE_EFI_ERROR("AllocatePages");

   BS->SetMem(TO_PTR(multiboot_mmap_paddr), 1 * PAGE_SIZE, 0);
   multiboot_mmap = TO_PTR(multiboot_mmap_paddr);

   status = GetMemoryMap(mapkey);
   HANDLE_EFI_ERROR("GetMemoryMap");

   mbi->flags |= MULTIBOOT_INFO_MEM_MAP;
   desc = (void *)mmap;

   do {

      UINT32 type = EfiToMultibootMemType(desc->Type);
      UINT64 start = desc->PhysicalStart;
      UINT64 end = start + desc->NumberOfPages * PAGE_SIZE;

      if (last_type != type || last_end != start) {

         /*
          * The new region is not contiguous with the previous one OR it has
          * a different type.
          */

         if (last_type != (UINT32)-1) {
            AddMemoryRegion(last_start, last_end, last_type);
         }

         last_type = type;
         last_start = start;
      }

      /*
       * last_type == type && last_end == start
       *
       * We're continuing a region of the same "multiboot type", just move the
       * end forward.
       */
      last_end = end;

      desc = (void *)desc + desc_size;

   } while ((UINTN)desc < (UINTN)mmap + mmap_size);

   AddMemoryRegion(last_start, last_end, last_type);

   mbi->mmap_addr = (UINTN)multiboot_mmap;
   mbi->mmap_length = mmap_elems_count * sizeof(multiboot_memory_map_t);

end:
   return status;
}

EFI_STATUS
MbiSetRamdisk(void)
{
   EFI_STATUS status = EFI_SUCCESS;
   EFI_PHYSICAL_ADDRESS multiboot_mod_addr = EFI_MBI_MAX_ADDR;
   multiboot_module_t *mod;

   status = BS->AllocatePages(AllocateMaxAddress,
                              EfiLoaderData,
                              1,
                              &multiboot_mod_addr);
   HANDLE_EFI_ERROR("AllocatePages");

   BS->SetMem(TO_PTR(multiboot_mod_addr), 1 * PAGE_SIZE, 0);

   mod = TO_PTR(multiboot_mod_addr);
   mod->mod_start = ramdisk_paddr;
   mod->mod_end = mod->mod_start + ramdisk_size;

   mbi->flags |= MULTIBOOT_INFO_MODS;
   mbi->mods_addr = (UINTN)mod;
   mbi->mods_count = 1;

end:
   return status;
}

EFI_STATUS
MbiSetBootloaderName(void)
{
   static char BootloaderName[] = "TILCK_EFI";

   EFI_STATUS status = EFI_SUCCESS;
   EFI_PHYSICAL_ADDRESS paddr = EFI_MBI_MAX_ADDR;

   status = BS->AllocatePages(AllocateMaxAddress,
                              EfiLoaderData,
                              1,
                              &paddr);
   HANDLE_EFI_ERROR("AllocatePages");

   BS->CopyMem(TO_PTR(paddr), BootloaderName, sizeof(BootloaderName));
   mbi->boot_loader_name = (u32)paddr;
   mbi->flags |= MULTIBOOT_INFO_BOOT_LOADER_NAME;

end:
   return status;
}

EFI_STATUS
MbiSetPointerToAcpiTable(void)
{
   static EFI_GUID gAcpi20Table = ACPI_20_TABLE_GUID;
   EFI_PHYSICAL_ADDRESS tablePaddr;
   void *table = NULL;

   for (UINTN i = 0; i < ST->NumberOfTableEntries; i++) {

      EFI_GUID *g = &ST->ConfigurationTable[i].VendorGuid;

      if (!CompareGuid(g, &gAcpi20Table)) {
         table = ST->ConfigurationTable[i].VendorTable;
         break;
      }
   }

   if (!table) {
      Print(L"ERROR: ACPI 2.0 configuration table not found\n");
      return EFI_NOT_FOUND;
   }

   tablePaddr = (EFI_PHYSICAL_ADDRESS)(ulong)table;

   if (tablePaddr >= UINT32_MAX) {

      Print(L"Warning: ACPI 2.0 RDSP (0x%08x) out of 32-bit space\n",
            tablePaddr);

      return EFI_SUCCESS;
   }

   /*
    * HACK: we're setting ACPI 2.0's RDSP to the `apm_table` field in
    * multiboot's MBI struct. That's wrong to do (in general), but
    * unfortunately multiboot 1 does not support ACPI. Until we make at
    * least this EFI bootloader and the kernel to support multiboot 2.0,
    * this hack will be used. Note: the "hack" is not an unsafe or dirty
    * hack simply because:
    *
    *       - We're booting ONLY the Tilck kernel
    *       - We didn't set MULTIBOOT_INFO_APM_TABLE in mbi->flags
    *       - We set the mbi->boot_load_name to "TILCK_EFI" which allows the
    *         kernel to recognize this particular bootloader.
    *
    * Of course, if Tilck is booted by GRUB in EFI mode, it won't get this
    * precious pointer and it will have to resort searching the root pointer
    * with AcpiFindRootPointer() which is unreliable on UEFI systems.
    * As a result, ACPI might be unable to find its root pointer and the whole
    * ACPICA won't be usable. This limitation will be removed when Tilck gets
    * support for multiboot 2.0 as well.
    *
    * History note: why Tilck does not support multiboot 2.0 from the beginning?
    * Because QEMU doesn't and it was so simply to just support only multiboot1
    * everywhere. Supporting only multiboot2 was not an option because would
    * require always booting Tilck with its bootloader under QEMU: that's slower
    * for tests and limiting for debugging purposes.
    */
   mbi->apm_table = (u32)tablePaddr;
   return EFI_SUCCESS;
}
