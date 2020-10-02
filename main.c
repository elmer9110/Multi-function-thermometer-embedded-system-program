

/**
 *
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include "lcd.h"
#include "inc/hw_ints.h"
#include "inc/hw_types.h"
#include "inc/hw_memmap.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "adc0.h"
#include "tm4c123gh6pm.h"


//PortD masks
#define AIN5_MASK 4

//PortE masks
#define AIN0_MASK 8
#define AIN3_MASK 1

//variable and data structure definitions
float filter[10];


void inithw()
{
    //Enable clock to run at 40Mhz
    SYSCTL_RCC_R = SYSCTL_RCC_XTAL_16MHZ | SYSCTL_RCC_OSCSRC_MAIN | SYSCTL_RCC_USESYSDIV | (4 << SYSCTL_RCC_SYSDIV_S);

    //Enable GPIO
    SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R3 | SYSCTL_RCGCGPIO_R4;
    _delay_cycles(3);

    //Set up potentiometer analog input 5
    GPIO_PORTD_AFSEL_R |= AIN5_MASK;
    GPIO_PORTD_DEN_R &= ~AIN5_MASK;
    GPIO_PORTD_AMSEL_R |= AIN5_MASK;

    //Set up temp sensor analog input 0
    GPIO_PORTE_AFSEL_R |= AIN0_MASK;
    GPIO_PORTE_DEN_R &= ~AIN0_MASK;
    GPIO_PORTE_AMSEL_R |= AIN0_MASK;

    //Set up humidity sensor analog input 3
    GPIO_PORTE_AFSEL_R |= AIN3_MASK;
    GPIO_PORTE_DEN_R &= ~AIN3_MASK;
    GPIO_PORTE_AMSEL_R |= AIN3_MASK;


    //Configuration of Hibernation Module
    HIB_IM_R |= HIB_IM_WC;
    HIB_CTL_R |=HIB_CTL_CLK32EN;
    while(HIB_MIS_R==0x0);
    HIB_CTL_R |= HIB_CTL_RTCEN;



}


void reverse(char* str, int len)
{
    int i = 0, j = len - 1, temp;
    while (i < j) {
        temp = str[i];
        str[i] = str[j];
        str[j] = temp;
        i++;
        j--;
    }
}


// Converts a given integer x to string str[].
// d is the number of digits required in the output.
// If d is more than the number of digits in x,
// then 0s are added at the beginning.
int intToStr(int x, char str[], int d)
{
    int i = 0;
    while (x) {
        str[i++] = (x % 10) + '0';
        x = x / 10;
    }

    // If number of digits required is more, then
    // add 0s at the beginning
    while (i < d)
        str[i++] = '0';

    reverse(str, i);
    str[i] = '\0';
    return i;
}

// Converts a floating-point/double number to a string.
void ftoa(float n, char* res, int afterpoint)
{
    // Extract integer part
    int ipart = (int)n;

    // Extract floating part
    float fpart = n - (float)ipart;

    // convert integer part to string
    int i = intToStr(ipart, res, 0);

    // check for display option after point
    if (afterpoint != 0) {
        res[i] = '.'; // add dot

        // Get the value of fraction part upto given no.
        // of points after dot. The third parameter
        // is needed to handle cases like 233.007
        fpart = fpart * pow(10, afterpoint);

        intToStr((int)fpart, res + i + 1, afterpoint);
    }
}

char* getCurrentTime()
{
    char daytime[16];
    char minutes[3];
    char semi_colon[1]=":";
    char am[2]="am";
    char pm[2]="pm";
    int hours=0;
    int minutes2=0;
    int current_sec=0;

    current_sec=HIB_RTCC_R;
    current_sec=current_sec % 86400;

    hours = (current_sec/3600);
    minutes2 = (current_sec -(3600*hours))/60;

    if(hours<12)
    {
        if(hours==0)
        {
            hours=12;
        }
        intToStr(hours,daytime,2);
        intToStr(minutes2,minutes,2);
        strcat(daytime,semi_colon);
        strcat(daytime,minutes);
        strcat(daytime,am);
    }
    else
    {
        if(hours==12)
        {
            hours=12;
        }
        else
        {
            hours=hours-12;
        }

        intToStr(hours,daytime,2);
        intToStr(minutes2,minutes,2);
        strcat(daytime,semi_colon);
        strcat(daytime,minutes);
        strcat(daytime,pm);
    }

    return daytime;

}

char* getPotVoltage()
{
    char result[16];
    char symbol[3]="%";
    float rawPot=0;
    setAdc0Ss3Mux(5);
    setAdc0Ss3Log2AverageCount(4);
    rawPot = readAdc0Ss3();
    rawPot=((rawPot+0.5) / 4096 * 3.3);
    rawPot=rawPot/4.94;
    rawPot=rawPot*100;
    ftoa(rawPot,result,1);
    strcat(result,symbol);
    return result;
}

char* getTemp()
{
    char temp[16];
    char deg[5]={223,'F'};
    float rawTemp=0;
    float avgTemp=0;
    int i;
    int N=10;


    for(i=N-1;i>0;i--)
    {
        filter[i]=filter[i-1];
    }

    setAdc0Ss3Mux(0);
    setAdc0Ss3Log2AverageCount(6);
    rawTemp= readAdc0Ss3();
    rawTemp=((rawTemp) / 4096 * 3.3);
    rawTemp= rawTemp*1000;
    rawTemp=rawTemp/10;
    rawTemp=((rawTemp * 9)/5 + 32);
    filter[0]=rawTemp;

    for(i=0;i<N;i++)
    {
        avgTemp+=filter[i];
    }
    avgTemp/=N;

    ftoa(avgTemp,temp,1);
    strcat(temp,deg);
    return temp;
}

char* getHumidity()
{
    char humid[10];
    char per[3]="%";
    float raw_voltage=0;
    float hum=0;

    setAdc0Ss3Mux(3);
    setAdc0Ss3Log2AverageCount(6);
    raw_voltage= readAdc0Ss3();
    raw_voltage=((raw_voltage) / 4096 * 3.3);

    hum=(raw_voltage-0.801)/0.030;
    ftoa(hum,humid,1);
    strcat(humid,per);
    return humid;


}


int main(void)
{

        inithw();
        LCD_init();
        initAdc0Ss3();
        char string[25]=NULL;
        int i;
        HIB_RTCLD_R=85200;

        while (1) {

            LCD_Print("Hello Elmer", "...................."); //Print 2 lines
            SysCtlDelay(80000000/3); //Delay
            LCD_Clear();


            strcpy(string,getPotVoltage());
            LCD_Print("Contrast Level:", string);
            SysCtlDelay(80000000/3); //Delay
            LCD_Clear();


            strcpy(string,getCurrentTime());
            LCD_Print("Current time:",string);
            SysCtlDelay(80000000/3); //Delay
            LCD_Clear();

            strcpy(string,getTemp());
            LCD_Print("Current temp:",string);
            SysCtlDelay(80000000/3); //Delay
            LCD_Clear();

            strcpy(string,getHumidity());
            LCD_Print("Humidity Level:",string);
            SysCtlDelay(80000000/3); //Delay
            LCD_Clear();

            LCD_PrintLn(0,"End of Message");
            SysCtlDelay(80000000/3); //Delay
            LCD_Clear();



        }


}
