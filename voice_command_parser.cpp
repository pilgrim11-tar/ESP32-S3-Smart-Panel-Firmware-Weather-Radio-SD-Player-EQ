#include "voice_command_parser.h"

namespace {

String normalize_transcript(const char *raw) {
  String out;
  if (!raw) {
    return out;
  }

  const uint8_t *p = reinterpret_cast<const uint8_t *>(raw);
  while (*p) {
    const uint8_t b = *p;
    char mapped = 0;

    // Basic UTF-8 Portuguese diacritic folding.
    if (b == 0xC3 && p[1] != 0) {
      const uint8_t b2 = p[1];
      switch (b2) {
        case 0x80: case 0x81: case 0x82: case 0x83: case 0x84:
        case 0xA0: case 0xA1: case 0xA2: case 0xA3: case 0xA4:
          mapped = 'a'; break;
        case 0x87: case 0xA7:
          mapped = 'c'; break;
        case 0x88: case 0x89: case 0x8A: case 0x8B:
        case 0xA8: case 0xA9: case 0xAA: case 0xAB:
          mapped = 'e'; break;
        case 0x8C: case 0x8D: case 0x8E: case 0x8F:
        case 0xAC: case 0xAD: case 0xAE: case 0xAF:
          mapped = 'i'; break;
        case 0x92: case 0x93: case 0x94: case 0x95: case 0x96:
        case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB6:
          mapped = 'o'; break;
        case 0x99: case 0x9A: case 0x9B: case 0x9C:
        case 0xB9: case 0xBA: case 0xBB: case 0xBC:
          mapped = 'u'; break;
        default:
          mapped = ' ';
          break;
      }
      p += 2;
    } else {
      char c = static_cast<char>(b);
      if (c >= 'A' && c <= 'Z') {
        c = static_cast<char>(c - 'A' + 'a');
      }
      if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
        mapped = c;
      } else {
        mapped = ' ';
      }
      p += 1;
    }

    if (mapped == ' ') {
      if (out.length() == 0 || out[out.length() - 1] == ' ') {
        continue;
      }
      out += ' ';
    } else {
      out += mapped;
    }
  }

  out.trim();
  if (out.length() > 0) {
    out = String(" ") + out + " ";
  }
  return out;
}

bool has_word(const String &text, const char *word) {
  if (!word || !word[0] || text.length() == 0) {
    return false;
  }
  String needle = " ";
  needle += word;
  needle += " ";
  return text.indexOf(needle) >= 0;
}

bool has_any(const String &text, const char *a, const char *b = nullptr, const char *c = nullptr, const char *d = nullptr) {
  return has_word(text, a) || has_word(text, b) || has_word(text, c) || has_word(text, d);
}

bool has_all(const String &text, const char *a, const char *b, const char *c = nullptr, const char *d = nullptr) {
  if (!has_word(text, a) || !has_word(text, b)) {
    return false;
  }
  if (c && !has_word(text, c)) {
    return false;
  }
  if (d && !has_word(text, d)) {
    return false;
  }
  return true;
}

uint16_t match_en(const String &text) {
  if (has_all(text, "go", "home") || has_all(text, "open", "home") || has_all(text, "home", "screen")) {
    return VOICE_COMMAND_GO_HOME;
  }
  if (has_all(text, "open", "radio")) {
    return VOICE_COMMAND_OPEN_RADIO;
  }
  if (has_all(text, "open", "settings") || has_all(text, "settings", "screen")) {
    return VOICE_COMMAND_OPEN_SETTINGS;
  }
  if (has_all(text, "open", "assistant") || has_all(text, "open", "ai")) {
    return VOICE_COMMAND_OPEN_ASSISTANT;
  }
  if (has_all(text, "open", "clock") || has_all(text, "show", "clock") || has_all(text, "show", "time")) {
    return VOICE_COMMAND_OPEN_CLOCK;
  }
  if (has_all(text, "next", "screen")) {
    return VOICE_COMMAND_NEXT_SCREEN;
  }
  if (has_all(text, "previous", "screen") || has_all(text, "back", "screen")) {
    return VOICE_COMMAND_PREVIOUS_SCREEN;
  }
  if (has_word(text, "play") && has_any(text, "hits", "pop")) {
    return VOICE_COMMAND_PLAY_HITS;
  }
  if (has_word(text, "play") && has_any(text, "lounge", "chill")) {
    return VOICE_COMMAND_PLAY_LOUNGE;
  }
  if (has_word(text, "play") && has_any(text, "portugal", "portuguese")) {
    return VOICE_COMMAND_PLAY_PORTUGAL;
  }
  if ((has_word(text, "stop") || has_word(text, "pause")) && has_any(text, "radio", "music")) {
    return VOICE_COMMAND_STOP_RADIO;
  }
  if (has_all(text, "volume", "up") || has_word(text, "louder")) {
    return VOICE_COMMAND_VOLUME_UP;
  }
  if (has_all(text, "volume", "down") || has_word(text, "quieter")) {
    return VOICE_COMMAND_VOLUME_DOWN;
  }
  if (has_all(text, "brightness", "up") || has_word(text, "brighter")) {
    return VOICE_COMMAND_BRIGHTNESS_UP;
  }
  if (has_all(text, "brightness", "down") || has_word(text, "dimmer")) {
    return VOICE_COMMAND_BRIGHTNESS_DOWN;
  }
  if (has_word(text, "self")) {
    return VOICE_COMMAND_SELF;
  }
  if (has_word(text, "ask")) {
    return VOICE_COMMAND_ASK;
  }
  if (has_word(text, "murmur")) {
    return VOICE_COMMAND_MURMUR;
  }
  if ((has_any(text, "mic", "microphone") && has_any(text, "on", "start", "listen")) || has_all(text, "listen", "on")) {
    return VOICE_COMMAND_MIC_ON;
  }
  if ((has_any(text, "mic", "microphone") && has_any(text, "off", "stop", "mute")) || has_all(text, "listen", "off")) {
    return VOICE_COMMAND_MIC_OFF;
  }
  if (has_all(text, "alarm", "on") || has_all(text, "enable", "alarm")) {
    return VOICE_COMMAND_ALARM_ON;
  }
  if (has_all(text, "alarm", "off") || has_all(text, "disable", "alarm")) {
    return VOICE_COMMAND_ALARM_OFF;
  }
  return VOICE_COMMAND_NONE;
}

