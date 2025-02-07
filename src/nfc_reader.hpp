#pragma once
#include <array>
#include <Adafruit_PN532.h>


template<size_t NPages>
struct nfc_data_t
{
    uint8_t uid[7];
    uint8_t uidLength; // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
    std::array<uint8_t, 4*NPages> data;
    size_t n_pages_read = 0;
};

Adafruit_PN532& init_pn532();
template<size_t NPages>
nfc_data_t<NPages> read_nfc(Adafruit_PN532& nfc)
{
    nfc_data_t<NPages> nfc_data;            

  // Wait for an NTAG203 card.  When one is found 'uid' will be populated with
  // the UID, and uidLength will indicate the size of the UUID (normally 7)
  const bool success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, nfc_data.uid, &nfc_data.uidLength);
  if (success) {
    // Display some basic information about the card
    Serial.println("Found an ISO14443A card");
    Serial.print("  UID Length: ");Serial.print(nfc_data.uidLength, DEC);Serial.println(" bytes");
    Serial.print("  UID Value: ");
    nfc.PrintHex(nfc_data.uid, nfc_data.uidLength);
    Serial.println("");

    if (nfc_data.uidLength == 7)
    {
        
        // We probably have an NTAG2xx card (though it could be Ultralight as well)
        Serial.println("Seems to be an NTAG2xx tag (7 byte UID)");
        
        // NTAG2x3 cards have 39*4 bytes of user pages (156 user bytes),
        // starting at page 4 ... larger cards just add pages to the end of
        // this range:
        
        // See: http://www.nxp.com/documents/short_data_sheet/NTAG203_SDS.pdf
        
        // TAG Type       PAGES   USER START    USER STOP
        // --------       -----   ----------    ---------
        // NTAG 203       42      4             39
        // NTAG 213       45      4             39
        // NTAG 215       135     4             129
        // NTAG 216       231     4             225
       
        while (nfc_data.n_pages_read < NPages)
        {
            // Read the 4 pages that contain user data
            const uint8_t page_to_read = static_cast<uint8_t>(nfc_data.n_pages_read);
            if(nfc.ntag2xx_ReadPage(page_to_read, nfc_data.data.data() + page_to_read*4))
            {
                nfc_data.n_pages_read++;
            }
            else
            {
                Serial.println("Unable to read the requested page!");

            }
        }
    }
    else
    {
      Serial.println("This doesn't seem to be an NTAG203 tag (UUID length != 7 bytes)!");
    }
    }
    return nfc_data;
}
