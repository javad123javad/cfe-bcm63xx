/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  
    *  bcm63xx board specific routines and commands.
    *  
    *  by:  seanl
    *
    *       April 1, 2002
    *
    *********************************************************************  
    *
    *  Copyright 2000,2001,2002,2003
    *  Broadcom Corporation. All rights reserved.
    *  
    *  This software is furnished under license and may be used and 
    *  copied only in accordance with the following terms and 
    *  conditions.  Subject to these conditions, you may download, 
    *  copy, install, use, modify and distribute modified or unmodified 
    *  copies of this software in source and/or binary form.  No title 
    *  or ownership is transferred hereby.
    *  
    *  1) Any source code used, modified or distributed must reproduce 
    *     and retain this copyright notice and list of conditions 
    *     as they appear in the source file.
    *  
    *  2) No right is granted to use any trade name, trademark, or 
    *     logo of Broadcom Corporation.  The "Broadcom Corporation" 
    *     name may not be used to endorse or promote products derived 
    *     from this software without the prior written permission of 
    *     Broadcom Corporation.
    *  
    *  3) THIS SOFTWARE IS PROVIDED "AS-IS" AND ANY EXPRESS OR
    *     IMPLIED WARRANTIES, INCLUDING BUT NOT LIMITED TO, ANY IMPLIED
    *     WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
    *     PURPOSE, OR NON-INFRINGEMENT ARE DISCLAIMED. IN NO EVENT 
    *     SHALL BROADCOM BE LIABLE FOR ANY DAMAGES WHATSOEVER, AND IN 
    *     PARTICULAR, BROADCOM SHALL NOT BE LIABLE FOR DIRECT, INDIRECT,
    *     INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
    *     (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
    *     GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
    *     BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
    *     OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR 
    *     TORT (INCLUDING NEGLIGENCE OR OTHERWISE), EVEN IF ADVISED OF 
    *     THE POSSIBILITY OF SUCH DAMAGE.
    ********************************************************************* */


#include "bcm63xx_util.h"
#include "flash_api.h"

#define FLASH_STAGING_BUFFER	    BOARD_IMAGE_DOWNLOAD_ADDRESS
#define FLASH_STAGING_BUFFER_SIZE	BOARD_IMAGE_DOWNLOAD_SIZE

extern int decompress_lzma_7z(unsigned char* in_data, unsigned in_size, 
                              unsigned char* out_data, unsigned out_size);
extern NVRAM_DATA nvramData;   
extern int ui_cmd_ifconfig(ui_cmdline_t *cmd,int argc,char *argv[]);
extern unsigned long cfe_sdramsize;

//roy
//#define GPIODIR 0xFFFE0404  //roy GPIO31~GPIO0
//#define GPIODATA 0xFFFE040C //roy GPIO31~GPIO0
//roy




// global 
int g_processing_cmd = 0;

char fakeConsole[2] = " ";

static int ui_cmd_set_board_param(ui_cmdline_t *cmd,int argc,char *argv[])
{
    return(setBoardParam());
}



static int ui_cmd_reset(ui_cmdline_t *cmd,int argc,char *argv[])
{
    kerSysMipsSoftReset();

    return 0;
}


// return 0 if 'y'
int yesno(void)
{
    char ans[5];

    printf(" (y/n):");
    console_readline ("", ans, sizeof (ans));
    if (ans[0] != 'y')
        return -1;

    return 0;
}


// erase Persistent sector
static int ui_cmd_erase_psi(ui_cmdline_t *cmd,int argc,char *argv[])
{
    printf("Erase persistent storage data?");
    if (yesno())
        return -1;

    kerSysErasePsi(0);

    return 0;
}



// erase some sectors
static int ui_cmd_erase(ui_cmdline_t *cmd,int argc,char *argv[])
{

    //FILE_TAG cfeTag;
    PFILE_TAG pTag;
    char *flag;
    int i, blk_start, blk_end;

    flag = cmd_getarg(cmd,0);

    if (!flag)
    {
        printf("Erase [n]vram, [p]ersistent storage or [a]ll flash except bootrom\nusage: e [n/p/a]\n");
        return 0;
    }

    switch (*flag)
    {
    case 'b':
        printf("Erase boot loader?");
        if (yesno())
            return 0;
        printf("\nNow think carefully.  Do you really,\n"
            "really want to erase the boot loader?");
        if (yesno())
            return 0;
        flash_sector_erase_int(0);        
        break;
    case 'n':
        printf("Erase nvram?");
        if (yesno())
            return 0;
        kerSysEraseNvRam();
        break;
    case 'a':
        printf("Erase all flash (except bootrom)?");
        if (yesno())
            return 0;

        blk_end = flash_get_numsectors();
        if ((pTag = getTagFromPartition(1)) != NULL)
            blk_start = flash_get_blk(atoi(pTag->rootfsAddress) + BOOT_OFFSET);
        else  // just erase all after cfe
        {
            for( blk_start = 0, i = 0; i < FLASH_LENGTH_BOOT_ROM &&
                 blk_start < blk_end; blk_start++ )
            {
                i += flash_get_sector_size(blk_start);
            }
            printf("No image tag found.  Erase the blocks start at [%d]\n",
                blk_start);
        }
        if( blk_start > 0 )
        {
            for (i = blk_start; i < blk_end; i++)
            {
                printf(".");
                flash_sector_erase_int(i);
            }
            printf("\n");
        }

        /* Preserve the NVRAM fields that are used in the 'b' command. */
        writeNvramData();
        setDefaultBootline();

        printf( "\nResetting board...\n" );
        kerSysMipsSoftReset();
        break;
    case 'p':
        ui_cmd_erase_psi(cmd,argc,argv);
        break;
    default:
        printf("Erase [n]vram, [p]ersistent storage or [a]ll flash except bootrom\nusage: e [n/p/a]\n");
        return 0;
    }

    return 0;
}


static int loadRaw(char *hostImageName, uint8_t *ptr)
{
    cfe_loadargs_t la;
    int res;

    printf("Loading %s ...\n", hostImageName);
    
    // tftp only
    la.la_filesys = "tftp";
    la.la_filename = hostImageName;
	la.la_device = NULL;
	la.la_address = (long)ptr;
	la.la_options = NULL;
	la.la_maxsize = FLASH_STAGING_BUFFER_SIZE;
	la.la_flags =  LOADFLG_SPECADDR;

	res = bcm63xx_cfe_rawload(&la); 
	if (res < 0)
    {
	    ui_showerror(res, "Loading failed.");
		return res;
    }
    printf("Finished loading %d bytes\n", res);

    return res;
}

// flash the image 
static int ui_cmd_flash_image(ui_cmdline_t *cmd,int argc,char *argv[])
{
    char hostImageName[BOOT_FILENAME_LEN + BOOT_IP_LEN];
    char *imageName;
    int res;
    uint8_t *ptr = (uint8_t *) KERNADDR(FLASH_STAGING_BUFFER);

    g_processing_cmd = 1;

    imageName = cmd_getarg(cmd, 0);
    
    if (imageName)
    {
        if (strchr(imageName, ':'))
            strcpy(hostImageName, imageName);
        else
        {
            strcpy(hostImageName, bootInfo.hostIp);
            strcat(hostImageName, ":");
            strcat(hostImageName, imageName);
        }
    }
    else  // use default flash file name
    {
        strcpy(hostImageName, bootInfo.hostIp);
        strcat(hostImageName, ":");
        strcat(hostImageName, bootInfo.flashFileName);
    }

    if ((res = loadRaw(hostImageName, ptr)) < 0)
	{
        g_processing_cmd = 0;
        return res;
	}

    // check and flash image
    res = flashImage(ptr);

    if( res == 0 )
    {
        char *p;

        for( p = nvramData.szBootline; p[2] != '\0'; p++ )
            if( p[0] == 'r' && p[1] == '=' && p[2] == 'h' )
            {
                /* Change boot source to "boot from flash". */
                p[2] = 'f';
                writeNvramData();
                break;
            }
        printf( "Resetting board...\n" );
        kerSysMipsSoftReset();
    }

    g_processing_cmd = 0;
    return( res );
}


// write the whole image
static int ui_cmd_write_whole_image(ui_cmdline_t *cmd,int argc,char *argv[])
{
    char hostImageName[BOOT_FILENAME_LEN + BOOT_IP_LEN];
    char *imageName;
    uint8_t *ptr = (uint8_t *) KERNADDR(FLASH_STAGING_BUFFER);
    int res;

    g_processing_cmd = 1;

    imageName = cmd_getarg(cmd, 0);
    if (!imageName) 
        return ui_showusage(cmd);

    if (strchr(imageName, ':'))
        strcpy(hostImageName, imageName);
    else
    {
        strcpy(hostImageName, bootInfo.hostIp);
        strcat(hostImageName, ":");
        strcat(hostImageName, imageName);
    } 
 
    if ((res = loadRaw(hostImageName, ptr)) < 0)
	{
        g_processing_cmd = 0;
        return res;
	}

    // check and flash image
    res = writeWholeImage(ptr, res);

    printf("Finished flashing image.\n");

    if (res == 0)
    {
        printf( "Resetting board...\n" );
        kerSysMipsSoftReset();
    }

    g_processing_cmd = 0;
    return( res );
}

// Used to flash an image that does not contain a FILE_TAG record. 
static int ui_cmd_flash_router_image(ui_cmdline_t *cmd,int argc,char *argv[])
{
    char hostImageName[BOOT_FILENAME_LEN + BOOT_IP_LEN];
    char *imageName;
    uint8_t *ptr = (uint8_t *) KERNADDR(FLASH_STAGING_BUFFER);
    int res;

    imageName = cmd_getarg(cmd, 0);
    if (!imageName) 
        return ui_showusage(cmd);

    if (strchr(imageName, ':'))
        strcpy(hostImageName, imageName);
    else
    {
        strcpy(hostImageName, bootInfo.hostIp);
        strcat(hostImageName, ":"); 
        strcat(hostImageName, imageName);
    }

    if ((res = loadRaw(hostImageName, ptr)) < 0)
        return res;

    // check and flash image
    if ((res = kerSysBcmImageSet(FLASH_IMAGE_START_ADDR, ptr, res, 1)) != 0)
        printf("Failed to flash image. Error: %d\n", res);
    else
        printf("Finished flashing image.\n");

    if (res == 0)
    {
        char *p;

        for( p = nvramData.szBootline; p[2] != '\0'; p++ )
            if( p[0] == 'r' && p[1] == '=' && p[2] == 'h' )
            {
                /* Change boot source to "boot from flash". */
                p[2] = 'f';
                writeNvramData();
                break;
            }
        printf( "Resetting board...\n" );
        kerSysMipsSoftReset();
    }

    return( res );
}


/*  *********************************************************************
    *  cfe_go(la)
    *  
    *  Starts a previously loaded program.  cfe_loadargs.la_entrypt
    *  must be set to the entry point of the program to be started
    *  
    *  Input parameters: 
    *  	   la - loader args
    *  	   
    *  Return value:
    *  	   does not return
    ********************************************************************* */

void cfe_go(cfe_loadargs_t *la)
{
    unsigned short gpio;

    if (la->la_entrypt == 0) {
        xprintf("No program has been loaded.\n");
        return;
    }

    if (net_getparam(NET_DEVNAME)) {
        xprintf("Closing network.\n");
        net_uninit();
    }

    xprintf("Starting program at 0x%p\n",la->la_entrypt);

    setPowerOnLedOn();
    if( BpGetBootloaderResetCfgLedGpio( &gpio ) == BP_SUCCESS )
        setLedOn( gpio );

    cfe_start(la->la_entrypt);
}

static int bootImage(char *fileSys, char *device, int zflag, char *imageName)
{
    cfe_loadargs_t la;
    int res;

    // elf only
    la.la_filesys = fileSys;
    la.la_filename = imageName;
	la.la_device = device;
	la.la_options = 0;
    la.la_maxsize = 0;
    la.la_address = 0;
	la.la_flags =  zflag;

	res = bcm63xx_cfe_elfload(&la); 
    if (res != 0)
        return res;

    if (la.la_flags & LOADFLG_NOISY) 
	    xprintf("Entry at 0x%p\n",la.la_entrypt);
    if ((la.la_flags & LOADFLG_EXECUTE) && (la.la_entrypt != 0)) {
	    cfe_go(&la);
	}

    return res;
}

// Compressed image head format in Big Endia:
// 1) Text Start address:    4 bytes
// 2) Program Entry point:   4 bytes
// 3) Compress image Length: 4 bytes
// 4) Compress data starts:  compressed data
static int bootCompressedImage(unsigned int *puiCmpImage, int retry)
{
	unsigned char *pucSrc;
	unsigned char *pucDst;
    unsigned char *pucEntry;
    unsigned int dataLen;
    int ret = 0;
    cfe_loadargs_t la;
    unsigned int *puiOrigCmpImage = puiCmpImage;
    unsigned int *puiNewCmpImage = NULL;
    unsigned int *puiOldCmpImage = NULL;
    PFILE_TAG pTag1 = getTagFromPartition(1);
    PFILE_TAG pTag2 = getTagFromPartition(2);
    PFILE_TAG pCurTag = NULL;
AlertLed_Off();//roy
    if( (unsigned long) puiOrigCmpImage > FLASH_BASE && pTag1 && pTag2 )
    {
        PFILE_TAG pNewTag = NULL;
        PFILE_TAG pOldTag = NULL;
        int seq1 = atoi(pTag1->imageSequence);
        int seq2 = atoi(pTag2->imageSequence);

        if( seq1 > seq2 )
        {
            pNewTag = pTag1;
            pOldTag = pTag2;
        }
        else
        {
            pNewTag = pTag2;
            pOldTag = pTag1;
        }

        puiNewCmpImage = (unsigned int *)
            (atoi(pNewTag->kernelAddress) + BOOT_OFFSET);
        puiOldCmpImage = (unsigned int *)
            (atoi(pOldTag->kernelAddress) + BOOT_OFFSET);

        if( puiOrigCmpImage == puiOldCmpImage )
        {
            printf("Booting from previous image (0x%8.8lx) ...\n",
                (unsigned long) atoi(pOldTag->rootfsAddress) +
                BOOT_OFFSET - TAG_LEN);
            pCurTag = pOldTag;
        }
        else
        {
            printf("Booting from latest image (0x%8.8lx) ...\n",
                (unsigned long) atoi(pNewTag->rootfsAddress) +
                BOOT_OFFSET - TAG_LEN);
            pCurTag = pNewTag;
        }
    }

    /* If the entire flash part is not memory mapped, copy the compressed
     * image to an SDRAM address.
     */
    if( flash_get_total_memory_mapped_size() != flash_get_total_size() )
    {
        /* Copy compressed image to SDRAM. */
        extern unsigned char *mem_topofmem;
        FLASH_ADDR_INFO info;
        unsigned char *pDest = (unsigned char *) mem_topofmem + 1024;
        unsigned char *pImgEnd;

        kerSysFlashAddrInfoGet( &info );
        pImgEnd = flash_get_memptr( info.flash_persistent_start_blk );
        kerSysReadFromFlash( pDest, (unsigned long) puiCmpImage,
            (unsigned long) pImgEnd - (unsigned long) puiCmpImage );

        puiCmpImage = (unsigned int *) pDest;
    }

    pucDst = (unsigned char *) *puiCmpImage;
    pucEntry = (unsigned char *) *(puiCmpImage + 1);
    dataLen = (unsigned int) *(puiCmpImage + 2);
    pucSrc = (unsigned char*) (puiCmpImage + 3);

    printf("Code Address: 0x%08X, Entry Address: 0x%08x\n",
        (unsigned int) pucDst, (unsigned int) pucEntry);

    if( pCurTag )
    {
        unsigned int *puiImg= NULL;
        int nImgLen = 0;
        unsigned long ulCrc, ulImgCrc;

        /* Check Linux file system CRC */
        ulImgCrc = *(unsigned long *) (pCurTag->imageValidationToken+CRC_LEN);
        if( ulImgCrc )
        {
            puiImg = (unsigned int *)(atoi(pCurTag->rootfsAddress)+BOOT_OFFSET);
            nImgLen = atoi(pCurTag->rootfsLen);

            ulCrc = CRC32_INIT_VALUE;
            ulCrc = getCrc32((char *) puiImg, (UINT32) nImgLen, ulCrc);      
            if( ulCrc != ulImgCrc)
            {
                printf("Linux file system CRC error.  Corrupted image?\n");
                ret = -1;
            }
        }

        /* Check Linux kernel CRC */
        ulImgCrc = *(unsigned long *)(pCurTag->imageValidationToken+(CRC_LEN*2));
        if( ulImgCrc )
        {
            puiImg = (unsigned int *) puiCmpImage;
            nImgLen = atoi(pCurTag->kernelLen);

            ulCrc = CRC32_INIT_VALUE;
            ulCrc = getCrc32((char *) puiImg, (UINT32) nImgLen, ulCrc);      
            if( ulCrc != ulImgCrc)
            {
                printf("Linux kernel CRC error.  Corrupted image?\n");
                ret = -1;
            }
        }
    }
    if( ret == 0 )
    {
        ret = decompress_lzma_7z(pucSrc, dataLen, pucDst, 23*1024*1024);
        if (ret != 0) 
            printf("Failed on decompression.  Corrupted image?\n");
    }
    if (ret != 0) 
    {
        /* Try to boot from the other flash image, if one exists. */
        if( retry == TRUE && (unsigned long) puiOrigCmpImage > FLASH_BASE &&
            pTag1 && pTag2 )
        {
            unsigned int *flash_addr_kernel;

            if( puiOrigCmpImage == puiOldCmpImage )
                flash_addr_kernel = puiNewCmpImage;
            else
                flash_addr_kernel = puiOldCmpImage;

            ret = bootCompressedImage( flash_addr_kernel, FALSE );
        }

        return ret;
    }
    printf("Decompression OK!\n");
    la.la_entrypt = (long) pucEntry;
    printf("Entry at 0x%p\n",la.la_entrypt);
    cfe_go(&la);  // never return...
    return ret;
}


static int autoRun(char *imageName)
{
    char ipImageName[BOOT_FILENAME_LEN + BOOT_IP_LEN];
    int ret;

    if (bootInfo.runFrom == 'f' && !imageName)
    {
        PFILE_TAG pTag = getBootImageTag();
        int flash_addr_kernel = atoi(pTag->kernelAddress) + BOOT_OFFSET;

        ret = bootCompressedImage((unsigned int *)flash_addr_kernel, TRUE);
    }
    else // loading from host
    {
        if (imageName)
        {
            if (strchr(imageName, ':'))
                strcpy(ipImageName, imageName);
            else
            {
                strcpy(ipImageName, bootInfo.hostIp);
                strcat(ipImageName, ":");
                strcat(ipImageName, imageName);
            }
        }
        else  // use default host file name
        {
            strcpy(ipImageName, bootInfo.hostIp);
            strcat(ipImageName, ":");
            strcat(ipImageName, bootInfo.hostFileName);
        }

        // try uncompressed image first
        ret = bootImage("tftp", "eth0",  LOADFLG_EXECUTE | LOADFLG_NOISY, ipImageName);

        if( ret == CFE_ERR_NOTELF )
        {
            uint8_t *ptr = (uint8_t *) KERNADDR(FLASH_STAGING_BUFFER);
            // next try as a compressed image
            printf("Retry loading it as a compressed image.\n");
            if ((ret = loadRaw(ipImageName, ptr)) > 0)
                bootCompressedImage((unsigned int *) ptr, TRUE);  
        }
    }

    return ret;
}

                     
// run program from compressed image in flash or from tftped program from host
static int ui_cmd_run_program(ui_cmdline_t *cmd,int argc,char *argv[])
{
    int ret;

    if( getBootImageTag() )
    {
        char *imageName;

        imageName = cmd_getarg(cmd, 0);
        g_processing_cmd = 1;
        ret = autoRun(imageName);
    }
    else
    {
        printf("ERROR: There is not a valid image to boot from.\n");
        ret = CFE_ERR_FILENOTFOUND;
    }

    return( ret );
}


static int ui_cmd_print_system_info(ui_cmdline_t *cmd,int argc,char *argv[])
{
    return printSysInfo();
}


static int ui_cmd_change_bootline(ui_cmdline_t *cmd,int argc,char *argv[])
{
    return changeBootLine();
}
extern int ui_init_netcmds(void);

static int ui_init_bcm63xx_cmds(void)
{
    console_name = fakeConsole;     // for cfe_rawfs strcmp
	// Used to flash an image that does not contain a FILE_TAG record.
    cmd_addcmd("flashimage",
	       ui_cmd_flash_router_image,
	       NULL,
	       "Flashes a compressed image after the bootloader.",
	       "eg. flashimage [hostip:]compressed_image_file_name",
	       "");

    cmd_addcmd("reset",
	       ui_cmd_reset,
	       NULL,
	       "Reset the board",
	       "",
	       "");

    cmd_addcmd("b",
	       ui_cmd_set_board_param,
	       NULL,
	       "Change board parameters",
	       "",
	       "");

    cmd_addcmd("i",
	       ui_cmd_erase_psi,
	       NULL,
	       "Erase persistent storage data",
	       "",
	       "");

    cmd_addcmd("f",
	       ui_cmd_flash_image,
	       NULL,
	       "Write image to the flash ",
           "eg. f [[hostip:]filename]<cr> -- if no filename, tftped from host with file name in 'Default host flash file name'",
           "");

    cmd_addcmd("c",
	       ui_cmd_change_bootline,
	       NULL,
	       "Change boot line parameters",
	       "",
	       "");

    cmd_addcmd("p",
	       ui_cmd_print_system_info,
	       NULL,
	       "Print boot line and board parameter info",
	       "",
	       "");

    cmd_addcmd("r",
	       ui_cmd_run_program,
	       NULL,
	       "Run program from flash image or from host depend on [f/h] flag",
	       "eg. r [[hostip:]filename]<cr> if no filename, use the file name in 'Default host run file name'",
	       "");

    cmd_addcmd("e",
	       ui_cmd_erase,
	       NULL,
	       "Erase [n]vram or [a]ll flash except bootrom",
	       "e [n/a]",
	       "");

    cmd_addcmd("w",
	       ui_cmd_write_whole_image,
	       NULL,
	       "Write the whole image start from beginning of the flash",
	       "eg. w [hostip:]whole_image_file_name",
	       "");

    return 0;
}


static int runDelay(int delayCount)
{
    int goAuto = 0;
    
    if (delayCount == 0)
    {
        goAuto = 1;
        return goAuto;
		}
				
    printf("***  Press any key to stop auto run (%d seconds) ***\n", delayCount);
    printf(" Auto run second count down: %d", delayCount);

//roy    cfe_sleep(CFE_HZ/8);        // about 1/4 second

    while (1)
    {
	    printf("\b%d", delayCount);
	    cfe_sleep(CFE_HZ);        // about 1 second
	   
        if (console_status())
            break; 
        if (--delayCount == 0)
        {
            goAuto = 1;
            break;
        }
	}
    printf("\b%d\n", delayCount);

    return goAuto;
}


void bcm63xx_run(int breakIntoCfe)
{
    ui_init_bcm63xx_cmds();
    printSysInfo();
    enet_init();
    if (!breakIntoCfe && runDelay(bootInfo.bootDelay))
        autoRun(NULL);        // never returns
}

