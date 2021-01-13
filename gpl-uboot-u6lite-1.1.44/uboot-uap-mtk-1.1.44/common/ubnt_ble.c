#include <common.h>
#include <command.h>
#include <console.h>
#include <dm.h>
#include <dm/uclass-internal.h>
#include <memalign.h>
#include <asm/byteorder.h>
#include <asm/unaligned.h>
#include <part.h>
#include <usb.h>
#include <stdio_dev.h>
#include <configs/mt7621.h>
#include <ubnt_export.h>
#include <serial.h>

#ifdef CONFIG_BLE_ADVERTISTING_EN
//#include <dm.h>
//#include <serial.h>
#include <asm/addrspace.h>
#include <stdio.h>
#include <errno.h>
#include <asm/gpio.h>


//#include <common.h>
#include <linux/types.h>
#include <debug_uart.h>
#include <asm/spl.h>
#include <asm/io.h>
#include <mach/mt7621_regs.h>


#define MT7621_UART3_BASE	0x1e000e00
#define CMD_RET_OK 1
#define CMD_RET_FAIL 0
#define TRUE 1
#define FALSE 0
#define BLE_FORMAT_HEADER_LEN 4
#define BLE_PW_ON_IDX 0
#define BLE_HCI_RESET_IDX 1
#define BLE_HCI_SET_ADV_PARAM_IDX 2
#define BLE_HCI_SET_ADV_DATA_IDX 3
#define BLE_HCI_SET_SCAN_RESP_DATA_IDX 4
#define BLE_HCI_SET_ADV_ENABLE_IDX 5
#define BLE_HCI_SET_BT_MAC_IDX 6
#define GPIO_CLEAR 0
#define GPIO_SET 1
#define MT7621_GPIO_MODE 0x1e000060
// BLE EXPECTED RX Messgae
const u8  pw_on_correct_code[] = {0x04, 0xe4, 0x05, 0x02, 0x06, 0x01, 0x00, 0x00};
const u8  hic_reset_correct_code[] = {0x04, 0x0E, 0x04, 0x01, 0x03, 0x0C, 0x00};
const u8  hic_le_set_adv_param_correct_code[] = {0x04, 0x0E, 0x04, 0x01, 0x06, 0x20, 0x00};
const u8  hic_le_set_adv_data_correct_code[] = {0x04, 0x0E, 0x04, 0x01, 0x08, 0x20, 0x00};
const u8  hic_le_set_scan_resp_data_correct_code[] = {0x04, 0x0E, 0x04, 0x01, 0x09, 0x20, 0x00};
const u8  hic_le_set_adv_en_correct_code[] = {0x04, 0x0E, 0x04, 0x01, 0x0A, 0x20, 0x00};
const u8  hic_le_set_bt_mac_correct_code[] = {0x04, 0x0E, 0x04, 0x01, 0x1A, 0xFC, 0x00};

#define LEN_HCI_CORRECT_POWER_ON_CODE        (sizeof(pw_on_correct_code)/sizeof(u8))
#define LEN_HCI_CORRECT_RESET_CODE           (sizeof(hic_reset_correct_code)/sizeof(u8))
#define LEN_HCI_LE_SET_ADV_PARAM_CODE        (sizeof(hic_le_set_adv_param_correct_code)/sizeof(u8))
#define LEN_HCI_LE_SET_ADV_DATA_CODE         (sizeof(hic_le_set_adv_data_correct_code)/sizeof(u8))
#define LEN_HCI_LE_SET_SCAN_RSP_DATA_CODE    (sizeof(hic_le_set_scan_resp_data_correct_code)/sizeof(u8))
#define LEN_HCI_LE_ADV_EN_CODE               (sizeof(hic_le_set_adv_en_correct_code)/sizeof(u8))
#define LEN_HCI_LE_SET_BT_MAC_CODE           (sizeof(hic_le_set_bt_mac_correct_code)/sizeof(char))

//BLE TX Command
const u8 bt_power_on_tx_cmd[] = {0x01, 0x6f, 0xfc, 0x06, 0x01, 0x06, 0x02, 0x00, 0x00, 0x01};

const u8 hci_reset_tx_cmd[] = {0x01, 0x03, 0x0C, 0x00};

const u8 hci_le_set_adv_param_tx_cmd[] = {	0x01, 0x06, 0x20, 0x0f,
	0x20, 0x00, //Advertising Interval Min (x0.625ms)
	0x20, 0x00, //Advertising Interval Max (x0.625ms)
	0x02, //Advertising Type
	0x00, //Own Address Type
	0x00, //Peer Address Type
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //Peer Address
	0x07, //Advertising Channel Map
	0x00 //Advertising Filter Policy
};
//Sample1: MT7915_BLE_ADV_TEST
const u8 hci_le_set_adv_data_tx_cmd[] = {	0x01, 0x08, 0x20, 0x20,
	0x18, //Advertising Data Length, Total 31 bytes
	0x02, 0x01, 0x1A, 0x14, 0x09, 0x4d, // Advertising Data Byte0~Byte6
	0x54, 0x37, 0x39, 0x31, 0x35, 0x5f, // Advertising Data Byte7~Byte12
	0x42, 0x4c, 0x45, 0x5f, 0x41, 0x44, // Advertising Data Byte13~Byte18
	0x56, 0x5f, 0x54, 0x45, 0x53, 0x54, // Advertising Data Byte19~Byte24
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Advertising Data Byte25~Byte30
	0x00 // Advertising Data Byte31
};

const u8 hci_le_set_scan_resp_data_tx_cmd[] = {	0x01, 0x09, 0x20, 0x20,
	0x18, //Scan Response Data Length, Total 31 bytes
	0x02, 0x01, 0x1A, 0x14, 0x09, 0x4d, // Scan Response Data Byte0~Byte6
	0x54, 0x37, 0x39, 0x31, 0x35, 0x5f, // Scan Response Data Byte7~Byte12
	0x42, 0x4c, 0x45, 0x5f, 0x41, 0x44, // Scan Response Data Byte13~Byte18
	0x56, 0x5f, 0x54, 0x45, 0x53, 0x54, // Scan Response Data Byte19~Byte24
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Scan Response Data Byte25~Byte30
	0x00 // Scan Response Data Byte31
};

const u8 hci_le_set_adv_enable_tx_cmd[] = {	0x01, 0x0A, 0x20, 0x01,
	0x01 //Advertising Enable/Disable
};

const char hci_le_set_bt_mac_tx_cmd[] = { 0x01, 0x1A, 0xFC, 0x06, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11 /*BT MAC Setting, ex:11:22:33:44:55:66*/ }; 

#define STP_HEAD_FORMAT_LEN 4
#define STP_TAIL_FORMAT_LEN 2

//STP format
const char stp_head_tx_pw_on_cmd[] = {0x80, 0x00, 0x0A, 0x00};
const char stp_head_tx_hci_reset_cmd[] = {0x80, 0x00, 0x04, 0x00};
const char stp_head_tx_adv_param_cmd[] = {0x80, 0x00, 0x13, 0x00};
const char stp_head_tx_adv_data_cmd[] = {0x80, 0x00, 0x24, 0x00};
const char stp_head_tx_scan_rsp_data_cmd[] = {0x80, 0x00, 0x24, 0x00};
const char stp_head_tx_adv_en_cmd[] = {0x80, 0x00, 0x05, 0x00};
const char stp_head_tx_bt_mac_cmd[] = {0x80, 0x00, 0x0A, 0x00};
const char stp_tail_txrx_cmd[] = {0x00, 0x00};

#define UUID128_SIZE		16

const char u6iw_factory_uuid[UUID128_SIZE] = {0xb8, 0x6d, 0x7f, 0xe8, 0x3b, 0x8d, 0x34, 0x97, 0xf0, 0x45, 0x9f, 0x10, 0xa5, 0x76, 0x61, 0xf0};
const char u6iw_config_uuid[UUID128_SIZE] = {0xb5, 0x0f, 0x30, 0xbb, 0x8f, 0x6d, 0x38, 0x91, 0x0b, 0x45, 0xa8, 0x2c, 0x6c, 0x0c, 0x86, 0x53};
const char u6mesh_factory_uuid[UUID128_SIZE] = {0x54, 0xbc, 0x00, 0xa5, 0x38, 0x0d, 0x06, 0xa9, 0xc7, 0x4a, 0x3c, 0x12, 0xaa, 0xba, 0x9d, 0x51};
const char u6mesh_config_uuid[UUID128_SIZE] = {0x1e, 0x45, 0xde, 0x16, 0x16, 0x7a, 0x3d, 0x9d, 0x3d, 0x46, 0x16, 0x5b, 0x88, 0xc5, 0xe3, 0x3b};
const char u6lite_factory_uuid[UUID128_SIZE] = {0xd0, 0xe9, 0x44, 0x5e, 0x47, 0x01, 0x4f, 0xa9, 0x9c, 0x42, 0xe5, 0x38, 0x47, 0x3f, 0x6e, 0x1e};
const char u6lite_config_uuid[UUID128_SIZE] = {0x21, 0x7d, 0xca, 0xab, 0x0a, 0x94, 0x77, 0x81, 0xf1, 0x4f, 0x4b, 0x60, 0x2d, 0x86, 0x65, 0x5a};
const char u6extender_factory_uuid[UUID128_SIZE] = {0x4f, 0xa9, 0x0d, 0x37, 0x5a, 0x7b, 0x27, 0x9b, 0xa3, 0x44, 0x91, 0x93, 0x50, 0x3f, 0x90, 0xc1};
const char u6extender_config_uuid[UUID128_SIZE] = {0x52, 0x3e, 0x5d, 0x63, 0x42, 0x1f, 0xfe, 0x9f, 0xe7, 0x40, 0xea, 0xe4, 0x41, 0xce, 0xf9, 0x70};

