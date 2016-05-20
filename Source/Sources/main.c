/******************************************************************************
 * PWM Pulse Width Measurement Program
 *
 * Description:
 *
 * A standalone program based on the Freescale HCS12D / MC9S12DT256 platform to 
 *
 * monitor a PWM signal source, and report the distribution of measured periods
 *
 * of a 1000 samples. Interrupts trigger capture of system time which is used to
 *
 * evaluate the time between two events (signal rising edges)
 *
 * Author:
 *  		Siddharth Ramkrishnan (sxr4316@rit.edu)	: 03.16.2016
 *
 *****************************************************************************/


// system includes
#include <hidef.h>      /* common defines and macros */
#include <stdio.h>      /* Standard I/O Library */

// project includes
#include "types.h"
#include "derivative.h" /* derivative-specific definitions */

// Definitions

// Change this value to change the frequency of the output compare signal.
//
#define OC_FREQ_HZ    ((UINT16)10)

// Macro definitions for determining the TC1 value for the desired frequency in Hz (OC_FREQ_HZ).
//
#define BUS_CLK_FREQ  ((UINT32) 2000000)   
#define PRESCALE      ((UINT16)  2)         
#define TC1_VAL       ((UINT16)  (((BUS_CLK_FREQ / PRESCALE) / 2) / OC_FREQ_HZ))

#define PERIODLOLIM   (UINT16)   950
#define PERIODHILIM   (UINT16)  1050

static  UINT8  edge_detected,error_detected;
static  UINT16 PulsTime[PERIODHILIM-PERIODLOLIM+1];
static  UINT16 PulsCntr,CurrTime,PrevTime;
static  UINT32 PulsPeriod;


// Initializes SCI0 for 8N1, 9600 baud, polled I/O
// The value for the baud selection registers is determined
// using the formula:
//
// SCI0 Baud Rate = ( 2 MHz Bus Clock ) / ( 16 * SCI0BD[12:0] )
//--------------------------------------------------------------
void InitializeSerialPort(void)
{
    // Set baud rate to ~9600 (See above formula)
    //
    SCI0BD = 13;          
    
    // Enable the transmitter and receiver.
    //
	SCI0CR2_TE = 1;
    SCI0CR2_RE = 1;
}


// Initializes I/O and timer settings for the demo.
//--------------------------------------------------------------       
void InitializeTimer(void)
{
  // Set the timer prescaler to %2, since the bus clock is at 2 MHz,timer should run at 1 MHz
  //
  TSCR2_PR0 = 1;
  TSCR2_PR1 = 0;
  TSCR2_PR2 = 0;      
    
  // Enable input capture on Channel 1 
  //
  TIOS_IOS1 = 0;
  
  
  // Set up input capture edge control to capture on a rising edge.
  //
  TCTL4_EDG1A = 1;
  TCTL4_EDG1B = 0;
   
  // Clear the input capture Interrupt Flag (Channel 1)
  //
  TFLG1 = TFLG1_C1F_MASK;
  
  // Enable the input capture interrupt on Channel 1;
  //
  TIE_C1I = 1; 
  
  // Enable the timer
  //
  TSCR1_TEN = 1;
   
  // Enable interrupts via macro provided by hidef.h
  //
  EnableInterrupts;
}


// Output Compare Channel 1 Interrupt Service Routine refreshes TC1 and clears the interrupt flag.
//          
// The following line must be added to the Project.prm file in order for this ISR to be placed in the correct location:
//
//		VECTOR ADDRESS 0xFFEC OC1_isr 
//
#pragma push
#pragma CODE_SEG __SHORT_SEG NON_BANKED
//--------------------------------------------------------------       
void interrupt 9 OC1_isr( void )
{

   // This interrupt trigggers  when positive edge is detected at the IO Port.
   //
   // The current time , when the edge was detected at the port is captured.
   //
           
      CurrTime = TC1;
      
      edge_detected = 1;
      
      if ( (PulsCntr < 1000) && (PrevTime!=0))
      {
        if(CurrTime  >= PrevTime)
        {
        
          PulsPeriod  = CurrTime  - PrevTime  ;
          
        } else
        {
           
           PulsPeriod  = (65535 + CurrTime)  - PrevTime ;
           
        }
          
        if(   (PulsPeriod  >=  PERIODLOLIM) &&  (PulsPeriod  <=  PERIODHILIM)  )
        {
          
          PulsTime[PulsPeriod-PERIODLOLIM]=PulsTime[PulsPeriod-PERIODLOLIM]+1;
          
          PulsCntr  = PulsCntr  + 1;
          
        }
      }
          
      PrevTime  =   CurrTime;
      
   // set the interrupt enable flag for that port because it is cleared every everytime an interrupt fires.
   //
   TFLG1   =   TFLG1_C1F_MASK;
}
#pragma pop


