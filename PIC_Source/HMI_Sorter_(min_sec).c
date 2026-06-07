/*
 MURAT-TECH SORTER v.1.00
 By rafamuratt 03.2026
 MURAT-TECH CHANNEL: https://www.youtube.com/@Murat-TechChannel-EN
 MURAT-TECH HUB: https://murat-tech.eu/
   
 License
 This project is licensed under the Murat-Tech Source Available License v1.0. 
 Free for personal and educational use with attribution.
 Commercial use requires written authorization — contact info@murat-tech.eu
 https://github.com/rafamuratt/Industrial-Sorter-PIC-LCD-EEPROM?tab=License-1-ov-file
   
 If this project is helpfull for your application, please consider to support:
 https://www.paypal.com/donate/?hosted_button_id=8S8BJ9TT368VN
 
 Memory optimized 16x2 LCD HMI (4 bits mode) with full system control (core: PIC16F628A @4MHz, internal oscillator)

 Runs continuosly a transport belt/ system, sorting objects in boxes with following conditions:
 - As long the emergency button is not active
 - As long objects are present and moving along the belt (not stuck)
 - As long the job counter is not finished
 - As long the user doesn't reset or power off
 - As long the ESC button is not pressed

 EEPROM AUTO SAVE (power loss recovery):
 - The counter of last full box
 - All parameter settings: Sort time (min & sec), max pices per box and max box quantity (job size)

 LIVE PARAMETERS UPDATE:
 - Sort time can be updated any time, no impact on the job process (for safety the transport belt stops while in settings page)
 - If quantities are updated (pieces per box or box quantitiy), all counters are reset and a new job start

 PARAMETERS
 Sorting time:     from 0 to 1 minute and 59 seconds  (Default: 3s)
 Objects count:    from 1 to 999 pieces               (Default: 5 pieces)
 Job size:         from 1 to 99 boxes                 (Default: 4 boxes)
 Objects timeout:  fixed parameter                    (Default: approx 30s)
 
 Note: ONE_SECOND definition has to be empirically fine tunned (to approx. 1 second) for correct 30s obj timeout
*/


/* ============================================================================================== */
/* Definitions */
#define ONE_SECOND 18335                                                        // Aprox 1s time base (empiric data)

// IO'S MAPPING
#define INC        PORTA.B0
#define DEC        PORTA.B1
#define ENTER      PORTA.B2
#define ESC        PORTA.B3
#define SENSOR     PORTA.B4
#define CHECK_EMS  if(PORTA.B6 == 1)ems_halt()                                  // Active-High EMS button
#define ACK_EMS    PORTA.B7
#define MOTOR_RUN  PORTB.B2
#define SORT_OUT   PORTB.B3

// Lcd pinout settings
sbit LCD_RS at RB0_bit;
sbit LCD_EN at RB1_bit;
sbit LCD_D4 at RB4_bit;
sbit LCD_D5 at RB5_bit;
sbit LCD_D6 at RB6_bit;
sbit LCD_D7 at RB7_bit;

// Pin direction
sbit LCD_RS_Direction at TRISB0_bit;
sbit LCD_EN_Direction at TRISB1_bit;
sbit LCD_D4_Direction at TRISB4_bit;
sbit LCD_D5_Direction at TRISB5_bit;
sbit LCD_D6_Direction at TRISB6_bit;
sbit LCD_D7_Direction at TRISB7_bit;

// EEPROM MAPPING
#define slotA   0x00                                                            // last full boxes qty
#define slotB   0x01                                                            // parameter sortMin
#define slotC   0x02                                                            // parameter sortSec
#define slotD   0x03                                                            // parameter sortQty
#define slotE   0x05                                                            // parameter jobLimit
#define slotKey 0x10                                                            // used only to load defaults (1st run after uC flash)


