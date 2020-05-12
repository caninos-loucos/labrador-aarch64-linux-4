
#define DE_SCLCOEF_ZOOMIN			0
#define DE_SCLCOEF_HALF_ZOOMOUT			1
#define DE_SCLCOEF_SMALLER_ZOOMOUT		2

/*
 * common registers
 */
#define DE_IRQENABLE				0x0000
#define DE_IRQSTATUS				0x0004
#define DE_MMU_EN				0x0008
#define DE_MMU_BASE				0x000c
#define DE_QOS					0x0014
#define DE_MAX_OUTSTANDING			0x0018

#define DE_HIST(n)				(0x0020 + (n) * 4)
#define DE_TOTAL				0x00a0

#define DE_OUTPUT_CON				0x1000
 #define DE_OUTPUT_PATH1_DEVICE_BEGIN_BIT	0
 #define DE_OUTPUT_PATH1_DEVICE_END_BIT		1
 #define DE_OUTPUT_PATH2_DEVICE_BEGIN_BIT	4
 #define DE_OUTPUT_PATH2_DEVICE_END_BIT		5

#define DE_OUTPUT_STAT				0x1004

/*
 * path registers
 */
#define DE_PATH_BASE				0x0100

#define DE_PATH_CTL(n)				(DE_PATH_BASE + (n) * 0x100 + 0x000)
 #define DE_PATH_ML_EN_BEGIN_BIT		0
 #define DE_PATH_ML_EN_END_BIT			3
 #define DE_PATH_CTL_ILACE_BIT			9
 #define DE_PATH_CTL_RGB_YUV_EN_BIT		13

#define DE_PATH_FCR(n)				(DE_PATH_BASE + (n) * 0x100 + 0x004)
 #define DE_PATH_FCR_BIT			0

#define DE_PATH_EN(n)				(DE_PATH_BASE + (n) * 0x100 + 0x008)
 #define DE_PATH_ENABLE_BIT			0

#define DE_PATH_BK(n)				(DE_PATH_BASE + (n) * 0x100 + 0x00c)

#define DE_PATH_SIZE(n)				(DE_PATH_BASE + (n) * 0x100 + 0x010)
 #define DE_PATH_SIZE_WIDTH			(1 << 12)
 #define DE_PATH_SIZE_HEIGHT			(1 << 12)
 #define DE_PATH_SIZE_WIDTH_BEGIN_BIT		0
 #define DE_PATH_SIZE_WIDTH_END_BIT		11
 #define DE_PATH_SIZE_HEIGHT_BEGIN_BIT		16
 #define DE_PATH_SIZE_HEIGHT_END_BIT		27

#define DE_PATH_GAMMA_IDX(n)			(DE_PATH_BASE + (n) * 0x100 + 0x040)
 #define DE_PATH_GAMMA_IDX_BUSY_BIT		(14)
 #define DE_PATH_GAMMA_IDX_OP_SEL_BEGIN_BIT	(12)
 #define DE_PATH_GAMMA_IDX_OP_SEL_END_BIT	(13)
 #define DE_PATH_GAMMA_IDX_INDEX_BEGIN_BIT	(0)
 #define DE_PATH_GAMMA_IDX_INDEX_END_BIT	(7)

#define DE_PATH_GAMMA_ENABLE(n)			DE_PATH_GAMMA_IDX((n))
 #define DE_PATH_GAMMA_ENABLE_BIT		16

#define DE_PATH_GAMMA_RAM(n)			(DE_PATH_BASE + (n) * 0x100 + 0x044)

#define DE_PATH_DITHER				(0x0250)
#define DE_PATH_DITHER_ENABLE_BIT		0
#define DE_PATH_DITHER_MODE_BEGIN_BIT		1
#define DE_PATH_DITHER_MODE_END_BIT		2

#define DE_PATH_BLENDER(n)			(DE_PATH_BASE + (n) * 0x100 + 0x054)
#define DE_PATH_DIM_ALPHA_BEGIN_BIT		8
#define DE_PATH_DIM_ALPHA_END_BIT		15
#define DE_PATH_DIM_EN_BEGIN_BIT		0
#define DE_PATH_DIM_EN_END_BIT			3