const char u6iw_str[] = {"U6-IW"};
const char u6mesh_str[] = {"U6-MESH"};
const char u6lite_str[] = {"U6-LITE"};
const char u6extender_str[] = {"U6-EXTENDER"};

/* ADV */
typedef struct {
    uint8_t len;
    uint8_t ad_type;
    uint8_t ad_data[UUID128_SIZE];
} ad_element_uuid_t;

typedef struct {
    uint8_t len;
    uint8_t ad_type;
    uint8_t ad_service_uuid[2];
    uint8_t ad_data;
} ad_element_mode_t;

/* Total 31 bytes */
typedef struct {
    uint8_t hci_packet_type;
    uint8_t opcode[2];
    uint8_t total_len;
    uint8_t ad_data_len;
    ad_element_uuid_t uuid;
    ad_element_mode_t mode;
    uint8_t reserved[8];
} hci_le_adv_data_cmd_t;

hci_le_adv_data_cmd_t hci_le_adv_data_cmd = {
    .hci_packet_type = 0x01,
    .opcode = {0x08, 0x20},
    .total_len = 0x20,
    .ad_data_len = 0x17,
    .uuid.len = 0x11,	//128-bits UUID
    .uuid.ad_type = 0x06,
    .uuid.ad_data = {0x00},
    .mode.len = 0x04,
    .mode.ad_type = 0x16,	//service data: 0x0 in uboot, 0x1 in kernel mode
    .mode.ad_service_uuid = {0x21, 0x20},
    .mode.ad_data = 0x00,
    .reserved = {0x00}
};

/* Scan Resposne */
typedef struct {
    uint8_t len;
    uint8_t ad_type;
    uint8_t ad_service_uuid[2];
    uint8_t ad_data[6];
} ad_element_mac_t;

typedef struct {
    uint8_t len;
    uint8_t ad_type;
    uint8_t ad_service_uuid[2];
    uint8_t time;
} ad_element_boottime_t;

typedef struct {
    uint8_t len;
    uint8_t ad_type;
    uint8_t ad_data[9];
} ad_element_name_t;

/* Total 31 bytes */
typedef struct {
    uint8_t hci_packet_type;
    uint8_t opcode[2];
    uint8_t total_len;
    uint8_t ad_data_len;
    ad_element_mac_t mac;
    ad_element_boottime_t boot_time;
    ad_element_name_t name;
    uint8_t reserved[5];
} hci_le_scan_rsp_data_cmd_t;

hci_le_scan_rsp_data_cmd_t hci_le_scan_rsp_data_cmd = {
    .hci_packet_type = 0x01,
    .opcode = {0x09, 0x20},
    .total_len = 0x20,
    .ad_data_len = 0x18,
    .mac.len = 0x09,
    .mac.ad_type = 0x16,    //service data: mac address
    .mac.ad_service_uuid = {0x2a, 0x25},
    .mac.ad_data = {0x00},
    .boot_time.len = 0x04,
    .boot_time.ad_type = 0x16,
    .boot_time.ad_service_uuid = {0x29, 0x25},
    .boot_time.time = 0x46,
    .name.len = 0x00,
    .name.ad_type = 0x08,	//short local name
    .name.ad_data = {0x00},
    .reserved = {0x00}

};

static char *p_macaddr = NULL;
static char *p_device_model = NULL;
int is_default;
bool is_ble_stp = false;
#endif


#ifdef _BLE_DEBUG
#define BLE_DEBUG(format, args...) printf("[%s:%d] "format, __FILE__, __LINE__, ##args)
#else
#define BLE_DEBUG(args...) do{}while(0)
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr)/sizeof(arr[0]))
#endif

#ifdef CSR8811_USB_SUPPORT

#define BD_ADDR_0_OFFSET_IN_BCCMD_SET_BD_ADDR_CMD            24
#define BD_ADDR_1_OFFSET_IN_BCCMD_SET_BD_ADDR_CMD            23
#define BD_ADDR_2_OFFSET_IN_BCCMD_SET_BD_ADDR_CMD            21
#define BD_ADDR_3_OFFSET_IN_BCCMD_SET_BD_ADDR_CMD            17
#define BD_ADDR_4_OFFSET_IN_BCCMD_SET_BD_ADDR_CMD            20
#define BD_ADDR_5_OFFSET_IN_BCCMD_SET_BD_ADDR_CMD            19

#define BLE_UUID_OFFSET_IN_ADVERTISING_DATA                  6
#define BLE_MAC_SERVICE_OFFSET_IN_SCAN_RESPONSE_DATA         10
#define BLE_FW_VERSION_SERVICE_OFFSET_IN_SCAN_RESPONSE_DATA  20

#define BCCMD_SET_BD_ADDR_CMD_INDEX                          1
#define BCCMD_RESET_CSR_CMD_INDEX                            2
#define BLE_HCI_LE_SET_ADVERTISING_DATA_INDEX                5
#define BLE_HCI_LE_SET_SCAN_RESPONSE_DATA_INDEX              6

static int expected_data_count=0;
static const uint8_t FLEX_HD_CONFIGURED_UUID[]={0x10,0xba,0x32,0x1a,0xe3,0x3b,0x47,0x39,0xa8,0x2d,0x7e,0x87,0x5c,0x8d,0xed,0x8e};
static const uint8_t FLEX_HD_UNCONFIGURED_UUID[]={0xc1,0xb0,0xc0,0xba,0xe3,0x3b,0x47,0x39,0xa8,0x2d,0x7e,0x87,0x5c,0x8a,0xea,0xae};

typedef struct BLE_hci_command {
    const uint8_t opcode[2];
    const uint8_t size;
    uint8_t command[32];
} ble_hci_command_t;

static ble_hci_command_t BLE_hci_commands[] = {
    /*CSR command - bccmd -d hci0 psset -r to set BD addr*/
    {.opcode= {0x0,0xfc}, .size = 0x13, .command = {0xc2,0x0,0x0,0x9,0x0,0x0,0x0,0x6,0x30,0x0,0x0,0x1,0x0,0x8,0x0,0x0,0x0,0x0,0x0}},
    /*Write newM BD addr: MAC[0]=command[24], MAC[1]=command[23], MAC[2]=command[21], MAC[3]=command[17], MAC[4]=command[20], MAC[5]=command[19]*/
    {.opcode= {0x0,0xfc}, .size = 0x19, .command = {0xc2,0x2,0x0,0xc,0x0,0x1,0x0,0x3,0x70,0x0,0x0,0x1,0x0,0x4,0x0,0x8,0x0,0x33,0x0,0x55,0x44,0x22,0x0,0x11,0x5}},
    /*Reset CSR chip - USB  will re-connect*/
    {.opcode= {0x0,0xfc}, .size = 0x13, .command = {0xc2,0x2,0x0,0x9,0x0,0x2,0x0,0x2,0x40,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0}},
    /*End - bccmd -d hci0 psset -r */
    /*LE_Set_Advertising_Parameters*/
    {.opcode= {0x6,0x20}, .size = 0xf, .command = {0xa0,0x0,0xa0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x7,0x0}},
    /*LE_Set_Advertise_Enable*/
    {.opcode= {0xa,0x20}, .size = 0x1, .command = {0x1}},
    /*LE_Set_Advertising_Data*/
    {.opcode= {0x8,0x20}, .size = 0x20, .command = {0x1d,0x2,0x1,0x6,0x11,0x6,0x10,0xba,0x32,0x1a,0xe3,0x3b,0x47,0x39,0xa8,0x2d,0x7e,0x87,0x5c,0x8d,0xed,0x8e,0x6,0x8,0x55,0x46,0x4c,0x48,0x44,0x0,0x0,0x0}},
    /*LE_Set_Scan_Responce_Data*/
    {.opcode= {0x09,0x20}, .size = 0x20, .command = {0x1e,0x4,0x16,0x21,0x20,0x0,0x9,0x16,0x2a,0x25,0x18,0xe8,0x29,0xb5,0x12,0xd1,0x6,0x16,0x2b,0x20,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0}},
};
#endif

#ifdef EFR32_SERIAL_SUPPORT

#define BLE_ADV_MAX_LEN 31

enum SET_ADV_PACKET_TYPE{
    SET_ADV_PACKET=0,
    SET_SCAN_RSP_PACKET=1
};