/* ============================================================================================== */
/* Global variables */
unsigned int obj = 0, sortQty, bkpQty, objTick = 0;                             // obj shared in some loops to save ROM. objTick = check if objects are present
unsigned char adj = 0, pageState = 0, lastPage = 0xFF;                          // adj used to select fields + blink (settings page)
unsigned char totalSortSeconds, objTimeOut = 0;                                 // objTimeOut throws an error and stop system if no object
unsigned short boxes = 0, sortMin, bkpSortMin, sortSec, bkpSortSec;             // bkpSortSec is also used in some loops to save memrory
unsigned short jobLimit, bkpJobLimit;

typedef enum {
    VAL_UNI,
    VAL_DEZ,
    VAL_CEN,
    VAL_MIL,
    VAL_DEZM
} DispFormat;


/* ============================================================================================== */
/* Prototypes */
void refreshUI();
void disp_num(unsigned int num, char row, char col, DispFormat range);          // DispFormat: VAL_UNI, VAL_DEZ, VAL_CEN, VAL_MIL, VAL_DEZM
                                                                                // convert up to 5 digit integer and excludes left zeroes
void disp_blink(char row, char col);                                            // Blink the cursor at given row & column position

void ems_halt() {                                                               // Pooling based "interruption preprocessor" due lack of memory
    MOTOR_RUN = 0;
    SORT_OUT = 0;
    Lcd_Cmd(0x0C);                                                              // Blink off
    Lcd_Cmd(_LCD_CLEAR);
    Lcd_Out(1,5,"EMERGENCY");
    ACK_EMS = 1;
    while(1);
}

void objectCount();


