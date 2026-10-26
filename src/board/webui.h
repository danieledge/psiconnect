// webui.h - the Atom modem's web pages: the same settings as AT$ (built
// from the schema, so the two cannot drift), the status, the log, a factory
// reset. An access point with a captive portal when the modem is
// unconfigured (no WiFi network saved), in "config mode" (the button) or
// when asked (AT$WEB=2); pages on the network with a password otherwise.
// The Arduino core's own WebServer and DNSServer: no extra library, and
// its handlers run in loop() on the modem's own thread.
// MIT licence (see LICENSE at the top of the repository).
#ifndef ATOM_WEBUI_H
#define ATOM_WEBUI_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "../modem.h"

class AtomHal;

namespace am {

class WebUi
	{
public:
	WebUi();
	void Begin(Modem& aModem, AtomHal& aHal);
	void Apply(const Settings2& aS2);         // the settings changed (or config mode did)
	void Loop();
	void ApInfo(char* aOut, size_t aMax) const;
	void WebInfo(char* aOut, size_t aMax) const;
	bool ApUp() const { return iApUp; }
	const char* ApName() const { return iApName; }
	const char* ApPass() const { return iApPass; }    // the access point's password (saved, or made at random)
	bool OtaBusy() const { return iOtaBegun; }        // a firmware upload is being written

private:
	bool Allowed();                            // the password, or the access point
	void Head(const char* aTitle);
	void Tail();
	void Send(const char* aText);
	void SendP(const __FlashStringHelper* aText);
	void SendEsc(const char* aText);
	void PageStatus();
	void PageWifi();
	void PageSettings();
	void PageLog();
	void PageCrash();
	void ScreenBmp();                     // GET /screen.bmp: the LCD as drawn (AtomS3R)
	void PageSystem();
	void ExtGet(Extension& aExt, const ExtRoute& aRoute);   // an extension module's route (ext.h)
	void PageVpn();
	void PostWg();
	void PostNet();
	void PostPsiProxyTest();                   // /pptest: is PsiProxy there?
	void Back(bool aBad, const char* aText, const char* aBack);
	void ApiSettings();
	void Post();
	void NotFound();
	void StartAp();
	void StopAp();
	void Field(const SchemaEntry& aE);
	bool FromAp();
	bool OtaAuth();                            // auth for a firmware upload (no response sent)

	Modem* iModem;
	AtomHal* iHal;
	WebServer iServer;
	DNSServer iDns;
	bool iApUp;
	bool iStarted;
	bool iDnsUp;
	char iApName[20];
	char iApPass[33];
	Settings2 iS2;
	uint32_t iLastScanMs;
	bool iOtaAllowed;                          // the current upload passed auth
	bool iOtaBegun;                            // Update.begin() succeeded for it
	// a large reply sent a piece each pass of the loop, not all at once: a
	// second's worth of WiFi held up the serial line, whose 4-16 KB buffer
	// fills in under a second at 230400 with no flow control, and the
	// Psion's PPP link failed (a 1.5 MB file did it). AT$WEBWHOLE=1: all
	// at once, as before
	void StreamTick();
	struct TStream
		{
		WiFiClient client;                     // (the server has let go of it)
		const uint8_t* at;                     // what is still to go
		size_t left;
		uint32_t ms;                           // when a piece last went
		const char* path;                      // its route (a second request for it waits: 503)
		};
	static const int KStreams = 4;             // (a page's files come at once)
	TStream iStreams[KStreams];
	};

} // namespace am

#endif
