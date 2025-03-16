# Glue-code to make this work without libdragon:

#define vzero $v00
#define vshift $v30
#define vshift8 $v31
#define K1       vshift.e7

setup_vsll      vshift, 0
setup_vsll8    vshift8, 0

#define K1       vshift.e7
#define K2       vshift.e6
#define K4       vshift.e5
#define K8       vshift.e4
#define K16      vshift.e3
#define K32      vshift.e2
#define K64      vshift.e1
#define K128     vshift.e0
#define K256     vshift8.e7
#define K512     vshift8.e6
#define K1024    vshift8.e5
#define K2048    vshift8.e4
#define K4096    vshift8.e3
#define K8192    vshift8.e2
#define K16384   vshift8.e1
#define K32768   vshift8.e0
#define KM32768  vshift8.e0    // note: this is actually -32768 for most arithmetic operations

#########################################################################
#
#  RDPQ_Triangle: assemble a RDP triangle command
#
##########################################################################

  #####################################################################
    # RDPQ_Triangle
    #
    # INPUT:
    # * a0: high 32-bit word of the triangle command. This will be
    #       completed with the left/right flag and the mipmap level.
    # * a1,a2,a3: pointer to the triangle structures in DMEM
    # * v0: 0=cull front, 1=cull back, any other value = culling disabled
    # * s3: output buffer pointer
    #####################################################################

    # Implementation limits of the RSP version. These are all edge cases that are probably
    # not necessary to get 100% right as they are really degenerate situations. Notice that
    # most extreme/degenerated/saturated cases are still handled correctly, as verified
    # by the fuzzing performed by test_rdpq_triangle; these are just the three leftovers.
    #
    # * Texture coordinates are accepted in s10.5 format, but a subtraction between two of them
    #   must not overflow a 16-bit number. This is a limit of the attribute calculation where the
    #   edges MA/HA are calculated with 16-bit numbers. It looks like it's not a real problem as
    #   it would mean having a triangle where either S or T spans more than 1024 texels within it.
    #   Fixing it wuold require changing MA/HA into 32-bit numbers, which has other annoying fallouts.
    # * In case of quasi-degenerate triangles (lines), when the Y difference between two vertices
    #   is just 0.25 (0x00000001), the correct normal reciprocal would be 1.0, but it is calculated
    #   as 0x7FFF8000 which is 0.5 (because it's basically saturating s15.16). This means that the calculated
    #   edge is twice as big. Again, it doesn't matter as it can't really be seen within a 0.25 slope.
    #   test_rdpq_triangle has a triangle that triggers this, commented out.
    # * In some cases, Z/W-related derivates (DwDx, DwDy, DzDx, DzDy) can saturate during calculation.
    #   in this case, the dependent D*De derivates will be wrong (how much it will depend on how far
    #   the real result is from the saturated number). In any case, much better than an overflow.
    #   test_rdpq_triangle checks if there's a saturation and skip checks for known-wrong values.

    #define tricmd a0
    #define vtx1   a1
    #define vtx2   a2
    #define vtx3   a3
    #define cull   v0

    #define yA     t4
    #define yB     t5
    #              t6
    #define x1     t7
    #define x2     t8
    #define x3     v0

    # r, g, b, a, s, t, w, z
    #define vfinal_i         $v01
    #define vfinal_f         $v02
    #define vdx_i            $v03
    #define vdx_f            $v04
    #define vde_i            $v05
    #define vde_f            $v06
    #define vdy_i            $v07
    #define vdy_f            $v08

    #define vattr1           $v09
    #define vattr2           $v10
    #define vattr3           $v11
    #define vma              $v12
    #define vha              $v13

    #define vinvw_i          $v14
    #define vinvw_f          $v15

    #define vedges_i         $v16
    #define vedges_f         $v17
    #define vnz_i            $v18
    #define vnz_f            $v19
    #define vslope_i         $v20
    #define vslope_f         $v21
    #define vxy32            $v22
    #define vxy21            $v23
    #define vhml             $v24
    #define vfy_i            $v25
    #define vfy_f            $v26

    #define vtmp             $v28
    #define v__              $v29
    #define invn_i           $v31.e4
    #define invn_f           $v31.e5
    #define invsh_i          $v31.e6
    #define invsh_f          $v31.e7
 
    #define VTX_ATTR_X      0
    #define VTX_ATTR_Y      2
    #define VTX_ATTR_Z      4
    #define VTX_ATTR_RGBA   8
    #define VTX_ATTR_S      12
    #define VTX_ATTR_T      14
    #define VTX_ATTR_W      16
    #define VTX_ATTR_INVWi  20
    #define VTX_ATTR_INVWf  22

    lh yA, VTX_ATTR_Y(vtx1)
    lh yB, VTX_ATTR_Y(vtx2)
    blt yA, yB, 1f
    move t1, vtx1
    move vtx1, vtx2
    move vtx2, t1
    xor cull, 1
