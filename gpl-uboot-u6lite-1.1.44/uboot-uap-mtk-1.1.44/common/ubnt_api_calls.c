// vim: ts=4:sw=4:expandtab
/*          Copyright Ubiquiti Networks Inc. 2014
**          All Rights Reserved.
*/

#include <common.h>
#include <command.h>
#include <image.h>
#include <asm/string.h>
#include <ubnt_export.h>
#include <version.h>
#include <net.h>
#include <spl.h>
#include <lzma/LzmaTypes.h>
#include <lzma/LzmaDec.h>
#include <lzma/LzmaTools.h>
#include <ubnt_boot_param.h>
#include <configs/mt7621.h>
#include <linux/ctype.h>

#if defined(CONFIG_FIT) || defined(CONFIG_OF_LIBFDT)
    #include <fdt.h>
    #include <linux/libfdt.h>
    #include <image.h>
#endif

#define SPI_FLASH_PAGE_SIZE             0x1000

/* header size comes from create_header() in mkheader.ph */
#define UBOOT_IMAGE_HEADER_SIZE     40


#define UBNT_FLASH_SECTOR_SIZE          (250 * SPI_FLASH_PAGE_SIZE)
#define UBNT_FWUPDATE_SECTOR_SIZE       (16 * SPI_FLASH_PAGE_SIZE)

#define UBNT_BOARD_ID_OFFSET            0xc
#define UBNT_BOARD_ID_PREFIX            0xec
#define UBNT_BOARD_ID_UDMB              0x25
#define UBNT_BOARD_ID_FLEXHD            0x26

#define UBNT_BOARD_AX_ID_PREFIX         0xa6
#define UBNT_BOARD_ID_UAP6IW            0x10
#define UBNT_BOARD_ID_UAP6MESH          0x11
#define UBNT_BOARD_ID_UAP6LITE          0x12
#define UBNT_BOARD_ID_UAP6EXTENDER      0x13
#define UBNT_BOARD_ID_UAP6LR            0x14

#define MAX_BOOTARG_STRING_LENGTH 512
#define ENV_BOOTARG "bootargs"
#define UBOOT_VER_ARG_STR "ubootver="

#define UBNT_APP_MAGIC 0x87564523
#define UBNT_APP_IMAGE_OFFSET           0x0
#define UBNT_APP_MAGIC_OFFSET           0x0
#define UBNT_APP_SHARE_DATA_OFFSET      0x20
#define UBNT_APP_START_OFFSET           0xA0
#define UBNT_APP_STR                    "ubntapp"

#define KERNEL_VALIDATION_UNKNOW    0
#define KERNEL_VALIDATION_VALID     1
#define KERNEL_VALIDATION_INVALID   2


DECLARE_GLOBAL_DATA_PTR;
static int ubnt_debug = 0;
#define DTB_CFG_LEN     64
#if defined(CONFIG_FIT)
static char dtb_config_name[DTB_CFG_LEN];
#endif

static ubnt_app_share_data_t * p_share_data = NULL;
static int ubnt_app_load_and_init_skip = 0;

static inline unsigned int spi_flash_addr(unsigned int off)
{
    return off + CFG_FLASH_BASE;
}

static int load_image(struct spl_image_info *spl_image, ulong addr)
{
    int ret;
    struct image_header *uhdr, hdr;
    size_t lzma_len;

    if (addr % sizeof (void *))
    {
        memcpy(&hdr, (const void *) addr, sizeof (hdr));
        uhdr = &hdr;
    }
    else
    {
        uhdr = (struct image_header *) addr;
    }

    if (image_get_magic(uhdr) == IH_MAGIC) {
        u32 header_size = sizeof(struct image_header);

        if (spl_image->flags & SPL_COPY_PAYLOAD_ONLY) {
            spl_image->load_addr = image_get_load(uhdr);
            spl_image->entry_point = image_get_ep(uhdr);
            spl_image->size = image_get_data_size(uhdr);
        } else {
            spl_image->entry_point = image_get_load(uhdr);
            spl_image->load_addr = spl_image->entry_point -
                header_size;
            spl_image->size = image_get_data_size(uhdr) +
                header_size;
        }
        spl_image->os = image_get_os(uhdr);
        spl_image->name = image_get_name(uhdr);
        if (strcmp(spl_image->name, UBNT_APP_STR)) {
            return -EINVAL;
        }
    } else {
        return -EINVAL;
    }

    if (!spl_image->entry_point)
        spl_image->entry_point = spl_image->load_addr;

    if (uhdr->ih_comp == IH_COMP_NONE)
    {
        if (spl_image->load_addr != (uintptr_t) (addr + sizeof(struct image_header)))
        {
            memmove((void *) (unsigned long) spl_image->load_addr,
                    (void *) (addr + sizeof(struct image_header)),
                    spl_image->size);
        }
    }
    else if (uhdr->ih_comp == IH_COMP_LZMA)
    {
        lzma_len = CONFIG_SYS_BOOTM_LEN;

        ret = lzmaBuffToBuffDecompress((u8 *) spl_image->load_addr, &lzma_len,
                (u8 *) (addr + sizeof(struct image_header)), spl_image->size);
        if (ret)
        {
            printf("Error: LZMA uncompression error: %d\n", ret);
            return ret;
        }

        spl_image->size = lzma_len;
    }
    else
    {
        debug("Warning: Unsupported compression method found in image header at offset 0x%08lx\n", addr);
        return -EINVAL;
    }
    flush_cache((unsigned long) spl_image->load_addr, spl_image->size);

    return 0;
}

