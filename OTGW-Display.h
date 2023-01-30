#ifndef OTGW_DISPLAY
#define OTGW_DISPLAY
#include <Arduino.h>
#include <U8g2lib.h>

/**
 * A small module that can use a small external display
 * to provide some useful feedback to the user.
 * For example:
 * * Current IP address.
 * * Current OTGW Version.
 * * Uptime
 * * Boiler status
 * * etc.
 *
 * To use this class, instantiate it. Call the begin function
 * after the iic is setup. Then periodically call tick() to draw a new page.
 */
class OTGW_Display
{

  public:
     // The different pages we can display.
    enum class Pages {
      DISPLAY_IP,
      DISPLAY_TIME,
      DISPLAY_UPTIME,
      DISPLAY_VERSION,
      DISPLAY_MESSAGE
    };
    // Constructor
    OTGW_Display ();
    // Destructor
    ~OTGW_Display ();

    // Start
    void begin();

    // Function to periodically call to update itself.
    void tick ();

    // Put a message over top.
    // This will also force an update.
    void message ( const String &str = "" );

  private:
    // The welcome screen.
    void draw_welcome();

    // Pages::DISPLAY_IP
    void draw_display_page_ip();
    // Pages::DISPLAY_TIME
    void draw_display_page_time();
    // Pages::DISPLAY_UPTIME
    void draw_display_page_uptime();
    // Pages::DISPLAY_VERSION
    void draw_display_page_version();
    // Pages::DISPLAY_MESSAGE
    void draw_display_page_message();

  private:
    // U8g2 Object used for interaction with Oled.
    U8G2 *u8g2 = nullptr;
    // A message that can be set from the code.
    String _message = "";
    // Current page.
    Pages page = Pages::DISPLAY_IP;
};

#endif // OTGW_DISPLAY