/* ============================================================================================== */
/* Main */
void main() {
    CMCON = 0x07;                                                               // disable comparators
    TRISA = 0x7F;                                                               // 0111 1111 = 0x7F (all input, only RA7 as output)
    PORTA = 0x00;
    TRISB = 0x00;                                                               // 0000 0000 = 0x00 (whole RB port is output)
    PORTB = 0x00;

    Lcd_Init();
    Lcd_Cmd(0x0C);                                                              // display ON, cursor OFF, Blink OFF

    // SPLASH STARTUP
    Lcd_Out(1,4,"MURAT-TECH");
    Lcd_Chr(2,6,'S');                                                           // write "SORTER" char to char (to save memory)
    Lcd_Chr_Cp('O');
    Lcd_Chr_Cp('R');
    Lcd_Chr_Cp('T');
    Lcd_Chr_Cp('E');
    Lcd_Chr_Cp('R');
    
    delay_ms(2000);
    pageState = 0;

    // DATA RECOVERY - INLINED TO SAVE ROM
    boxes = EEPROM_Read(slotA);
    sortMin = EEPROM_Read(slotB);
    sortSec = EEPROM_Read(slotC);
    sortQty = (EEPROM_Read(slotD + 1) << 8) | EEPROM_Read(slotD);
    jobLimit = EEPROM_Read(slotE);

    if(EEPROM_Read(slotKey) != 42){                                             // used only to load defaults (very 1st run after uC flash)
        boxes = 0; sortMin = 0; sortSec = 3; sortQty = 5; jobLimit = 4;
    }

    bkpQty = sortQty; 
    bkpSortMin = sortMin; 
    bkpSortSec = sortSec; 
    bkpJobLimit = jobLimit;
    refreshUI();

    while (1) {
        CHECK_EMS;                                                              // Check emergency every 50ms (pooling on preprocessor)

        if(ENTER || ESC){
            char enterFlag;
            enterFlag = ENTER;
            while(ENTER || ESC);

            if(pageState == 0){                                                 // Confirmation page (START?)
                if(enterFlag){
                   pageState = 1;
                   // IF THE 1ST RUN, COMMIT RAM DEFAULTS TO EEPROM
                   if(EEPROM_Read(slotKey) != 42) {
                       EEPROM_Write(slotA, boxes);                              // Save to EEPROM (power loss recovery)
                       adj = 4;                                                 // Trick the logic to think just finished adjusting
                       pageState = 3;                                           // Force transition to the SAVE block
                   }
                }
                else
                   pageState = 2;
            }
            else if(pageState == 1){                                            // Home page (RUNNING)
                if(!enterFlag){
                   pageState = 2;
                   adj = 1;
                   MOTOR_RUN = 0;
                }
            }
            else if(pageState == 2){                                            // Settings page
                if(enterFlag){
                adj++;
                if(adj > 4)
                  pageState = 3;
                }
                else
                  pageState = 3;
            }
            else if(pageState == 3){                                            // Confirmation page (SAVE?)
                if(enterFlag){
                    // SAVE LIMIT (8-BITS)
                    EEPROM_Write(slotB, sortMin);
                    EEPROM_Write(slotC, sortSec);
                    EEPROM_Write(slotE, jobLimit);
                    EEPROM_Write(slotKey, 42);                                  // Mark EEPROM as initialized

                    // SAVE LIMIT (16-BITS)
                    EEPROM_Write(slotD, (char)sortQty);
                    delay_ms(20);
                    EEPROM_Write(slotD + 1, (char)(sortQty >> 8));
                    delay_ms(20);

                    if(sortQty != bkpQty || jobLimit != bkpJobLimit){           // reset counters only if quantities are changed (not the sort time)
                       boxes = 0;
                       obj = 0;
                    }

                    bkpQty = sortQty;                                           // update reference data if saved
                    bkpJobLimit = jobLimit;
                    bkpSortMin = sortMin;
                    bkpSortSec = sortSec;

                    ACK_EMS = 1;
                    for(obj=0; obj<10000; obj++);                               // Acknowledge blink
                    ACK_EMS = 0;
                    obj = 0;

                }
                else{
                    sortQty = bkpQty;                                           // Recover data if new entries are not saved
                    sortMin = bkpSortMin;
                    sortSec = bkpSortSec;
                    jobLimit = bkpJobLimit;
                    adj = 0;
                }
                pageState = 0;
            }
            refreshUI();                                                        // Only update screen on button press

        }

        if(pageState == 1){                                                     // Make sure boxes qty is not bigger than job size
            if(boxes >= jobLimit)
              boxes = 0;
            else{
              MOTOR_RUN = 1;
              objectCount();
            }
        }else if(pageState == 2){                                               // Update settings according with limits
            if(INC || DEC){
                short d;
                if(INC)
                  d = 1;
                else
                  d = -1;

                if(adj == 1){
                    sortMin += d;
                    if(sortMin > 1)
                       sortMin = 0;
                    if(sortMin < 0)
                       sortMin = 1;
                    }
                else if(adj == 2){
                    sortSec += d;
                    if(sortSec > 59)
                       sortSec = 0;
                    if(sortSec < 0)
                       sortSec = 59;
                }
                else if(adj == 3){
                    sortQty += d;
                    if(sortQty > 999)
                       sortQty = 1;
                    if(sortQty < 1)
                       sortQty = 999;
                }
                else if(adj == 4){
                    jobLimit += d;
                    if(jobLimit > 99)
                       jobLimit = 1;
                    if(jobLimit < 1)
                       jobLimit = 99;
                }
                refreshUI();
                while(INC || DEC);
            }
        }
    }
}


