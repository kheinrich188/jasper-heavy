#include "BufferStore.h"

#include <SPIFFS.h>

#include "Config.h"

namespace
{
  SemaphoreHandle_t bufferMutex = nullptr;

  class BufferLock
  {
  public:
    BufferLock()
    {
      if (bufferMutex != nullptr)
      {
        xSemaphoreTake(bufferMutex, portMAX_DELAY);
      }
    }

    ~BufferLock()
    {
      if (bufferMutex != nullptr)
      {
        xSemaphoreGive(bufferMutex);
      }
    }
  };

  void clearFile(const char *path)
  {
    File file = SPIFFS.open(path, FILE_WRITE);
    if (!file)
    {
      Serial.printf("SPIFFS: Konnte Datei nicht leeren: %s\n", path);
      return;
    }
    file.close();
  }

  size_t fileSize(const char *path)
  {
    File file = SPIFFS.open(path, FILE_READ);
    if (!file)
    {
      return 0;
    }
    const size_t size = file.size();
    file.close();
    return size;
  }
} // namespace

namespace buffer_store
{
  bool begin(bool purgeOnBoot)
  {
    bufferMutex = xSemaphoreCreateMutex();
    if (bufferMutex == nullptr)
    {
      Serial.println("SPIFFS: Konnte Buffer-Mutex nicht erstellen.");
      return false;
    }

    if (!SPIFFS.begin(true))
    {
      Serial.println("SPIFFS: Initialisierung fehlgeschlagen.");
      return false;
    }

    if (purgeOnBoot)
    {
      clearFile(config::BUFFER_FILE);
      clearFile(config::SYNC_BUFFER_FILE);
      Serial.println("SPIFFS: Alter Upload-Buffer beim Start geloescht.");
    }

    return true;
  }

  void appendLine(const String &line)
  {
    BufferLock lock;

    if (fileSize(config::BUFFER_FILE) > config::MAX_BUFFER_BYTES)
    {
      clearFile(config::BUFFER_FILE);
      Serial.println("SPIFFS: Aktiver Buffer war zu gross und wurde geleert.");
    }

    File file = SPIFFS.open(config::BUFFER_FILE, FILE_APPEND);
    if (!file)
    {
      Serial.println("SPIFFS: Konnte Buffer-Datei nicht oeffnen.");
      return;
    }
    // Influx line protocol expects LF line breaks. CRLF causes parse errors on numeric fields.
    file.print(line);
    file.print('\n');
    file.close();
  }

  bool prepareSyncSnapshot()
  {
    BufferLock lock;

    if (fileSize(config::SYNC_BUFFER_FILE) > 0)
    {
      return true;
    }

    if (fileSize(config::BUFFER_FILE) == 0)
    {
      return false;
    }

    SPIFFS.remove(config::SYNC_BUFFER_FILE);
    if (!SPIFFS.rename(config::BUFFER_FILE, config::SYNC_BUFFER_FILE))
    {
      Serial.println("SPIFFS: Konnte Sync-Snapshot nicht erstellen.");
      return false;
    }

    return true;
  }

  String readSyncPayload()
  {
    BufferLock lock;

    File file = SPIFFS.open(config::SYNC_BUFFER_FILE, FILE_READ);
    if (!file)
    {
      return "";
    }
    String payload = file.readString();
    file.close();
    payload.replace("\r", "");
    payload.replace("cat_wheel,device=heltec_v3pulses", "cat_wheel,device=heltec_v3 pulses");
    return payload;
  }

  void completeSyncSnapshot()
  {
    BufferLock lock;
    SPIFFS.remove(config::SYNC_BUFFER_FILE);
  }

  void discardOversizedBuffers()
  {
    BufferLock lock;

    if (fileSize(config::BUFFER_FILE) > config::MAX_BUFFER_BYTES)
    {
      clearFile(config::BUFFER_FILE);
      Serial.println("SPIFFS: Aktiver Buffer war zu gross und wurde geleert.");
    }
    if (fileSize(config::SYNC_BUFFER_FILE) > config::MAX_BUFFER_BYTES)
    {
      SPIFFS.remove(config::SYNC_BUFFER_FILE);
      Serial.println("SPIFFS: Sync-Snapshot war zu gross und wurde geloescht.");
    }
  }
} // namespace buffer_store
