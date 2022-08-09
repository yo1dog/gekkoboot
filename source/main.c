#include <stdio.h>
#include <stdlib.h>
#include <sdcard/gcsd.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <ogc/lwp_watchdog.h>
#include <fcntl.h>
#include <ogc/system.h>
#include "ffshim.h"
#include "fatfs/ff.h"
#include "types.h"
#include "config.h"

#include "stub.h"
#define STUB_ADDR  0x80001000
#define STUB_STACK 0x80003000

#define MAX_NUM_ARGV 1024

struct boot_payload
{
    BOOT_TYPE type,
    char *dol,
    char *argv[MAX_NUM_ARGV],
    int argc
}

int debug_enabled = false;
int verbose_enabled = false;
u16 all_buttons_held;

void scan_all_buttons_held()
{
    PAD_ScanPads();
    all_buttons_held = (
        PAD_ButtonsHeld(PAD_CHAN0) |
        PAD_ButtonsHeld(PAD_CHAN1) |
        PAD_ButtonsHeld(PAD_CHAN2) |
        PAD_ButtonsHeld(PAD_CHAN3)
    );
}

u8 *dol_alloc(int size)
{
    kprintf("DOL size is %iB\n", size);

    if (size <= 0)
    {
        kprintf("Empty DOL\n");
        return;
    }

    u8 *dol = (u8 *) memalign(32, size);

    if (!dol)
    {
        kprintf("Couldn't allocate memory\n");
    }

    return dol;
}

u8 *read_dol_file(const char *path)
{
    kprintf("Reading %s\n", path);
    FIL file;
    FRESULT open_result = f_open(&file, path, FA_READ);
    if (open_result != FR_OK)
    {
        kprintf("Failed to open file: %s\n", get_fresult_message(open_result));
        return NULL;
    }

    size_t size = f_size(&file);
    u8* dol = dol_alloc(size);
    if (!dol) return NULL;

    UINT _;
    f_read(&file, dol, size, &_);
    f_close(&file);

    return dol;
}

const char *read_cli_file(const char *dol_path)
{
    size_t path_len = strlen(dol_path);
    if (path_len > 4 && strncmp(dol_path + path_len - 4, ".dol", 4) == 0)
    {
        kprintf("Not reading CLI file: DOL path does not end in \".dol\"\n");
        return;
    }

    char path[path_len + 1];
    memcpy(path, dol_path, path_len - 3);
    path[path_len - 3] = 'c';
    path[path_len - 2] = 'l';
    path[path_len - 1] = 'i';
    path[path_len    ] = '\0';

    kprintf("Reading %s\n", path);
    FIL file;
    FRESULT result = f_open(&file, path, FA_READ);
    if (result != FR_OK)
    {
        if (result == FR_NO_FILE)
        {
            kprintf("CLI file not found\n");
        }
        else
        {
            kprintf("Failed to open CLI file: %s\n", get_fresult_message(result));
        }
        return;
    }

    size_t size = f_size(&file);
    kprintf("CLI file size is %iB\n", size);

    if (size <= 0)
    {
        kprintf("Empty CLI file\n");
        return;
    }

    char *cli = (char *) malloc(size + 1);

    if (!cli)
    {
        kprintf("Couldn't allocate memory for CLI file\n");
        return;
    }

    UINT _;
    f_read(&file, cli, size, &_);
    f_close(&file);

    if (cli[size - 1] != '\0')
    {
      cli[size] = '\0';
      size++;
    }

    return cli;
}

