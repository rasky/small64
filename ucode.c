#ifdef DEBUG
  #include "build-debug/rsp_u3d.inc"
#else
  #include "build/rsp_u3d.inc"
#endif

/**
 * Loads the ucode onto the RSP making it ready to be run.
 * To start, use ucode_run()
 */
static inline void ucode_init()
{
  // shift values usually set by rspq in libdragon
  uint32_t shift_val = 0x08040201;
  SP_DMEM[0] = shift_val << 4;
  SP_DMEM[1] = shift_val;

  // note: rsp_code_len is in bytes and padded
 for(int i=0; i<rsp_code_len/4; ++i) {
  SP_IMEM[i] = ((uint32_t*)(rsp_code))[i];
 }
}

/**
 * Set start and end address of vertices to process
 * @param addr 
 */
static inline void ucode_set_vertices_address(uint32_t addr, uint32_t addrEnd) {
  ((volatile uint32_t*)SP_DMEM)[2] = addr;
  ((volatile uint32_t*)SP_DMEM)[3] = addrEnd;
}

static inline void ucode_set_displace(int factor) 
{
  SP_DMEM[16/4] = factor;
}

static inline void ucode_set_srt(float scale, float rot[3], uint32_t posX, uint32_t posY) 
{
  float cosR0 = mm_cosf(rot[0]);
  float cosR2 = mm_cosf(rot[2]);
  float cosR1 = mm_cosf(rot[1]);

  float sinR0 = mm_sinf(rot[0]);
  float sinR1 = mm_sinf(rot[1]);
  float sinR2 = mm_sinf(rot[2]);

  // @TODO: split this up into an extra scale vector
  // @TODO: after the above use this for lighting in the ucode
  float mat[3*3] = {
    cosR2 * cosR1, 
    (cosR2 * sinR1 * sinR0 - sinR2 * cosR0), 
    (cosR2 * sinR1 * cosR0 + sinR2 * sinR0),
    sinR2 * cosR1, 
    (sinR2 * sinR1 * sinR0 + cosR2 * cosR0), 
    (sinR2 * sinR1 * cosR0 - cosR2 * sinR0),
    -sinR1, 
    cosR1 * sinR0, 
    cosR1 * cosR0
  };

  // @TODO: set scale 
  uint32_t* DMEM_BASE = (uint32_t*)&SP_DMEM[24/4];
  DMEM_BASE[0] = ((int32_t)(mat[0] * 0x7FFF) << 16) | ((int32_t)(mat[1] * 0x7FFF) & 0xFFFF);
  DMEM_BASE[1] = ((int32_t)(mat[2] * 0x7FFF) << 16) | posX;
  DMEM_BASE[2] = ((int32_t)(mat[3] * 0x7FFF) << 16) | ((int32_t)(mat[4] * 0x7FFF) & 0xFFFF);
  DMEM_BASE[3] = ((int32_t)(mat[5] * 0x7FFF) << 16) | posY;
  DMEM_BASE[4] = ((int32_t)(mat[6] * 0x7FFF) << 16) | ((int32_t)(mat[7] * 0x7FFF) & 0xFFFF);
  DMEM_BASE[5] = ((int32_t)(mat[8] * 0x7FFF) << 16);
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