#define UDMB_UUID_INDEX_IN_ADV     1
#define UDMB_MAC_INDEX_IN_SCAN_RSP 1
#define UDMB_FW_VER_INDEX_IN_SCAN_RSP 2
static const uint8_t UDMB_CONFIGURED_UUID[]={0xa0,0xb1,0xc2,0xba,0xe3,0x3b,0x47,0x39,0xa8,0x2d,0x7e,0x87,0x5c,0x8d,0xed,0x0e};
static const uint8_t UDMB_UNCONFIGURED_UUID[]={0xc1,0xb0,0xc0,0xba,0xe3,0x3b,0x47,0x39,0xa8,0x2d,0x7e,0x87,0x5c,0x8a,0xea,0x3e};

struct ble_adv_format
{
    const uint8_t len;
    const uint8_t type;
    uint8_t data[BLE_ADV_MAX_LEN-2]; // 128-bit UUID or UUID + Data
};

struct ble_adv_format adv_data_with_uuid[3]=
{
    {.len=0x2,  .type=0x1, .data={0x6}},
    {.len=0x11, .type=0x6, .data={0xa0,0xb1,0xc2,0xba,0xe3,0x3b,0x47,0x39,0xa8,0x2d,0x7e,0x87,0x5c,0x8d,0xed,0x0e}},
    {.len=0x5,  .type=0x8, .data={0x55,0x44,0x4d,0x42}},
};

struct ble_adv_format scan_rsp_data[3]=
{
    {.len=0x4, .type=0x16, .data={0x21,0x20,0x0}},
    {.len=0x9, .type=0x16, .data={0x2a,0x25,0x18,0xe8,0x29,0xb5,0x12,0xd1}},
    {.len=0x6, .type=0x16, .data={0x2b,0x20,0x0,0x0,0x0}},
};

#endif

static int get_default_state(void)
{
    char *is_default = NULL;
    is_default = env_get("is_default");
    if (is_default && (strcmp(is_default, "false") == 0)) {
        BLE_DEBUG("is_default = false\n");
        return 0;
    }

    printf("is_default %s\n", is_default);
    if (is_default == NULL) {
        env_set("is_default", "true");
        printf("set is_default true\n");
    }
    BLE_DEBUG("is_default = true or NULL\n");
    return 1;
}

static int get_ble_stp_state(void)
{
    char *is_ble_stp = NULL;
    is_ble_stp = env_get("is_ble_stp");
    if (is_ble_stp && (strcmp(is_ble_stp, "true") == 0)) {
        printf("is_ble_stp = true\n");
        return 1;
    }

    printf("is_ble_stp %s\n", is_ble_stp);
    if (is_ble_stp == NULL) {
        env_set("is_ble_stp", "false");
    }
    printf("is_ble_stp = false or NULL\n");
    return 0;
}

static int parse_fw_version_to_bytes(uint8_t *fw_ver)
{
    char *tmp, *fw_version_env = env_get("fw_version");
    if (fw_version_env) {
        for (int i = 0; i < 3; i++) {
            fw_ver[i] = (uint8_t) simple_strtoul(fw_version_env, &tmp, 10);
            fw_version_env = tmp + 1;
        }
        return 0;
    }
    return -1;
}

#ifdef EFR32_SERIAL_SUPPORT
static void ble_release_reset(void)
{
    BLE_DEBUG("Release BLE reset pin\n");
    gpio_output_set(14,1); // recovery pin - pull high
    mdelay(100);
    gpio_output_set(13,0);
    mdelay(100);
    gpio_output_set(13,1); // reset pin - pull high
    mdelay(100);         // delay
}
#endif

#if defined(_BLE_DEBUG) && defined(EFR32_SERIAL_SUPPORT)
static int ble_show_mac_addr(void)
{
    static ble_head_pass[4]={0x20,0x6,0x1,0x3};
    uint8_t ble_addr[10];
    int cnt;
    memset(ble_addr,0,sizeof(ble_addr));

    serial2_putc(0x20); // 0x20 0x0 0x1 0x3 - read MAC address from BLE
    serial2_putc(0x0);
    serial2_putc(0x1);
    serial2_putc(0x3);
    mdelay(10);   // Delay before start to read reponse
    printf("Start to read BLE MAC\n");
    cnt=0;
    while(serial2_tstc() && cnt < 10) // check if data is available?
    {
        //printf("%x ",serial2_getc()); // Read data
        ble_addr[cnt]=serial2_getc();
        cnt++;
    }

    if(cnt == 10 && ble_addr[0] == ble_head_pass[0] && 
            ble_addr[1] == ble_head_pass[1] && 
            ble_addr[2] == ble_head_pass[2] &&
            ble_addr[3] == ble_head_pass[3]){
        printf("###############################\n");
        printf("BLE MAC=%02x:%02x:%02x:%02x:%02x:%02x\n",ble_addr[9],ble_addr[8],ble_addr[7],
                ble_addr[6],ble_addr[5],ble_addr[4]);
        printf("#################################\n");
        return 1;
    }

    return 0;
}
#endif

#ifdef EFR32_SERIAL_SUPPORT
static int ble_send_adv_data(uint8_t packet_type)
{
    const static char expected_return_val[6]={0x20,0x2,0x3,0xc,0x0,0x0};
    char recv_data;
    int cnt,data_cnt;
    // EFR32 BLE Header
    serial2_putc(0x20);
    serial2_putc(0x22);
    serial2_putc(0x3);
    serial2_putc(0xc);
    serial2_putc(0x0);
    serial2_putc(packet_type);
    serial2_putc(BLE_ADV_MAX_LEN);

    if (packet_type == SET_ADV_PACKET) {
        for (cnt = 0; cnt < ARRAY_SIZE(adv_data_with_uuid); cnt++) {
            BLE_DEBUG("Adv Data -[%d]->\n",cnt);
            serial2_putc(adv_data_with_uuid[cnt].len);
            BLE_DEBUG("len=%x\n",adv_data_with_uuid[cnt].len);
            serial2_putc(adv_data_with_uuid[cnt].type);
            BLE_DEBUG("type=%x\n",adv_data_with_uuid[cnt].type);
            BLE_DEBUG("Send data=>");
            for (data_cnt = 0; data_cnt < ((adv_data_with_uuid[cnt].len) - 1); data_cnt++) {
                BLE_DEBUG("%x ",adv_data_with_uuid[cnt].data[data_cnt]);

                if (cnt == UDMB_UUID_INDEX_IN_ADV) {
                    if (get_default_state()) {
                        serial2_putc(UDMB_UNCONFIGURED_UUID[data_cnt]);
                    } else {
                        serial2_putc(UDMB_CONFIGURED_UUID[data_cnt]);
                    }
                } else {
                    serial2_putc(adv_data_with_uuid[cnt].data[data_cnt]);
                }
            }
            BLE_DEBUG("---End\n");
        }

    } else if(packet_type == SET_SCAN_RSP_PACKET) {
        unsigned char *eth_mac = eth_get_ethaddr();

        for (cnt = 0; cnt < ARRAY_SIZE(scan_rsp_data); cnt++) {
            serial2_putc(scan_rsp_data[cnt].len);
            serial2_putc(scan_rsp_data[cnt].type);
            if (cnt == UDMB_MAC_INDEX_IN_SCAN_RSP) {
                memcpy(&scan_rsp_data[cnt].data[2], eth_mac, 6);
            } else if (cnt == UDMB_FW_VER_INDEX_IN_SCAN_RSP) {
                parse_fw_version_to_bytes(&scan_rsp_data[cnt].data[2]);
            }

            for (data_cnt = 0; data_cnt < (scan_rsp_data[cnt].len-1); data_cnt++) {
                serial2_putc(scan_rsp_data[cnt].data[data_cnt]);
            }
        }

    } else {
        printf("[%s] Error, No any packet type meet\n",__func__);
        return -1;
    }

    while (cnt < BLE_ADV_MAX_LEN) {
        serial2_putc(0x0);
        cnt++;
    }

    mdelay(100);
    BLE_DEBUG("Read status of set_adv_data\n");
    cnt = 0;
    while(serial2_tstc()) // check if data is available?
    {
        recv_data=serial2_getc();
        if(expected_return_val[cnt] == recv_data)
            cnt++;
        //Polling response data
        BLE_DEBUG("%x ",recv_data); // Read data
    }
    BLE_DEBUG("\n");
    if(cnt == sizeof(expected_return_val))
        return 0;

    return -1;
}

static int ble_start_adv(void)
{
    static char expected_return_val[6]={0x20,0x2,0x3,0x14,0x0,0x0};
    int cnt;
    char recv_data;
    //EFR32 - Start adv - 0x4 - user data, 0x2 - connectable, scannable
    static char start_adv[]={0x20,0x3,0x3,0x14,0x0,0x4,0x2};
    BLE_DEBUG("starting BLE ADV\n");
    for(cnt=0;cnt<sizeof(start_adv);cnt++)
    {
        serial2_putc(start_adv[cnt]);
    }
    mdelay(100);
    BLE_DEBUG("Start adv - Read status\n");
    cnt=0;
    while(serial2_tstc()) // check if data is available?
    {
        recv_data=serial2_getc();
        if(expected_return_val[cnt] == recv_data)
            cnt++;
        //Read response data
        BLE_DEBUG("%x ",recv_data); // Read data
    }
    BLE_DEBUG("\n");

    if(cnt == sizeof(expected_return_val))
        return 0;

    return -1;

}
#endif

