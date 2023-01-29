#ifndef OTGW_DISPLAY
#define OTGW_DISPLAY
#include <Arduino.h>
#include <U8g2lib.h>

class OTGW_Display
{

	public:
    enum class Pages {
      DISPLAY_IP,
      DISPLAY_TIME,
      DISPLAY_UPTIME,
      DISPLAY_VERSION
    };
		OTGW_Display ();
    ~OTGW_Display ();

    // Start
    void begin();

    // Function to periodically call to update itself.
    void tick ();

    // Put a message over top.
    // This will also force an update.
    void message ( const String &str = "" );

  private:
    void draw_welcome();
    void draw_message();

    void draw_display_ip();
    void draw_display_time();
    void draw_display_uptime();
    void draw_display_version();
    
	private:
    // U8g2 Object used for interaction with Oled.
    U8G2 *u8g2 = nullptr;
    String _message = "";
    Pages page = Pages::DISPLAY_IP;
};

#endif // OTGW_DISPLAY
