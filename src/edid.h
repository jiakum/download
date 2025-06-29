#ifndef _MJK_EDID_H__
#define _MJK_EDID_H__

#include <stdint.h>

typedef uint8_t u8;
typedef uint32_t u32;
typedef uint16_t __le16;


#define EDID_LENGTH 128
#define DDC_ADDR 0x50
#define DDC_ADDR2 0x52 /* E-DDC 1.2 - where DisplayID can hide */

#define CEA_EXT	    0x02
#define VTB_EXT	    0x10
#define DI_EXT	    0x40
#define LS_EXT	    0x50
#define MI_EXT	    0x60
#define DISPLAYID_EXT 0x70

struct est_timings {
	u8 t1;
	u8 t2;
	u8 mfg_rsvd;
} __attribute__((packed));

/* 00=16:10, 01=4:3, 10=5:4, 11=16:9 */
#define EDID_TIMING_ASPECT_SHIFT 6
#define EDID_TIMING_ASPECT_MASK  (0x3 << EDID_TIMING_ASPECT_SHIFT)

/* need to add 60 */
#define EDID_TIMING_VFREQ_SHIFT  0
#define EDID_TIMING_VFREQ_MASK   (0x3f << EDID_TIMING_VFREQ_SHIFT)

struct std_timing {
	u8 hsize; /* need to multiply by 8 then add 248 */
	u8 vfreq_aspect;
} __attribute__((packed));

#define DRM_EDID_PT_HSYNC_POSITIVE (1 << 1)
#define DRM_EDID_PT_VSYNC_POSITIVE (1 << 2)
#define DRM_EDID_PT_SEPARATE_SYNC  (3 << 3)
#define DRM_EDID_PT_STEREO         (1 << 5)
#define DRM_EDID_PT_INTERLACED     (1 << 7)

/* If detailed data is pixel timing */
struct detailed_pixel_timing {
	u8 hactive_lo;
	u8 hblank_lo;
	u8 hactive_hblank_hi;
	u8 vactive_lo;
	u8 vblank_lo;
	u8 vactive_vblank_hi;
	u8 hsync_offset_lo;
	u8 hsync_pulse_width_lo;
	u8 vsync_offset_pulse_width_lo;
	u8 hsync_vsync_offset_pulse_width_hi;
	u8 width_mm_lo;
	u8 height_mm_lo;
	u8 width_height_mm_hi;
	u8 hborder;
	u8 vborder;
	u8 misc;
} __attribute__((packed));

/* If it's not pixel timing, it'll be one of the below */
struct detailed_data_string {
	u8 str[13];
} __attribute__((packed));

#define DRM_EDID_RANGE_OFFSET_MIN_VFREQ (1 << 0) /* 1.4 */
#define DRM_EDID_RANGE_OFFSET_MAX_VFREQ (1 << 1) /* 1.4 */
#define DRM_EDID_RANGE_OFFSET_MIN_HFREQ (1 << 2) /* 1.4 */
#define DRM_EDID_RANGE_OFFSET_MAX_HFREQ (1 << 3) /* 1.4 */

#define DRM_EDID_DEFAULT_GTF_SUPPORT_FLAG   0x00 /* 1.3 */
#define DRM_EDID_RANGE_LIMITS_ONLY_FLAG     0x01 /* 1.4 */
#define DRM_EDID_SECONDARY_GTF_SUPPORT_FLAG 0x02 /* 1.3 */
#define DRM_EDID_CVT_SUPPORT_FLAG           0x04 /* 1.4 */

#define DRM_EDID_CVT_FLAGS_STANDARD_BLANKING (1 << 3)
#define DRM_EDID_CVT_FLAGS_REDUCED_BLANKING  (1 << 4)

struct detailed_data_monitor_range {
	u8 min_vfreq;
	u8 max_vfreq;
	u8 min_hfreq_khz;
	u8 max_hfreq_khz;
	u8 pixel_clock_mhz; /* need to multiply by 10 */
	u8 flags;
	union {
		struct {
			u8 reserved;
			u8 hfreq_start_khz; /* need to multiply by 2 */
			u8 c; /* need to divide by 2 */
			__le16 m;
			u8 k;
			u8 j; /* need to divide by 2 */
		} __attribute__((packed)) gtf2;
		struct {
			u8 version;
			u8 data1; /* high 6 bits: extra clock resolution */
			u8 data2; /* plus low 2 of above: max hactive */
			u8 supported_aspects;
			u8 flags; /* preferred aspect and blanking support */
			u8 supported_scalings;
			u8 preferred_refresh;
		} __attribute__((packed)) cvt;
	} __attribute__((packed)) formula;
} __attribute__((packed));