// This function is called by printf in order to output data. Our implementation will 
// use polled serial I/O on SCI0 to output the character.
//
// Parameters: character to output
//--------------------------------------------------------------       
void TERMIO_PutChar(INT8 ch)
{
    // Poll for the last transmit to be complete
    do
    {
      // Nothing  
    } while (SCI0SR1_TC == 0);
    
    // write the data to the output shift register
    SCI0DRL = ch;
}


// Polls for a character on the serial port.
//
// Returns: Received character
//--------------------------------------------------------------       
UINT8 GetChar(void)
{ 
  // Poll for data
  do
  {
    // Nothing
  } while(SCI0SR1_RDRF == 0);
   
  // Fetch and return data from SCI0
  return SCI0DRL;
}

//Initialize the Pulse Period Capture data
void initPulseCapture()
{
        UINT16 loop_counter;

        TFLG1   =   0;
        
        PrevTime  =   0;
   
        CurrTime  =   0;
        
        edge_detected = 0;
        
        error_detected = 0;
        
        for(loop_counter=0;loop_counter<=(PERIODHILIM-PERIODLOLIM);loop_counter++)
        
          PulsTime[loop_counter]=0;
        
        PulsCntr  =   0;
        
        TFLG1   =   TFLG1_C1F_MASK;
}

// Entry point of our application code
//--------------------------------------------------------------       
//
void main(void)
{

  UINT8 loop_counter,user_input,post_error;
  
  UINT16 start_time,current_time;
  
  InitializeSerialPort();
  
  InitializeTimer();
   
  (void)printf("\n\rDigital Square Pulse Period Evaluation\n\r");
  
  do 
  {
    
    (void)initPulseCapture();
    
    start_time = TC1;
  
    current_time = start_time ;
  
    while(((current_time>start_time)?(current_time-start_time):(current_time+65536-start_time))<5000)
    {

      current_time = TC1;

    }
  
    if( edge_detected == 1) 
    {
      
      (void)printf("\n\rPOST Diagnosis successful\n\rSignal Measurment Initiated\n\r");
      
      (void)printf("\n\rPress any key to start Signal Evaluation\n\r");
      
      post_error = 0;
      
      do 
      {
        (void)initPulseCapture();
        
        do
        {

        // No operation. Hold the system until sufficient number of pulses

        }while(PulsCntr<1000);
        
        if(PulsCntr>=1000)
          {

            for (loop_counter=0;loop_counter<=(PERIODHILIM-PERIODLOLIM);loop_counter++)
            {

              if( PulsTime[loop_counter] != 0)
              {

                (void)printf("\n\rPulse Period %d us : %d ",PERIODLOLIM+loop_counter,PulsTime[loop_counter]);

                PulsTime[loop_counter]=0;

              }

            }

          }
   
        (void)printf("\n\rDo you want to measure again : (Y)es to start)");
        
        user_input = GetChar();
        
      }while(user_input==89||user_input==121);
  
    }
    else
    {
      
      post_error = 1;
       
      (void)printf("\n\rPOST Diagnosis failed. Please check hardware configuration\n\r");
    
      (void)printf("\n\rDo you want to test again ? (Y)es to start\n\r");
    
      user_input = GetChar();
      
    }
    
  }while((post_error == 1)&&(user_input==89||user_input==121));
  
  (void)printf("\n\rEnd of Program Execution\n\r");
  
}