# Not implemented
.macro makeNotImplemented name
    .macro \name _:vararg
        \name\()_na
    .endm
.endm

makeNotImplemented ldl
makeNotImplemented ldr
makeNotImplemented lwl
makeNotImplemented lwr
makeNotImplemented lwu
makeNotImplemented swl
makeNotImplemented sdl
makeNotImplemented sdr
makeNotImplemented swr
makeNotImplemented ll
makeNotImplemented lld
makeNotImplemented ldc1
makeNotImplemented ldc2
makeNotImplemented ld
makeNotImplemented sdc1
makeNotImplemented sdc2
makeNotImplemented sd
makeNotImplemented sc
makeNotImplemented scd
makeNotImplemented beql
makeNotImplemented bnel
makeNotImplemented blezl
makeNotImplemented bgtzl
makeNotImplemented bltzl
makeNotImplemented bgezl
makeNotImplemented bltzall
makeNotImplemented bgtzall
makeNotImplemented bgezall
makeNotImplemented mfhi
makeNotImplemented mthi
makeNotImplemented mflo
makeNotImplemented mtlo
makeNotImplemented daddi
makeNotImplemented daddiu
makeNotImplemented dsllv
makeNotImplemented dsrlv
makeNotImplemented dsrav
makeNotImplemented dmult
makeNotImplemented dmultu
makeNotImplemented ddiv
makeNotImplemented ddivu
makeNotImplemented dadd
makeNotImplemented daddu
makeNotImplemented dsub
makeNotImplemented dsubu
makeNotImplemented dsll
makeNotImplemented dsrl
makeNotImplemented dsra
makeNotImplemented dsll32
makeNotImplemented dsrl32
makeNotImplemented dsra32
makeNotImplemented mult
makeNotImplemented multu
makeNotImplemented div
makeNotImplemented divu
makeNotImplemented syscall
makeNotImplemented sync
makeNotImplemented bc1f
makeNotImplemented bc1t
makeNotImplemented tge
makeNotImplemented tgeu
makeNotImplemented tlt
makeNotImplemented tltu
makeNotImplemented teq
makeNotImplemented tne
makeNotImplemented tgei
makeNotImplemented tgeiu
makeNotImplemented tlti
makeNotImplemented tltiu
makeNotImplemented teqi
makeNotImplemented tnei

.macro scalarRegisters
    .set S_ZERO, $0
    .set S_AT, $1
    .set S_V0, $2
    .set S_V1, $3
    .set S_A0, $4
    .set S_A1, $5
    .set S_A2, $6
    .set S_A3, $7
    .set S_T0, $8
    .set S_T1, $9
    .set S_T2, $10
    .set S_T3, $11
    .set S_T4, $12
    .set S_T5, $13
    .set S_T6, $14
    .set S_T7, $15
    .set S_S0, $16
    .set S_S1, $17
    .set S_S2, $18
    .set S_S3, $19
    .set S_S4, $20
    .set S_S5, $21
    .set S_S6, $22
    .set S_S7, $23
    .set S_T8, $24
    .set S_T9, $25
    .set S_K0, $26
    .set S_K1, $27
    .set S_GP, $28
    .set S_SP, $29
    .set S_FP, $30
    .set S_RA, $31
.endm

.macro hexRegisters
    .set VEC0, 0x0
    .set VEC1, 0x1
    .set VEC2, 0x2
    .set VEC3, 0x3
    .set VEC4, 0x4
    .set VEC5, 0x5
    .set VEC6, 0x6
    .set VEC7, 0x7
    .set VEC8, 0x8
    .set VEC9, 0x9
    .set VEC10, 0xA
    .set VEC11, 0xB
    .set VEC12, 0xC
    .set VEC13, 0xD
    .set VEC14, 0xE
    .set VEC15, 0xF
    .set VEC16, 0x10
    .set VEC17, 0x11
    .set VEC18, 0x12
    .set VEC19, 0x13
    .set VEC20, 0x14
    .set VEC21, 0x15
    .set VEC22, 0x16
    .set VEC23, 0x17
    .set VEC24, 0x18
    .set VEC25, 0x19
    .set VEC26, 0x1A
    .set VEC27, 0x1B
    .set VEC28, 0x1C
    .set VEC29, 0x1D
    .set VEC30, 0x1E
    .set VEC31, 0x1F
.endm