struct detailed_data_wpindex {
	u8 white_yx_lo; /* Lower 2 bits each */
	u8 white_x_hi;
	u8 white_y_hi;
	u8 gamma; /* need to divide by 100 then add 1 */
} __attribute__((packed));

struct detailed_data_color_point {
	u8 windex1;
	u8 wpindex1[3];
	u8 windex2;
	u8 wpindex2[3];
} __attribute__((packed));

struct cvt_timing {
	u8 code[3];
} __attribute__((packed));

struct detailed_non_pixel {
	u8 pad1;
	u8 type; /* ff=serial, fe=string, fd=monitor range, fc=monitor name
		    fb=color point data, fa=standard timing data,
		    f9=undefined, f8=mfg. reserved */
	u8 pad2;
	union {
		struct detailed_data_string str;
		struct detailed_data_monitor_range range;
		struct detailed_data_wpindex color;
		struct std_timing timings[6];
		struct cvt_timing cvt[4];
	} __attribute__((packed)) data;
} __attribute__((packed));

#define EDID_DETAIL_EST_TIMINGS 0xf7
#define EDID_DETAIL_CVT_3BYTE 0xf8
#define EDID_DETAIL_COLOR_MGMT_DATA 0xf9
#define EDID_DETAIL_STD_MODES 0xfa
#define EDID_DETAIL_MONITOR_CPDATA 0xfb
#define EDID_DETAIL_MONITOR_NAME 0xfc
#define EDID_DETAIL_MONITOR_RANGE 0xfd
#define EDID_DETAIL_MONITOR_STRING 0xfe
#define EDID_DETAIL_MONITOR_SERIAL 0xff

struct detailed_timing {
	__le16 pixel_clock; /* need to multiply by 10 KHz */
	union {
		struct detailed_pixel_timing pixel_data;
		struct detailed_non_pixel other_data;
	} __attribute__((packed)) data;
} __attribute__((packed));

#define DRM_EDID_INPUT_SERRATION_VSYNC (1 << 0)
#define DRM_EDID_INPUT_SYNC_ON_GREEN   (1 << 1)
#define DRM_EDID_INPUT_COMPOSITE_SYNC  (1 << 2)
#define DRM_EDID_INPUT_SEPARATE_SYNCS  (1 << 3)
#define DRM_EDID_INPUT_BLANK_TO_BLACK  (1 << 4)
#define DRM_EDID_INPUT_VIDEO_LEVEL     (3 << 5)
#define DRM_EDID_INPUT_DIGITAL         (1 << 7)
#define DRM_EDID_DIGITAL_DEPTH_MASK    (7 << 4) /* 1.4 */
#define DRM_EDID_DIGITAL_DEPTH_UNDEF   (0 << 4) /* 1.4 */
#define DRM_EDID_DIGITAL_DEPTH_6       (1 << 4) /* 1.4 */
#define DRM_EDID_DIGITAL_DEPTH_8       (2 << 4) /* 1.4 */
#define DRM_EDID_DIGITAL_DEPTH_10      (3 << 4) /* 1.4 */
#define DRM_EDID_DIGITAL_DEPTH_12      (4 << 4) /* 1.4 */
#define DRM_EDID_DIGITAL_DEPTH_14      (5 << 4) /* 1.4 */
#define DRM_EDID_DIGITAL_DEPTH_16      (6 << 4) /* 1.4 */
#define DRM_EDID_DIGITAL_DEPTH_RSVD    (7 << 4) /* 1.4 */
#define DRM_EDID_DIGITAL_TYPE_MASK     (7 << 0) /* 1.4 */
#define DRM_EDID_DIGITAL_TYPE_UNDEF    (0 << 0) /* 1.4 */
#define DRM_EDID_DIGITAL_TYPE_DVI      (1 << 0) /* 1.4 */
#define DRM_EDID_DIGITAL_TYPE_HDMI_A   (2 << 0) /* 1.4 */
#define DRM_EDID_DIGITAL_TYPE_HDMI_B   (3 << 0) /* 1.4 */
#define DRM_EDID_DIGITAL_TYPE_MDDI     (4 << 0) /* 1.4 */
#define DRM_EDID_DIGITAL_TYPE_DP       (5 << 0) /* 1.4 */
#define DRM_EDID_DIGITAL_DFP_1_X       (1 << 0) /* 1.3 */

