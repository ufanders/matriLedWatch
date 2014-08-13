#include <pic.h>

//#include "Compiler.h"
#include "GenericTypeDefs.h"
#include "BMA150.h"

// Registers for the SPI module you want to use
#define SPICON1             SSP1CON1
#define SPISTAT             SSP1STAT
#define SPIBUF              SSP1BUF
#define SPISTAT_RBF         SSP1STATbits.BF
#define SPICON1bits         SSP1CON1bits
#define SPISTATbits         SSP1STATbits
#define SPIENABLE           SSP1CON1bits.SSPEN
#define SPI_INTERRUPT_FLAG  PIR1bits.SSPIF

// Tristate values for SCK/SDI/SDO lines
#define SPICLOCK            TRISCbits.TRISC3
#define SPIIN               TRISCbits.TRISC4
#define SPIOUT              TRISCbits.TRISC5

// Latch pins for SCK/SDI/SDO lines
#define SPICLOCKLAT         LATCbits.LATC3
#define SPIINLAT            LATCbits.LATC4
#define SPIOUTLAT           LATCbits.LATC5

// Port pins for SCK/SDI/SDO lines
#define SPICLOCKPORT        PORTCbits.RC3
#define SPIINPORT           PORTCbits.RC4
#define SPIOUTPORT          PORTCbits.RC5

#define BMA150_CS           LATCbits.LATC2
#define BMA150_CS_TRIS      TRISCbits.TRISC2

void InitBma150(void)
{
	WORD i;
	BMA150_REG reg;
	
    SPISTAT = 0x00;               // power on state
    SPICON1 = 0x82;
    SPISTATbits.CKE = 0;
    SPICLOCK = 0;
    SPIOUT = 0;                  // define SDO1 as output (master or slave)
    SPIIN = 1;                  // define SDI1 as input (master or slave)
    SPICON1bits.CKP = 1;
    SPIENABLE = 1;             // enable synchronous serial port

    BMA150_CS_TRIS = 0;
		
	reg.chip_id = BMA150_ReadByte(BMA150_CHIP_ID);
    Nop();	

    if(reg.chip_id != 0x2)
    {
        //while(1);
        return;
    }
	
	/* 
	Bits 5, 6 and 7 of register addresses 14h and 34h do contain critical sensor individual 
	calibration data which must not be changed or deleted by any means. 
 
	In order to properly modify addresses 14h and/or 34h for range and/or bandwidth selection 
	using bits 0, 1, 2, 3 and 4, it is highly recommended to read-out the complete byte, perform bit-
	slicing and write back the complete byte with unchanged bits 5, 6 and 7.  */ 
 
	reg.val = BMA150_ReadByte(BMA150_ADDR14);
    reg.range = BMA150_RANGE_8G;
	reg.bandwidth = BMA150_BW_50;
    BMA150_WriteByte(BMA150_ADDR14,reg.val);

	i = BMA150_ReadByte(BMA150_VERSION);
    Nop();

    i = BMA150_ReadByte(BMA150_ADDR11);
    Nop();

    BMA150_WriteByte(BMA150_ADDR11, 0x00);
    Nop();

    i = BMA150_ReadByte(BMA150_ADDR11);
    Nop();
}

void BMA150_WriteByte(BYTE address, BYTE data)
{
    BYTE storage;

    //See Important Notes section on page 10 note 1 of the v1.5 datasheet
    storage = 0x00;
    if(address == 0x14 || address == 0x34)
    {
        storage = BMA150_ReadByte(0x14) & 0xE0;
    }

    BMA150_CS = 0;
    SPI_INTERRUPT_FLAG = 0;
    SPIBUF = address;
    while (!SPI_INTERRUPT_FLAG);

    Nop();Nop();Nop();Nop();Nop();Nop();Nop();Nop(); //is this the 14msec pause?
    Nop();Nop();Nop();Nop();Nop();Nop();Nop();Nop();
    Nop();Nop();Nop();Nop();Nop();Nop();Nop();Nop();
    Nop();Nop();Nop();Nop();Nop();Nop();Nop();Nop();
    Nop();Nop();Nop();Nop();Nop();Nop();Nop();Nop();
    Nop();Nop();Nop();Nop();Nop();Nop();Nop();Nop();
    Nop();Nop();Nop();Nop();Nop();Nop();Nop();Nop();
    Nop();Nop();Nop();Nop();Nop();Nop();Nop();Nop();

    SPI_INTERRUPT_FLAG = 0;

    //See Important Notes section on page 10 note 2 of the v1.5 datasheet
    if(address == 0x0A)
    {
        SPIBUF = data & 0x7F;
    }
    else
    {
        SPIBUF = data | storage;
    }

    while (!SPI_INTERRUPT_FLAG);
    BMA150_CS = 1;
}


BYTE BMA150_ReadByte(BYTE address)
{
    BMA150_CS = 0;
    SPI_INTERRUPT_FLAG = 0;
    SPIBUF = BMA150_READ | address;
    while (!SPI_INTERRUPT_FLAG);

    Nop();Nop();Nop();Nop();Nop();Nop();Nop();Nop(); //is this the 14msec pause?
    Nop();Nop();Nop();Nop();Nop();Nop();Nop();Nop();
    Nop();Nop();Nop();Nop();Nop();Nop();Nop();Nop();
    Nop();Nop();Nop();Nop();Nop();Nop();Nop();Nop();
    Nop();Nop();Nop();Nop();Nop();Nop();Nop();Nop();
    Nop();Nop();Nop();Nop();Nop();Nop();Nop();Nop();
    Nop();Nop();Nop();Nop();Nop();Nop();Nop();Nop();
    Nop();Nop();Nop();Nop();Nop();Nop();Nop();Nop();

    SPI_INTERRUPT_FLAG = 0;
    SPIBUF = 0x00;
    while (!SPI_INTERRUPT_FLAG);
    BMA150_CS = 1;

    return SPIBUF;
}
