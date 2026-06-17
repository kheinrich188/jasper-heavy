#include "InfluxClient.h"

#include <HTTPClient.h>
#include <WiFi.h>

#include "Config.h"

namespace
{
  String urlEncode(const String &input)
  {
    String out;
    out.reserve(input.length() * 3);
    for (size_t i = 0; i < input.length(); i++)
    {
      const char c = input[i];
      const bool isAlphaNum = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
      if (isAlphaNum || c == '-' || c == '_' || c == '.' || c == '~')
      {
        out += c;
        continue;
      }
      char buf[4];
      snprintf(buf, sizeof(buf), "%%%02X", static_cast<unsigned char>(c));
      out += buf;
    }
    return out;
  }

  String buildWriteUrl()
  {
    String url = String(config::INFLUX_BASE_URL);
    if (!url.endsWith("/api/v2/write"))
    {
      if (url.endsWith("/"))
      {
        url.remove(url.length() - 1);
      }
      url += "/api/v2/write";
    }
    url += "?org=" + urlEncode(String(config::INFLUX_ORG)) +
           "&bucket=" + urlEncode(String(config::INFLUX_BUCKET)) + "&precision=s";
    return url;
  }

  bool isValidBufferedLine(const String &line)
  {
    if (!line.startsWith("cat_wheel,device=heltec_v3"))
    {
      return false;
    }
    if (line.indexOf("distance_total_m=") < 0 || line.indexOf("uptime_ms=") < 0 || line.indexOf("unix_ts=") < 0)
    {
      return false;
    }
    return !line.endsWith("=");
  }
} // namespace

namespace influx_client
{
  bool configured()
  {
    return strlen(config::INFLUX_BASE_URL) > 0 && strlen(config::INFLUX_ORG) > 0 &&
           strlen(config::INFLUX_BUCKET) > 0 && strlen(config::INFLUX_TOKEN) > 0;
  }

  String sanitizePayload(const String &payload, size_t &droppedLines)
  {
    droppedLines = 0;
    String clean;
    int start = 0;
    while (start < payload.length())
    {
      int end = payload.indexOf('\n', start);
      if (end < 0)
      {
        end = payload.length();
      }
      String line = payload.substring(start, end);
      line.trim();
      if (line.length() > 0)
      {
        if (isValidBufferedLine(line))
        {
          clean += line;
          clean += '\n';
        }
        else
        {
          droppedLines++;
        }
      }
      start = end + 1;
    }
    return clean;
  }

  bool uploadPayload(const String &payload)
  {
    if (WiFi.status() != WL_CONNECTED || !configured())
    {
      return false;
    }

    HTTPClient http;
    const String url = buildWriteUrl();
    if (!http.begin(url))
    {
      Serial.println("Sync: HTTP begin fehlgeschlagen.");
      return false;
    }

    http.addHeader("Authorization", "Token " + String(config::INFLUX_TOKEN));
    http.addHeader("Content-Type", "text/plain; charset=utf-8");
    http.addHeader("Accept", "application/json");
    http.setConnectTimeout(config::HTTP_CONNECT_TIMEOUT_MS);
    http.setTimeout(config::HTTP_RESPONSE_TIMEOUT_MS);
    Serial.printf("Sync: Upload startet (%d Bytes)\n", payload.length());
    const int code = http.POST(payload);
    const String responseBody = http.getString();
    http.end();

    if (code >= 200 && code < 300)
    {
      Serial.printf("Sync: %d Bytes erfolgreich uebertragen.\n", payload.length());
      return true;
    }

    Serial.printf("Sync: Upload fehlgeschlagen (HTTP %d), Daten bleiben lokal.\n", code);
    if (responseBody.length() > 0)
    {
      Serial.printf("Sync: Server-Fehler: %s\n", responseBody.c_str());
    }
    return false;
  }
} // namespace influx_client