1:
    lh yA, VTX_ATTR_Y(vtx2)
    lh yB, VTX_ATTR_Y(vtx3)
    blt yA, yB, 1f
    move t1, vtx2
    move vtx2, vtx3
    move vtx3, t1
    xor cull, 1
1:
    lh yA, VTX_ATTR_Y(vtx1)
    lh yB, VTX_ATTR_Y(vtx2)
    blt yA, yB, 1f
    move t1, vtx1
    move vtx1, vtx2
    move vtx2, t1
    xor cull, 1
1:

    # We want to build this layout
    #  vxy32 = X3 X2 X3 --    Y3 Y2 Y3 --
    #  vxy21 = X1 -- X2 --    Y1 -- Y2 --

    lsv vxy32.e0, VTX_ATTR_X,vtx3
    lsv vxy32.e4, VTX_ATTR_Y,vtx3
    vor vxy32, vzero, vxy32.h0
    lsv vxy32.e1, VTX_ATTR_X,vtx2
    lsv vxy32.e5, VTX_ATTR_Y,vtx2

    lsv vxy21.e0, VTX_ATTR_X,vtx1
    lsv vxy21.e2, VTX_ATTR_X,vtx2
    lsv vxy21.e4, VTX_ATTR_Y,vtx1
    lsv vxy21.e6, VTX_ATTR_Y,vtx2

    # Store Y values in output
    ssv vxy21.e4, 6,s3    # y1
    ssv vxy32.e5, 4,s3    # y2
    ssv vxy32.e4, 2,s3    # y3

    # Now calculate:
    #  vxy32    = X3 X2 X3 --    Y3 Y2 Y3 --
    #    -
    #  vxy21.0q = X1 X1 X2 X2    Y1 Y1 Y2 Y2
    #    = 
    #  vhml     = HX MX LX --    HY MY LY --
    vsubc vhml, vxy32, vxy21.q0
    #define hx  vhml.e0
    #define mx  vhml.e1
    #define lx  vhml.e2
    #define my1 vhml.e3
    #define hy  vhml.e4
    #define my  vhml.e5
    #define ly  vhml.e6
    #define mx1 vhml.e7

    # Duplicate MX and MY into the two empty lanes.
    #  vhml     = HX MX LX MY    HY MY LY MX
    vmov mx1, mx
    vmov my1, my

    # Calculate normal: compute 32-bit cross product:
    #
    #  vhml     = HX MX LX MY    HY MY LY MX
    #    *
    #  vhml.3h  = MY MY MY MY    MX MX MX MX
    #    =
    #  nz       = HX*MY -- -- --    HY*MX -- -- -- --
    vmudh vnz_f, vhml, vhml.h3
    vsar vnz_i, COP2_ACC_HI
    vsar vnz_f, COP2_ACC_MD

    # Compute HY*MX - HX*MY. Result in e4.
    vsubc vnz_f, vnz_f.e0
    vsub  vnz_i, vnz_i.e0

    # Extract left flag from the sign of NZ.
    # Since we calculated -NZ, we need to reverse the sign
    mfc2 t0, vnz_i.e4
    sge t0, t0, zero
    beq t0, cull, RDPQ_Triangle_Skip
    sll t0, 7
    or tricmd, t0, TRI_COMMAND

    # Calculate reciprocal of normal
    vrcph vnz_i.e0, vnz_i.e4
    vrcpl vnz_f.e0, vnz_f.e4
    vrcph vnz_i.e0, v__.e0
    #define inz_f   vnz_f.e0
    #define inz_i   vnz_i.e0

    # Compute SLOPE vector
    # slope    =  -- -- -- --    1/HY 1/MY 1/LY 1/NZ

    # Compute ISL (L slope). 1/LY  (s14.1)
    vrcp  vslope_f.e6, vhml.e6
    vrcph vslope_i.e6, vhml.e6
    # Compute ISM (M slope). 1/MY  (s14.1)
    vrcp  vslope_f.e5, vhml.e5
    vrcph vslope_i.e5, vhml.e5
    # Compute ISH (H slope). 1/HY  (s14.1)
    vrcp  vslope_f.e4, vhml.e4
    vrcph vslope_i.e4, vhml.e4

    ##################
    # 1 NR pass
    ##################
    vmov vslope_f.e7, inz_f
    vmov vslope_i.e7, inz_i

    # Adjust multiplying by 2 (required after reciprocal)
    #vmudn vslope_f, vslope_f, K2
    #vmadh vslope_i, vslope_i, K2
    vaddc vslope_f, vslope_f
    vadd  vslope_i, vslope_i

    # Prepare 32-bit number containing the source of the reciprocal
    # Notice that we're calculating NR over 1 32-bit input (NZ) and
    # 3 16-bit inputs (HY, MY, LY), for which we provide 0 in the lower
    # part.
    #    vhml      =   HX MX LX MY    HY   MY   LY   NZf
    #    v__       =    0 0  0  0     0    0    0    NZi
    vxor v__, v__
    vmov v__.e7,  vnz_i.e4
    vmov vhml.e7, vnz_f.e4

    #define vtmp_f  vattr1
    #define vtmp_i  vattr2
    #define vk2     vattr3

    # NR: R*X
    vmudl vtmp_f, vslope_f, vhml
    vmadm vtmp_f, vslope_i, vhml
    vmadn vtmp_f, vslope_f, v__
    vmadh vtmp_i, vslope_i, v__
