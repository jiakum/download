
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "edid.h"


static const u8 edid_header[] = {
	0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00
};

static int drm_edid_header_is_valid(const void *_edid)
{
	const struct edid *edid = _edid;
	int i, score = 0;

	for (i = 0; i < sizeof(edid_header); i++) {
		if (edid->header[i] == edid_header[i])
			score++;
	}

	return score;
}

static int edid_block_compute_checksum(const void *_block)
{
	const u8 *block = _block;
	int i;
	u8 csum = 0, crc = 0;

	for (i = 0; i < EDID_LENGTH - 1; i++)
		csum += block[i];

	crc = 0x100 - csum;

	return crc;
}

int main(int argc, char **argv)
{
    FILE *fp = fopen("edid.txt", "rb");
    struct edid ed;
    int i, first = 1;

    if (fp == NULL) {
        printf("can not open edid.txt, %s\n", strerror(errno));
        return 0;
    }

    while (fread((void *)&ed, 1, sizeof(ed), fp) == sizeof(ed)) {
        u8 *ext = (u8*)&ed;
        int n;
        if (first) {
            printf("check edid header:%d, version:%d, revision:%d\n",
                    drm_edid_header_is_valid(&ed), ed.version, ed.revision);
            printf("check edid crc: %08x: %08x\n", ed.checksum, edid_block_compute_checksum(&ed));
            n = 4;
        } else {
            n = (127 - ext[2]) / 18;
        }
        for (i = 0;i < n;i++) {
            struct detailed_timing *dt;
            if (first) {
                dt = &ed.detailed_timings[i];
            } else {
                dt = (struct detailed_timing *)(ext + ext[2] + 18 * i);
            }
            if (dt->pixel_clock != 0 ) {
                struct detailed_pixel_timing *pt = &dt->data.pixel_data;
                printf ("detailed timimng: %d, pixel clock:%d\n", i, dt->pixel_clock);
                printf ("hactive:%d, vactive:%d, hblank:%d, vblank:%d, hsync offset:%d, hsync: pulse:%d, "
                        "vsync offset:%d, vsync pulse:%d, mm width:%d, mm height:%d, misc:%x\n",
                        (u32)pt->hactive_lo | (((u32)pt->hactive_hblank_hi & 0x0f0) << 4),
                        (u32)pt->vactive_lo | (((u32)pt->vactive_vblank_hi & 0x0f0) << 4),
                        (u32)pt->hblank_lo | (((u32)pt->hactive_hblank_hi & 0x0f) << 8),
                        (u32)pt->vblank_lo | (((u32)pt->vactive_vblank_hi & 0x0f) << 8),
                        (u32)pt->hsync_offset_lo | (((u32)pt->hsync_vsync_offset_pulse_width_hi & 0x0c0) << 2),
                        (u32)pt->hsync_pulse_width_lo | (((u32)pt->hsync_vsync_offset_pulse_width_hi & 0x030) << 4),
                        ((u32)pt->vsync_offset_pulse_width_lo >> 4) | (((u32)pt->hsync_vsync_offset_pulse_width_hi & 0x0c) << 2),
                        ((u32)pt->vsync_offset_pulse_width_lo & 0x0f) | (((u32)pt->hsync_vsync_offset_pulse_width_hi & 0x03) << 4),
                        (u32)pt->width_mm_lo | (((u32)pt->width_height_mm_hi & 0x0f0) << 4),
                        (u32)pt->height_mm_lo | (((u32)pt->width_height_mm_hi & 0x0f) << 8),
                        pt->misc
                       );
            } else {
                struct detailed_non_pixel *np = &dt->data.other_data;
                printf ("detailed none pixel time:%d, type:%x\n", i, np->type);
                if (np->type == 0xfc) {
                    unsigned char str[16];
                    strncpy(str, np->data.str.str, 13);
                    str[13] = '\0';
                    printf ("monitor name:%s\n", str);
                } else if (np->type == 0xfd) {
                    struct detailed_data_monitor_range *range = &np->data.range;
                    printf ("min_vfreq:%d, max_vfreq:%d, min_hfreq_khz:%d, max_hfreq_khz:%d, pixel_clock_mhz:%d, flags:%x\n",
                            range->min_vfreq, range->max_vfreq,
                            range->min_hfreq_khz, range->max_hfreq_khz,
                            range->pixel_clock_mhz, range->flags);
                }
            }
        }
        printf ("finish one:\n\n");
        first = 0;
    }

    fclose(fp);

    return 0;
}