// https://github.com/emukidid/swiss-gc/blob/a0fa06d81360ad6d173acd42e4dd5495e268de42/cube/swiss/source/swiss.c#L1236
int parse_cli_options(const char** argv, const char *cli)
{
    const char* line_indices[MAX_NUM_ARGV];
    int line_lengths[MAX_NUM_ARGV];
    int num_lines = 0;
    
    int line_start_index = 0;
    
    // Search for the others after each newline
    int cli_len = strlen(cli) + 1;
    for (int i = 0; i < cli_len; i++)
    {
        if (i = cli_len || cli[i] == '\r' || cli[i] == '\n')
        {
            int line_len = i - line_start_index - 1;
            if (line_len > 0)
            {
                line_indices[num_lines] = line_start_index;
                line_lengths[num_lines] = line_len;
                num_lines++;
                
                if (num_lines >= MAX_NUM_ARGV)
                {
                    kprintf("Reached max of %i arg lines.\n", MAX_NUM_ARGV);
                    break;
                }
            }
            line_start_index = i;
        }
    }
    
    kprintf("Found %i CLI args\n", argc);

    if (is_verbose_enabled)
    {
        for (int i = 0; i < argc; ++i)
        {
            kprintf("arg%i: %s\n", i, argv[i]);
        }
    }

    return argc;
}

int load_fat(struct boot_payload *payload, const char *slot_name, const DISC_INTERFACE *iface_)
{
    int res = 0;

    kprintf("Trying %s\n", slot_name);

    // Mount device.
    FATFS fs;
    iface = iface_;
    FRESULT mount_result = f_mount(&fs, "", 1);
    if (mount_result != FR_OK)
    {
        kprintf("Couldn't mount %s: %s\n", slot_name, get_fresult_message(mount_result));
        goto end;
    }

    char name[256];
    f_getlabel(slot_name, name, NULL);
    kprintf("Mounted %s from %s\n", name, slot_name);

    // Read and parse config file for mounted FAT device.
    configuration config;
    if (!read_parse_config(&config))
    {
        goto unmount;
    }
    
    res = 1;
    boot_payload->type = BOOT_TYPE_NONE;
    
    is_debug_enabled = config->is_debug_enabled;
    is_verbose_enabled = config->is_verbose_enabled;

    kprintf("Found config file\n");
    if (is_verbose_enabled)
    {
        kprintf("-----\n");
        print_config(config);
        kprintf("-----\n");
    }

    // Check shortcuts and choose action.
    struct boot_action *action = &config->default_action;
    
    for (int i = 0; i < NUM_SHORTCUTS; i++)
    {
        if (config->shortcut_actions[i].type == BOOT_TYPE_NONE) continue;
        if (all_buttons_held & shortcuts[i].pad_buttons)
        {
            action = &config->shortcut_actions[i];
            kprintf("Using %s button shortcut\n", shortcuts[i].name);
            break;
        }
    }

    if (action->type == BOOT_ACTION_ONBOARD)
    {
        kprintf("Will reboot to onboard IPL\n");
        boot_payload->type = BOOT_ACTION_ONBOARD;
        goto unmount;
    }

    if (action->type != BOOT_TYPE_DOL)
    {
        // Should never happen.
        kprintf("Will do nothing\n");
        goto unmount;
    }

    // Read DOL file.
    u8* dol = read_dol_file(action->dol_path);
    if (!dol)
    {
        goto unmount;
    }

    // Attempt to read CLI file if CLI options were not configured.
    const char* dol_cli_options;
    if (action->dol_cli_options)
    {
        dol_cli_options = action->dol_cli_options;
        kprintf("Not reading CLI file: CLI options defined in config\n");
    }
    else
    {
        dol_cli_options = read_cli_file(action->dol_path);
    }

    // Parse CLI options.
    args *dol_args;
    if (dol_cli_options)
    {
        kprintf("Parsing CLI options...\n");
        dol_args = parse_cli_options(dol_cli_options);
    }

    boot_payload->type = BOOT_TYPE_NONE;
    boot_payload->dol = dol;
    boot_payload->dol_args = dol_args;

unmount:
    kprintf("Unmounting %s\n", slot_name);
    iface->shutdown();
    iface = NULL;

end:
    return res;
}

unsigned int convert_int(unsigned int in)
{
    unsigned int out;
    char *p_in = (char *) &in;
    char *p_out = (char *) &out;
    p_out[0] = p_in[3];
    p_out[1] = p_in[2];
    p_out[2] = p_in[1];
    p_out[3] = p_in[0];
    return out;
}