#ifdef CSR8811_USB_SUPPORT
static int usb_ble_write_data(struct usb_device *dev, unsigned long pipe, void *buffer, int len)
{
#ifdef _BLE_DEBUG
    static int send_count=0;
#endif
    expected_data_count+=len;
    int timeout=USB_CNTL_TIMEOUT;
    ALLOC_CACHE_ALIGN_BUFFER(struct devrequest, setup_packet, 1);
    int err;
    mdelay(60);
    /* set setup command */
    setup_packet->requesttype = 0x20;
    setup_packet->request = 0;
    setup_packet->value = 0;
    setup_packet->index = 0;
    setup_packet->length = cpu_to_le16(len);
    dev->status = USB_ST_NOT_PROC; /*not yet processed */
    err = submit_control_msg(dev, pipe, buffer, len, setup_packet);
    if (err < 0)
    {
#ifdef _BLE_DEBUG
        send_count++;
        BLE_DEBUG("[%d] submit_control_msg error=%d\n",send_count,err);
#endif
        return err;
    }
    if (timeout == 0)
        return (int)len;

    /*
     * Wait for status to update until timeout expires, USB driver
     * interrupt handler may set the status when the USB operation has
     * been completed.
     */
    while (timeout--) {
        if (!((volatile unsigned long)dev->status & USB_ST_NOT_PROC))
            break;
        mdelay(1);
    }

#ifdef _BLE_DEBUG
    send_count++;
    if(dev->status)
        BLE_DEBUG("[%d] command fail [dev->status=%d]\n",send_count,dev->status);
    else
        BLE_DEBUG("[%d] command success[Sent count=%d]\n",send_count,dev->act_len);
#endif
    if (dev->status)
        return -1;

    return dev->act_len;
}

static struct usb_device *usb_find_device(int devnum)
{
#ifdef CONFIG_DM_USB
    struct usb_device *udev;
    struct udevice *hub;
    struct uclass *uc;
    int ret;

    /* Device addresses start at 1 */
    devnum++;
    ret = uclass_get(UCLASS_USB_HUB, &uc);
    if (ret)
        return NULL;

    uclass_foreach_dev(hub, uc) {
        struct udevice *dev;

        if (!device_active(hub))
            continue;
        udev = dev_get_parent_priv(hub);
        if (udev->devnum == devnum)
            return udev;

        for (device_find_first_child(hub, &dev);
                dev;
                device_find_next_child(&dev)) {
            if (!device_active(hub))
                continue;

            udev = dev_get_parent_priv(dev);
            if (udev->devnum == devnum)
                return udev;
        }
    }
#else
    struct usb_device *udev;
    int d;

    for (d = 0; d < USB_MAX_DEVICE; d++) {
        udev = usb_get_dev_index(d);
        if (udev == NULL)
            return NULL;
        if (udev->devnum == devnum)
            return udev;
    }
#endif

    return NULL;
}

static int start_csr8811_ble(void)
{
    if (usb_init() != 0) {
        printf("Init USB fail");
        return -1;
    }

    struct usb_device *udev = usb_find_device(1);
    if (udev == NULL) {
        printf("UDEV = NULL\n");
        return -1;
    }

    unsigned char *eth_mac = eth_get_ethaddr();
    expected_data_count=0;
    int return_val=0;

    for (int cnt = 0; cnt < ARRAY_SIZE(BLE_hci_commands); cnt++) {
        ble_hci_command_t *cmd = &BLE_hci_commands[cnt];
        switch (cnt) {
            case BCCMD_RESET_CSR_CMD_INDEX:
            {
                usb_ble_write_data(udev, usb_sndctrlpipe(udev, 0x00), cmd, sizeof(BLE_hci_commands[cnt].opcode) + sizeof(BLE_hci_commands[cnt].size) + BLE_hci_commands[cnt].size);
                usb_stop();
                usb_init();

                udev = usb_find_device(1);
                if (udev == NULL) {
                    printf("UDEV = NULL\n");
                    return -1;
                }
                return_val += sizeof(BLE_hci_commands[BCCMD_RESET_CSR_CMD_INDEX].opcode) +
                              sizeof(BLE_hci_commands[BCCMD_RESET_CSR_CMD_INDEX].size) +
                              BLE_hci_commands[BCCMD_RESET_CSR_CMD_INDEX].size;
                continue; // continue the commands loop without executing usb command one more time
            }
            break;
            case BCCMD_SET_BD_ADDR_CMD_INDEX:
            {
                cmd->command[BD_ADDR_0_OFFSET_IN_BCCMD_SET_BD_ADDR_CMD] = eth_mac[0];
                cmd->command[BD_ADDR_1_OFFSET_IN_BCCMD_SET_BD_ADDR_CMD] = eth_mac[1];
                cmd->command[BD_ADDR_2_OFFSET_IN_BCCMD_SET_BD_ADDR_CMD] = eth_mac[2];
                cmd->command[BD_ADDR_3_OFFSET_IN_BCCMD_SET_BD_ADDR_CMD] = eth_mac[3];
                cmd->command[BD_ADDR_4_OFFSET_IN_BCCMD_SET_BD_ADDR_CMD] = eth_mac[4];
                // Make last byte of bdaddr different than ethernet mac
                cmd->command[BD_ADDR_5_OFFSET_IN_BCCMD_SET_BD_ADDR_CMD] = eth_mac[4] ^ eth_mac[5];
            }
            break;
            case BLE_HCI_LE_SET_ADVERTISING_DATA_INDEX:
            {
                if (get_default_state()) {
                    memcpy(&cmd->command[BLE_UUID_OFFSET_IN_ADVERTISING_DATA], FLEX_HD_UNCONFIGURED_UUID, sizeof(FLEX_HD_CONFIGURED_UUID));
                } else {
                    memcpy(&cmd->command[BLE_UUID_OFFSET_IN_ADVERTISING_DATA], FLEX_HD_CONFIGURED_UUID, sizeof(FLEX_HD_CONFIGURED_UUID));
                }
            }
            break;
            case BLE_HCI_LE_SET_SCAN_RESPONSE_DATA_INDEX:
            {
                memcpy(&cmd->command[BLE_MAC_SERVICE_OFFSET_IN_SCAN_RESPONSE_DATA], eth_mac, 6);
                parse_fw_version_to_bytes(&BLE_hci_commands[cnt].command[BLE_FW_VERSION_SERVICE_OFFSET_IN_SCAN_RESPONSE_DATA]);
            }
            break;
            default:
            break;
        }

        return_val += usb_ble_write_data(udev, usb_sndctrlpipe(udev, 0x00), cmd, sizeof(BLE_hci_commands[cnt].opcode) + sizeof(BLE_hci_commands[cnt].size) + BLE_hci_commands[cnt].size);
    }
    usb_stop();
    //Recovery all USB registers to default
    writel(0x00001801,0x1e1d0700); //SSUSB_IP_PW_CTRL0
    writel(0x0000200e,0x1e1d0710); //SSUSB_IP_PW_STS1
    writel(0x02000000,0x1e1d0814); //U2PHYAC0
    writel(0x0c680008,0x1e1d0818); //U2PHYACR2
    writel(0x02000000,0x1e1d0868); //U2PHYDTM0
    writel(0x00000000,0x1e1d086c); //U2PHYDTM1
    writel(0x00000000,0x1e1d0878); //U2PHYDMON2
    writel(0x00000150,0x1e1d090c); //PHYD_LFPS1
    writel(0x02180802,0x1e1d095c); //PHYD_CDR1
    writel(0x00a04028,0x1e1d0a28); //B2_PHYD_RXDET1
    writel(0x00005020,0x1e1d0a2c); //B2_PHYD_RXDET2
    writel(0x00000400,0x1e1d0a64); //B2_ROSC_9



    if (expected_data_count == return_val)
        return 0;

    return -1;
}

#endif
#ifdef EFR32_SERIAL_SUPPORT
static int start_efr32_ble(void)
{
    ble_release_reset();
#ifdef _BLE_DEBUG
    if(!ble_show_mac_addr())
    {
        printf("Can't get BLE MAC Address.\n");
        return -1;
    }
#endif
    BLE_DEBUG("Start to set UDMB BLE\n");
    if(ble_send_adv_data(SET_ADV_PACKET) < 0)
        return -1;
    if(ble_send_adv_data(SET_SCAN_RSP_PACKET) < 0)
        return -1;
    if(ble_start_adv() < 0)
        return -1;

    return 0;
}
#endif

#ifdef CONFIG_BLE_ADVERTISTING_EN
static void _serial_putc(struct udevice *dev, char ch)
{
	struct dm_serial_ops *ops = serial_get_ops(dev);
	int err;

	do {
		err = ops->putc(dev, ch);
	} while (err == -EAGAIN);
}