.macro renameRegisters
    .set VEC0, $0
    .set VEC1, $1
    .set VEC2, $2
    .set VEC3, $3
    .set VEC4, $4
    .set VEC5, $5
    .set VEC6, $6
    .set VEC7, $7
    .set VEC8, $8
    .set VEC9, $9
    .set VEC10, $10
    .set VEC11, $11
    .set VEC12, $12
    .set VEC13, $13
    .set VEC14, $14
    .set VEC15, $15
    .set VEC16, $16
    .set VEC17, $17
    .set VEC18, $18
    .set VEC19, $19
    .set VEC20, $20
    .set VEC21, $21
    .set VEC22, $22
    .set VEC23, $23
    .set VEC24, $24
    .set VEC25, $25
    .set VEC26, $26
    .set VEC27, $27
    .set VEC28, $28
    .set VEC29, $29
    .set VEC30, $30
    .set VEC31, $31
    scalarRegisters
.endm

/** @brief Syntactic sugar for cop2 instuctions */
.macro vectorOp opcode, vd, vs, vt, element
    hexRegisters
    cop2 (\element << 21 | \vt << 16 | \vs << 11 | \vd << 6 | \opcode)
.endm

/** @brief Syntactic sugar for lwc2 instuctions */
.macro loadVector, opcode, vt, element, offset, base
    renameRegisters
    lwc2 \vt, (\opcode << 11 | \element << 7 | \offset) (\base)
.endm

/** @brief Syntactic sugar for swc2 instuctions */
.macro storeVector opcode, vt, element, offset, base
    renameRegisters
    swc2 \vt, (\opcode << 11 | \element << 7 | \offset) (\base)
.endm

.macro makeOpInstruction name, opcode
    .macro \name vd, vs, vt, element
        .ifgt (\element >> 4)
            .error "Invalid element"
        .else
            vectorOp \opcode, \vd, \vs, \vt, \element
        .endif
    .endm
.endm

.macro makeLoadInstruction name, opcode
    .macro \name vt, element, offset, base
        .ifgt (\element >> 4)
            .error "Invalid element"
        .else
            .ifgt (\offset >> 7)
                .error "Invalid offset"
            .else
                loadVector \opcode, \vt, \element, \offset, \base
            .endif
        .endif
    .endm
.endm

.macro makeStoreInstruction name, opcode
    .macro \name vt, element, offset, base
        .ifgt (\element >> 4)
            .error "Invalid element"
        .else
            .ifgt (\offset >> 7)
                .error "Invalid offset"
            .else
                storeVector \opcode, \vt, \element, \offset, \base
            .endif
        .endif
    .endm
.endm