#define PC_READY 0x80
#define PC_OK    0x81
#define GC_READY 0x88
#define GC_OK    0x89

int load_usb(struct loaded_boot_action *loaded_action, char slot)
{
    kprintf("Trying USB Gecko in slot %c\n", slot);

    int channel, res = 1;

    switch (slot)
    {
    case 'B':
        channel = 1;
        break;

    case 'A':
    default:
        channel = 0;
        break;
    }

    if (!usb_isgeckoalive(channel))
    {
        kprintf("Not present\n");
        res = 0;
        goto end;
    }

    usb_flush(channel);

    char data;

    kprintf("Sending ready\n");
    data = GC_READY;
    usb_sendbuffer_safe(channel, &data, 1);

    kprintf("Waiting for ack...\n");
    while ((data != PC_READY) && (data != PC_OK))
        usb_recvbuffer_safe(channel, &data, 1);

    if(data == PC_READY)
    {
        kprintf("Respond with OK\n");
        // Sometimes the PC can fail to receive the byte, this helps
        usleep(100000);
        data = GC_OK;
        usb_sendbuffer_safe(channel, &data, 1);
    }

    kprintf("Getting DOL size\n");
    int size;
    usb_recvbuffer_safe(channel, &size, 4);
    size = convert_int(size);

    u8 *dol = dol_alloc(size);
    unsigned char* pointer = dol;

    if(!dol)
    {
        res = 0;
        goto end;
    }

    kprintf("Receiving file...\n");
    while (size > 0xF7D8)
    {
        usb_recvbuffer_safe(channel, (void *) pointer, 0xF7D8);
        size -= 0xF7D8;
        pointer += 0xF7D8;
    }
    if(size)
        usb_recvbuffer_safe(channel, (void *) pointer, size);

    loaded_action->type = BOOT_TYPE_DOL;
    loaded_action->dol = dol;

end:
    return res;
}

extern u8 __xfb[];

void wait_for_confirmation() {
    // Wait until the A button or reset button is pressed.
    int cur_state = true;
    int last_state;
    do
    {
        VIDEO_WaitVSync();
        scan_all_buttons_held();
        last_state = cur_state;
        cur_state = all_buttons_held & PAD_BUTTON_A;
    }
    while (!last_state && cur_state);
}
void delay_exit() {
    if (is_debug_enabled)
    {
        // When debug is enabled, always wait for confirmation before exit.
        kprintf("\nDEBUG: Press A to continue\n");
        wait_for_confirmation();
        return;
    }

    // Wait while the d-pad down direction or reset button is held.
    if (all_buttons_held & PAD_BUTTON_DOWN)
    {
        kprintf("\nRelease d-pad down to continue\n");
    }
    if (SYS_ResetButtonDown())
    {
        kprintf("\nRelease reset button to continue\n");
    }

    while (all_buttons_held & PAD_BUTTON_DOWN || SYS_ResetButtonDown())
    {
        VIDEO_WaitVSync();
        scan_all_buttons_held();
    }
}