/* ============================================================================================== */
/* GUI write & variables update with shared text to save memory */
void refreshUI() {

    // MENUS TEXT BUILD
    if (pageState != lastPage) {
        Lcd_Cmd(0x0C);                                                          // Blink off
        Lcd_Cmd(_LCD_CLEAR);

        if (pageState == 0 || pageState == 3) {                                 // Pages 0 and 3
            if (pageState == 0)
               Lcd_Out(1,6,"START?");

            else
               Lcd_Out(1,6,"SAVE?");

            // SHARED BOTTOM LINE
            Lcd_Out(2,5,"ENT/ESC");

        }
        else {                                                                  // Pages 1 and 2
            if (pageState == 1)
               Lcd_Out(1,6,"RUNNING");
            else
               Lcd_Out(1,3,"Sort:   m   s");

            // SHARED BOTTOM LINE
            Lcd_Out(2,3,"Pc:    Box:");
        }
        lastPage = pageState;
    }

    // VARIABLE UPDATES
    if (pageState == 1) {
        disp_num(obj, 2, 6, VAL_CEN);
        disp_num(boxes, 2, 14, VAL_DEZ);
    }
    else if (pageState == 2) {
        disp_num(sortMin, 1, 10, VAL_UNI);
        disp_num(sortSec, 1, 13, VAL_DEZ);
        disp_num(sortQty, 2, 6, VAL_CEN);
        disp_num(jobLimit, 2, 14, VAL_DEZ);
        
    // BLINK SELECTOR (Memory efficient)
        if(adj == 1)
          disp_blink(1, 9);                                                     // Blink over minutes
        if(adj == 2)
          disp_blink(1, 13);                                                    // Blink over seconds
        if(adj == 3)
          disp_blink(2, 7);                                                     // Blink over pieces (quantity)
        if(adj == 4)
          disp_blink(2, 14);                                                    // Blink over boxes (job size)
    }
}


/* ============================================================================================== */
/* Object count update only when leaves the presence sensor */
void objectCount() {
    static char objFlag = 0;
    objTick++;
    
    if(objTick >= ONE_SECOND){                                                  // Aprox 1s time base (empiric data)
       objTick = 0;
       objTimeOut++;
       if(objTimeOut >= 30){                                                    // If no object in 30s (or object stuck), stop & throw error
          while(1){
              Lcd_Out(1,4,"OBJ TIMEOUT");
              MOTOR_RUN = 0;
              ACK_EMS = 1;
          }
       }
    }
    
    if(SENSOR)objFlag = 1;

    if (!SENSOR && objFlag) {
        objTick = 0;
        objTimeOut = 0;
        obj++;
        refreshUI();                                                            // Update Pc count on screen

        if (obj >= sortQty) {
            MOTOR_RUN = 0;
            SORT_OUT = 1;
            Lcd_Out(1,5,"SORTING..");
            
            // SORTER TIME DEFINED IN SETTINGS
            totalSortSeconds =  ((sortMin << 6) - (sortMin << 2)) + sortSec;    // sortMin(64 -4)--> sortMin = 60. Same as(sortMin*60) but shift and sub op are memory efficient

            for(bkpSortSec = 0; bkpSortSec < totalSortSeconds; bkpSortSec++) {
                // 20 ITERACTIONS OF 50MS = 1 SECOND
                for(obj = 0; obj < 20; obj++) {
                    CHECK_EMS;                                                  // Check the emergency state every 50ms even during sorting time
                    delay_ms(50);
                }
            }

            SORT_OUT = 0;
            obj = 0;
            boxes++;
            EEPROM_Write(slotA, boxes);                                         // Save to EEPROM (power loss recovery)

            if (jobLimit > 0 && boxes >= jobLimit) {                            // Check the end of job
                MOTOR_RUN = 0;
                pageState = 0;
                boxes = 0;
                EEPROM_Write(slotA, boxes);                                     // prepare data for the new job (after restart)
                Lcd_Cmd(_LCD_CLEAR);
                Lcd_Out(1,5,"JOB END");
                ACK_EMS = 1;                                                    // Acknowledge on until press reset or power loss
                while(1);
            }
            lastPage = 0xFF;                                                    // Force redraw of "RUNNING" labels
            refreshUI();
        }
        objFlag = 0;
    }
}