/** @brief Vector Absolute Value of Short Elements */
makeOpInstruction vabs, 0b010011
/** @brief Vector Add of Short Elements */
makeOpInstruction vadd, 0b010000
/** @brief Vector Add Short Elements With Carry */
makeOpInstruction vaddc, 0b010100
/** @brief Vector AND of Short Elements */
makeOpInstruction vand, 0b010100
/** @brief Vector Select Clip Test High */
makeOpInstruction vch, 0b100101
/** @brief Vector Select Clip Test Low */
makeOpInstruction vcl, 0b100100
/** @brief Vector Select Crimp Test Low */
makeOpInstruction vcr, 0b100110
/** @brief Vector Select Equal */
makeOpInstruction veq, 0b100001
/** @brief Vector Select Greater Than or Equal */
makeOpInstruction vge, 0b100011
/** @brief Vector Select Less Than */
makeOpInstruction vlt, 0b100000
/** @brief Vector Multiply-Accumulate of Signed Fractions */
makeOpInstruction vmacf, 0b001000
/** @brief Vector Accumulator Oddification */
makeOpInstruction vmacq, 0b001011
/** @brief Vector Multiply-Accumulate of Unsigned Fractions */
makeOpInstruction vmacu, 0b001001
/** @brief Vector Multiply-Accumulate of High Partial Products */
makeOpInstruction vmadh, 0b001111
/** @brief Vector Multiply-Accumulate of Low Partial Products */
makeOpInstruction vmadl, 0b001100
/** @brief Vector Multiply-Accumulate of Mid Partial Products */
makeOpInstruction vmadm, 0b001101
/** @brief Vector Multiply-Accumulate of Mid Partial Products */
makeOpInstruction vmadn, 0b001110
/** @brief Vector Element Scalar Move */
makeOpInstruction vmov, 0b110011
/** @brief Vector Select Merge */
makeOpInstruction vmrg, 0b100111
/** @brief Vector Multiply of High Partial Products */
makeOpInstruction vmudh, 0b000111
/** @brief Vector Multiply of Low Partial Products */
makeOpInstruction vmudl, 0b000100
/** @brief Vector Multiply of Mid Partial Products */
makeOpInstruction vmudm, 0b000101
/** @brief Vector Multiply of Mid Partial Products */
makeOpInstruction vmudn, 0b000110
/** @brief Vector Multiply of Signed Fractions */
makeOpInstruction vmulf, 0b000000
/** @brief Vector Multiply MPEG Quantization */
makeOpInstruction vmulq, 0b000011
/** @brief Vector Multiply of Unsigned Fractions */
makeOpInstruction vmulu, 0b000001
/** @brief Vector NAND of Short Elements */
makeOpInstruction vnand, 0b101001
/** @brief Vector Select Not Equal */
makeOpInstruction vne, 0b100010
/** @brief Vector Null Instruction */
makeOpInstruction vnop, 0b110111
/** @brief Vector NOR of Short Elements */
makeOpInstruction vnor, 0b101011
/** @brief Vector NXOR of Short Elements */
makeOpInstruction vnxor, 0b101101
/** @brief Vector OR of Short Elements */
makeOpInstruction vor, 0b101010
/** @brief Vector Element Scalar Reciprocal (Single Precision) */
makeOpInstruction vrcp, 0b110000
/** @brief Vector Element Scalar Reciprocal (Double Prec. High) */
makeOpInstruction vrcph, 0b110010
/** @brief Vector Element Scalar Reciprocal (Double Prec. Low) */
makeOpInstruction vrcpl, 0b110001
/** @brief Vector Accumulator DCT Rounding (Negative) */
makeOpInstruction vrndn, 0b001010
/** @brief Vector Accumulator DCT Rounding (Positive) */
makeOpInstruction vrndp, 0b000010
/** @brief Vector Element Scalar SQRT Reciprocal */
makeOpInstruction vrsq, 0b110100
/** @brief Vector Element Scalar SQRT Reciprocal (Double Prec. High) */
makeOpInstruction vrsqh, 0b110110
/** @brief Vector Element Scalar SQRT Reciprocal (Double Prec. Low) */
makeOpInstruction vrsql, 0b110101
/** @brief Vector Accumulator Read (and Write) */
makeOpInstruction vsar, 0b011101
/** @brief Vector Subtraction of Short Elements */
makeOpInstruction vsub, 0b010001
/** @brief Vector Subtraction of Short Elements With Carry */
makeOpInstruction vsubc, 0b010101
/** @brief Vector XOR of Short Elements */
makeOpInstruction vxor, 0b101100

/** @brief Load Byte into Vector Register */
makeLoadInstruction lbv, 0b00000
/** @brief Load Double into Vector Register */
makeLoadInstruction ldv, 0b00011
/** @brief Load Packed Fourth into Vector Register */
makeLoadInstruction lfv, 0b01001
/** @brief Load Packed Half into Vector Register */
makeLoadInstruction lhv, 0b01000
/** @brief Load Long into Vector Register */
makeLoadInstruction llv, 0b00010
/** @brief Load Packed Bytes into Vector Register */
makeLoadInstruction lpv, 0b00110
/** @brief Load Quad into Vector Register */
makeLoadInstruction lqv, 0b00100
/** @brief Load Quad (Rest) into Vector Register */
makeLoadInstruction lrv, 0b00101
/** @brief Load Short into Vector Register */
makeLoadInstruction lsv, 0b00001
/** @brief Load Transpose into Vector Register */
makeLoadInstruction ltv, 0b01011
/** @brief Load Unsigned Packed into Vector Register */
makeLoadInstruction luv, 0b00111

/** @brief Store Byte from Vector Register */
makeStoreInstruction sbv, 0b00000,
/** @brief Store Double from Vector Register */
makeStoreInstruction sdv, 0b00011,
/** @brief Store Packed Fourth from Vector Register */
makeStoreInstruction sfv, 0b01001,
/** @brief Store Packed Half from Vector Register */
makeStoreInstruction shv, 0b01000,
/** @brief Store Long from Vector Register */
makeStoreInstruction slv, 0b00010,
/** @brief Store Packed Bytes from Vector Register */
makeStoreInstruction spv, 0b00110,
/** @brief Store Quad from Vector Register */
makeStoreInstruction sqv, 0b00100,
/** @brief Store Quad (Rest) from Vector Register */
makeStoreInstruction srv, 0b00101,
/** @brief Store Short from Vector Register */
makeStoreInstruction ssv, 0b00001,
/** @brief Store Transpose from Vector Register */
makeStoreInstruction stv, 0b01011,
/** @brief Store Unsigned Packed from Vector Register */
makeStoreInstruction suv, 0b00111,
/** @brief Store Wrapped vector from Vector Register */
makeStoreInstruction swv, 0b00111,

.set noat
