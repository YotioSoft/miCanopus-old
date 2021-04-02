#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/PrintLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/DiskIo2.h>
#include <Protocol/BlockIo.h>
#include <Guid/FileInfo.h>

// gBS: ブートサービス（グローバル変数）
// gRT: ランタイムサービス（グローバル変数）

// メモリマップの構造体
struct MemoryMap {
  UINTN buffer_size;
  VOID* buffer;
  UINTN map_size;
  UINTN map_key;
  UINTN descriptor_size;
  UINT32 descriptor_version;
};

// メモリマップを取得
EFI_STATUS GetMemoryMap(struct MemoryMap* map) {
  if (map->buffer == NULL) {
    return EFI_BUFFER_TOO_SMALL;
  }

  map->map_size = map->buffer_size;
  return gBS->GetMemoryMap(&map->map_size, (EFI_MEMORY_DESCRIPTOR*)map->buffer, &map->map_key, &map->descriptor_size, &map->descriptor_version);

  // gBS->GetMemoryMap ->p.57
  // 1: MemoryMapSize: IN:メモリマップ書き込み用のメモリ領域の大きさ（バイト数） OUT:実際のメモリマップの大きさ
  // 2: MemoryMap: メモリマップ書き込み用のメモリ領域の先頭ポインタ
  // 3: MapKey: メモリマップを識別するための値を書き込む変数(この値はgBS->ExitBootServicesを呼び出すときに必要)
  // 4: DescriptorSize: メモリマップのここの行を表すメモリディスクリプタのバイト数
  // 5: DescriptorVersion: メモリディスクリプタの構造体のバージョン番号（使わない）
}

// メモリのタイプを取得
const CHAR16* GetMemoryTypeUnicode(EFI_MEMORY_TYPE type) {
  switch (type) {
    case EfiReservedMemoryType:
      return L"EfiReservedMemoryType";
    case EfiLoaderCode:
      return L"EfiLoaderCode";
    case EfiLoaderData:
      return L"EfiLoaderData";
    case EfiBootServicesCode:
      return L"EfiBootServicesCode";
    case EfiBootServicesData:
      return L"EfiBootServicesData";
    case EfiRuntimeServicesCode:
      return L"EfiRuntimeServicesCode";
    case EfiRuntimeServicesData:
      return L"EfiRuntimeServicesData";
    case EfiConventionalMemory:
      return L"EfiConventionalMemory";
    case EfiUnusableMemory:
      return L"EfiUnusableMemory";
    case EfiACPIReclaimMemory:
      return L"EfiACPIReclaimMemory";
    case EfiACPIMemoryNVS:
      return L"EfiACPIMemoryNVS";
    case EfiMemoryMappedIO:
      return L"EfiMemoryMappedIO";
    case EfiMemoryMappedIOPortSpace:
      return L"EfiMemoryMappedIOPortSpace";
    case EfiPalCode:
      return L"EfiPalCode";
    case EfiPersistentMemory:
      return L"EfiPersistentMemory";
    case EfiMaxMemoryType:
      return L"EfiMaxMemoryType";
    default:
      return L"InvalidMemoryType";
  }
}

// メモリマップの保存
// 引数で与えられたメモリマップをCSV形式で書き出す
EFI_STATUS SaveMemoryMap(struct MemoryMap* map, EFI_FILE_PROTOCOL* file) {
  CHAR8 buf[256];
  UINTN len;

  CHAR8* header = "Index, Type, Type(name), PhysicalStart, NumberOfPages, Attribute\n";
  len = AsciiStrLen(header);
  file->Write(file, &len, header);

  Print(L"map->buffer = %08lx, map->map_size = %08lx\n", map->buffer, map->map_size);

  EFI_PHYSICAL_ADDRESS iter;
  int i;
  for (iter = (EFI_PHYSICAL_ADDRESS)map->buffer, i=0; iter < (EFI_PHYSICAL_ADDRESS)map->buffer + map->map_size; iter += map->descriptor_size, i++) {
    // 整数からポインタへ
    EFI_MEMORY_DESCRIPTOR* desc = (EFI_MEMORY_DESCRIPTOR*)iter;
    // メモリディスクリプタの値を文字列に変換
    len = AsciiSPrint(buf, sizeof(buf), "%u, %x, %-ls, %08lx, %lx, %lx\n",
                    i, desc->Type, GetMemoryTypeUnicode(desc->Type), desc->PhysicalStart, desc->NumberOfPages, desc->Attribute & 0xffffflu);
    file->Write(file, &len, buf);
  }

  return EFI_SUCCESS;
}

// ルートディレクトリを開く
EFI_STATUS OpenRootDir(EFI_HANDLE image_handle, EFI_FILE_PROTOCOL** root) {
  EFI_LOADED_IMAGE_PROTOCOL* loaded_image;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* fs;

  gBS->OpenProtocol(image_handle, &gEfiLoadedImageProtocolGuid, (VOID**)&loaded_image, image_handle, NULL, EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);

  gBS->OpenProtocol(loaded_image->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (VOID**)&fs, image_handle, NULL, EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);

  fs->OpenVolume(fs, root);

  return EFI_SUCCESS;
}