/* ============================================================================================== */
/* convert up to 5 digit integer and excludes left zeroes */
// To count more than 65535, use unsigned long (requires a lot of RAM)
void disp_num(unsigned int num, char row, char col, DispFormat range){          // DispFormat: VAL_UNI, VAL_DEZ, VAL_CEN, VAL_MIL, VAL_DEZM
     char dezM, mil, cen, dez, uni, size;
     short noZero = 0;

     switch(range){

           case VAL_DEZM:
              /* convert up to 5 digit integer and excludes left zeroes */
           dezM = (char)(num/10000);
           num %= 10000;
           mil  = (char)(num/1000);
           num %= 1000;
           cen  = (char)(num/100);
           num %= 100;
           dez  = (char)(num/10);
           uni  = (char)(num%10);

            if(!dezM && !noZero)                                                // if variable is zero and noZero is = zero
               Lcd_Chr(row, col, ' ');                                          // print a blanc space (to delete the left zeroes)
            else{
               Lcd_Chr(row, col, dezM+0x30);                                    // else sum '0' (ASCII) to the variable and print at given row and column
               noZero = 1;                                                      // when noZero = 1, all remaining checks will be false in order to fill up the next digits
        }

           if(!mil && !noZero) Lcd_Chr_Cp(' ');

           else{
              Lcd_Chr_Cp(mil+0x30);
              noZero = 1;
           }

           if(!cen && !noZero) Lcd_Chr_Cp(' ');

           else{
              Lcd_Chr_Cp(cen+0x30);
              noZero = 1;
              }

           if(!dez && !noZero) Lcd_Chr_Cp(' ');

           else{
              Lcd_Chr_Cp(dez+0x30);
              noZero = 1;
              }

           Lcd_Chr_Cp(uni+0x30);                                                   // no test about zero because to print a zero here has to be possible
        break;

        case VAL_MIL:
              /* convert up to 4 digit integer and excludes left zeroes */
           mil  = (char)(num/1000);
           num %= 1000;
           cen  = (char)(num/100);
           num %= 100;
           dez  = (char)(num/10);
           uni  = (char)(num%10);

           if(!mil && !noZero) Lcd_Chr(row, col, ' ');

           else{
              Lcd_Chr(row,col,mil+0x30);
              noZero = 1;
           }

           if(!cen && !noZero) Lcd_Chr_Cp(' ');

           else{
              Lcd_Chr_Cp(cen+0x30);
              noZero = 1;
              }

           if(!dez && !noZero) Lcd_Chr_Cp(' ');

           else{
              Lcd_Chr_Cp(dez+0x30);
              noZero = 1;
              }

           Lcd_Chr_Cp(uni+0x30);
        break;

        case VAL_CEN:
              /* convert up to 3 digit integer and excludes left zeroes */
           cen  = (char)(num/100);
           num %= 100;
           dez  = (char)(num/10);
           uni  = (char)(num%10);

           if(!cen && !noZero) Lcd_Chr(row, col, ' ');

           else{
              Lcd_Chr(row, col, cen+0x30);
              noZero = 1;
              }

           if(!dez && !noZero) Lcd_Chr_Cp(' ');

           else{
              Lcd_Chr_Cp(dez+0x30);
              noZero = 1;
              }

           Lcd_Chr_Cp(uni+0x30);
        break;

        case VAL_DEZ:
              /* convert up to 2 digit integer and excludes left zeroes */
           dez  = (char)(num/10);
           uni  = (char)(num%10);

           if(!dez && !noZero) Lcd_Chr(row, col, ' ');

           else{
              Lcd_Chr(row, col, dez+0x30);
              noZero = 1;
              }

           Lcd_Chr_Cp(uni+0x30);
        break;

        case VAL_UNI:
              Lcd_Chr(row, col, num+0x30);

        break;

        default:
              Lcd_Chr(row, col, num+0x30);
     }
}


/* ============================================================================================== */
/*   Blink the cursor at given row & column position    */
void disp_blink(char row, char col){
     if(row == 1) Lcd_Cmd(0x80 + col);                                         // Force Address jump (This stops the "auto-increment" from sticking)
     else         Lcd_Cmd(0xC0 + col);

     Lcd_Cmd(0x0D);                                                             // 0000 1101 = 0x0D = display ON, cursor OFF and blink ON
     delay_us(100);
}
