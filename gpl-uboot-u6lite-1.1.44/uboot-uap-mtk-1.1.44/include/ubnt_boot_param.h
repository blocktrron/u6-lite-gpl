#ifndef _UBNT_BOOT_PARAM_H_
#define _UBNT_BOOT_PARAM_H_

struct ubnt_boot_param {
    unsigned image_search_start_addr;
    unsigned image_search_end_addr;
    unsigned image_search_sector_size;
};

#endif