static int _serial_getc(struct udevice *dev)
{
	struct dm_serial_ops *ops = serial_get_ops(dev);
	int err;

	do {
		err = ops->getc(dev);
	} while (err == -EAGAIN);

	return err >= 0 ? err : 0;
}

static int _serial_tstc(struct udevice *dev)
{
	struct dm_serial_ops *ops = serial_get_ops(dev);

	if (ops->pending)
		return ops->pending(dev, true);

	return 1;
}

static void _serial_setbrg(struct udevice *dev, int baudrate)
{
	struct dm_serial_ops *ops = serial_get_ops(dev);

	if (ops->setbrg)
		ops->setbrg(dev, baudrate);
}

static int get_ble_tx_cmd(u8 idx, u8 tx_cmd_buf[])
{
	u8 len = 0;
	int8_t mac_offset = 0;
	char *tmpmac;
	#ifndef REVERT_TO_CHECK_ORIGIN_PATCH
	int8_t i;
	char *end;
	#endif

	memset(tx_cmd_buf, 0x0, sizeof(tx_cmd_buf));
	switch(idx)
	{
		case BLE_PW_ON_IDX:
			if (is_ble_stp)
			{
				memcpy(tx_cmd_buf, stp_head_tx_pw_on_cmd, STP_HEAD_FORMAT_LEN);
				len = STP_HEAD_FORMAT_LEN;
			}
			memcpy(&tx_cmd_buf[len], bt_power_on_tx_cmd, sizeof(bt_power_on_tx_cmd));
			len = len + (sizeof(bt_power_on_tx_cmd) / sizeof(u8));
			break;
		case BLE_HCI_RESET_IDX:
			if (is_ble_stp)
			{
				memcpy(tx_cmd_buf, stp_head_tx_hci_reset_cmd, STP_HEAD_FORMAT_LEN);
				len = STP_HEAD_FORMAT_LEN;
			}
			memcpy(&tx_cmd_buf[len], hci_reset_tx_cmd, sizeof(hci_reset_tx_cmd));
			len = len + (sizeof(hci_reset_tx_cmd) / sizeof(u8));
			break;
		case BLE_HCI_SET_ADV_PARAM_IDX:
			if (is_ble_stp)
			{
				memcpy(tx_cmd_buf, stp_head_tx_adv_param_cmd, STP_HEAD_FORMAT_LEN);
				len = STP_HEAD_FORMAT_LEN;
			}
			memcpy(&tx_cmd_buf[len], hci_le_set_adv_param_tx_cmd, sizeof(hci_le_set_adv_param_tx_cmd));
			len = len + (sizeof(hci_le_set_adv_param_tx_cmd) / sizeof(u8));
			break;
		case BLE_HCI_SET_ADV_DATA_IDX:
			#ifdef REVERT_TO_CHECK_ORIGIN_PATCH
			if (is_ble_stp)
			{
				memcpy(tx_cmd_buf, stp_head_tx_adv_data_cmd, STP_HEAD_FORMAT_LEN);
				len = STP_HEAD_FORMAT_LEN;
			}
			memcpy(&tx_cmd_buf[len], hci_le_set_adv_data_tx_cmd, sizeof(hci_le_set_adv_data_tx_cmd));
			len = len + (sizeof(hci_le_set_adv_data_tx_cmd) / sizeof(u8));
			#else
			hci_le_adv_data_cmd.ad_data_len = 5; //Constant hci_le_adv_data_cmd.mode
			if(p_device_model != NULL) {
				hci_le_adv_data_cmd.ad_data_len += 18; //1 byte ad_data len + 1 byte ad_type + 16 bytes uuid
				if(strcmp(p_device_model, u6iw_str) == 0) {
					if (is_default == 1) {
						memcpy((void *)&hci_le_adv_data_cmd.uuid.ad_data[0], (void *)&u6iw_factory_uuid[0], UUID128_SIZE);
					} else {
						memcpy((void *)&hci_le_adv_data_cmd.uuid.ad_data[0], (void *)&u6iw_config_uuid[0], UUID128_SIZE);
					}
				} else if(strcmp(p_device_model, u6mesh_str) == 0) {
					if (is_default == 1) {
						memcpy((void *)&hci_le_adv_data_cmd.uuid.ad_data[0], (void *)&u6mesh_factory_uuid[0], UUID128_SIZE);
					} else {
						memcpy((void *)&hci_le_adv_data_cmd.uuid.ad_data[0], (void *)&u6mesh_config_uuid[0], UUID128_SIZE);
					}
				} else if(strcmp(p_device_model, u6lite_str) == 0) {
					if (is_default == 1) {
						memcpy((void *)&hci_le_adv_data_cmd.uuid.ad_data[0], (void *)&u6lite_factory_uuid[0], UUID128_SIZE);
					} else {
						memcpy((void *)&hci_le_adv_data_cmd.uuid.ad_data[0], (void *)&u6lite_config_uuid[0], UUID128_SIZE);
					}
				} else if(strcmp(p_device_model, u6extender_str) == 0) {
					if (is_default == 1) {
						memcpy((void *)&hci_le_adv_data_cmd.uuid.ad_data[0], (void *)&u6extender_factory_uuid[0], UUID128_SIZE);
					} else {
						memcpy((void *)&hci_le_adv_data_cmd.uuid.ad_data[0], (void *)&u6extender_config_uuid[0], UUID128_SIZE);
					}
				}
			} else {
				printf("\n[ERROR]Empty p_device_model: %s\n", p_device_model);
			}

			if (is_ble_stp)
			{
				memcpy(tx_cmd_buf, stp_head_tx_adv_data_cmd, STP_HEAD_FORMAT_LEN);
				len = STP_HEAD_FORMAT_LEN;
			}
			memcpy(&tx_cmd_buf[len], (u8 *)&hci_le_adv_data_cmd, sizeof(hci_le_adv_data_cmd));
			len = len + (sizeof(hci_le_adv_data_cmd) / sizeof(u8));
			#endif
			break;
		case BLE_HCI_SET_SCAN_RESP_DATA_IDX:
			#ifdef REVERT_TO_CHECK_ORIGIN_PATCH
			if (is_ble_stp)
			{
				memcpy(tx_cmd_buf, stp_head_tx_scan_rsp_data_cmd, STP_HEAD_FORMAT_LEN);
				len = STP_HEAD_FORMAT_LEN;
			}
			memcpy(&tx_cmd_buf[len], hci_le_set_scan_resp_data_tx_cmd, sizeof(hci_le_set_scan_resp_data_tx_cmd));
			len = len + (sizeof(hci_le_set_scan_resp_data_tx_cmd) / sizeof(u8));
			#else
			hci_le_scan_rsp_data_cmd.ad_data_len = 5;
			if(p_device_model != NULL) {
				if(strcmp(p_device_model, u6iw_str) == 0) {
					hci_le_scan_rsp_data_cmd.ad_data_len += sizeof(u6iw_str)+1;
					hci_le_scan_rsp_data_cmd.name.len = sizeof(u6iw_str);
					memcpy((void *)&hci_le_scan_rsp_data_cmd.name.ad_data, (void *)&u6iw_str, sizeof(u6iw_str));
				} else if(strcmp(p_device_model, u6mesh_str) == 0) {
					hci_le_scan_rsp_data_cmd.ad_data_len += sizeof(u6mesh_str)+1;
					hci_le_scan_rsp_data_cmd.name.len = sizeof(u6mesh_str);
					memcpy((void *)&hci_le_scan_rsp_data_cmd.name.ad_data, (void *)&u6mesh_str, sizeof(u6mesh_str));
				} else if(strcmp(p_device_model, u6lite_str) == 0) {
					hci_le_scan_rsp_data_cmd.ad_data_len += sizeof(u6lite_str)+1;
					hci_le_scan_rsp_data_cmd.name.len = sizeof(u6lite_str);
					memcpy((void *)&hci_le_scan_rsp_data_cmd.name.ad_data, (void *)&u6lite_str, sizeof(u6lite_str));
				} else if(strcmp(p_device_model, u6extender_str) == 0) {
					hci_le_scan_rsp_data_cmd.ad_data_len += sizeof(u6extender_str)+1;
					hci_le_scan_rsp_data_cmd.name.len = sizeof(u6extender_str);
					memcpy((void *)&hci_le_scan_rsp_data_cmd.name.ad_data, (void *)&u6extender_str, sizeof(u6extender_str));
				}
			} else {
				printf("\n[ERROR]Empty p_device_model: %s\n", p_device_model);
			}

			tmpmac = p_macaddr;
			if(tmpmac != NULL) {
				hci_le_scan_rsp_data_cmd.ad_data_len += 10; //1 byte ad_data len + 1 byte ad_type + 2 bytes service_uuid + 6 byte mac
				for (i = 0; i < 6; ++i) {
					hci_le_scan_rsp_data_cmd.mac.ad_data[i] = tmpmac ? simple_strtoul(tmpmac, &end, 16) : 0;
					if (tmpmac) {
						tmpmac = (*end) ? end + 1 : end;
					}
				}
			}

			if (is_ble_stp)
			{
				memcpy(tx_cmd_buf, stp_head_tx_scan_rsp_data_cmd, STP_HEAD_FORMAT_LEN);
				len = STP_HEAD_FORMAT_LEN;
			}
			memcpy(&tx_cmd_buf[len], (u8 *)&hci_le_scan_rsp_data_cmd, sizeof(hci_le_scan_rsp_data_cmd));
			len = len + (sizeof(hci_le_scan_rsp_data_cmd) / sizeof(u8));
			#endif
			break;
		case BLE_HCI_SET_ADV_ENABLE_IDX:
			if (is_ble_stp)
			{
				memcpy(tx_cmd_buf, stp_head_tx_adv_en_cmd, STP_HEAD_FORMAT_LEN);
				len = STP_HEAD_FORMAT_LEN;
			}
			memcpy(&tx_cmd_buf[len], hci_le_set_adv_enable_tx_cmd, sizeof(hci_le_set_adv_enable_tx_cmd));
			len = len + (sizeof(hci_le_set_adv_enable_tx_cmd) / sizeof(u8));
			break;
		case BLE_HCI_SET_BT_MAC_IDX:
			if(is_ble_stp)
			{
				memcpy(tx_cmd_buf, stp_head_tx_bt_mac_cmd, STP_HEAD_FORMAT_LEN);
				len = STP_HEAD_FORMAT_LEN;
				mac_offset = STP_HEAD_FORMAT_LEN;
			}
			memcpy(&tx_cmd_buf[len], hci_le_set_bt_mac_tx_cmd, sizeof(hci_le_set_bt_mac_tx_cmd));
			len = len + (sizeof(hci_le_set_bt_mac_tx_cmd) / sizeof(u8));

			tmpmac = p_macaddr;
			if(tmpmac != NULL) {
				for (i = 5; i >= 0; --i) {
					tx_cmd_buf[mac_offset+4+i] = tmpmac ? simple_strtoul(tmpmac, &end, 16) : 0;
					if(i == 0) {
						//bt mac address generation rule is to add 3 at the last byte of mac address
						tx_cmd_buf[mac_offset+4+i] += 3;
					}
					if (tmpmac) {
						tmpmac = (*end) ? end + 1 : end;
					}
				}
			}
			break;
		default:
			break;
	}

	if (is_ble_stp)
	{
		memcpy(&tx_cmd_buf[len], stp_tail_txrx_cmd, sizeof(stp_tail_txrx_cmd));
		len = len + STP_TAIL_FORMAT_LEN;
	}
	return len;
}
static int judge_rx_msg(u8 msg_idx, u8 rx_msg_buf[])
{
	u8 ret = 0;
	u8 num = 0;

	switch(msg_idx)
	{
		case BLE_PW_ON_IDX:
			num = sizeof(pw_on_correct_code);
			if (is_ble_stp) {
				ret = memcmp( &rx_msg_buf[STP_HEAD_FORMAT_LEN], pw_on_correct_code, num);
			} else {
				ret = memcmp( rx_msg_buf, pw_on_correct_code, num);
			}
			//printf("[judge_rx_msg]: num=%d, ret=%d\n", num, ret);
			break;
		case BLE_HCI_RESET_IDX:
			num = sizeof(hic_reset_correct_code);
			if (is_ble_stp) {
				ret = memcmp( &rx_msg_buf[STP_HEAD_FORMAT_LEN], hic_reset_correct_code, num);
			} else {
				ret = memcmp( rx_msg_buf, hic_reset_correct_code, num);
			}
			//printf("[judge_rx_msg]: num=%d, ret=%d\n", num, ret);
			break;
		case BLE_HCI_SET_ADV_PARAM_IDX:
			num = sizeof(hic_le_set_adv_param_correct_code);
			if (is_ble_stp) {
				ret = memcmp( &rx_msg_buf[STP_HEAD_FORMAT_LEN], hic_le_set_adv_param_correct_code, num);
			} else {
				ret = memcmp( rx_msg_buf, hic_le_set_adv_param_correct_code, num);
			}
			//printf("[judge_rx_msg]: num=%d, ret=%d\n", num, ret);
			break;
		case BLE_HCI_SET_ADV_DATA_IDX:
			num = sizeof(hic_le_set_adv_data_correct_code);
			if (is_ble_stp) {
				ret = memcmp( &rx_msg_buf[STP_HEAD_FORMAT_LEN], hic_le_set_adv_data_correct_code, num);
			} else {
				ret = memcmp( rx_msg_buf, hic_le_set_adv_data_correct_code, num);
			}
			//printf("[judge_rx_msg]: num=%d, ret=%d\n", num, ret);
			break;
		case BLE_HCI_SET_SCAN_RESP_DATA_IDX:
			num = sizeof(hic_le_set_scan_resp_data_correct_code);
			if (is_ble_stp) {
				ret = memcmp( &rx_msg_buf[STP_HEAD_FORMAT_LEN], hic_le_set_scan_resp_data_correct_code, num);
			} else {
				ret = memcmp( rx_msg_buf, hic_le_set_scan_resp_data_correct_code, num);
			}
			//printf("[judge_rx_msg]: num=%d, ret=%d\n", num, ret);
			break;
		case BLE_HCI_SET_ADV_ENABLE_IDX:
			num = sizeof(hic_le_set_adv_en_correct_code);
			if (is_ble_stp) {
				ret = memcmp( &rx_msg_buf[STP_HEAD_FORMAT_LEN], hic_le_set_adv_en_correct_code, num);
			} else {
				ret = memcmp( rx_msg_buf, hic_le_set_adv_en_correct_code, num);
			}
			//printf("[judge_rx_msg]: num=%d, ret=%d\n", num, ret);
			break;
		case BLE_HCI_SET_BT_MAC_IDX:
			num = sizeof(hic_le_set_bt_mac_correct_code);
			if (is_ble_stp) {
				ret = memcmp( &rx_msg_buf[STP_HEAD_FORMAT_LEN], hic_le_set_bt_mac_correct_code, num);
			} else {
				ret = memcmp( rx_msg_buf, hic_le_set_bt_mac_correct_code, num);
			}
			//printf("[judge_rx_msg]: num=%d, ret=%d\n", num, ret);
			break;
		default:
			break;
	}
	if(ret == 0 )
		return CMD_RET_OK;
	else
		return CMD_RET_FAIL;
}
static int do_ble_adveristing_start(void )
{
	// START BT PROCESS
	struct udevice *uart;
	u8 bt_tx_cmd[50]={0x0};
	u8 bt_rx_msg[40]={0x0};
	u8 num = 0;
	u8 len = 0;
	u8 i = 0;
	u8 ch = 0;
	u8 curr_step_pass = 0;
	u32 reg;
	int ret;
	int value;
	u8 gpio;
	u8 count = 4;
	const char *str_gpio = NULL;
	const char *str_gpio_action = NULL;
	u32 start_timep;
	u32 curr_timep;

	printf("=========================GPIO INIT=====================\n");

	writel(0x404A4,(void __iomem *) MT7621_GPIO_MODE); // disable JTAG
	while(count)
	{
		if(count == 4)
		{
			value = GPIO_CLEAR;
			str_gpio = "16";
			str_gpio_action = "output low";
		}
		else if(count == 3)
		{
			value = GPIO_SET;
			str_gpio = "16";
			str_gpio_action = "output high";
		}
		else if(count == 2)
		{
			value = GPIO_CLEAR;
			str_gpio = "19";
			str_gpio_action = "output low";

		}
		else if(count == 1)
		{
			value = GPIO_SET;
			str_gpio = "19";
			str_gpio_action = "output high";
		}else
			return -1;

		ret = gpio_lookup_name(str_gpio, NULL, NULL, &gpio);
		if (ret)
		{
			printf("GPIO: '%s' not found\n", str_gpio);
			return -1;
		}

		ret = gpio_request(gpio, "cmd_gpio");
		if (ret && ret != -EBUSY)
		{
			printf("gpio: requesting pin %u failed\n", gpio);
			return -1;
		}
		printf("GPIO_%s, action is %s\n", str_gpio, str_gpio_action);
		gpio_direction_output(gpio, value);
		count--;
		mdelay(100);
	}

	mdelay(100);

	printf("=========================UART_3 INIT=====================\n");
	ret = uclass_first_device_check(UCLASS_SERIAL, &uart);
	if (!uart) {
		printf("No valid serial device found\n");
		return CMD_RET_FAILURE;
	}

	while (uart) {
		if (ret >= 0) {
			reg = CPHYSADDR(ofnode_get_addr(uart->node));
			debug("%s, %08x\n", uart->name, reg);

			if (reg == MT7621_UART3_BASE){
				printf("%s bring up, %08x\n", uart->name, reg);
				break;
			}
			else
				printf("%s, %08x\n", uart->name, reg);
		}

		ret = uclass_next_device_check(&uart);
	}

	if (!uart) {
		printf("UART3 device not found\n");
		return CMD_RET_FAILURE;
	}

	_serial_setbrg(uart, 115200);

	//FLOW 1: BT Power On
	printf("=========================FLOW 1=====================\n");
	memset(bt_tx_cmd, 0x0, sizeof(bt_tx_cmd));
	if (is_ble_stp) {
		len = (sizeof(pw_on_correct_code)/sizeof(u8)) + STP_HEAD_FORMAT_LEN + STP_TAIL_FORMAT_LEN;
	}
	else {
		len = sizeof(pw_on_correct_code)/sizeof(u8);
	}
	ret = get_ble_tx_cmd(BLE_PW_ON_IDX, bt_tx_cmd);
	if(ret > 0)
	{
		num = ret;
		for (i = 0; i < num; i++)
		{
			//putc(bt_tx_cmd[i]);
			_serial_putc(uart, bt_tx_cmd[i]);
		}
	}

	i = 0;
	memset(bt_rx_msg, 0x0, sizeof(bt_rx_msg));
	start_timep = get_timer(0);
	while (i < len) {
		if (_serial_tstc(uart)) {
			ch = _serial_getc(uart);
			//putc(ch);
			memcpy(&bt_rx_msg[i], &ch, sizeof(ch));

			i++;
		}
		curr_timep = get_timer(0);
		if(curr_timep - start_timep > 1500)
			break;
	}

	ret = judge_rx_msg(BLE_PW_ON_IDX, bt_rx_msg);
	if(ret == CMD_RET_OK){
		curr_step_pass = TRUE;
		printf("[BT Power On Result] Success\n");
	} else {
		printf("len = %d, num = %d\n", LEN_HCI_CORRECT_POWER_ON_CODE, num);
		printf("[BT Power On]: Tx Cmd=");
		for (i = 0; i < num; i++)
		{
			printf("%02x ",bt_tx_cmd[i]);
		}
		printf("\n[BT Power On]: Rx Msg=");
		for (i = 0; i < LEN_HCI_CORRECT_POWER_ON_CODE; i++)
		{
			printf("%02x ",bt_rx_msg[i]);
		}
		printf("\n[BT Power On Result] Fail\n");
		curr_step_pass = FALSE;
	}
	//FLOW 2: HCI_RESET
	printf("\n=========================FLOW 2=====================\n");
	if(curr_step_pass == TRUE)
	{
		memset(bt_tx_cmd, 0x0, sizeof(bt_tx_cmd));
		if (is_ble_stp) {
			len = (sizeof(hic_reset_correct_code)/sizeof(u8))+ STP_HEAD_FORMAT_LEN + STP_TAIL_FORMAT_LEN;
		}
		else {
			len = sizeof(hic_reset_correct_code)/sizeof(u8);
		}
		ret = get_ble_tx_cmd(BLE_HCI_RESET_IDX, bt_tx_cmd);
		if(ret > 0)
		{
			num = ret;
			for (i = 0; i < num; i++)
			{
				//putc(bt_tx_cmd[i]);
				_serial_putc(uart, bt_tx_cmd[i]);
			}
		}

		i = 0;
		memset(bt_rx_msg, 0x0, sizeof(bt_rx_msg));
		start_timep = get_timer(0);
		while (i < len) {
			if (_serial_tstc(uart)) {
				ch = _serial_getc(uart);
				//putc(ch);
				memcpy(&bt_rx_msg[i], &ch, sizeof(ch));
				i++;
			}
			curr_timep = get_timer(0);
			if(curr_timep - start_timep > 1500)
				break;
		}

		ret = judge_rx_msg(BLE_HCI_RESET_IDX, bt_rx_msg);
		if(ret == CMD_RET_OK){
			curr_step_pass = TRUE;
			printf("[HCI RESET Result] Success\n");
		}
		else{
			printf("len= %d, num= %d\n", LEN_HCI_CORRECT_RESET_CODE, num);
			printf("[HCI RESET]: Tx Cmd=");
			for (i = 0; i < num; i++)
			{
				printf("%02x ",bt_tx_cmd[i]);
			}
			printf("\n[HCI RESET]: Rx Msg=");
			for (i = 0; i < LEN_HCI_CORRECT_RESET_CODE; i++)
			{
				printf("%02x ",bt_rx_msg[i]);
			}
			printf("\n[HCI RESET Result] Fail\n");
			curr_step_pass = FALSE;
		}
	}

	//Extend FLOW: HCI_LE_Set_BT_MAC_ADDR
	printf("\n=========================Extend FLOW=====================\n");
	if(curr_step_pass == TRUE)
	{
		memset(bt_tx_cmd, 0x0, sizeof(bt_tx_cmd));
		if (is_ble_stp) {
			len = (sizeof(hic_le_set_bt_mac_correct_code)/sizeof(u8))+ STP_HEAD_FORMAT_LEN + STP_TAIL_FORMAT_LEN;
		}
		else {
			len = sizeof(hic_le_set_bt_mac_correct_code)/sizeof(u8);
		}
		ret = get_ble_tx_cmd(BLE_HCI_SET_BT_MAC_IDX, bt_tx_cmd);
		if(ret > 0)
		{
			num = ret;
			for (i = 0; i < num; i++)
			{
				//putc(bt_tx_cmd[i]);
				_serial_putc(uart, bt_tx_cmd[i]);
			}
		}

		i = 0;
		memset(bt_rx_msg, 0x0, sizeof(bt_rx_msg));
		start_timep = get_timer(0);
		while (i < len)
		{
			if(_serial_tstc(uart))
			{
				ch = _serial_getc(uart);
				//putc(ch);
				memcpy(&bt_rx_msg[i], &ch, sizeof(ch));
				i++;
			}

			curr_timep = get_timer(0);
			if(curr_timep - start_timep > 1500)
				break;
		}

		ret = judge_rx_msg(BLE_HCI_SET_BT_MAC_IDX, bt_rx_msg);
		if(ret == CMD_RET_OK)
		{
			curr_step_pass = TRUE;
			printf("[HCI LE BT MAC ADDR Result] Success\n");
		}
		else
		{
			printf("len= %d, num= %d\n", LEN_HCI_LE_SET_BT_MAC_CODE, num);
			printf("[HCI LE_Set BT MAC ADDR]: Tx Cmd=");
			for (i = 0; i < num; i++)
			{
				printf("%02x ",bt_tx_cmd[i]);
			}
			printf("\n[HCI LE_Set BT MAC ADDR]: Rx Msg=");
			for (i = 0; i < LEN_HCI_LE_SET_BT_MAC_CODE; i++)
			{
				printf("%02x ",bt_rx_msg[i]);
			}
			printf("\n[HCI LE BT MAC ADDR Result] Fail\n");
			curr_step_pass = FALSE;
		}
	}

	//FLOW 3: HCI_LE_Set_Advertising_Parameters
	printf("\n=========================FLOW 3=====================\n");
	if(curr_step_pass == TRUE)
	{
		memset(bt_tx_cmd, 0x0, sizeof(bt_tx_cmd));
		if (is_ble_stp) {
			len = (sizeof(hic_le_set_adv_param_correct_code)/sizeof(u8))+ STP_HEAD_FORMAT_LEN + STP_TAIL_FORMAT_LEN;
		}
		else {
			len = sizeof(hic_le_set_adv_param_correct_code)/sizeof(u8);
		}
		ret = get_ble_tx_cmd(BLE_HCI_SET_ADV_PARAM_IDX, bt_tx_cmd);
		if(ret > 0)
		{
			num = ret;
			for (i = 0; i < num; i++)
			{
				//putc(bt_tx_cmd[i]);
				_serial_putc(uart, bt_tx_cmd[i]);
			}
		}

		i = 0;
		memset(bt_rx_msg, 0x0, sizeof(bt_rx_msg));
		start_timep = get_timer(0);
		while (i < len) {
			if (_serial_tstc(uart)) {
				ch = _serial_getc(uart);
				//putc(ch);
				memcpy(&bt_rx_msg[i], &ch, sizeof(ch));
				i++;
			}
			curr_timep = get_timer(0);
			if(curr_timep - start_timep > 1500)
				break;
		}

		ret = judge_rx_msg(BLE_HCI_SET_ADV_PARAM_IDX, bt_rx_msg);
		if(ret == CMD_RET_OK){
			curr_step_pass = TRUE;
			printf("[HCI LE SET ADVERTISING PARAMETER Result] Success\n");
		}
		else{
			printf("len= %d, num= %d\n", LEN_HCI_LE_SET_ADV_PARAM_CODE, num);
			printf("[HCI LE_Set Advertising Parameters]: Tx Cmd =");
			for (i = 0; i < num; i++)
			{
				printf("%02x ",bt_tx_cmd[i]);
			}
			printf("\n[HCI LE_Set Advertising Parameters]: Rx Msg =");
			for (i = 0; i < LEN_HCI_LE_SET_ADV_PARAM_CODE; i++)
			{
				printf("%02x ",bt_rx_msg[i]);
			}
			printf("\n[HCI LE  SET ADVERTISING PARAMETER Result]  Fail\n");
			curr_step_pass = FALSE;
		}
	}

	//FLOW 4: HCI_LE_Set_Advertising_Data
	printf("\n=========================FLOW 4=====================\n");
	if(curr_step_pass == TRUE)
	{
		memset(bt_tx_cmd, 0x0, sizeof(bt_tx_cmd));
		if(is_ble_stp) {
			len = (sizeof(hic_le_set_adv_data_correct_code)/sizeof(u8))+ STP_HEAD_FORMAT_LEN + STP_TAIL_FORMAT_LEN;
		}
		else {
			len = sizeof(hic_le_set_adv_data_correct_code)/sizeof(u8);
		}
		ret = get_ble_tx_cmd(BLE_HCI_SET_ADV_DATA_IDX, bt_tx_cmd);
		if(ret > 0)
		{
			num = ret;
			for (i = 0; i < num; i++)
			{
				//putc(bt_tx_cmd[i]);
				_serial_putc(uart, bt_tx_cmd[i]);
			}
		}

		i = 0;
		memset(bt_rx_msg, 0x0, sizeof(bt_rx_msg));
		start_timep = get_timer(0);
		while (i < len) {
			if (_serial_tstc(uart)) {
				ch = _serial_getc(uart);
				//putc(ch);
				memcpy(&bt_rx_msg[i], &ch, sizeof(ch));
				i++;
			}
			curr_timep = get_timer(0);
			if(curr_timep - start_timep > 1500)
				break;
		}

		ret = judge_rx_msg(BLE_HCI_SET_ADV_DATA_IDX, bt_rx_msg);
		if(ret == CMD_RET_OK){
			curr_step_pass = TRUE;
			printf("[HCI LE  SET ADVERTISING DATA Result] Success\n");
		}
		else{
			printf("len= %d, num= %d\n", LEN_HCI_LE_SET_ADV_DATA_CODE, num);
			printf("[HCI LE_Set Advertising Data]: Tx Cmd=");
			for (i = 0; i < num; i++)
			{
				printf("%02x ",bt_tx_cmd[i]);
			}
			printf("\n[HCI LE_Set Advertising Data]: Rx Msg=");

			for (i = 0; i < LEN_HCI_LE_SET_ADV_DATA_CODE; i++)
			{
				printf("%02x ",bt_rx_msg[i]);
			}
			printf("\n[HCI LE  SET ADVERTISING DATA Result] Fail\n");
			curr_step_pass = FALSE;
		}
	}

	//FLOW 5: HCI_LE_Set_Scan_Response_Data
	printf("\n=========================FLOW 5=====================\n");
	if(curr_step_pass == TRUE)
	{
		memset(bt_tx_cmd, 0x0, sizeof(bt_tx_cmd));
		if (is_ble_stp) {
			len = (sizeof(hic_le_set_scan_resp_data_correct_code)/sizeof(u8))+ STP_HEAD_FORMAT_LEN + STP_TAIL_FORMAT_LEN;
		}
		else {
			len = sizeof(hic_le_set_scan_resp_data_correct_code)/sizeof(u8);
		}
		ret = get_ble_tx_cmd(BLE_HCI_SET_SCAN_RESP_DATA_IDX, bt_tx_cmd);
		if(ret > 0)
		{
			num = ret;
			for (i = 0; i < num; i++)
			{
				_serial_putc(uart, bt_tx_cmd[i]);
			}
		}

		i = 0;
		memset(bt_rx_msg, 0x0, sizeof(bt_rx_msg));
		start_timep = get_timer(0);
		while (i < len) {
			if (_serial_tstc(uart)) {
				ch = _serial_getc(uart);
				//putc(ch);
				memcpy(&bt_rx_msg[i], &ch, sizeof(ch));
				//    printf("%02x ",bt_rx_msg[i]);
				i++;
			}
			curr_timep = get_timer(0);
			if(curr_timep - start_timep > 1500)
				break;
		}

		ret = judge_rx_msg(BLE_HCI_SET_SCAN_RESP_DATA_IDX, bt_rx_msg);
		if(ret == CMD_RET_OK){
			curr_step_pass = TRUE;
			printf("[HCI LE  SET SCAN RESPONSE Result] Success\n");
		}
		else{
			printf("len= %d, num= %d\n", LEN_HCI_LE_SET_SCAN_RSP_DATA_CODE, num);
			printf("[HCI LE_Set Scan Response Data]: Tx Cmd=");
			for (i = 0; i < num; i++)
			{
				printf("%02x ",bt_tx_cmd[i]);
			}
			printf("\n[HCI LE_Set Scan Response Data ]: Rx Msg=");
			for (i = 0; i < LEN_HCI_LE_SET_SCAN_RSP_DATA_CODE; i++)
			{
				printf("%02x ",bt_rx_msg[i]);
			}
			printf("\n[HCI LE  SET SCAN RESPONSE Result] Fail\n");
			curr_step_pass = FALSE;
		}
	}

	//FLOW 6: HCI_LE_Set_Advertising_Enable
	printf("\n=========================FLOW 6=====================\n");
	if(curr_step_pass == TRUE)
	{
		memset(bt_tx_cmd, 0x0, sizeof(bt_tx_cmd));
		if (is_ble_stp) {
			len = (sizeof(hic_le_set_adv_en_correct_code)/sizeof(u8))+ STP_HEAD_FORMAT_LEN + STP_TAIL_FORMAT_LEN;
		}
		else {
			len = sizeof(hic_le_set_adv_en_correct_code)/sizeof(u8);
		}
		ret = get_ble_tx_cmd(BLE_HCI_SET_ADV_ENABLE_IDX, bt_tx_cmd);
		if(ret > 0)
		{
			num = ret;
			for (i = 0; i < num; i++)
			{
				//putc(bt_tx_cmd[i]);
				_serial_putc(uart, bt_tx_cmd[i]);
			}
		}

		i = 0;
		memset(bt_rx_msg, 0x0, sizeof(bt_rx_msg));
		start_timep = get_timer(0);
		while (i < len) {
			if (_serial_tstc(uart)) {
				ch = _serial_getc(uart);
				//putc(ch);
				memcpy(&bt_rx_msg[i], &ch, sizeof(ch));
				i++;
			}
			curr_timep = get_timer(0);
			if(curr_timep - start_timep > 1500)
				break;
		}

		ret = judge_rx_msg(BLE_HCI_SET_ADV_ENABLE_IDX, bt_rx_msg);
		if(ret == CMD_RET_OK){
			curr_step_pass = TRUE;
			printf("[HIC LE  SET ADVERISTING ENABLE Result] Success\n");
		}
		else{
			printf("len= %d, num= %d\n", BLE_HCI_SET_ADV_ENABLE_IDX, num);
			printf("[HCI LE_Set Advertising Enable]: Tx Cmd=");
			for (i = 0; i < num; i++)
			{
				printf("%02x ",bt_tx_cmd[i]);
			}
			printf("\n[HCI LE_Set Advertising Enable]: Rx Msg=");
			for (i = 0; i < LEN_HCI_LE_ADV_EN_CODE; i++)
			{
				printf("%02x ",bt_rx_msg[i]);
			}
			printf("\n[HCI LE  SET ADVERISTING ENABLE Result] Fail\n");
			curr_step_pass = FALSE;
		}
	}
	if(curr_step_pass == TRUE) {
		return CMD_RET_OK;
	}
	return CMD_RET_FAILURE;
	
}
#endif

