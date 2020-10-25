/* 
***************************************************************************  
**  Filename  : safeTimers.h
**
**  Copyright (c) 2020 Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/


/*
 * safeTimers.h (original name timers.h) is developed by Erik
 * 
 * Willem Aandewiel and Robert van den Breemen made some changes due 
 * to the "how can I handle the millis() rollover" by Edgar Bonet and added 
 * CHANGE_INTERVAL() and RESTART_TIMER() macro's
 *
 * see: https://arduino.stackexchange.com/questions/12587/how-can-i-handle-the-millis-rollover
 * 
 * DECLARE_TIMER_MIN(timername, interval, <timerType>)     // interval in minutes
 * DECLARE_TIMER_SEC(timername, interval, <timerType>)     // interval in seconds
 * DECLARE_TIMER_MS(timername,  interval, <timerType>)     // interval in milliseconds
 * DECLARE_TIMER(timername,     interval, <timerType>)     // interval in milliseconds
 *  Declares three static vars: 
 *    <timername>_due (uint32_t) for next execution
 *    <timername>_interval (uint32_t) for interval in seconds
 *    <timername>_type (byte)
 * 
 * <timerType> can either be:
 *   SKIP_MISSED_TICKS
 *   CATCH_UP_MISSED_TICKS
 *   SKIP_MISSED_TICKS_WITH_SYNC 
 *   TIMER_TYPE_4 
 * 
 * TIME_LEFT_MIN(timerName)
 *  returns the time left in minutes
 * TIME_LEFT_SEC(timerName)
 *  returns the time left in seconds
 * TIME_LEFT_MS(timerName)
 *  returns the time left in milliseconds
 * TIME_LEFT(timerName)
 *  returns the time left in milliseconds
 * 
 * CHANGE_INTERVAL_MIN(timername, interval, <timerType>)
 * CHANGE_INTERVAL_SEC(timername, interval, <timerType>)
 * CHANGE_INTERVAL_MS(timername,  interval, <timerType>)
 * CHANGE_INTERVAL(timername,     interval, <timerType>)
 *  Changes the static vars declared by DECLARE_TIMER(): 
 *    <timername>_due (uint32_t) for next execution
 *    <timername>_interval (uint32_t) for interval
 *    <timername>_type (byte) 
 *    
 * RESTART_TIMER(timername)
 *  updates <timername>_due = millis() + <timername>_interval
 *    
 * DUE(timername) 
 *  returns false (0) if interval hasn't elapsed since last DUE-time
 *          true (current millis) if it has
 *  updates <timername>_due
 *  
 *  Usage example:
 *  
 *  DECLARE_TIMER(screenUpdate, 200, SKIP_MISSED_TICKS)          // update screen every 200 ms
 *  ...
 *  loop()
 *  {
 *  ..
 *    if ( DUE(screenUpdate) ) {
 *      // update screen code
 *    }
 *    
 *    // to change the screenUpdate interval:
 *    CHANGE_INTERVAL(screenUpdate, 500, CATCH_UP_MISSED_TICKS); // change interval to 500 ms
 *    
 *    // to restart the screenUpdate interval:
 *    RESTART_TIMER(screenUpdate);                                // restart timer so next DUE is in 500ms
 *  }
 *  
*/

//--- timerType's -----------------------
#define SKIP_MISSED_TICKS             0
#define CATCH_UP_MISSED_TICKS         1
#define SKIP_MISSED_TICKS_WITH_SYNC   2
#define TIMER_TYPE_4                  3