uint16_t match_pt(const String &text) {
  if (has_all(text, "ir", "casa") || has_all(text, "ecra", "inicial")) {
    return VOICE_COMMAND_GO_HOME;
  }
  if (has_all(text, "abrir", "radio") || has_all(text, "ir", "radio")) {
    return VOICE_COMMAND_OPEN_RADIO;
  }
  if (has_all(text, "abrir", "definicoes") || has_all(text, "abrir", "configuracoes")) {
    return VOICE_COMMAND_OPEN_SETTINGS;
  }
  if (has_all(text, "abrir", "assistente")) {
    return VOICE_COMMAND_OPEN_ASSISTANT;
  }
  if (has_all(text, "abrir", "relogio") || has_all(text, "mostrar", "hora")) {
    return VOICE_COMMAND_OPEN_CLOCK;
  }
  if (has_all(text, "ecra", "seguinte") || has_all(text, "proximo", "ecra")) {
    return VOICE_COMMAND_NEXT_SCREEN;
  }
  if (has_all(text, "ecra", "anterior")) {
    return VOICE_COMMAND_PREVIOUS_SCREEN;
  }
  if (has_word(text, "tocar") && has_any(text, "hits", "pop")) {
    return VOICE_COMMAND_PLAY_HITS;
  }
  if (has_word(text, "tocar") && has_any(text, "lounge", "chill")) {
    return VOICE_COMMAND_PLAY_LOUNGE;
  }
  if ((has_word(text, "tocar") || has_word(text, "ouvir")) && has_any(text, "portugal", "portuguesa", "portugues")) {
    return VOICE_COMMAND_PLAY_PORTUGAL;
  }
  if (has_all(text, "parar", "radio") || has_all(text, "parar", "musica")) {
    return VOICE_COMMAND_STOP_RADIO;
  }
  if (has_all(text, "aumentar", "volume") || has_all(text, "mais", "volume")) {
    return VOICE_COMMAND_VOLUME_UP;
  }
  if (has_all(text, "diminuir", "volume") || has_all(text, "menos", "volume")) {
    return VOICE_COMMAND_VOLUME_DOWN;
  }
  if (has_all(text, "aumentar", "brilho") || has_all(text, "mais", "brilho")) {
    return VOICE_COMMAND_BRIGHTNESS_UP;
  }
  if (has_all(text, "diminuir", "brilho") || has_all(text, "menos", "brilho")) {
    return VOICE_COMMAND_BRIGHTNESS_DOWN;
  }
  if (has_word(text, "self")) {
    return VOICE_COMMAND_SELF;
  }
  if (has_word(text, "ask") || has_word(text, "pergunta")) {
    return VOICE_COMMAND_ASK;
  }
  if (has_word(text, "murmur")) {
    return VOICE_COMMAND_MURMUR;
  }
  if (has_all(text, "microfone", "ligado") || has_all(text, "ativar", "microfone") || has_all(text, "ouvir", "ligar")) {
    return VOICE_COMMAND_MIC_ON;
  }
  if (has_all(text, "microfone", "desligado") || has_all(text, "desativar", "microfone") || has_all(text, "ouvir", "parar")) {
    return VOICE_COMMAND_MIC_OFF;
  }
  if (has_all(text, "alarme", "ligado") || has_all(text, "ativar", "alarme")) {
    return VOICE_COMMAND_ALARM_ON;
  }
  if (has_all(text, "alarme", "desligado") || has_all(text, "desativar", "alarme")) {
    return VOICE_COMMAND_ALARM_OFF;
  }
  return VOICE_COMMAND_NONE;
}

}  // namespace

uint16_t voice_command_match_transcript(const char *transcript, const char *language_code) {
  const String text = normalize_transcript(transcript);
  if (text.length() == 0) {
    return VOICE_COMMAND_NONE;
  }

  bool prefer_pt = false;
  if (language_code && language_code[0]) {
    String lang = language_code;
    lang.toLowerCase();
    prefer_pt = lang.startsWith("pt");
  }

  uint16_t id = prefer_pt ? match_pt(text) : match_en(text);
  if (id == VOICE_COMMAND_NONE) {
    id = prefer_pt ? match_en(text) : match_pt(text);
  }
  return id;
}