static int append_bootargs(char * fmt, ...)
{
    int w_idx = 0;
    char *env_var_str;
    va_list args;
    char bootargs[MAX_BOOTARG_STRING_LENGTH];

    env_var_str = env_get(ENV_BOOTARG);

    if(env_var_str) {
        w_idx += strlen(env_var_str);
        if (w_idx >= sizeof(bootargs) - 1) {
            printf("**WARNING**, bootargs do not fit in buffer.\n");
            return CMD_RET_FAILURE;
        }
        // There appears to be a bug in vsnprintf, such that calling it after snprintf
        // causes no string data to be appended. Use strcpy instead.
        strcpy(bootargs, env_var_str);
        bootargs[w_idx++] = ' ';
        bootargs[w_idx] = '\0';
    }

    va_start(args, fmt);
    w_idx += vsnprintf(bootargs + w_idx, (size_t)(sizeof(bootargs) - w_idx), fmt, args);
    va_end(args);

    if (w_idx >= sizeof(bootargs)) {
        printf("**WARNING**, cannot append bootargs, buffer too small.\n");
        return CMD_RET_FAILURE;
    }

    env_set(ENV_BOOTARG, bootargs);

    return CMD_RET_SUCCESS;
}

void load_sernum_ethaddr(void)
{
    char buf[64];
    uchar *eeprom = (uchar *)spi_flash_addr(UBNT_PART_EEPROM_OFF);
    /* Use default address if the eeprom address is invalid */
    if (eeprom[0] & 0x01 || memcmp(eeprom, "\0\0\0\0\0\0", 6) == 0)
    {
        return;
    }
    snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
            eeprom[0], eeprom[1], eeprom[2],
            eeprom[3], eeprom[4], eeprom[5]);
    env_set("ethaddr", buf);
}
typedef struct {
    ubnt_ramload_t *ramload;
    int is_valid;
    unsigned int addr;
} ramload_state_t;

static ramload_state_t ramload_state;

unsigned long ubnt_ramload_init(unsigned long addr, int update_state)
{
    ubnt_ramload_t *ramload;
    unsigned int xor;
    int is_valid;
    addr -= sizeof(ubnt_ramload_placeholder_t);
    ramload = (ubnt_ramload_t *)addr;
    xor = ramload->magic ^ ramload->size;
    is_valid = (ramload->magic == UBNT_RAMLOAD_MAGIC
                && ramload->xor_m_s == xor);
    if (update_state) {
        ramload_state.is_valid = is_valid;
        ramload_state.ramload = ramload;
    }
    if (is_valid) {
        printf("UBNT Ramload image of size 0x%x found\n", ramload->size);
        addr -= ramload->size;
        if (update_state) {
            ramload_state.addr = addr;
        }
    }
    return addr;
}

/* Loads the application into memory */
int ubnt_app_load(void)
{
    unsigned int magic_value;
    struct ubnt_boot_param *boot_param = \
        (struct ubnt_boot_param *) (CONFIG_SYS_TEXT_BASE - sizeof(struct ubnt_boot_param));
    unsigned int image_search_start = boot_param->image_search_start_addr;
    unsigned int image_search_end = boot_param->image_search_end_addr;
    unsigned int image_search_sector_size = boot_param->image_search_sector_size;
    struct spl_image_info spl_image = {0};

    image_search_start = ALIGN(image_search_start, image_search_sector_size);
    spl_image.flags |= SPL_COPY_PAYLOAD_ONLY;
    while (image_search_start < image_search_end)
    {
        if (!load_image(&spl_image, image_search_start)) {
            printf("load ubntapp ok\n");
            break;
        }
        image_search_start += image_search_sector_size;
    }
    magic_value = *((unsigned int *) ((unsigned int) spl_image.load_addr + UBNT_APP_MAGIC_OFFSET));
    if ( UBNT_APP_MAGIC != htonl(magic_value) ) {
        printf(" *WARNING*: UBNT APP Magic mismatch, addr=%x, magic=%x \n",
               ((unsigned int) spl_image.load_addr + UBNT_APP_MAGIC_OFFSET),
               magic_value);
        return -1;
    }

    p_share_data = (ubnt_app_share_data_t *) ((unsigned int) spl_image.load_addr + UBNT_APP_SHARE_DATA_OFFSET);
    env_set_addr("ubntaddr", (void *) ((unsigned int) spl_image.load_addr + UBNT_APP_START_OFFSET));

    return 0;
}

static void erase_spi_flash(int offset, int size) {
    char runcmd[256];
    int ret;

    snprintf(runcmd, sizeof(runcmd), "sf erase 0x%x 0x%x",
             offset, size);
    if (ubnt_debug) {
        printf("DEBUG, erase spi flash command: %s\n", runcmd);
    }
    ret = run_command(runcmd, 0);
    if (ret != CMD_RET_SUCCESS) {
        printf(" *WARNING*: flash erase error, command: %s, ret: %d \n", runcmd, ret);
    }
}