#define DRM_EDID_FEATURE_DEFAULT_GTF      (1 << 0) /* 1.2 */
#define DRM_EDID_FEATURE_CONTINUOUS_FREQ  (1 << 0) /* 1.4 */
#define DRM_EDID_FEATURE_PREFERRED_TIMING (1 << 1)
#define DRM_EDID_FEATURE_STANDARD_COLOR   (1 << 2)
/* If analog */
#define DRM_EDID_FEATURE_DISPLAY_TYPE     (3 << 3) /* 00=mono, 01=rgb, 10=non-rgb, 11=unknown */
/* If digital */
#define DRM_EDID_FEATURE_COLOR_MASK	  (3 << 3)
#define DRM_EDID_FEATURE_RGB		  (0 << 3)
#define DRM_EDID_FEATURE_RGB_YCRCB444	  (1 << 3)
#define DRM_EDID_FEATURE_RGB_YCRCB422	  (2 << 3)
#define DRM_EDID_FEATURE_RGB_YCRCB	  (3 << 3) /* both 4:4:4 and 4:2:2 */

#define DRM_EDID_FEATURE_PM_ACTIVE_OFF    (1 << 5)
#define DRM_EDID_FEATURE_PM_SUSPEND       (1 << 6)
#define DRM_EDID_FEATURE_PM_STANDBY       (1 << 7)

#define DRM_EDID_HDMI_DC_48               (1 << 6)
#define DRM_EDID_HDMI_DC_36               (1 << 5)
#define DRM_EDID_HDMI_DC_30               (1 << 4)
#define DRM_EDID_HDMI_DC_Y444             (1 << 3)

/* YCBCR 420 deep color modes */
#define DRM_EDID_YCBCR420_DC_48		  (1 << 2)
#define DRM_EDID_YCBCR420_DC_36		  (1 << 1)
#define DRM_EDID_YCBCR420_DC_30		  (1 << 0)
#define DRM_EDID_YCBCR420_DC_MASK (DRM_EDID_YCBCR420_DC_48 | \
				    DRM_EDID_YCBCR420_DC_36 | \
				    DRM_EDID_YCBCR420_DC_30)

/* HDMI 2.1 additional fields */
#define DRM_EDID_MAX_FRL_RATE_MASK		0xf0
#define DRM_EDID_FAPA_START_LOCATION		(1 << 0)
#define DRM_EDID_ALLM				(1 << 1)
#define DRM_EDID_FVA				(1 << 2)

/* Deep Color specific */
#define DRM_EDID_DC_30BIT_420			(1 << 0)
#define DRM_EDID_DC_36BIT_420			(1 << 1)
#define DRM_EDID_DC_48BIT_420			(1 << 2)

/* VRR specific */
#define DRM_EDID_CNMVRR				(1 << 3)
#define DRM_EDID_CINEMA_VRR			(1 << 4)
#define DRM_EDID_MDELTA				(1 << 5)
#define DRM_EDID_VRR_MAX_UPPER_MASK		0xc0
#define DRM_EDID_VRR_MAX_LOWER_MASK		0xff
#define DRM_EDID_VRR_MIN_MASK			0x3f

/* DSC specific */
#define DRM_EDID_DSC_10BPC			(1 << 0)
#define DRM_EDID_DSC_12BPC			(1 << 1)
#define DRM_EDID_DSC_16BPC			(1 << 2)
#define DRM_EDID_DSC_ALL_BPP			(1 << 3)
#define DRM_EDID_DSC_NATIVE_420			(1 << 6)
#define DRM_EDID_DSC_1P2			(1 << 7)
#define DRM_EDID_DSC_MAX_FRL_RATE_MASK		0xf0
#define DRM_EDID_DSC_MAX_SLICES			0xf
#define DRM_EDID_DSC_TOTAL_CHUNK_KBYTES		0x3f

/* ELD Header Block */
#define DRM_ELD_HEADER_BLOCK_SIZE	4

#define DRM_ELD_VER			0
# define DRM_ELD_VER_SHIFT		3
# define DRM_ELD_VER_MASK		(0x1f << 3)
# define DRM_ELD_VER_CEA861D		(2 << 3) /* supports 861D or below */
# define DRM_ELD_VER_CANNED		(0x1f << 3)

