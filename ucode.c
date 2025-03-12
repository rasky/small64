extern uint8_t _binary_build_debug_rsp_u3d_text_bin_start[];
extern uint8_t _binary_build_debug_rsp_u3d_text_bin_end[];

/**
 * Loads the ucode onto the RSP making it ready to be run.
 * To start, use ucode_run()
 */
static inline void ucode_init()
{
  // DMAing too much doesn't matter, but we can skip any size calculation
  const uint32_t IMEM_SIZE = 0x1000-1;

  // shift values usually set by rspq in libdragon
  uint32_t shift_val = 0x08040201;
  SP_DMEM[0] = shift_val << 4;
  SP_DMEM[1] = shift_val;
  rsp_dma_from_rdram((void*)0x1000, _binary_build_debug_rsp_u3d_text_bin_start, IMEM_SIZE);
}

/**
 * Set start and end address of vertices to process
 * @param addr 
 */
static inline void ucode_set_vertices_address(uint32_t addr, uint32_t addrEnd) {
  ((volatile uint32_t*)SP_DMEM)[2] = addr;
  ((volatile uint32_t*)SP_DMEM)[3] = addrEnd;
}

/**
 * Sets new address where the ucode will write RDP commands to
 * @param addr
 */
static inline void ucode_set_rdp_queue(uint32_t addr) {
  ((volatile uint32_t*)SP_DMEM)[4] = addr;
}

static inline void ucode_set_srt(float scale, float rot[3]) {

  float cosR0 = mm_cosf(rot[0]);
  float cosR2 = mm_cosf(rot[2]);
  float cosR1 = mm_cosf(rot[1]);

  float sinR0 = mm_sinf(rot[0]);
  float sinR1 = mm_sinf(rot[1]);
  float sinR2 = mm_sinf(rot[2]);

  // @TODO: split this up into an extra scale vector
  // @TODO: after the above use this for lighting in the ucode
  float mat[3*3] = {
    scale * cosR2 * cosR1, 
    scale * (cosR2 * sinR1 * sinR0 - sinR2 * cosR0), 
    scale * (cosR2 * sinR1 * cosR0 + sinR2 * sinR0),
    scale * sinR2 * cosR1, 
    scale * (sinR2 * sinR1 * sinR0 + cosR2 * cosR0), 
    scale * (sinR2 * sinR1 * cosR0 - cosR2 * sinR0),
    -scale * sinR1, 
    scale * cosR1 * sinR0, 
    scale * cosR1 * cosR0
  };

  SP_DMEM[24/4 + 0] = ((int32_t)(mat[0] * 0x7FFF) << 16) | ((int32_t)(mat[1] * 0x7FFF) & 0xFFFF);
  SP_DMEM[24/4 + 1] = ((int32_t)(mat[2] * 0x7FFF) << 16);
  SP_DMEM[24/4 + 2] = ((int32_t)(mat[3] * 0x7FFF) << 16) | ((int32_t)(mat[4] * 0x7FFF) & 0xFFFF);
  SP_DMEM[24/4 + 3] = ((int32_t)(mat[5] * 0x7FFF) << 16);
  SP_DMEM[24/4 + 4] = ((int32_t)(mat[6] * 0x7FFF) << 16) | ((int32_t)(mat[7] * 0x7FFF) & 0xFFFF);
  SP_DMEM[24/4 + 5] = ((int32_t)(mat[8] * 0x7FFF) << 16);
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

static inline void ucode_sync()
{
  while(!(*SP_STATUS & SP_STATUS_HALTED)){}
}