#define DECLARE_TIMER_MIN(timerName, ...) \
                      static uint32_t timerName##_interval = (getParam(0, __VA_ARGS__, 0) * 60 * 1000),\
                                      timerName##_due  = millis()                           \
                                                        +timerName##_interval               \
                                                        +random(timerName##_interval / 3);  \
                      static byte     timerName##_type = getParam(1, __VA_ARGS__, 0);

#define DECLARE_TIMER_SEC(timerName, ...) \
                      static uint32_t timerName##_interval = (getParam(0, __VA_ARGS__, 0) * 1000),\
                                      timerName##_due  = millis()                           \
                                                        +timerName##_interval               \
                                                        +random(timerName##_interval / 3);  \
                      static byte     timerName##_type = getParam(1, __VA_ARGS__, 0);

#define DECLARE_TIMER_MS(timerName, ...)  \
                      static uint32_t timerName##_interval = (getParam(0, __VA_ARGS__, 0)), \
                                      timerName##_due  = millis()                           \
                                                        +timerName##_interval               \
                                                        +random(timerName##_interval / 3);  \
                      static byte     timerName##_type = getParam(1, __VA_ARGS__, 0);

#define DECLARE_TIMER   DECLARE_TIMER_MS


#define CHANGE_INTERVAL_MIN(timerName, ...) \
                                      timerName##_interval = (getParam(0, __VA_ARGS__, 0) *60*1000),\
                                      timerName##_due  = millis()                                   \
                                                       +timerName##_interval                        \
                                                       +random(timerName##_interval / 3);           \
                                      timerName##_type = getParam(1, __VA_ARGS__, 0);
#define CHANGE_INTERVAL_SEC(timerName, ...) \
                                      timerName##_interval = (getParam(0, __VA_ARGS__, 0) *1000),   \
                                      timerName##_due  = millis()                                   \
                                                       +timerName##_interval                        \
                                                       +random(timerName##_interval / 3);           \
                                      timerName##_type = getParam(1, __VA_ARGS__, 0);
#define CHANGE_INTERVAL_MS(timerName, ...)  \
                                      timerName##_interval = (getParam(0, __VA_ARGS__, 0) ),        \
                                      timerName##_due  = millis()                                   \
                                                       +timerName##_interval                        \
                                                       +random(timerName##_interval / 3);           \
                                      timerName##_type = getParam(1, __VA_ARGS__, 0);

#define CHANGE_INTERVAL CHANGE_INTERVAL_MS

#define TIME_LEFT(timerName)          ( __TimeLeft__(timerName##_due) ) 
#define TIME_LEFT_MS(timerName)       ( (TIME_LEFT(timerName) ) )
#define TIME_LEFT_MIN(timerName)      ( (TIME_LEFT(timerName) ) / (60 * 1000))
#define TIME_LEFT_SEC(timerName)      ( (TIME_LEFT(timerName) ) / 1000 )

#define TIME_PAST(timerName)          ( (timerName##_interval - TIME_LEFT(timerName)) )
#define TIME_PAST_MS(timerName)       ( (TIME_PAST(timerName) )
#define TIME_PAST_SEC(timerName)      ( (TIME_PAST(timerName) / 1000) )
#define TIME_PAST_MIN(timerName)      ( (TIME_PAST(timerName) / (60*1000)) )

#define RESTART_TIMER(timerName)      ( timerName##_due = millis()+timerName##_interval ); 

#define DUE(timerName)                ( __Due__(timerName##_due, timerName##_interval, timerName##_type) )


uint32_t __Due__(uint32_t &timer_due, uint32_t timer_interval, byte timerType)
{
  if ((int32_t)(millis() - timer_due) >= 0) 
  {
    switch (timerType) {
        case CATCH_UP_MISSED_TICKS:   
                  timer_due += timer_interval;
                  break;
        case SKIP_MISSED_TICKS_WITH_SYNC:
                  // this will calculate the next due, and skips passed due events 
                  // (missing due events)
                  // timer_due +=  (((uint32_t)(( timer_due - millis()) / timer_interval)+1) *timer_interval);
                  while ((int32_t)(millis() - timer_due) >= 0) 
                  {
                    timer_due  += timer_interval;
                  }
                  break;
        case TIMER_TYPE_4:
                  if ((millis() - timer_due) >= (uint32_t)(timer_interval * 0.05))
                  {
                    timer_due  += timer_interval;
                    return 0;
                  }
                  while ((int32_t)(millis() - timer_due) >= 0) 
                  {
                    timer_due  += timer_interval;
                  }
                  break;
        // SKIP_MISSED_TICKS is default
        default:  timer_due = millis() + timer_interval;
                  break;
    }
    return timer_due;  
  }
  
  return 0;
  
} // __Due__()


uint32_t __TimeLeft__(uint32_t timer_due)
{
  uint32_t tmp;
  byte state = 0;
  
  // timeline 0-------------------------SIGNED-MAX-------------------------UMAX
  // state=0  0---------------------------T-|---D--------------------------UMAX
  // state=0  0---------------------------d-|---t--------------------------UMAX
  // state=0  0-----------------------------|---------------T-------D------UMAX
  // state=0  0-----------------------------|---------------d--t-----------UMAX
  // state=0  0-------T-D-------------------|------------------------------UMAX
  // state=0  0---------d--t----------------|------------------------------UMAX
    
  // state=1  0---T-------------------------|--------------------------D---UMAX
  if ( (timer_due >= INT32_MAX) && (millis() < INT32_MAX) ) state = 1;  // millis() rolled-over
  
  // state=2  0--------D--------------------|---------------------T--------UMAX
  if ( (timer_due <= INT32_MAX) && (millis() > INT32_MAX) ) state = 2;  // _due rolled-over

  switch(state) {
        case 1:     //--- millis() rolled-over
        case 2:     //--- _due rolled-over
                    if ( (int32_t)((timer_due + UINT32_MAX) - millis()) >= 0 )
                          tmp = (timer_due + UINT32_MAX) - millis();
                    else  tmp = 0;
                    break;
        default:    if ( (int32_t)(timer_due - millis()) >= 0 )
                          tmp = timer_due - millis();
                    else  tmp = 0;
  }

  return tmp;
  
} // __TimeLeft__()


// process variadic from macro's
uint32_t getParam(int i, ...) 
{
  uint32_t parm, p;
  va_list vl;
  va_start(vl,i);
  for (p=0; p<=i; p++)
  {
    parm=va_arg(vl,uint32_t);
  }
  va_end(vl);
  return parm;
} // getParam()


/***************************************************************************
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to permit
* persons to whom the Software is furnished to do so, subject to the
* following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
* OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
* THE USE OR OTHER DEALINGS IN THE SOFTWARE.
* 
****************************************************************************
*/