/* path m, microlayer n, coor */
#define DE_PATH_COOR(m, n)			(DE_PATH_BASE + (m) * 0x100 + (n) * 0x4 + 0x20)

/*
 * micro- & sub- layer registers
 * microlayer x
 * microlayer x sublayer y
 */

#define DE_ML_BASE				0x0400

#define DE_ML_CFG(x)				(DE_ML_BASE + (x) * 0x200  +  0x0000)
 #define DE_ML_EN_BEGIN_BIT			0
 #define DE_ML_EN_END_BIT			3
 #define DE_ML_ROT180_BIT			20

#define DE_ML_ISIZE(x)				(DE_ML_BASE + (x) * 0x200  +  0x0004)

#define DE_ML_CSC(x)				(DE_ML_BASE + (x) * 0x200  +  0x0014)

#define DE_ML_BK(x)				(DE_ML_BASE + (x) * 0x200 + 0x01c)

#define DE_SL_CFG(x, y)				(DE_ML_BASE + (x) * 0x200 + 0x020 + (y) * 0x80)
 #define DE_SL_CFG_FMT_BEGIN_BIT		0
 #define DE_SL_CFG_FMT_END_BIT			4
 #define DE_SL_CFG_GLOBAL_ALPHA_BEGIN_BIT	8
 #define DE_SL_CFG_GLOBAL_ALPHA_END_BIT		15
 #define DE_SL_CFG_DATA_MODE_BIT		16

#define DE_SL_COOR(x, y)			(DE_ML_BASE + (x) * 0x200 + 0x028 + (y) * 0x80)

#define DE_SL_FB(x, y)				(DE_ML_BASE + (x) * 0x200 + 0x02c + (y) * 0x80)
#define DE_SL_FB_RIGHT(x, y)			(DE_ML_BASE + (x) * 0x200 + 0x038 + (y) * 0x80)
#define DE_SL_STR(x, y)				(DE_ML_BASE + (x) * 0x200 + 0x044 + (y) * 0x80)

#define DE_SL_CROPSIZE(x, y)			(DE_ML_BASE + (x) * 0x200 + 0x048 + (y) * 0x80)

/*
 * scaler registers
 */

#define DE_SCALER_BASE				0x0C00

#define DE_SCALER_CFG(x)			(DE_SCALER_BASE + (x) * 0x80 + 0x0000)
 #define DE_SCALER_CFG_ENABLE_BIT		0
 #define DE_SCALER_CFG_4K_SINGLE_BIT		2
 #define DE_SCALER_CFG_RGB_LINE_BIT		3
 #define DE_SCALER_CFG_SEL_BEGIN_BIT		4
 #define DE_SCALER_CFG_SEL_END_BIT		5

#define DE_SCALER_OSZIE(x)			(DE_SCALER_BASE + (x) * 0x80 + 0x0004)

#define DE_SCALER_HSR(x)			(DE_SCALER_BASE + (x) * 0x80 + 0x0008)
#define DE_SCALER_VSR(x)			(DE_SCALER_BASE + (x) * 0x80 + 0x000C)

#define DE_SCALER_SCOEF0(x)			(DE_SCALER_BASE + (x) * 0x80 + 0x0020)
#define DE_SCALER_SCOEF1(x)			(DE_SCALER_BASE + (x) * 0x80 + 0x0024)
#define DE_SCALER_SCOEF2(x)			(DE_SCALER_BASE + (x) * 0x80 + 0x0028)
#define DE_SCALER_SCOEF3(x)			(DE_SCALER_BASE + (x) * 0x80 + 0x002C)
#define DE_SCALER_SCOEF4(x)			(DE_SCALER_BASE + (x) * 0x80 + 0x0030)
#define DE_SCALER_SCOEF5(x)			(DE_SCALER_BASE + (x) * 0x80 + 0x0034)
#define DE_SCALER_SCOEF6(x)			(DE_SCALER_BASE + (x) * 0x80 + 0x0038)
#define DE_SCALER_SCOEF7(x)			(DE_SCALER_BASE + (x) * 0x80 + 0x003C)