#if defined(CONFIG_FIT)
/* Ported from boot_get_kernel() of cmd_bootm.c. */
static int ubnt_validate_fit_kernel(const void *fit)
{
    int image_valid = KERNEL_VALIDATION_INVALID;
    char runcmd[256];
    int ret = -1;

    snprintf(runcmd, sizeof(runcmd), "fdt addr 0x%p", fit);
    ret = run_command(runcmd, 0);
    if (ret != CMD_RET_SUCCESS) {
        printf("UBNT, Bad fdt address.\n");
        return image_valid;
    }

    snprintf(runcmd, sizeof(runcmd), "fdt checksign");
    ret = run_command(runcmd, 0);
    if (ret != CMD_RET_SUCCESS) {
        printf("UBNT, Bad Data Hash for FIT kernel image.\n");
        return image_valid;
    }

    image_valid = KERNEL_VALIDATION_VALID;
    return image_valid;
}
#endif /* CONFIG_FIT */

/* This function only validate FIT kernel image for now. */
static int ubnt_validate_kernel_image(const void * img_addr) {
    int image_valid = KERNEL_VALIDATION_UNKNOW;
    int image_type = genimg_get_format(img_addr);

    switch (image_type) {
#if defined(CONFIG_FIT)
    case IMAGE_FORMAT_FIT:
        image_valid = ubnt_validate_fit_kernel(img_addr);
        break;
#endif
    default:
        /* legacy image is validated by ubnt_app */
        break;
    }

    return image_valid;
}

static int get_image_size(unsigned int img_addr, int *size) {
#if defined (CONFIG_IMAGE_FORMAT_LEGACY)
    image_header_t *hdr;
#endif
    char *fdt;
    int image_type;
    int ret = -1;

    image_type = genimg_get_format((void *)img_addr);
    switch (image_type) {
#if defined (CONFIG_IMAGE_FORMAT_LEGACY)
    case IMAGE_FORMAT_LEGACY:
        hdr = (image_header_t *)img_addr;
        if (uimage_to_cpu(hdr->ih_magic) == IH_MAGIC)
        {
            *size = uimage_to_cpu(hdr->ih_size) + sizeof(image_header_t);
            ret = 0;
        }
        break;
#endif
#if defined(CONFIG_FIT)
    case IMAGE_FORMAT_FIT:
        fdt = (char *)img_addr;
        *size = fdt_totalsize(fdt);
        ret = 0;
        break;
#endif
    default:
        break;
    }

    return ret;
}

static int ubnt_read_kernel_image(void) {
    unsigned int load_addr = CONFIG_SYS_LOAD_ADDR;
    unsigned int current_offset;
    int current_size, size_left, img_size = 0;
    unsigned int offset;
    unsigned int size;
    char runcmd[256];
    int ret;
    int image_valid = KERNEL_VALIDATION_INVALID;

    if (p_share_data->kernel_index != 0) {
        offset = UBNT_PART_KERNEL1_OFF;
        size = UBNT_PART_KERNEL1_SIZE;
    } else {
        offset = UBNT_PART_KERNEL0_OFF;
        size = UBNT_PART_KERNEL0_SIZE;
    }

    current_offset = offset;
    size_left = (int)size;
    current_size = (size_left < SPI_FLASH_PAGE_SIZE) ? size_left : SPI_FLASH_PAGE_SIZE;

    /* read first page to find f/w size */
    snprintf(runcmd, sizeof(runcmd), "sf read 0x%x 0x%x 0x%x", load_addr, current_offset, current_size);
    ret = run_command(runcmd, 0);
    if (ret != CMD_RET_SUCCESS) {
        printf(" *WARNING*: flash read error (Kernel), command: %s, ret: %d \n", runcmd, ret);
        return -1;
    }
    ret = get_image_size(load_addr, &img_size);
    if (ret == 0) {
        size_left = img_size < size ? img_size : size;
        /* align to page size */
        size_left = (size_left + SPI_FLASH_PAGE_SIZE - 1) & (~(SPI_FLASH_PAGE_SIZE - 1));
    } else {
        return -1;
    }
    printf("reading kernel %d from: 0x%x, size: 0x%08x\n", p_share_data->kernel_index, offset, size_left);

    size_left -= current_size;
    load_addr += current_size;
    current_offset += current_size;

    while ( size_left > 0 ) {
        if ( size_left > UBNT_FLASH_SECTOR_SIZE) {
            current_size = UBNT_FLASH_SECTOR_SIZE;
        } else {
            current_size = size_left;
        }
        snprintf(runcmd, sizeof(runcmd), "sf read 0x%x 0x%x 0x%x", load_addr, current_offset, current_size);
        ret = run_command(runcmd, 0);
        if (ret != CMD_RET_SUCCESS) {
            printf(" *WARNING*: flash read error (Kernel), command: %s, ret: %d \n", runcmd, ret);
            return image_valid;
        }

        ubnt_app_continue(UBNT_EVENT_READING_FLASH);
        size_left -= current_size;
        load_addr += current_size;
        current_offset += current_size;
    }

    p_share_data->kernel_addr = CONFIG_SYS_LOAD_ADDR;
    image_valid = ubnt_validate_kernel_image((void *)p_share_data->kernel_addr);

    if (ubnt_debug) {
        printf("UBNT, read kernel image done, kernel valid: %d.\n", image_valid);
    }

    return image_valid;
}