int main()
{
    VIDEO_Init();
    PAD_Init();
    GXRModeObj *rmode = VIDEO_GetPreferredMode(NULL);
    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(__xfb);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    VIDEO_WaitVSync();
    CON_Init(__xfb, 0, 0, rmode->fbWidth, rmode->xfbHeight, rmode->fbWidth * VI_DISPLAY_PIX_SZ);

    kprintf("\n\niplboot\n");

    // Disable Qoob
    u32 val = 6 << 24;
    u32 addr = 0xC0000000;
    EXI_Lock(EXI_CHANNEL_0, EXI_DEVICE_1, NULL);
    EXI_Select(EXI_CHANNEL_0, EXI_DEVICE_1, EXI_SPEED8MHZ);
    EXI_Imm(EXI_CHANNEL_0, &addr, 4, EXI_WRITE, NULL);
    EXI_Sync(EXI_CHANNEL_0);
    EXI_Imm(EXI_CHANNEL_0, &val, 4, EXI_WRITE, NULL);
    EXI_Sync(EXI_CHANNEL_0);
    EXI_Deselect(EXI_CHANNEL_0);
    EXI_Unlock(EXI_CHANNEL_0);
    // Since we've disabled the Qoob, we wil reboot to the Nintendo IPL

    // Set the timebase properly for games
    // Note: fuck libogc and dkppc
    u32 t = ticks_to_secs(SYS_Time());
    settime(secs_to_ticks(t));

    scan_all_buttons_held();

    // Check if we should skip.
    if (all_buttons_held & PAD_BUTTON_LEFT || SYS_ResetButtonDown())
    {
        kprintf("Skipped. Rebooting into original IPL...\n");
        delay_exit();
        return 0;
    }

    // Attempt to load from each device.
    int mram_size = SYS_GetArenaHi() - SYS_GetArenaLo();
    kprintf("Memory available: %iB\n", mram_size);

    struct loaded_boot_action loaded_action;
    int res = (
           load_usb(&loaded_action, 'B')
        || load_fat(&loaded_action, "sdb", &__io_gcsdb, paths, num_paths)
        || load_usb(&loaded_action, 'A')
        || load_fat(&loaded_action, "sda", &__io_gcsda, paths, num_paths)
        || load_fat(&loaded_action, "sd2", &__io_gcsd2, paths, num_paths)
    );

    if (!res)
    {
        // If we reach here, all attempts to load configuration failed.
        kprintf("Configuration not found\n");
        kprintf("\nPress A to reboot into onboard IPL\n");
        wait_for_confirmation();
        return 0;
    }

    if (loaded_boot_action.action == BOOT_ACTION_ONBOARD)
    {
        kprintf("Rebooting into onboard IPL...\n");
        delay_exit();
        return 0;
    }

    if (loaded_boot_action.action == BOOT_ACTION_NONE)
    {
        // We should only reach here if the DOL failed to load.
        kprintf("\nPress A to reboot into onboard IPL\n");
        wait_for_confirmation();
        return 0;
    }

    // Prepare DOL argv.
    struct __argv dolargs;
    dolargs.commandLine = (char *) NULL;
    dolargs.length = 0;
    
    // https://github.com/emukidid/swiss-gc/blob/f5319aab248287c847cb9468325ebcf54c993fb1/cube/swiss/source/aram/sidestep.c#L350
    if (loaded_boot_action.dol_args)
    {
        char** dol_argv = loaded_boot_action.dol_args->argv;
        int dol_argc = loaded_boot_action.dol_args->argc;
        
        dolargs.argvMagic = ARGV_MAGIC;
        dolargs.argc = dol_argc;
        dolargs.length = 1;

        for (int i = 0; i < dol_argc; i++)
        {
            size_t arg_length = strlen(dol_argv[i]) + 1;
            dolargs.length += arg_length;
        }

        kprintf("CLI argv size is %iB\n", dolargs.length);
        dolargs.commandLine = (char *) malloc(dolargs.length);

        if (!dolargs.commandLine)
        {
            kprintf("Couldn't allocate memory for CLI argv\n");
            dolargs.length = 0;
        }
        else
        {
            unsigned int position = 0;
            for (int i = 0; i < dol_argc; i++)
            {
                size_t arg_length = strlen(dol_argv[i]) + 1;
                memcpy(dolargs.commandLine + position, dol_argv[i], arg_length);
                position += arg_length;
            }
            dolargs.commandLine[dolargs.length - 1] = '\0';
            DCStoreRange(dolargs.commandLine, dolargs.length);
        }
    }

    // Boot DOL.
    memcpy((void *) STUB_ADDR, stub, stub_size);
    DCStoreRange((void *) STUB_ADDR, stub_size);

    delay_exit();

    SYS_ResetSystem(SYS_SHUTDOWN, 0, FALSE);
    SYS_SwitchFiber((intptr_t) dol, 0,
                    (intptr_t) dolargs.commandLine, dolargs.length,
                    STUB_ADDR, STUB_STACK);
    
    // Will never reach here.
    return 0;
}