#define DRM_ELD_BASELINE_ELD_LEN	2	/* in dwords! */

/* ELD Baseline Block for ELD_Ver == 2 */
#define DRM_ELD_CEA_EDID_VER_MNL	4
# define DRM_ELD_CEA_EDID_VER_SHIFT	5
# define DRM_ELD_CEA_EDID_VER_MASK	(7 << 5)
# define DRM_ELD_CEA_EDID_VER_NONE	(0 << 5)
# define DRM_ELD_CEA_EDID_VER_CEA861	(1 << 5)
# define DRM_ELD_CEA_EDID_VER_CEA861A	(2 << 5)
# define DRM_ELD_CEA_EDID_VER_CEA861BCD	(3 << 5)
# define DRM_ELD_MNL_SHIFT		0
# define DRM_ELD_MNL_MASK		(0x1f << 0)

#define DRM_ELD_SAD_COUNT_CONN_TYPE	5
# define DRM_ELD_SAD_COUNT_SHIFT	4
# define DRM_ELD_SAD_COUNT_MASK		(0xf << 4)
# define DRM_ELD_CONN_TYPE_SHIFT	2
# define DRM_ELD_CONN_TYPE_MASK		(3 << 2)
# define DRM_ELD_CONN_TYPE_HDMI		(0 << 2)
# define DRM_ELD_CONN_TYPE_DP		(1 << 2)
# define DRM_ELD_SUPPORTS_AI		(1 << 1)
# define DRM_ELD_SUPPORTS_HDCP		(1 << 0)

#define DRM_ELD_AUD_SYNCH_DELAY		6	/* in units of 2 ms */
# define DRM_ELD_AUD_SYNCH_DELAY_MAX	0xfa	/* 500 ms */

#define DRM_ELD_SPEAKER			7
# define DRM_ELD_SPEAKER_MASK		0x7f
# define DRM_ELD_SPEAKER_RLRC		(1 << 6)
# define DRM_ELD_SPEAKER_FLRC		(1 << 5)
# define DRM_ELD_SPEAKER_RC		(1 << 4)
# define DRM_ELD_SPEAKER_RLR		(1 << 3)
# define DRM_ELD_SPEAKER_FC		(1 << 2)
# define DRM_ELD_SPEAKER_LFE		(1 << 1)
# define DRM_ELD_SPEAKER_FLR		(1 << 0)

#define DRM_ELD_PORT_ID			8	/* offsets 8..15 inclusive */
# define DRM_ELD_PORT_ID_LEN		8

#define DRM_ELD_MANUFACTURER_NAME0	16
#define DRM_ELD_MANUFACTURER_NAME1	17

#define DRM_ELD_PRODUCT_CODE0		18
#define DRM_ELD_PRODUCT_CODE1		19

#define DRM_ELD_MONITOR_NAME_STRING	20	/* offsets 20..(20+mnl-1) inclusive */

#define DRM_ELD_CEA_SAD(mnl, sad)	(20 + (mnl) + 3 * (sad))

struct edid {
	u8 header[8];
	/* Vendor & product info */
	u8 mfg_id[2];
	u8 prod_code[2];
	u32 serial; /* FIXME: byte order */
	u8 mfg_week;
	u8 mfg_year;
	/* EDID version */
	u8 version;
	u8 revision;
	/* Display info: */
	u8 input;
	u8 width_cm;
	u8 height_cm;
	u8 gamma;
	u8 features;
	/* Color characteristics */
	u8 red_green_lo;
	u8 blue_white_lo;
	u8 red_x;
	u8 red_y;
	u8 green_x;
	u8 green_y;
	u8 blue_x;
	u8 blue_y;
	u8 white_x;
	u8 white_y;
	/* Est. timings and mfg rsvd timings*/
	struct est_timings established_timings;
	/* Standard timings 1-8*/
	struct std_timing standard_timings[8];
	/* Detailing timings 1-4 */
	struct detailed_timing detailed_timings[4];
	/* Number of 128 byte ext. blocks */
	u8 extensions;
	/* Checksum */
	u8 checksum;
} __attribute__((packed));

#define EDID_PRODUCT_ID(e) ((e)->prod_code[0] | ((e)->prod_code[1] << 8))

/* Short Audio Descriptor */
struct cea_sad {
	u8 format;
	u8 channels; /* max number of channels - 1 */
	u8 freq;
	u8 byte2; /* meaning depends on format */
};


#endif