void start_ble(void)
{
    if (run_command("setbleenv", 0) != CMD_RET_SUCCESS)
        return;

    char *ble_mode = env_get("ble_mode");
#ifdef CONFIG_BLE_ADVERTISTING_EN
	p_device_model = env_get("device_model");
	is_default = get_default_state();
	is_ble_stp = get_ble_stp_state();

	printf("\n~~~ p_device_model:%s\n", p_device_model);
	printf("~~~ is_default:%d ~~~\n", is_default);
	p_macaddr = env_get("macaddr");
	printf("~~~ p_macaddr:%s ~~~\n", p_macaddr);
	printf("~~~ is_ble_stp:%d ~~~\n", is_ble_stp);
#endif

    if ((ble_mode == NULL) || (ble_mode && strcmp(ble_mode, "none") == 0)) {
        return;
    }
#ifdef CSR8811_USB_SUPPORT
    else if (strcmp(ble_mode, "usb") == 0) {
        if (start_csr8811_ble() == 0)
            printf("CSR8811 BLE broadcasting successfully\n");
        else
            printf("CSR8811 BLE broadcasting FAILED\n");
    }
#endif
#ifdef EFR32_SERIAL_SUPPORT
    else if (strcmp(ble_mode, "serial") == 0) {
        if (start_efr32_ble() == 0) 
            printf("EFR32 BLE broadcasting successfully\n");
        else
            printf("EFR32 BLE broadcasting FAILED\n");
    }
#endif
#ifdef CONFIG_BLE_ADVERTISTING_EN
    else if (strcmp(ble_mode, "serial") == 0) {
        if (do_ble_adveristing_start() == CMD_RET_OK)
            printf("MT7915 BLE broadcasting successfully\n");
        else
            printf("MT7915 BLE broadcasting FAILED\n");
    }
#endif


}