static int ubnt_update_partition(
    const char * partition_name,
    unsigned int partition_offset,
    unsigned int partition_size,
    unsigned int image_address,
    unsigned int image_size
    )
{
    char runcmd[256];
    int ret;

    printf("Updating %s partition (and skip identical blocks) ...... ", partition_name);
    if (image_size > partition_size) {
        printf("Cannot update partition %s with image size %x. "
               "Partition is only %x.\n",
               partition_name, image_size, partition_size);
        return -1;
    }

    snprintf(runcmd, sizeof(runcmd), "sf update 0x%x 0x%x 0x%x", image_address, partition_offset, image_size);
    if(ubnt_debug) {
        printf("DEBUG, update uboot command: %s\n", runcmd);
    }
    ret = run_command(runcmd, 0);
    if (ret != CMD_RET_SUCCESS) {
        printf(" *WARNING*: flash update error (%s), command: %s, ret: %d \n", partition_name, runcmd, ret);
        return -1;
    }
    printf("done.\n");

    return 0;
}

static int ubnt_update_uboot_image(void)
{
    return ubnt_update_partition(
        UBNT_PART_UBOOT_NAME,
        UBNT_PART_UBOOT_OFF,
        UBNT_PART_UBOOT_SIZE,
        p_share_data->update.uboot.addr,
        p_share_data->update.uboot.size
        );
}

static int ubnt_update_bootselect(void)
{
    return ubnt_update_partition(
        UBNT_PART_BOOTSELECT_NAME,
        UBNT_PART_BOOTSELECT_OFF,
        UBNT_PART_BOOTSELECT_SIZE,
        p_share_data->update.bootselect.addr,
        p_share_data->update.bootselect.size
        );
}

static int ubnt_update_kernel_image(int kernel_index)
{
    unsigned int load_addr;
    unsigned int current_offset, current_size, size_left;
    char runcmd[256];
    const char * partition_name = NULL;
    int ret;

    if (kernel_index != 0) {
        partition_name = UBNT_PART_KERNEL1_NAME;
        current_offset = UBNT_PART_KERNEL1_OFF;
        load_addr = p_share_data->update.kernel1.addr;
        size_left = p_share_data->update.kernel1.size;
    } else {
        partition_name = UBNT_PART_KERNEL0_NAME;
        current_offset = UBNT_PART_KERNEL0_OFF;
        load_addr = p_share_data->update.kernel0.addr;
        size_left = p_share_data->update.kernel0.size;
    }

    printf("Updating %s partition (and skip identical blocks) ...... ", partition_name);
    while( size_left > 0 ) {
        if( size_left > UBNT_FWUPDATE_SECTOR_SIZE) {
            current_size = UBNT_FWUPDATE_SECTOR_SIZE;
        } else {
            current_size = size_left;
        }

        snprintf(runcmd, sizeof(runcmd), "sf update 0x%x 0x%x 0x%x", load_addr, current_offset, current_size);
        ret = run_command(runcmd, 0);
        if (ret != CMD_RET_SUCCESS) {
            printf(" *WARNING*: flash update error (%s), command: %s, ret: %d \n", partition_name, runcmd, ret);
            return -1;
        }

        ubnt_app_continue(UBNT_EVENT_WRITING_FLASH);
        size_left -= current_size;
        load_addr += current_size;
        current_offset += current_size;
    }
    printf("done.\n");

    if(ubnt_debug) {
        printf("UBNT, update kernel done.\n");
    }

    return 0;
}

typedef enum {
    STATE_1,
    STATE_2,
    STATE_3,
    STATE_4,
    STATE_5,
    STATE_END,
    STATE_ERROR
} state_type;

static int ubnt_check_fw_version_string(const char *ptr)
{
    /* currenly, only x.x.xxxxx.ooooo is allowed */
    state_type state = STATE_1;
    int count = 0;

    while (*ptr != '\0') {
        switch(state) {
            case STATE_1:
                if (isdigit(*ptr)) {
                    state = STATE_2;
                } else {
                    state = STATE_ERROR;
                }
                break;
            case STATE_2:
                if (*ptr == '.') {
                    state = STATE_3;
                } else {
                    state = STATE_ERROR;
                }
                break;
            case STATE_3:
                if (isdigit(*ptr)) {
                    state = STATE_4;
                } else {
                    state = STATE_ERROR;
                }
                break;
            case STATE_4:
                if (*ptr == '.') {
                    state = STATE_5;
                } else {
                    state = STATE_ERROR;
                }
                break;
            case STATE_5:
                if (count > 5) {
                    state = STATE_ERROR;
                } else if (isdigit(*ptr)) {
                    state = STATE_5;
                    count++;
                } else if (*ptr == '.'){
                    state = STATE_END;
                } else {
                    state = STATE_ERROR;
                }
                break;
            default:
                state = STATE_ERROR;
        }
        if (state == STATE_ERROR || state == STATE_END)
            break;
        ptr++;
    }

    if (state != STATE_END)
        return -1;
    return 0;
}

