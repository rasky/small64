extern uint8_t _binary_build_debug_rsp_u3d_text_bin_start[];
extern uint8_t _binary_build_debug_rsp_u3d_text_bin_end[];

/**
 * Loads the ucode onto the RSP making it ready to be run.
 * To start, use ucode_run()
 */
static inline void ucode_init()
{
  // DMAing too much doesn't matter, but we can skip any size calculation
  const uint32_t IMEM_SIZE = 1024-1;

  // shift values usually set by rspq in libdragon
  uint32_t shift_val = 0x08040201;
  SP_DMEM[0] = shift_val << 4;
  SP_DMEM[1] = shift_val;
  rsp_dma_from_rdram((void*)0x1000, _binary_build_debug_rsp_u3d_text_bin_start, IMEM_SIZE);
}

/**
 * Set address of vertices to process
 * @param addr 
 */
static inline void ucode_set_vertices_address(uint32_t addr) {
  ((volatile uint32_t*)SP_DMEM)[2] = addr;
}

/**
 * Sets new address where the ucode will write RDP commands to
 * @param addr start of queue
 */
static inline void ucode_set_rdp_queue(uint32_t addr) {
  ((volatile uint32_t*)SP_DMEM)[3] = addr;
}

/**
 * Starts the ucode, it will stop itself once finished.
 * Parameters can be set before, the RSP must be halted before doing so.
 */
static inline void ucode_run()
{
  *SP_PC = 0;
  MEMORY_BARRIER();
  *SP_STATUS = SP_WSTATUS_CLEAR_HALT | SP_WSTATUS_CLEAR_BROKE | SP_WSTATUS_SET_INTR_BREAK;
}

/**
 * Wait for the RSP to be halted.
 * After this parameters can be changed.
 */
static inline void ucode_sync()
{
  while(!(*SP_STATUS & SP_STATUS_HALTED)){}
}