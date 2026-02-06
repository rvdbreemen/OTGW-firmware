/* 
***************************************************************************  
**  Filename  : safeTimers.h
**  Version : 1.0.0
**
**  Copyright (c) 2020 Willem Aandewiel
**  Copyright (c) 2026 Robert van den Breemen (improvements v1.0.0)
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/
#ifndef SAFETIMERS_H
#define SAFETIMERS_H

/*
 * safeTimers.h (original name timers.h) is developed by Erik
 * 
 * Willem Aandewiel and Robert van den Breemen made some changes due 
 * to the "how can I handle the millis() rollover" by Edgar Bonet and added 
 * CHANGE_INTERVAL() and RESTART_TIMER() macro's
 *
 * v1.0.0 Improvements (Robert van den Breemen):
 * 1. Fixed __TimeLeft__ logic using standard integer overflow arithmetic.
 * 2. Added "Spiral of Death" protection in __Due__ to prevent blocking loops.
 * 3. Optimize sync logic in SKIP_MISSED_TICKS_WITH_SYNC to O(1).
 * 4. Removed unseeded random() offset from DECLARE_TIMER macros.
 *    Rationale: random() was called before randomSeed(), producing predictable
 *    "random" values. This defeated the purpose of timer desynchronization.
 *    Timers now start synchronized, but the Spiral of Death protection (#2)
 *    prevents blocking if multiple timers fire simultaneously.
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
                                                        +timerName##_interval;              \
                      static byte     timerName##_type = getParam(1, __VA_ARGS__, 0);

#define DECLARE_TIMER_SEC(timerName, ...) \
                      static uint32_t timerName##_interval = (getParam(0, __VA_ARGS__, 0) * 1000),\
                                      timerName##_due  = millis()                           \
                                                        +timerName##_interval;              \
                      static byte     timerName##_type = getParam(1, __VA_ARGS__, 0);

#define DECLARE_TIMER_MS(timerName, ...)  \
                      static uint32_t timerName##_interval = (getParam(0, __VA_ARGS__, 0)), \
                                      timerName##_due  = millis()                           \
                                                        +timerName##_interval;              \
                      static byte     timerName##_type = getParam(1, __VA_ARGS__, 0);

#define DECLARE_TIMER   DECLARE_TIMER_MS


#define CHANGE_INTERVAL_MIN(timerName, ...) \
                                      timerName##_interval = (getParam(0, __VA_ARGS__, 0) *60*1000),\
                                      timerName##_due  = millis()                                   \
                                                       +timerName##_interval;                       \
                                      timerName##_type = getParam(1, __VA_ARGS__, 0);
#define CHANGE_INTERVAL_SEC(timerName, ...) \
                                      timerName##_interval = (getParam(0, __VA_ARGS__, 0) *1000),   \
                                      timerName##_due  = millis()                                   \
                                                       +timerName##_interval;                       \
                                      timerName##_type = getParam(1, __VA_ARGS__, 0);
#define CHANGE_INTERVAL_MS(timerName, ...)  \
                                      timerName##_interval = (getParam(0, __VA_ARGS__, 0) ),        \
                                      timerName##_due  = millis()                                   \
                                                       +timerName##_interval;                       \
                                      timerName##_type = getParam(1, __VA_ARGS__, 0);

#define CHANGE_INTERVAL CHANGE_INTERVAL_MS

#define TIME_LEFT(timerName)          ( __TimeLeft__(timerName##_due) ) 
#define TIME_LEFT_MS(timerName)       ( (TIME_LEFT(timerName) ) )
#define TIME_LEFT_MIN(timerName)      ( (TIME_LEFT(timerName) ) / (60 * 1000))
#define TIME_LEFT_SEC(timerName)      ( (TIME_LEFT(timerName) ) / 1000 )

#define ONCE(timerName)                ( __Once__(timerName##_due) )
#define ONCE_MS(timerName)             ( (ONCE(timerName) ) )
#define ONCE_MIN(timerName)            ( (ONCE(timerName) ) / (60 * 1000))
#define ONCE_SEC(timerName)            ( (ONCE(timerName) ) / 1000 ) 

#define TIME_PAST(timerName)          ( (timerName##_interval - TIME_LEFT(timerName)) )
#define TIME_PAST_MS(timerName)       ( (TIME_PAST(timerName) ) )
#define TIME_PAST_SEC(timerName)      ( (TIME_PAST(timerName) / 1000) )
#define TIME_PAST_MIN(timerName)      ( (TIME_PAST(timerName) / (60*1000)) )

#define RESTART_TIMER(timerName)      ( timerName##_due = millis()+timerName##_interval ); 

#define DUE(timerName)                ( __Due__(timerName##_due, timerName##_interval, timerName##_type) )

uint32_t __Due__(uint32_t &timer_due, uint32_t timer_interval, byte timerType)
{
  uint32_t now = millis();
  if ((int32_t)(now - timer_due) >= 0) 
  {
    // --- SPIRAL OF DEATH PROTECTION ---
    // If we are significantly behind (e.g. > 10 intervals), don't try to catch up or execute.
    // Reset the timer to now + interval to allow the main loop to recover.
    if ((int32_t)(now - timer_due) > (int32_t)(10 * timer_interval)) {
       timer_due = now + timer_interval;
       return 0;
    }

    switch (timerType) {
        case CATCH_UP_MISSED_TICKS:   
                  timer_due += timer_interval;
                  break;
        case SKIP_MISSED_TICKS_WITH_SYNC:
                  // this will calculate the next due, and skips passed due events 
                  // (missing due events)
                  // Use O(1) math instead of while loop to prevent blocking
                  {
                    uint32_t intervals_passed = (now - timer_due) / timer_interval;
                    timer_due += (intervals_passed + 1) * timer_interval;
                  }
                  break;
        case TIMER_TYPE_4:
                  if ((now - timer_due) >= (uint32_t)(timer_interval * 0.05))
                  {
                    // Too late - skip execution, align to next future slot
                    uint32_t intervals_passed = (now - timer_due) / timer_interval;
                    timer_due += (intervals_passed + 1) * timer_interval;
                    return 0;
                  }
                  // On time - execute, and schedule next aligned slot
                  {
                     uint32_t intervals_passed = (now - timer_due) / timer_interval;
                     timer_due += (intervals_passed + 1) * timer_interval;
                  }
                  break;
        // SKIP_MISSED_TICKS is default
        default:  timer_due = now + timer_interval;
                  break;
    }
    return timer_due;  
  }
  
  return 0;
  
} // __Due__()

uint32_t __TimeLeft__(uint32_t timer_due)
{
  // Simple subtraction casts to signed int handles rollover correctly 
  // as long as the interval is < ~24.8 days.
  int32_t remain = (int32_t)(timer_due - millis());
  if (remain >= 0) return (uint32_t)remain;
  return 0;
  
} // __TimeLeft__()

uint32_t __Once__(uint32_t timer_due)
{
  //false if timer is not due
  if (__TimeLeft__(timer_due) > 0) return 0;
  else return 1;
} // __Once__()

// process variadic from macro's
uint32_t getParam(uint32_t i, ...) 
{
  uint32_t parm = 0, p;
  va_list vl;
  va_start(vl,i);
  for (p=0; p<=i; p++)
  {
    parm=va_arg(vl,uint32_t);
  }
  va_end(vl);
  return parm;
} // getParam()

#endif

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