static int ubnt_set_fw_version_env(void)
{
    char runcmd[128], fw_version[13] = {0};
    unsigned char *start, *end;
    const int PREFETCH_HEADER_SIZE = 0X100;
    int ret, i;

    ret = run_command("sf probe", 0);
    if (ret != CMD_RET_SUCCESS) {
        printf(" *WARNING*: sf probe failed, ret: %d.\n", ret);
        return -1;
    }

    ret = ubnt_app_load();
    if (ret != 0)
        return -1;

    /* In this case, we have no chance to pass any parameters to uappinit */
    snprintf(runcmd, sizeof(runcmd), "go ${ubntaddr} uappinit");
    ret = run_command(runcmd, 0);
    if (ret != 0) {
        /* do not return here to give urescue a chance to run */
        printf(" *WARNING*: uappinit failed, ret: %d.\n", ret);
        return -1;
    }

    ubnt_app_load_and_init_skip = 1;

#if defined(CONFIG_FIT)
    const unsigned int offset = (p_share_data->kernel_index != 0) ?
        UBNT_PART_KERNEL1_OFF : UBNT_PART_KERNEL0_OFF;

    snprintf(runcmd, sizeof(runcmd), "sf read 0x%x 0x%x 0x%x", CONFIG_SYS_LOAD_ADDR,
            offset, PREFETCH_HEADER_SIZE);
    ret = run_command(runcmd, 0);
    if (ret != CMD_RET_SUCCESS) {
        printf(" *WARNING*: flash read error (Kernel), command: %s, ret: %d \n", runcmd, ret);
        return -1;
    }

    /* Purpose: Find beginning of FIT image description for the following string
     * "Ubiquiti Linux v4.0.42.10433.190518.0923"
     */
    const char description_hdr[] = "Ubiquiti Linux ";
    int desc_len = strlen(description_hdr);

    /* Step 1: check if the starting address of CONFIG_SYS_LOAD_ADDR contains
     * FDT_MAGIC
     */
    if (fdt32_to_cpu(*((unsigned int * )CONFIG_SYS_LOAD_ADDR)) != FDT_MAGIC) {
        return -1;
    }

    /* Step 2: parse the FDT property from the begining of dt_struct */
    start = (unsigned char *)CONFIG_SYS_LOAD_ADDR + fdt_off_dt_struct(CONFIG_SYS_LOAD_ADDR);
    end = (unsigned char *)CONFIG_SYS_LOAD_ADDR + PREFETCH_HEADER_SIZE;

    if (fdt32_to_cpu(*((unsigned int *) start)) != FDT_BEGIN_NODE) {
        return -1;
    }

    /* Step 3: Find the specific FDT_PROP that contains the version string */
    for (; start < end; start += FDT_TAGSIZE) {
        if (fdt32_to_cpu(*((unsigned int *) start)) == FDT_PROP) {
            const struct fdt_property *prop = (struct fdt_property *) start;

            /* prop->len = length of string plus '\0' */
            if (prop->len < (desc_len + 1)) {
                continue;
            }

            if (strncmp(prop->data, description_hdr, desc_len) == 0) {
                const char *ptr = prop->data + desc_len;
                char *ptr_end;

                /* search the prefix of 'v' */
                ptr = strchr(ptr, 'v');
                if (ptr == NULL) {
                    return -1;
                }

                /* skip the prefix of 'v' */
                ptr = ptr + 1;
                if (ubnt_check_fw_version_string(ptr) < 0) {
                    return -1;
                }

                snprintf(fw_version, sizeof(fw_version), "%s", ptr);
                ptr_end = fw_version;
                /* truncate from the third '.' */
                for (i = 0; i < 3; i++) {
                    ptr_end = strchr(ptr_end, '.');
                    if (ptr_end == NULL)
                        return -1;
                    ptr_end = ptr_end + 1;
                }
                *(ptr_end - 1) = '\0';

                env_set("fw_version", fw_version);
                return 0;
            } else {
                continue;
            }
        }
    }
#else
    /* For non-FIT image, we only return -1 */
#endif
    return -1;
}

static int ubnt_set_ble_mode_env(void) {
    uint8_t *ubnt_board_id = (uint8_t*) spi_flash_addr(UBNT_PART_EEPROM_OFF + UBNT_BOARD_ID_OFFSET);

    if (ubnt_board_id[0] == UBNT_BOARD_ID_PREFIX) {
        switch (ubnt_board_id[1]) {
        case UBNT_BOARD_ID_UDMB:
            env_set("ble_mode", "serial");
            env_set("device_model", "UDMB");
            break;
        case UBNT_BOARD_ID_FLEXHD:
            env_set("ble_mode", "usb");
            env_set("device_model", "FLEXHD");
            break;
        default:
            env_set("ble_mode", "none");
            return -1;
        }
        return 0;
    } else if (ubnt_board_id[0] == UBNT_BOARD_AX_ID_PREFIX) {
        switch (ubnt_board_id[1]) {
        case UBNT_BOARD_ID_UAP6IW:
            env_set("ble_mode", "serial");
            env_set("device_model", "U6-IW");
            break;
        case UBNT_BOARD_ID_UAP6MESH:
            env_set("ble_mode", "serial");
            env_set("device_model", "U6-MESH");
            break;
        case UBNT_BOARD_ID_UAP6LITE:
            env_set("ble_mode", "serial");
            env_set("device_model", "U6-LITE");
            break;
        case UBNT_BOARD_ID_UAP6EXTENDER:
            env_set("ble_mode", "serial");
            env_set("device_model", "U6-EXTENDER");
            break;
        case UBNT_BOARD_ID_UAP6LR:
            env_set("ble_mode", "serial");
            env_set("device_model", "U6-LR");
            break;

        default:
            env_set("ble_mode", "none");
            return -1;
        }
        return 0;
    }
    return -1;
}

