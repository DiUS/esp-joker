# esp-joker

This is a small demo of Speech Recognition (SR) and Text-to-Speech (TTS) for the Espressif ESP-S3-BOX development kit. Contrary to the typical SR/TTS these days, both are performed directly on the microcontroller rather than relying on a cloud server.

When asked, the demo will deliver one of 200+ "dad" jokes of varying amusement level. Each response is generated on the fly to demonstrate the ability to turn any text into speech on demand, rather than having pre-generated it.

The TTS engine used is PicoTTS. It is not as polished as those engines running on far more capable server hardware, but it is a big step up from the older "Talkie" style TTS.

## Wake word and commands

As configured, it will respond to the "hey willow" wake word/phrase. Once woken, you can use any one of the following to request a joke:

 - tell me a joke
 - tell me another joke
 - another joke
 - entertain me
 - amuse me

You can also ask it to restart itself with "restart please".


## Building and flashing

Assuming the ESP-IDF has been set up and activated, and the ESP-S3-BOX is connected, simply:

```
  idf.py build
  idf.py flash
```

If you wish to also monitor the console output, use `idf.py monitor`.