#if 0
    # NR: 2 - R*X   
    vor vk2, vzero, K2
    vsubc vtmp_f, vzero, vtmp_f
    vsub  vtmp_i, vk2,   vtmp_i

    # NR: X * (2 - R*X)
    vmudl vk2, vtmp_f, vslope_f
    vmadm vk2, vtmp_i, vslope_f
    vmadn vslope_f, vtmp_f, vslope_i
    vmadh vslope_i, vtmp_i, vslope_i
#endif
    #vmadn vslope_f, vzero, vzero     # re-read vslope_f in case of overflow

    # vhml      =   HX MX LX MY    HY   MY   LY   NZf
    # v__       =    0 0  0  0     0    0    0    NZi
    # slope     =   -- -- -- --   1/HY 1/MY 1/LY  1/NZ
    
    vmov vnz_f.e0, vslope_f.e7
    vmov vnz_i.e0, vslope_i.e7

    # Rotate slope
    # slope     =   1/HY 1/MY 1/LY 1/NZ   -- -- -- --
    sqv vslope_f.e4, 0x10,s3
    lqv vslope_f.e0  0x10,s3
    sqv vslope_i.e4, 0x10,s3
    lqv vslope_i.e0  0x10,s3

    # Shift left NZ (that contains INVNZ) by 2, to align with the fixed point precision
    # that will be required later.
    vmudn vnz_f, vnz_f, K4
    vmadh vnz_i, vnz_i, K4

    # FY.e4 = fy (s15.16)
    vsll8 vfy_f, vxy21, 14
    vsra  vfy_i, vxy21, 2
    # FY.e4 = floorf(y1) - y1
    # TODO: this is always a negative fraction, so fy_i is always 0xFFFF (or fy_i=fy_f=0).
    # See if we can take advantage of this somehow to simplify later.
    vsubc vfy_f, vzero, vfy_f
    vsub  vfy_i, vfy_i

    # Finalize slope divisions by multiplying by the reciprocal.
    #  vhml     =   HX    MX    LX    1     HY MY LY MX
    #    *
    #  slope    =  1/HY  1/MY  1/LY  1/NZ   -- -- -- --
    #    =
    #  slope    =  HX/HY MX/MY LX/LY  --    -- -- -- --
    vmudn v__,     vslope_f, vhml
    vmadh v__,     vslope_i, vhml
    vsar  vslope_f, COP2_ACC_MD
    vsar  vslope_i, COP2_ACC_HI

    #define ish_f   vslope_f.e0
    #define ish_i   vslope_i.e0
    #define ism_f   vslope_f.e1
    #define ism_i   vslope_i.e1
    #define isl_f   vslope_f.e2
    #define isl_i   vslope_i.e2

    # Store slopes
    ssv isl_f, 14,s3
    ssv isl_i, 12,s3
    ssv ism_f, 30,s3
    ssv ism_i, 28,s3
    ssv ish_f, 22,s3
    ssv ish_i, 20,s3

    #  vxy21 =   X1   --    X2   --  Y1 -- Y2 --
    #  slope = HX/HY MX/MY LX/LY --  -- -- -- --

    # FINAL = X1/X2 in 16.16 precision
    # TODO: maybe fold into the next MAC sequence?
    vsra  vfinal_i, vxy21, 2
    vsll8 vfinal_f, vxy21, 14

    # Store X2 value in output (as XL)
    ssv vfinal_f.e2, 10,s3  # XL_F
    ssv vfinal_i.e2,  8,s3  # Xl_I

    # Compute XH/XM
    # TODO: fy_i is always 0xFFFFFFFF here. See if we can benefit from this.
    vmudl v__,      vslope_f, vfy_f.e4
    vmadm v__,      vslope_i, vfy_f.e4
    vmadn vedges_f, vslope_f, vfy_i.e4
    vmadh vedges_i, vslope_i, vfy_i.e4

    vaddc vedges_f, vfinal_f.q0
    vadd  vedges_i, vfinal_i.q0

    ssv vedges_f.e1, 26,s3  # XM_F
    ssv vedges_i.e1, 24,s3  # XM_I
    ssv vedges_f.e0, 18,s3  # XH_F
    ssv vedges_i.e0, 16,s3  # XH_I

    sh tricmd, 0(s3)
    add s3, 32

    # Load attributes into ATTR registers.
    # TODO: we can interleave these in all the code above, and at that point
    # it's useless to test for tricmd to save loads. Just load them all.

    #define attr1_r     vattr1.e0
    #define attr2_r     vattr2.e0
    #define attr3_r     vattr3.e0
    #define attr1_s     vattr1.e4
    #define attr2_s     vattr2.e4
    #define attr3_s     vattr3.e4

    #define attr1_z     vattr1.e7
    #define attr2_z     vattr2.e7
    #define attr3_z     vattr3.e7
    luv attr1_r, VTX_ATTR_RGBA,vtx1 # RGBA
    luv attr2_r, VTX_ATTR_RGBA,vtx2
    luv attr3_r, VTX_ATTR_RGBA,vtx3

    llv attr1_s, VTX_ATTR_S,vtx1  # S & T
    llv attr2_s, VTX_ATTR_S,vtx2
    llv attr3_s, VTX_ATTR_S,vtx3

    vsubc vinvw_f, $v00, $v30.e7
    vmov vinvw_f.e0, $v31.e6
    vmov vinvw_f.e1, $v31.e6
    vmov vinvw_f.e2, $v31.e6
    
    vmudm vattr1, vinvw_f.h0
    vmudm vattr2, vinvw_f.h1
    vmudm vattr3, vinvw_f.h2

    lsv attr1_z, VTX_ATTR_Z,vtx1  # Load Z
    lsv attr2_z, VTX_ATTR_Z,vtx2
    lsv attr3_z, VTX_ATTR_Z,vtx3

    ########################################################
    # ATTRIBUTES
    ########################################################

    # MA = A2 - A1
    # HA = A3 - A1
    # NOTE: S/T coordinates are kept as s10.5, so they can overflow here.
    # The subtraction is saturated so the error is minimized, but it is 
    # indeed there. To fix this, we would have to produce a 32-bit result here
    # and then change the DX/DY calculations to use 32-bit numbers as well.
    # Note also that we need "vsubc zero,zero" to clear the VCC (carry) bit
    # which vsub reads as input.
    vsubc vzero, vzero
    vsub vma, vattr2, vattr1
    vsub vha, vattr3, vattr1

    #  vhml     = HX MX LX MY1    HY MY LY MX1

    # TODO: find other strategies to negate MY and HX?
    # Or maybe this is good as we can probably interleave it, being scalar ops.
    # TODO: or we could also compute -MA / -HA. But that's even more vector ops.
    mfc2 t0, my
    mfc2 t1, hx
    neg t0
    neg t1
    mtc2 t0, my
    mtc2 t1, hx

    # DX = MA * HY - HA * MY
    vmudh vdx_f, vma, hy
    vmadh vdx_f, vha, my
    vsar vdx_f, COP2_ACC_MD
    vsar vdx_i, COP2_ACC_HI

    # DY = HA * MX - MA * HX
    vmudh vdy_f, vha, mx
    vmadh vdy_f, vma, hx
    vsar vdy_f, COP2_ACC_MD
    vsar vdy_i, COP2_ACC_HI

    # DX * 1/N (TODO: check if we can pre-multiply edges to avoid this)
    vmudl v__,  vdx_f, inz_f
    vmadm v__,  vdx_i, inz_f
    vmadn vdx_f, vdx_f, inz_i
    vmadh vdx_i, vdx_i, inz_i

    # DY * 1/N (TODO: check if we can pre-multiply edges to avoid this)
    vmudl v__,  vdy_f, inz_f
    vmadm v__,  vdy_i, inz_f
    vmadn vdy_f, vdy_f, inz_i
    vmadh vdy_i, vdy_i, inz_i

    # DE = DX * invsh + DY
    vmadl v__,  vdx_f, ish_f
    vmadm v__,  vdx_i, ish_f
    vmadn vde_f, vdx_f, ish_i
    vmadh vde_i, vdx_i, ish_i

    # FINAL = vATTR1 + DE * FY
    # TODO: fy_i is always 0xFFFFFFFF here. See if we can benefit from this.
    # TODO: actually, it can also be fy_i = fy_f = 0.
    vmudl v__,      vde_f, vfy_f.e4
    vmadm v__,      vde_i, vfy_f.e4
    vmadn vfinal_f, vde_f, vfy_i.e4
    vmadh vfinal_i, vde_i, vfy_i.e4
    vmadh vfinal_i, vattr1, K1

    # Store color
    sdv vfinal_i.e0, 0x00,s3
    sdv    vdx_i.e0, 0x08,s3
    sdv vfinal_f.e0, 0x10,s3
    sdv    vdx_f.e0, 0x18,s3
    sdv    vde_i.e0, 0x20,s3
    sdv    vdy_i.e0, 0x28,s3
    sdv    vde_f.e0, 0x30,s3
    sdv    vdy_f.e0, 0x38,s3
    
    addi s3, 0x40
 
    # Store texture
    sdv vfinal_i.e4, 0x00,s3
    sdv    vdx_i.e4, 0x08,s3
    sdv vfinal_f.e4, 0x10,s3
    sdv    vdx_f.e4, 0x18,s3
    sdv    vde_i.e4, 0x20,s3
    sdv    vdy_i.e4, 0x28,s3
    sdv    vde_f.e4, 0x30,s3
    sdv    vdy_f.e4, 0x38,s3

    addi s3, 0x50 # advance DMEM pointer

    # Store z (ignore slope and use a uniform depth for a triangle)
    sqv    vzero, (-0x10),s3
    ssv vfinal_i.e7, (0x00 - 0x10),s3
    ssv vfinal_f.e7, (0x02 - 0x10),s3

    #undef tricm
    #undef vtx1 
    #undef vtx2 
    #undef vtx3 
    #undef cull 

    #undef y1   
    #undef y2   
    #undef y3   
    #undef x1   
    #undef x2   
    #undef x3   

    # r, g, b, a, s, t, w, z
    #undef vfinal_i         
    #undef vfinal_f         
    #undef vdx_i            
    #undef vdx_f            
    #undef vde_i            
    #undef vde_f            
    #undef vdy_i            
    #undef vdy_f            

    #undef vattr1           
    #undef vattr2           
    #undef vattr3           
    #undef vma              
    #undef vha              

    #undef vinvw_i          
    #undef vinvw_f          

    #undef vedges_i         
    #undef vedges_f         
    #undef vnz_i            
    #undef vnz_f            
    #undef vslope_i         
    #undef vslope_f         
    #undef vxy32            
    #undef vxy21            
    #undef vhml             
    #undef vfy_i            
    #undef vfy_f            

    #undef vtmp
    #undef v__         
    #undef invn_i      
    #undef invn_f      
    #undef invsh_i     
    #undef invsh_f     

    #undef VTX_ATTR_X   
    #undef VTX_ATTR_Y   
    #undef VTX_ATTR_Z   
    #undef VTX_ATTR_RGBA
    #undef VTX_ATTR_S    
    #undef VTX_ATTR_T    
    #undef VTX_ATTR_W    
    #undef VTX_ATTR_INVWi
    #undef VTX_ATTR_INVWf