static int ubnt_set_mac_addr_env(void)
{
    char buf[64];
    uchar *eeprom = spi_flash_addr(UBNT_PART_EEPROM_OFF);
    /* Use default address if the eeprom address is invalid */
    if (eeprom[0] & 0x01 || memcmp(eeprom, "\0\0\0\0\0\0", 6) == 0) {
        return;
    }
    snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
             eeprom[0], eeprom[1], eeprom[2],
             eeprom[3], eeprom[4], eeprom[5]);
    env_set("macaddr", buf);
}

int do_ubnt_set_ble_env(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
    char *ubnt_flag = NULL;

    /* UAP/UAP6 WAR: uboot MTD may be corrupted by certain reason.
     * fw_setenv will restore it to bootp. Hence, restore
     * it back to bootubnt to have a chance for upgrading  */
    ubnt_flag = env_get("devmode");

    if (!ubnt_flag || (strncmp(ubnt_flag, "TRUE", 4))) {
        ubnt_flag = env_get("bootcmd");

        if (ubnt_flag && (strncmp(ubnt_flag, "bootubnt", 8))) {
            run_command("env default -a", 0);
            run_command("saveenv", 0);
        }
    }

    if (ubnt_set_ble_mode_env() != 0) {
        printf(" *WARNING*: Could not set BLE mode for this device\n");
        return -1;
    }
    if (ubnt_set_fw_version_env() != 0) {
        printf(" *WARNING*: Could not parse FW version, please check FW format\n");
        env_set("fw_version", "9.9.9");
    }
    ubnt_set_mac_addr_env();
    return 0;
}

U_BOOT_CMD(setbleenv, 1, 0, do_ubnt_set_ble_env,
           "setbleenv - set ubnt ble environment",
           "setbleenv - set ubnt ble environment");