// GOPを開く
EFI_STATUS OpenGOP(EFI_HANDLE image_handle, EFI_GRAPHICS_OUTPUT_PROTOCOL** gop) {
  UINTN num_gop_handles = 0;
  EFI_HANDLE* gop_handles = NULL;
  gBS->LocateHandleBuffer(ByProtocol, &gEfiGraphicsOutputProtocolGuid, NULL, &num_gop_handles, &gop_handles);
  gBS->OpenProtocol(gop_handles[0], &gEfiGraphicsOutputProtocolGuid, (VOID**)gop, image_handle, NULL, EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);

  FreePool(gop_handles);

  return EFI_SUCCESS;
}

const CHAR16* GetPixelFormatUnicode(EFI_GRAPHICS_PIXEL_FORMAT fmt) {
  switch (fmt) {
    case PixelRedGreenBlueReserved8BitPerColor:
      return L"PixelRedGreenBlueReserved8BitPerColor";
    case PixelBlueGreenRedReserved8BitPerColor:
      return L"PixelBlueGreenRedReserved8BitPerColor";
    case PixelBitMask:
      return L"PixelBitMask";
    case PixelBltOnly:
      return L"PixelBltOnly";
    case PixelFormatMax:
      return L"PixelFormatMax";
    default:
      return L"InvalidPixelFormat";
  }
}

EFI_STATUS EFIAPI UefiMain (EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *system_table) {
  Print(L"Hello, Mikan World!\n");

  CHAR8 memmap_buf[4096*4]; // メモリマップは16KB
  struct MemoryMap memmap = {sizeof(memmap_buf), memmap_buf, 0, 0, 0, 0};
  GetMemoryMap(&memmap);

  // ルートディレクトリを開く
  EFI_FILE_PROTOCOL* root_dir;
  OpenRootDir(image_handle, &root_dir);

  // memmapを開く
  EFI_FILE_PROTOCOL* memmap_file;
  root_dir->Open(root_dir, &memmap_file, L"\\memmap", EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);

  // memmapに取得したメモリマップを保存
  SaveMemoryMap(&memmap, memmap_file);
  memmap_file->Close(memmap_file);

  // GOPを取得して画面描画する
  EFI_GRAPHICS_OUTPUT_PROTOCOL* gop;
  OpenGOP(image_handle, &gop);
  Print(L"Resolution: %ux%u, Pixel Format: %s, %u pixels/line\n",
    gop->Mode->FrameBufferBase, gop->Mode->FrameBufferBase + gop->Mode->FrameBufferSize, gop->Mode->FrameBufferSize);

  UINT8* frame_buffer = (UINT8*)gop->Mode->FrameBufferBase;
  for (UINTN i = 0; i < gop->Mode->FrameBufferSize; i++) {
    frame_buffer[i] = 255;
  }

  // カーネル(kernel/main.cpp)の読み込み
  Print(L"Loading kernel.elf ");

  EFI_FILE_PROTOCOL* kernel_file;
  root_dir->Open(root_dir, &kernel_file, L"\\kernel.elf", EFI_FILE_MODE_READ, 0);

  UINTN file_info_size = sizeof(EFI_FILE_INFO) + sizeof(CHAR16) * 12;
  UINT8 file_info_buffer[file_info_size];
  kernel_file->GetInfo(kernel_file, &gEfiFileInfoGuid, &file_info_size, file_info_buffer);

  EFI_FILE_INFO* file_info = (EFI_FILE_INFO*)file_info_buffer;
  UINTN kernel_file_size = file_info->FileSize;

  EFI_PHYSICAL_ADDRESS kernel_base_addr = 0x100000;   // カーネルファイルは0x100000に配置
  gBS->AllocatePages(AllocateAddress, EfiLoaderData, (kernel_file_size + 0xfff) / 0x1000, &kernel_base_addr);
  kernel_file->Read(kernel_file, &kernel_file_size, (VOID*)kernel_base_addr);
  Print(L"-> Done.\n");
  Print(L"Kernel: 0x%0lx (%lu bytes)\n", kernel_base_addr, kernel_file_size);

  // UEFI BIOSのブートサービスを停止
  EFI_STATUS status;
  status = gBS->ExitBootServices(image_handle, memmap.map_key);
  if (EFI_ERROR(status)) {
    status = GetMemoryMap(&memmap);
    if (EFI_ERROR(status)) {
      Print(L"[ERR B1 Lv.1]failed to get memory map: %r\n", status);
      while(1);
    }
    status = gBS->ExitBootServices(image_handle, memmap.map_key);
    if (EFI_ERROR(status)) {
      Print(L"[ERR B2 Lv.3]Could not exit boot service: %r\n", status);
      while(1);
    }
  }

  // カーネルの起動
  UINT64 entry_addr = *(UINT64*)(kernel_base_addr + 24);

  typedef void EntryPointType(UINT64, UINT64);
  EntryPointType* entry_point = (EntryPointType*)entry_addr;
  entry_point(gop->Mode->FrameBufferBase, gop->Mode->FrameBufferSize);

  // All done!
  Print(L"All done\n");

  while(1);
  return EFI_SUCCESS;
}
