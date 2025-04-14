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
  // Loads data & code into DMEM/IMEM in one go
  // the data section is zero padded to allow for this
 for(int i=0; i<sizeof(rsp_data_code)/4; ++i) {
  SP_DMEM[i] = ((uint32_t*)(rsp_data_code))[i];
 }
}


static inline void ucode_set_displace(int factor)
{
  SP_DMEM[16/4] = factor;
}

__attribute__((noinline))
static uint32_t to_short(float f) {
  return (int32_t)(f * 0x7FFF) & 0xFFFF;
}

__attribute__((noinline))
static uint32_t to_short_upper(float f) {
  return (int32_t)(f * 0x7FFF) << 16;
}

static void ucode_set_srt(float scale, float rot[3], uint32_t posX, uint32_t posY)
{
  float cosR0 = mm_cosf(rot[0]);
  float cosR2 = mm_cosf(rot[2]);
  float cosR1 = mm_cosf(rot[1]);

  float sinR0 = mm_sinf(rot[0]);
  float sinR1 = mm_sinf(rot[1]);
  float sinR2 = mm_sinf(rot[2]);

  // @TODO: split this up into an extra scale vector
  // @TODO: after the above use this for lighting in the ucode

  // // @TODO: set scale
  uint32_t* DMEM_BASE = (uint32_t*)&SP_DMEM[24/4];

  DMEM_BASE[0] = to_short_upper(cosR2 * cosR1) | to_short_upper(cosR2 * sinR1 * sinR0 - sinR2 * cosR0) >> 16;
  DMEM_BASE[1] = to_short_upper(cosR2 * sinR1 * cosR0 + sinR2 * sinR0) | posX;
  DMEM_BASE[2] = to_short_upper(sinR2 * cosR1) | to_short_upper(sinR2 * sinR1 * sinR0 + cosR2 * cosR0) >> 16;
  DMEM_BASE[3] = to_short_upper(sinR2 * sinR1 * cosR0 - cosR2 * sinR0) | posY;
  DMEM_BASE[4] = to_short_upper(-sinR1) | to_short_upper(cosR1 * sinR0) >> 16;
  DMEM_BASE[5] = to_short_upper(cosR1 * cosR0);
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