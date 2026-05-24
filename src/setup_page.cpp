#include "setup_page.h"

#include <algorithm>
#include <sstream>

#include "timezone_catalog.h"

namespace homedeck {

std::vector<WifiNetwork> selectTopWifiNetworks(
    const std::vector<WifiNetwork>& networks,
    std::size_t limit) {
  std::vector<WifiNetwork> selected = networks;
  std::sort(selected.begin(), selected.end(), [](const WifiNetwork& left, const WifiNetwork& right) {
    return left.rssi > right.rssi;
  });
  if (selected.size() > limit) {
    selected.resize(limit);
  }
  return selected;
}

std::string htmlEscape(const std::string& value) {
  std::string escaped;
  for (const char ch : value) {
    switch (ch) {
      case '&':
        escaped += "&amp;";
        break;
      case '<':
        escaped += "&lt;";
        break;
      case '>':
        escaped += "&gt;";
        break;
      case '"':
        escaped += "&quot;";
        break;
      default:
        escaped += ch;
        break;
    }
  }
  return escaped;
}

std::string buildSetupPageHtml(
    const std::string& apSsid,
    const SetupConfig& values,
    const std::vector<WifiNetwork>& networks,
    const std::string& message) {
  std::ostringstream html;
  html << "<!doctype html><html><head><meta charset=\"utf-8\">";
  html << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html << "<title>HomeDeck Setup</title>";
  html << "<style>body{font-family:sans-serif;margin:24px;max-width:680px}label{display:block;margin-top:14px}input,select,button{font-size:16px;padding:8px;width:100%;box-sizing:border-box}.wifi button{margin:4px 0}.msg{color:#b00020}</style>";
  html << "</head><body><h1>HomeDeck Setup</h1>";
  html << "<p>AP: " << htmlEscape(apSsid) << " / 192.168.4.1</p>";
  if (!message.empty()) {
    html << "<p class=\"msg\">" << htmlEscape(message) << "</p>";
  }
  html << "<div class=\"wifi\"><strong>Wi-Fi 列表</strong>";
  for (const auto& network : networks) {
    html << "<button type=\"button\" onclick=\"pickSsid('" << htmlEscape(network.ssid) << "')\">";
    html << htmlEscape(network.ssid) << " (" << network.rssi << " dBm)</button>";
  }
  html << "</div><form method=\"post\" action=\"/save\">";
  html << "<label>Wi-Fi SSID<input id=\"wifi_ssid\" name=\"wifi_ssid\" value=\"" << htmlEscape(values.wifiSsid) << "\"></label>";
  html << "<label>Wi-Fi 密码<input name=\"wifi_password\" type=\"password\" value=\"" << htmlEscape(values.wifiPassword) << "\"></label>";
  html << "<label>时区<select name=\"timezone\">";
  std::size_t timezoneCount = 0;
  const auto* timezones = timezoneCatalog(&timezoneCount);
  for (std::size_t i = 0; i < timezoneCount; ++i) {
    html << "<option value=\"" << timezones[i].iana << "\"";
    if (values.timezoneIana == timezones[i].iana) {
      html << " selected";
    }
    html << ">" << timezones[i].label << "</option>";
  }
  html << "</select></label>";
  html << "<label><input id=\"auto_rtc\" name=\"auto_rtc\" type=\"checkbox\" value=\"1\"";
  if (values.autoRtcCorrection) {
    html << " checked";
  }
  if (values.wifiSsid.empty()) {
    html << " disabled";
  }
  html << "> 自动纠正 RTC</label>";
  html << "<label>NTP 服务器<input name=\"ntp_server\" value=\"" << htmlEscape(values.ntpServer) << "\"></label>";
  html << "<label>手动日期时间<input name=\"manual_datetime\" type=\"datetime-local\"></label>";
  html << "<button type=\"submit\">保存</button></form>";
  html << "<script>const ssid=document.getElementById('wifi_ssid');const auto=document.getElementById('auto_rtc');function sync(){auto.disabled=ssid.value.trim()==='';if(auto.disabled)auto.checked=false;}function pickSsid(v){ssid.value=v;sync();}ssid.addEventListener('input',sync);sync();</script>";
  html << "</body></html>";
  return html.str();
}

}  // namespace homedeck