int ubnt_app_start(int argc, char *const argv[]) {
    char *ubnt_flag;
    char tmp_str[64];
    int ret, i;
    int ubnt_app_event;
    int do_urescue = 0;
    int do_uresetbs = 0;
    char *is_ble_stp = NULL;

    ret = run_command("sf probe", 0);
    if (ret != CMD_RET_SUCCESS) {
        printf(" *WARNING*: sf probe failed, ret: %d.\n", ret);
        return -1;
    }

    if (ubnt_app_load_and_init_skip != 1) {
        /* UAP/UAP6 WAR: uboot MTD may be corrupted by certain reason.
         * fw_setenv will restore it to bootp. Hence, restore
         * it back to bootubnt to have a chance for upgrading  */
        ubnt_flag = env_get("devmode");

        if (!ubnt_flag || (strncmp(ubnt_flag, "TRUE", 4))) {
            ubnt_flag = env_get("bootcmd");

            if (ubnt_flag && (strncmp(ubnt_flag, "bootubnt", 8))) {
                run_command("env default -a", 0);
                run_command("saveenv", 0);
            }
        }

        ret = ubnt_app_load();
        if (ret != 0) {
            return -1;
        }

        snprintf(tmp_str, sizeof(tmp_str), "go ${ubntaddr} uappinit");
        for (i = 0; i < argc; i++) {
            snprintf(tmp_str, sizeof(tmp_str), "%s %s", tmp_str, argv[i]);
        }
        if(ubnt_debug) {
            printf("UBNT, init command: %s\n", tmp_str);
        }
        ret = run_command(tmp_str, 0);
        if (ret != 0) {
            /* do not return here to give urescue a chance to run */
            printf(" *WARNING*: uappinit failed, ret: %d.\n", ret);
            return -1;
        }
    }

    if (ramload_state.is_valid) {
        snprintf(tmp_str, sizeof(tmp_str), "go ${ubntaddr} uramload 0x%x 0x%x",
                ramload_state.addr, ramload_state.ramload->size);
        ret = run_command(tmp_str, 0);
        if (ret == 0) {
            append_bootargs("ubntramloaded");
            printf("Booting from ramload\n");
            return 0;
        }
    }

    ubnt_app_event = UBNT_EVENT_UNKNOWN;
    while(1) {
        snprintf(tmp_str, sizeof(tmp_str),
                "go ${ubntaddr} uappcontinue %d", ubnt_app_event);
        run_command(tmp_str, 0);
        ubnt_app_event = UBNT_EVENT_UNKNOWN;

        ubnt_flag = env_get("ubnt_do_boot");
        if (ubnt_flag && (!strncmp(ubnt_flag, "TRUE", 4))) {
            break;
        }

        ubnt_flag = env_get("do_urescue");
        if (ubnt_flag && (!strncmp(ubnt_flag, "TRUE", 4))) {
            do_urescue = 1;
            break;
        } else if ((ubnt_flag = env_get("do_uresetbs"))
                && (!strncmp(ubnt_flag, "TRUE", 4))){
            do_uresetbs = 1;
            break;
        } else if ((ubnt_flag = env_get("do_uindicaterma"))
                && (!strncmp(ubnt_flag, "TRUE", 4))){
                ubnt_app_continue(UBNT_EVENT_RMA_FLASH);
        } else {
            ret = ubnt_read_kernel_image();

            if (ret == KERNEL_VALIDATION_INVALID) {
                ubnt_app_event = UBNT_EVENT_KERNEL_INVALID;
            } else if (ret == KERNEL_VALIDATION_VALID) {
                ubnt_app_event = UBNT_EVENT_KERNEL_VALID;
            }
        }
    }

    ubnt_flag = env_get("ubnt_clearcfg");
    if (ubnt_flag && (!strncmp(ubnt_flag, "TRUE", 4))) {
        printf("Erasing %s partition ...... ", UBNT_PART_CFG_NAME);
        erase_spi_flash(UBNT_PART_CFG_OFF,
                        UBNT_PART_CFG_SIZE);
        printf("done.\n");
    }

    if (do_urescue) {
        env_set_addr("loadaddr", (void *) CONFIG_SYS_LOAD_ADDR);
        env_set_addr("gd_fdt_addr", (void *) gd_fdt_blob());
        while(1) {
            run_command("tftpsrv", 0);
            ubnt_app_continue(UBNT_EVENT_TFTP_DONE);
            ubnt_flag = env_get("ubnt_do_boot");
            if (ubnt_flag && (!strncmp(ubnt_flag, "TRUE", 4))) {
                break;
            }
        }
        /* has done urescue, need to do following
         * 1) update kernel 0
         * 2) erase vendor data partition
         * 3) erase cfg partition
         * 4) erase uboot environment variables
         */
        if(ubnt_debug) {
            printf("addresses, kernel0: 0x%08x, kernel1: 0x%08x, uboot: 0x%08x, "
                    "bootselect: 0x%08x\n",
                    p_share_data->update.kernel0.addr,
                    p_share_data->update.kernel1.addr,
                    p_share_data->update.uboot.addr,
                    p_share_data->update.bootselect.addr
                  );
        }

        if (p_share_data->update.uboot.addr != UBNT_INVALID_ADDR) {
            ubnt_update_uboot_image();
        }
        if (p_share_data->update.bootselect.addr != UBNT_INVALID_ADDR) {
            ubnt_update_bootselect();
        }
        if (p_share_data->update.kernel0.addr != UBNT_INVALID_ADDR) {
            ubnt_update_kernel_image(0);
        }
        if (p_share_data->update.kernel1.addr != UBNT_INVALID_ADDR) {
            ubnt_update_kernel_image(1);
        }

        // Move here, original place will clear necessary env variable and cause
        // ubnt_fw_check failed
        ubnt_flag = env_get("ubnt_clearenv");
        is_ble_stp = env_get("is_ble_stp");
        if (ubnt_flag && (!strncmp(ubnt_flag, "TRUE", 4))) {
            printf("Erasing %s partition ...... ", UBNT_PART_UBOOT_ENV_NAME);
            run_command("env default -a", 0);
            load_sernum_ethaddr();
            if (is_ble_stp) {
                env_set("is_ble_stp", is_ble_stp);
            }
            run_command("saveenv", 0);
            printf("done.\n");
        }

        /* reset system */
        run_command("reset", 0);
        return 0;
    }

    if (do_uresetbs) {
        if (p_share_data->update.bootselect.addr != UBNT_INVALID_ADDR) {
            ubnt_update_bootselect();
        }
        /* reset system */
        run_command("reset", 0);
    }

    return 0;
}

int ubnt_is_kernel_loaded(void) {
    char *ubnt_flag;
    int ret;
    ubnt_flag = env_get("ubnt_do_boot");
    if (ubnt_flag && (!strncmp(ubnt_flag, "TRUE", 4))) {
        ret = 1;
    } else {
        ret = 0;
    }

    return ret;
}

void ubnt_app_continue(int event) {
    char runcmd[128];
    char *ubnt_flag;

    ubnt_flag = env_get("ubntaddr");
    if (ubnt_flag) {
        snprintf(runcmd, sizeof(runcmd),
                "go ${ubntaddr} uappcontinue %d", event);
        run_command(runcmd, 0);
    } else {
        if(ubnt_debug) {
            printf("ubnt_app_continue, ubntaddr not set. \n");
        }
    }
}

/**
 * Load Kernel image and transfer control to kernel
 */
static int load_kernel(unsigned int addr)
{
    char runcmd[256];
#if defined(CONFIG_FIT)
    unsigned char bootconfig;
#endif

    if (ubnt_debug) {
        run_command("printenv bootargs", 0);
        printf("Booting from flash\n");
    }

#if defined(CONFIG_FIT)
    bootconfig = simple_strtol(env_get("bootconfig"), NULL, 0);

    if (bootconfig <= 0) {
        return CMD_RET_FAILURE;
    }

    snprintf(dtb_config_name,
                sizeof(dtb_config_name),"#config@%d", bootconfig);

    snprintf(runcmd, sizeof(runcmd),"bootm 0x%x%s",
        addr,
        dtb_config_name);
#else
    snprintf(runcmd, sizeof(runcmd),"bootm 0x%x\n", addr);
#endif

    if (run_command(runcmd, 0) != CMD_RET_SUCCESS) {
        return CMD_RET_FAILURE;
    }

    return CMD_RET_SUCCESS;
}

