#include "simeng/Elf.hh"

#include <cstring>
#include <fstream>

namespace simeng {

/**
 * Extract information from an ELF binary.
 * 32-bit and 64-bit architectures have variance in the structs
 * used to define the structure of an ELF binary. All information
 * presented as documentation has been referenced from:
 * https://man7.org/linux/man-pages/man5/elf.5.html
 */

Elf::Elf(std::string path, char** imagePointer) {
  std::ifstream file(path, std::ios::binary);

  if (!file.is_open()) {
    return;
  }

  /**
   * In the Linux source tree the ELF header
   * is defined by the elf64_hdr struct for 64-bit systems.
   * `elf64_hdr->e_ident` is an array of bytes which specifies
   * how to interpret the ELF file, independent of the
   * processor or the file's remaining contents. All ELF
   * files start with the ELF header.
   */

  /**
   * First four bytes of the ELF header represent the ELF Magic Number.
   */
  char elfMagic[4] = {0x7f, 'E', 'L', 'F'};
  char fileMagic[4];
  file.read(fileMagic, 4);
  if (std::memcmp(elfMagic, fileMagic, sizeof(elfMagic))) {
    return;
  }

  /**
   * The fifth byte of the ELF Header identifies the architecture
   * of the ELF binary i.e 32-bit or 64-bit.
   */

  // Check whether this is a 32 or 64-bit executable
  char bitFormat;
  file.read(&bitFormat, sizeof(bitFormat));
  if (bitFormat != ElfBitFormat::Format64) {
    return;
  }

  isValid_ = true;

  /**
   * Starting from the 24th byte of the ELF header a 64-bit value
   * represents the virtual address to which the system first transfers
   * control, thus starting the process.
   * In `elf64_hdr` this value maps to the member `Elf64_Addr e_entry`.
   */

  // Seek to the entry point of the file.
  // The information in between is discarded
  file.seekg(0x18);
  file.read(reinterpret_cast<char*>(&entryPoint_), sizeof(entryPoint_));

  /**
   * Starting from the 32nd byte of the ELF Header a 64-bit value
   * represents the offset of the ELF Program header or
   * Program header table in the ELF file.
   * In `elf64_hdr` this value maps to the member `Elf64_Addr e_phoff`.
   */

  // Seek to the byte representing the start of the header offset table.
  uint64_t headerOffset;
  file.read(reinterpret_cast<char*>(&headerOffset), sizeof(headerOffset));

  /**
   * Starting 54th byte of the ELF Header a 16-bit value indicates
   * the size of each entry in the ELF Program header. In the `elf64_hdr`
   * struct this value maps to the member `Elf64_Half e_phentsize`. All
   * header entries have the same size.
   * Starting from the 56th byte a 16-bit value represents the number
   * of header entries in the ELF Program header. In the `elf64_hdr`
   * struct this value maps to `Elf64_Half e_phnum`.
   */

  // Seek to the byte representing header entry size.
  file.seekg(0x36);
  uint16_t headerEntrySize;
  file.read(reinterpret_cast<char*>(&headerEntrySize), sizeof(headerEntrySize));
  uint16_t headerEntries;
  file.read(reinterpret_cast<char*>(&headerEntries), sizeof(headerEntries));

  // Resize the header to equal the number of header entries.
  headers_.resize(headerEntries);
  processImageSize_ = 0;

  // Loop over all headers and extract them.
  for (size_t i = 0; i < headerEntries; i++) {
    // Since all headers entries have the same size.
    // We can extract the nth header using the header offset
    // and header entry size.
    file.seekg(headerOffset + (i * headerEntrySize));
    auto& header = headers_[i];

    /**
     * Like the ELF Header, the ELF Program header is also defined
     * using a struct:
     * typedef struct {
     *    uint32_t   p_type;
     *    uint32_t   p_flags;
     *    Elf64_Off  p_offset;
     *    Elf64_Addr p_vaddr;
     *    Elf64_Addr p_paddr;
     *    uint64_t   p_filesz;
     *    uint64_t   p_memsz;
     *    uint64_t   p_align;
     *  } Elf64_Phdr;
     *
     * The ELF Program header table is an array of structures,
     * each describing a segment or other information the system
     * needs to prepare the program for execution. A segment
     * contains one or more sections (ELF Program Section).
     *
     * The `p_vaddr` field holds the virtual address at which the first
     * byte of the segment resides in memory and the `p_memsz` field
     * holds the number of bytes in the memory image of the segment.
     * It may be zero. The `p_offset` member holds the offset from the
     * beginning of the file at which the first byte of the segment resides.
     */

    // Each address-related field is 8 bytes in a 64-bit ELF file
    const int fieldBytes = 8;
    file.read(reinterpret_cast<char*>(&(header.type)), sizeof(header.type));
    file.seekg(4, std::ios::cur);  // Skip flags
    file.read(reinterpret_cast<char*>(&(header.offset)), fieldBytes);
    file.read(reinterpret_cast<char*>(&(header.virtualAddress)), fieldBytes);
    file.read(reinterpret_cast<char*>(&(header.physicalAddress)), fieldBytes);
    file.read(reinterpret_cast<char*>(&(header.fileSize)), fieldBytes);
    file.read(reinterpret_cast<char*>(&(header.memorySize)), fieldBytes);

    // To construct the process we look for the largest virtual address and
    // add it to the memory size of the header. This way we obtain a very
    // large array which can hold data at large virtual address.
    // However, this way we end up creating a sparse array, in which most
    // of the entries are unused. Also SimEng internally treats these
    // virtual address as physical addresses to index into this large array.
    if (header.virtualAddress + header.memorySize > processImageSize_) {
      processImageSize_ = header.virtualAddress + header.memorySize;
    }
  }

  *imagePointer = (char*)malloc(processImageSize_ * sizeof(char));
  /**
   * The ELF Program header has a member called `p_type`, which represents
   * the kind of data or memory segments described by the program header.
   * The value PT_LOAD=1 represents a loadable segment. In other words,
   * it contains initialized data that contributes to the program's
   * memory image.
   */

  // Process headers; only observe LOAD sections for this basic implementation
  for (const auto& header : headers_) {
    if (header.type == 1) {  // LOAD
      file.seekg(header.offset);
      // Read `fileSize` bytes from `file` into the appropriate place in process
      // memory
      file.read(*imagePointer + header.virtualAddress, header.fileSize);
    }
  }

  file.close();
  return;
}

Elf::~Elf() {}

uint64_t Elf::getProcessImageSize() const { return processImageSize_; }

uint64_t Elf::getEntryPoint() const { return entryPoint_; }

bool Elf::isValid() const { return isValid_; }

}  // namespace simeng
