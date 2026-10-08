PHDRS { text PT_LOAD FLAGS(5); rodata PT_LOAD FLAGS(4); data PT_LOAD FLAGS(6); dyn PT_DYNAMIC; }
SECTIONS {
  . = 0;
  .text : { *(.text.start) *(.text .text.*) *(.plt*) } :text
  . = ALIGN(0x1000);
  .rodata : { *(.rodata.mod0) *(.rodata .rodata.*) } :rodata
  .dynsym : { *(.dynsym) } :rodata
  .dynstr : { *(.dynstr) } :rodata
  .hash : { *(.hash) } :rodata
  .gnu.hash : { *(.gnu.hash) } :rodata
  .rela.dyn : { *(.rela.dyn) *(.rela.plt) } :rodata
  .eh_frame : { *(.eh_frame) } :rodata
  . = ALIGN(0x1000);
  .dynamic : { *(.dynamic) } :data :dyn
  .got : { *(.got) *(.got.plt) } :data
  .init_array : { KEEP(*(.init_array*)) } :data
  .fini_array : { KEEP(*(.fini_array*)) } :data
  .data : { *(.data .data.*) } :data
  __bss_start__ = .;
  .bss : { *(.bss .bss.*) *(COMMON) } :data
  . = ALIGN(8);
  __bss_end__ = .;
  /DISCARD/ : { *(.comment) *(.note*) *(.interp) }
}