/**
 * set boot argument: uboot version
 */
static void set_bootargs_uboot_version(void) {
    const char uboot_version[] = U_BOOT_VERSION;
    char *ver_start, *ver_end_delimiter;
    int ver_str_len;

    /* uboot version example:
     * U-Boot 2012.07-00069-g7963d5b-dirty [UniFi,ubnt_feature_ubntapp.69]
     */
    ver_start = strchr(uboot_version, ',') + 1;
    ver_end_delimiter = strchr(uboot_version, ']');
    ver_str_len = ver_end_delimiter - ver_start;

    if (append_bootargs("%s%.*s", UBOOT_VER_ARG_STR, ver_str_len, ver_start)
            != CMD_RET_SUCCESS) {
        printf("**WARNING**, buffer too small for uboot version bootargs.\n");
    }

    if (ubnt_debug) {
        printf("uboot version: %s\nbootargs: %s\n", uboot_version, env_get(ENV_BOOTARG));
    }
}

#ifdef CONFIG_UBNT_PERSISTENT_LOG
static void set_bootargs_ramoops(int * pmem_size) {

    if (append_bootargs("ramoops.mem_address=0x%x ramoops.mem_size=%d ramoops.ecc=1",
            *pmem_size,
            CONFIG_SYS_MEM_TOP_HIDE) != CMD_RET_SUCCESS)
        printf("**WARNING**, buffer too small for ramoops bootargs.\n");
}
#endif

static void set_bootargs_ramload(int * pmem_size) {
    char * ramload_size = env_get("ubntramload.size");
    int size;

    ubnt_ramload_init(*pmem_size, 1);
    // Invalidate magic so can't be used again on reboot.
    ramload_state.ramload->magic = 0;

    *pmem_size = (int) ramload_state.ramload;
    if (!ramload_size)
        return;

    size = simple_strtoul(ramload_size, NULL, 0);
    *pmem_size -= size;
    if(append_bootargs("ubntramload.size=0x%x ubntramload.addr=0x%x ubntramload.control=0x%x",
            size,
            *pmem_size,
            (unsigned int)ramload_state.ramload) != CMD_RET_SUCCESS)
        printf("**WARNING**, buffer too small for ramload bootargs.\n");
}

static int do_bootubnt(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
    int ret;
    int linux_mem = gd->ram_size;

    printf("ubnt boot ...\n");
    set_bootargs_uboot_version();
#ifdef CONFIG_UBNT_PERSISTENT_LOG
    set_bootargs_ramoops(&linux_mem);
#endif
    set_bootargs_ramload(&linux_mem);
    append_bootargs("mem=%luK", linux_mem >> 10);

#if defined(CONFIG_FIT)
    /*
     * set fdt_high parameter to all ones, so that u-boot will pass the
     * loaded in-place fdt address to kernel instead of relocating the fdt.
     */
    if (env_set_addr("fdt_high", (void *)0xffffffff)
            != CMD_RET_SUCCESS) {
        printf("Cannot set fdt_high to %x to avoid relocation\n",
            0xffffffff);
    }
#endif

    ret = ubnt_app_start(--argc, &argv[1]);
    if (ret != 0) {
        printf("UBNT app error.\n");
        return CMD_RET_FAILURE;
    }

    return load_kernel(p_share_data->kernel_addr);
}

U_BOOT_CMD(bootubnt, 5, 0, do_bootubnt,
       "bootubnt- ubnt boot from flash device\n",
       "bootubnt - Load image(s) and boots the kernel\n");

#define VERSION_LEN_MAX 128
/* The SSID is located after two 6-byte MAC addresses */
#define UBNT_SSID_OFFSET 12

uint8_t prepare_beacon(uint8_t *buf) {
    uint8_t *pkt = buf;
    uint8_t *len_p;

    // Lengths
    uint8_t total_len;
    uint8_t header_len;
    uint8_t body_len;

    // U-Boot Version
    char *version = U_BOOT_VERSION;
    uint8_t version_len = strlen(version);
    if (version_len > VERSION_LEN_MAX)
        version_len = VERSION_LEN_MAX;

    // SSID
    uint16_t *ubnt_ssid_p = (uint16_t *) spi_flash_addr(UBNT_PART_EEPROM_OFF + UBNT_SSID_OFFSET);

    /* header */
    *pkt++ = 0x03; // version
    *pkt++ = 0x07; // type (net rescue beacon)
    len_p = pkt++; // beacon size
    *pkt++ = 0x03; // bootloader version

    header_len = pkt - buf;

    /* uboot */
    *pkt++ = version_len;
    memcpy(pkt, version, version_len);
    pkt += version_len;

    /* board */
    memcpy(pkt, ubnt_ssid_p, sizeof(*ubnt_ssid_p));
    pkt += sizeof(*ubnt_ssid_p);

    total_len = pkt - buf;
    body_len = total_len - header_len;

    // Include size in beacon.
    *len_p = body_len;

    return total_len;
}
